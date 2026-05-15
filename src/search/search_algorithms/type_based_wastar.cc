#include "type_based_wastar.h"

#include "../evaluation_context.h"
#include "../evaluator.h"
#include "../plugins/options.h"
#include "../task_utils/successor_generator.h"
#include "../utils/logging.h"

#include <cassert>
#include <limits>
#include <memory>
#include <set>

using namespace std;

namespace type_based_wastar {

TypeBasedWAstar::TypeBasedWAstar(const plugins::Options &opts)
    : SearchAlgorithm(opts),
      h_evaluator(opts.get<shared_ptr<Evaluator>>("h")),
      w(opts.get<double>("w")),
      reopen_closed_nodes(opts.get<bool>("reopen_closed")),
      step_counter(0),
      rng(opts.get<int>("random_seed")) {
    if (w < 1.0) {
        cerr << "TYPE WA*: weight w must be >= 1.0" << endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
}

// ---------------------------------------------------------------------------
// OPEN tracking helpers
// ---------------------------------------------------------------------------

void TypeBasedWAstar::add_to_open(const State &state, int g, int h) {
    wa_open.push({g + w * h, g, state.get_id()});

    TypeKey key = {h, g};
    type_buckets[key].push_back(state.get_id());
    type_open_count[key]++;

    int f = g + h;
    count_f[f]++;

    cached_h[state] = h;
    cached_f[state] = f;
    in_open[state] = true;
}

void TypeBasedWAstar::remove_from_open_tracking(const State &state) {
    int h = cached_h[state];
    int f = cached_f[state];
    TypeKey key = {h, f - h};  // g = f - h

    auto it = type_open_count.find(key);
    if (it != type_open_count.end()) {
        if (--it->second == 0)
            type_open_count.erase(it);
    }

    auto cit = count_f.find(f);
    if (cit != count_f.end()) {
        if (--cit->second == 0)
            count_f.erase(cit);
    }

    in_open[state] = false;
}

// ---------------------------------------------------------------------------
// FOCAL type-based selection
// ---------------------------------------------------------------------------

StateID TypeBasedWAstar::select_from_focal() {
    if (count_f.empty())
        return StateID::no_state;

    int fmin = count_f.begin()->first;
    double threshold = w * static_cast<double>(fmin);

    // Collect all type keys in FOCAL that have open states.
    vector<TypeKey> focal_types;
    for (auto &[key, cnt] : type_open_count) {
        if (cnt > 0 && static_cast<double>(key.first + key.second) <= threshold)
            focal_types.push_back(key);
    }

    if (focal_types.empty())
        return StateID::no_state;

    // Randomly shuffle to achieve uniform type selection.
    rng.shuffle(focal_types);

    for (const TypeKey &type_key : focal_types) {
        auto bit = type_buckets.find(type_key);
        if (bit == type_buckets.end())
            continue;

        vector<StateID> &bucket = bit->second;
        int expected_f = type_key.first + type_key.second;

        // Scan bucket: collect valid open entries, discard stale ones.
        vector<StateID> valid;
        vector<StateID> survivors;
        for (StateID id : bucket) {
            State s = state_registry.lookup_state(id);
            SearchNode node = search_space.get_node(s);
            if (!node.is_closed() && cached_f[s] == expected_f) {
                valid.push_back(id);
                survivors.push_back(id);
            }
            // Stale entries (closed or g-updated to a different bucket) are dropped.
        }
        bucket = move(survivors);

        if (!valid.empty()) {
            int idx = rng.random(static_cast<int>(valid.size()));
            return valid[idx];
        }
    }

    return StateID::no_state;
}

// ---------------------------------------------------------------------------
// Shared expansion logic
// ---------------------------------------------------------------------------

SearchStatus TypeBasedWAstar::do_expansion(const State &s, SearchNode &node) {
    if (check_goal_and_set_plan(s))
        return SOLVED;

    // Remove from OPEN tracking before closing — order matters for count_f.
    remove_from_open_tracking(s);
    node.close();
    statistics.inc_expanded();

    vector<OperatorID> applicable_ops;
    successor_generator.generate_applicable_ops(s, applicable_ops);

    for (OperatorID op_id : applicable_ops) {
        OperatorProxy op = task_proxy.get_operators()[op_id];
        if ((node.get_real_g() + op.get_cost()) >= bound)
            continue;

        State succ_state = state_registry.get_successor_state(s, op);
        statistics.inc_generated();

        SearchNode succ_node = search_space.get_node(succ_state);

        for (Evaluator *evaluator : path_dependent_evaluators)
            evaluator->notify_state_transition(s, op_id, succ_state);

        if (succ_node.is_dead_end())
            continue;

        int succ_g = node.get_g() + get_adjusted_cost(op);

        if (succ_node.is_new()) {
            EvaluationContext succ_ctx(succ_state, succ_g, false, &statistics);
            statistics.inc_evaluated_states();

            if (succ_ctx.is_evaluator_value_infinite(h_evaluator.get())) {
                succ_node.mark_as_dead_end();
                statistics.inc_dead_ends();
                continue;
            }

            int h = succ_ctx.get_evaluator_value(h_evaluator.get());
            succ_node.open(node, op, get_adjusted_cost(op));
            add_to_open(succ_state, succ_g, h);

            if (search_progress.check_progress(succ_ctx))
                statistics.print_checkpoint_line(succ_node.get_g());

        } else if (succ_node.get_g() > succ_g) {
            // Found a cheaper path to this state.
            if (reopen_closed_nodes) {
                if (succ_node.is_closed()) {
                    statistics.inc_reopened();
                } else if (in_open[succ_state]) {
                    // State is in OPEN with a worse g — remove old tracking entry.
                    remove_from_open_tracking(succ_state);
                }
                succ_node.reopen(node, op, get_adjusted_cost(op));
                // h is path-independent for admissible heuristics; reuse cached value.
                int h = cached_h[succ_state];
                add_to_open(succ_state, succ_node.get_g(), h);
            } else {
                succ_node.update_parent(node, op, get_adjusted_cost(op));
            }
        }
    }

    return IN_PROGRESS;
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void TypeBasedWAstar::initialize() {
    log << "Conducting TYPE WA* search, w=" << w
        << (reopen_closed_nodes ? ", reopening closed nodes" : "")
        << ", bound=" << bound << endl;

    assert(h_evaluator);

    set<Evaluator *> evals;
    h_evaluator->get_path_dependent_evaluators(evals);
    path_dependent_evaluators.assign(evals.begin(), evals.end());

    State initial_state = state_registry.get_initial_state();
    h_evaluator->notify_initial_state(initial_state);

    EvaluationContext eval_ctx(initial_state, 0, false, &statistics);
    statistics.inc_evaluated_states();

    if (eval_ctx.is_evaluator_value_infinite(h_evaluator.get())) {
        log << "Initial state is a dead end." << endl;
        return;
    }

    int h = eval_ctx.get_evaluator_value(h_evaluator.get());
    SearchNode node = search_space.get_node(initial_state);
    node.open_initial();
    add_to_open(initial_state, 0, h);

    if (search_progress.check_progress(eval_ctx))
        statistics.print_checkpoint_line(0);
}

// ---------------------------------------------------------------------------
// Search step
// ---------------------------------------------------------------------------

SearchStatus TypeBasedWAstar::step() {
    ++step_counter;

    if (step_counter % 2 == 1) {
        // ── Odd step: WA* ────────────────────────────────────────────────
        // Expand the node with minimum f_w = g + w*h.
        while (!wa_open.empty()) {
            WAEntry entry = wa_open.top();
            wa_open.pop();

            State s = state_registry.lookup_state(entry.id);
            SearchNode node = search_space.get_node(s);

            if (node.is_closed())
                continue;  // expanded by a type-based step already

            if (node.get_g() != entry.g)
                continue;  // stale entry: state reopened with a better g

            return do_expansion(s, node);
        }

        log << "Completely explored state space -- no solution!" << endl;
        return FAILED;

    } else {
        // ── Even step: type-based focal ──────────────────────────────────
        // Randomly pick a (h,g) type from FOCAL, then a state from that type.
        if (count_f.empty()) {
            log << "Completely explored state space -- no solution!" << endl;
            return FAILED;
        }

        StateID id = select_from_focal();
        if (id == StateID::no_state) {
            // FOCAL is non-empty if OPEN is non-empty (guaranteed by theory),
            // but fall back to WA* if all sampled buckets turned out stale.
            while (!wa_open.empty()) {
                WAEntry entry = wa_open.top();
                wa_open.pop();
                State s = state_registry.lookup_state(entry.id);
                SearchNode node = search_space.get_node(s);
                if (node.is_closed() || node.get_g() != entry.g)
                    continue;
                return do_expansion(s, node);
            }
            log << "Completely explored state space -- no solution!" << endl;
            return FAILED;
        }

        State s = state_registry.lookup_state(id);
        SearchNode node = search_space.get_node(s);
        return do_expansion(s, node);
    }
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

void TypeBasedWAstar::print_statistics() const {
    statistics.print_detailed_statistics();
    search_space.print_statistics();
}

void add_options_to_feature(plugins::Feature &feature) {
    SearchAlgorithm::add_options_to_feature(feature);
}

}  // namespace type_based_wastar
