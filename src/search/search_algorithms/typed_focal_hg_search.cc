#include "typed_focal_hg_search.h"

#include "../evaluation_context.h"
#include "../evaluator.h"
#include "../open_list_factory.h"
#include "search_common.h"

#include "../plugins/options.h"
#include "../task_utils/successor_generator.h"
#include "../utils/logging.h"
#include "../open_lists/best_first_open_list.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <memory>
#include <optional>
#include <set>

using namespace std;

namespace typed_focal_hg_search {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

TypedFocalHGSearch::TypedFocalHGSearch(const plugins::Options &opts)
    : SearchAlgorithm(opts),
      k(opts.get<int>("k")),
      pref_balance(opts.get<int>("pref_balance")),
      w(opts.get<double>("w")),
      open_evaluator(opts.get<shared_ptr<Evaluator>>("open_eval")),
      focal_evaluator(opts.get<shared_ptr<Evaluator>>("focal_eval")),
      preferred_evaluator(opts.get<shared_ptr<Evaluator>>("pref_eval", nullptr)),
      next_from_pref(false),
      rng(opts.get<int>("random_seed")) {

    plugins::Options openlist_opts(opts);
    openlist_opts.set("eval", open_evaluator);
    openlist_opts.set("pref_only", false);
    shared_ptr<OpenListFactory> olf =
        make_shared<standard_scalar_open_list::BestFirstOpenListFactory>(openlist_opts);
    open_list = olf->create_state_open_list();
}

// ---------------------------------------------------------------------------
// Bucket helpers — key is (h_focal, g)
// ---------------------------------------------------------------------------

void TypedFocalHGSearch::insert_typed(const State &s, int hf, int g, bool is_pref) {
    auto &buckets = use_pref_list(is_pref) ? typed_pref : typed_focal;
    TypeKey key{hf, g};
    buckets[key].push_back(s.get_id());
    h_focal_val[s]        = hf;
    generated_by_pref[s]  = is_pref;
    in_focal[s]           = true;
}

void TypedFocalHGSearch::remove_typed(const State &s) {
    int hf       = h_focal_val[s];
    int g        = search_space.get_node(s).get_g();
    bool is_pref = generated_by_pref[s];
    auto &buckets = use_pref_list(is_pref) ? typed_pref : typed_focal;

    TypeKey key{hf, g};
    auto it = buckets.find(key);
    if (it == buckets.end()) return;

    auto &vec  = it->second;
    StateID sid = s.get_id();
    for (int i = 0; i < static_cast<int>(vec.size()); ++i) {
        if (vec[i] == sid) {
            vec[i] = vec.back();
            vec.pop_back();
            break;
        }
    }
    if (vec.empty()) buckets.erase(it);
    in_focal[s] = false;
}

// Pick a bucket uniformly at random (like TypeWA*'s typed step), then pick
// a state uniformly at random within that bucket.
StateID TypedFocalHGSearch::extract_typed(bool from_pref) {
    auto &buckets = from_pref ? typed_pref : typed_focal;
    assert(!buckets.empty());

    // Uniform random bucket selection — std::advance is O(B) but B is small.
    int bucket_idx = rng.random(static_cast<int>(buckets.size()));
    auto it = buckets.begin();
    std::advance(it, bucket_idx);

    auto &vec  = it->second;
    assert(!vec.empty());

    int idx    = rng.random(static_cast<int>(vec.size()));
    StateID id = vec[idx];

    vec[idx] = vec.back();
    vec.pop_back();
    if (vec.empty()) buckets.erase(it);

    in_focal[state_registry.lookup_state(id)] = false;
    return id;
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void TypedFocalHGSearch::initialize() {
    const char *pb_str[] = {"typed_focal only", "alternating", "pref_first"};
    log << "Conducting Typed Focal HG Search (K=" << k << ")"
        << " without reopening closed nodes"
        << ", W suboptimality bound = " << w
        << ", pref_balance = " << pb_str[pref_balance]
        << ", preferred_evaluator = "
        << (!preferred_evaluator ? "none"
            : preferred_evaluator == focal_evaluator ? "focal_eval" : "custom")
        << endl;

    assert(open_list);

    set<Evaluator *> evals;
    open_list->get_path_dependent_evaluators(evals);
    focal_evaluator->get_path_dependent_evaluators(evals);
    if (preferred_evaluator)
        preferred_evaluator->get_path_dependent_evaluators(evals);
    path_dependent_evaluators.assign(evals.begin(), evals.end());

    State initial_state = state_registry.get_initial_state();
    open_evaluator->notify_initial_state(initial_state);
    focal_evaluator->notify_initial_state(initial_state);
    if (preferred_evaluator && preferred_evaluator != focal_evaluator)
        preferred_evaluator->notify_initial_state(initial_state);

    EvaluationContext eval_ctx(initial_state, 0, false, &statistics);
    statistics.inc_evaluated_states();

    if (open_list->is_dead_end(eval_ctx)) {
        log << "Initial state is a dead end." << endl;
        return;
    }

    if (search_progress.check_progress(eval_ctx))
        statistics.print_checkpoint_line(0);

    start_f_value_statistics(eval_ctx);

    SearchNode node = search_space.get_node(initial_state);
    node.open_initial();

    int fv = eval_ctx.get_evaluator_value_or_infinity(open_evaluator.get());
    int hf = eval_ctx.get_evaluator_value_or_infinity(focal_evaluator.get());
    f_value[initial_state]            = fv;
    generated_by_pref[initial_state]  = false;
    insert_typed(initial_state, hf, 0, false);

    count_f[fv]++;
    f_min = fv;

    print_initial_evaluator_values(eval_ctx);
}

// ---------------------------------------------------------------------------
// Search step
// ---------------------------------------------------------------------------

SearchStatus TypedFocalHGSearch::step() {
    const int    prev_f_min = f_min;
    const double w_f_min    = w * f_min;

    for (int batch = 0; batch < k; ++batch) {
        // ── Select extraction source ──────────────────────────────────────
        bool try_pref_first = (pref_balance == 2) ||
                              (pref_balance == 1 && next_from_pref);
        if (pref_balance == 1)
            next_from_pref = !next_from_pref;

        // ── Pick a state from FOCAL ───────────────────────────────────────
        StateID id = StateID::no_state;
        if (try_pref_first) {
            if (!typed_pref.empty())       id = extract_typed(true);
            else if (!typed_focal.empty()) id = extract_typed(false);
        } else {
            if (!typed_focal.empty())      id = extract_typed(false);
            else if (!typed_pref.empty())  id = extract_typed(true);
        }

        if (id == StateID::no_state) break;

        State s        = state_registry.lookup_state(id);
        SearchNode node = search_space.get_node(s);

        count_f[f_value[s]]--;
        if (count_f[f_value[s]] == 0)
            count_f.erase(f_value[s]);

        node.close();
        assert(!node.is_dead_end());
        statistics.inc_expanded();

        if (check_goal_and_set_plan(s)) return SOLVED;

        // ── Preferred operators at parent state ───────────────────────────
        const vector<OperatorID> *pref_ops = nullptr;
        vector<OperatorID> empty_pref;
        optional<EvaluationContext> opt_pref_ctx;
        if (preferred_evaluator && pref_balance > 0) {
            opt_pref_ctx.emplace(s, node.get_g(), false, &statistics, true);
            pref_ops = &opt_pref_ctx->get_preferred_operators(preferred_evaluator.get());
        } else {
            pref_ops = &empty_pref;
        }

        // ── Expand ────────────────────────────────────────────────────────
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

            if (succ_node.is_dead_end()) continue;

            const bool is_pref = find(pref_ops->begin(), pref_ops->end(), op_id)
                                  != pref_ops->end();

            if (succ_node.is_new()) {
                int succ_g = node.get_g() + get_adjusted_cost(op);
                EvaluationContext succ_ctx(succ_state, succ_g, false, &statistics);
                statistics.inc_evaluated_states();

                int fv = succ_ctx.get_evaluator_value_or_infinity(open_evaluator.get());

                if (open_list->is_dead_end(succ_ctx)) {
                    succ_node.mark_as_dead_end();
                    statistics.inc_dead_ends();
                    continue;
                }

                int hf = succ_ctx.get_evaluator_value_or_infinity(focal_evaluator.get());
                f_value[succ_state]           = fv;
                h_focal_val[succ_state]       = hf;
                generated_by_pref[succ_state] = is_pref;

                succ_node.open(node, op, get_adjusted_cost(op));

                if (fv <= w_f_min) {
                    insert_typed(succ_state, hf, succ_g, is_pref);
                    count_f[fv]++;
                } else {
                    open_list->insert(succ_ctx, succ_state.get_id());
                    in_focal[succ_state] = false;
                }

                if (search_progress.check_progress(succ_ctx))
                    statistics.print_checkpoint_line(succ_node.get_g());

            } else if (succ_node.get_g() > node.get_g() + get_adjusted_cost(op)) {
                succ_node.update_parent(node, op, get_adjusted_cost(op));
                generated_by_pref[succ_state] = is_pref;
            }
        }
    }

    // ── Recompute f_min ───────────────────────────────────────────────────
    f_min = numeric_limits<int>::max();
    if (!open_list->empty()) {
        StateID id_top = open_list->get_min();
        f_min = min(f_min, f_value[state_registry.lookup_state(id_top)]);
    }
    if (!count_f.empty())
        f_min = min(f_min, count_f.begin()->first);

    if (typed_focal.empty() && typed_pref.empty() && open_list->empty()) {
        log << "Completely explored state space -- no solution!" << endl;
        return FAILED;
    }

    assert(f_min < numeric_limits<int>::max());

    // ── Transfer OPEN → FOCAL for the new (larger) f_min ─────────────────
    if (f_min > prev_f_min) {
        while (!open_list->empty()) {
            StateID id_top = open_list->get_min();
            State s_top    = state_registry.lookup_state(id_top);
            if (f_value[s_top] > w * f_min) break;
            open_list->remove_min();
            int g_top = search_space.get_node(s_top).get_g();
            insert_typed(s_top, h_focal_val[s_top], g_top, generated_by_pref[s_top]);
            count_f[f_value[s_top]]++;
        }
    }

    return IN_PROGRESS;
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

void TypedFocalHGSearch::print_statistics() const {
    statistics.print_detailed_statistics();
    search_space.print_statistics();
}

void TypedFocalHGSearch::dump_search_space() const {
    search_space.dump(task_proxy);
}

void TypedFocalHGSearch::start_f_value_statistics(EvaluationContext &eval_context) {
    if (focal_evaluator) {
        int fv = eval_context.get_evaluator_value(focal_evaluator.get());
        statistics.report_f_value_progress(fv);
    }
}

void add_options_to_feature(plugins::Feature &feature) {
    SearchAlgorithm::add_pruning_option(feature);
    SearchAlgorithm::add_options_to_feature(feature);
}

}  // namespace typed_focal_hg_search
