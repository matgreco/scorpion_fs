#include "discrepancy_focal_search.h"

#include "../evaluation_context.h"
#include "../evaluator.h"
#include "../open_list_factory.h"
#include "search_common.h"

#include "../plugins/options.h"
#include "../task_utils/successor_generator.h"
#include "../utils/logging.h"
#include "../utils/system.h"
#include "../open_lists/best_first_open_list.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <memory>
#include <optional>
#include <set>

using namespace std;

namespace discrepancy_focal_search {

static const int INF = numeric_limits<int>::max();

static string rotation_to_string(Rotation r) {
    switch (r) {
    case Rotation::H:   return "h";
    case Rotation::P:   return "p";
    case Rotation::D:   return "d";
    case Rotation::HP:  return "hp";
    case Rotation::HD:  return "hd";
    case Rotation::PD:  return "pd";
    case Rotation::HPD: return "hpd";
    }
    ABORT("Unknown rotation");
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

DiscrepancyFocalSearch::DiscrepancyFocalSearch(const plugins::Options &opts)
    : SearchAlgorithm(opts),
      k(opts.get<int>("k")),
      w(opts.get<double>("w")),
      rotation(rotation_to_string(opts.get<Rotation>("rotation"))),
      open_evaluator(opts.get<shared_ptr<Evaluator>>("open_eval")),
      focal_evaluator(opts.get<shared_ptr<Evaluator>>("focal_eval")),
      preferred_evaluator(opts.get<shared_ptr<Evaluator>>("pref_eval", nullptr)),
      f_min(0),
      rotation_idx(0),
      rng(opts.get<int>("random_seed")),
      expanded_from_h(0),
      expanded_from_pref(0),
      expanded_from_disc(0) {

    plugins::Options openlist_opts(opts);
    openlist_opts.set("eval", open_evaluator);
    openlist_opts.set("pref_only", false);
    shared_ptr<OpenListFactory> olf =
        make_shared<standard_scalar_open_list::BestFirstOpenListFactory>(openlist_opts);
    open_list = olf->create_state_open_list();
}

// ---------------------------------------------------------------------------
// FOCAL view helpers (lazy deletion)
// ---------------------------------------------------------------------------

void DiscrepancyFocalSearch::push_views(
        const State &s, int hf, int g, int d, bool is_pref) {
    HeapEntry entry{hf, g, s.get_id()};
    focal_h.push(entry);
    if (is_pref && preferred_evaluator)
        focal_pref.push(entry);
    focal_disc[d].push(entry);
}

void DiscrepancyFocalSearch::insert_focal(
        const State &s, int hf, int g, int d, bool is_pref) {
    push_views(s, hf, g, d, is_pref);
    count_f[f_value[s]]++;
    in_focal[s] = true;
}

StateID DiscrepancyFocalSearch::extract_heap(MinHHeap &heap, bool require_pref) {
    while (!heap.empty()) {
        HeapEntry e = heap.top();
        heap.pop();
        State s = state_registry.lookup_state(e.id);
        SearchNode node = search_space.get_node(s);
        if (node.is_closed() || node.get_g() != e.g)
            continue;  // stale: expanded, or reached on a better path since
        if (require_pref && !generated_by_pref[s])
            continue;  // pref flag flipped by a path update
        return e.id;
    }
    return StateID::no_state;
}

StateID DiscrepancyFocalSearch::extract_disc() {
    while (!focal_disc.empty()) {
        // Uniform random bucket — std::advance is O(B) but B (number of
        // distinct discrepancy values in FOCAL) is small.
        int bucket_idx = rng.random(static_cast<int>(focal_disc.size()));
        auto it = focal_disc.begin();
        std::advance(it, bucket_idx);
        const int bucket_d = it->first;
        MinHHeap &heap = it->second;

        while (!heap.empty()) {
            HeapEntry e = heap.top();
            heap.pop();
            State s = state_registry.lookup_state(e.id);
            SearchNode node = search_space.get_node(s);
            if (node.is_closed() || node.get_g() != e.g ||
                d_value[s] != bucket_d)
                continue;  // stale entry
            if (heap.empty())
                focal_disc.erase(it);
            return e.id;
        }
        // Bucket contained only stale entries.
        focal_disc.erase(it);
    }
    return StateID::no_state;
}

StateID DiscrepancyFocalSearch::extract_source(char src, char &used_src) {
    // Primary source, then the remaining views as fallback.
    static const char ALL[] = {'h', 'd', 'p'};
    char order[3];
    order[0] = src;
    int n = 1;
    for (char c : ALL)
        if (c != src)
            order[n++] = c;

    for (int i = 0; i < 3; ++i) {
        StateID id = StateID::no_state;
        switch (order[i]) {
        case 'h': id = extract_heap(focal_h, false); break;
        case 'p': id = extract_heap(focal_pref, true); break;
        case 'd': id = extract_disc(); break;
        }
        if (id != StateID::no_state) {
            used_src = order[i];
            return id;
        }
    }
    return StateID::no_state;
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void DiscrepancyFocalSearch::initialize() {
    log << "Conducting Discrepancy Focal Search (K=" << k << ")"
        << " without reopening closed nodes"
        << ", W suboptimality bound = " << w
        << ", rotation = " << rotation
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
    f_value[initial_state]           = fv;
    h_focal_val[initial_state]       = hf;
    d_value[initial_state]           = 0;
    generated_by_pref[initial_state] = false;
    insert_focal(initial_state, hf, 0, 0, false);
    f_min = fv;

    print_initial_evaluator_values(eval_ctx);
}

// ---------------------------------------------------------------------------
// Search step
// ---------------------------------------------------------------------------

SearchStatus DiscrepancyFocalSearch::step() {
    const double w_f_min = w * f_min;

    for (int batch = 0; batch < k; ++batch) {
        // ── Pick the view for this expansion ─────────────────────────────
        char src = rotation[rotation_idx % rotation.size()];
        ++rotation_idx;
        char used_src = src;
        StateID id = extract_source(src, used_src);
        if (id == StateID::no_state)
            break;  // FOCAL exhausted; refilled from OPEN below

        switch (used_src) {
        case 'h': ++expanded_from_h; break;
        case 'p': ++expanded_from_pref; break;
        case 'd': ++expanded_from_disc; break;
        }

        State s = state_registry.lookup_state(id);
        SearchNode node = search_space.get_node(s);

        count_f[f_value[s]]--;
        if (count_f[f_value[s]] == 0)
            count_f.erase(f_value[s]);
        in_focal[s] = false;

        node.close();
        assert(!node.is_dead_end());
        statistics.inc_expanded();

        if (check_goal_and_set_plan(s))
            return SOLVED;

        const int parent_d = d_value[s];

        // ── Preferred operators at the parent state ──────────────────────
        const vector<OperatorID> *pref_ops = nullptr;
        vector<OperatorID> empty_pref;
        optional<EvaluationContext> opt_pref_ctx;
        if (preferred_evaluator) {
            opt_pref_ctx.emplace(s, node.get_g(), false, &statistics, true);
            pref_ops = &opt_pref_ctx->get_preferred_operators(preferred_evaluator.get());
        } else {
            pref_ops = &empty_pref;
        }

        // ── Phase 1: generate and evaluate ALL successors ────────────────
        // The sibling minimum is needed before any discrepancy can be
        // assigned, so successors are buffered and inserted in a second pass.
        struct Candidate {
            State state;
            OperatorID op_id;
            bool is_new;
            int g;
            int fv;
            int hf;
            bool is_pref;
            optional<EvaluationContext> ctx;  // only for new states
        };
        vector<Candidate> cands;
        int min_h = INF;

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

            const bool is_pref = find(pref_ops->begin(), pref_ops->end(), op_id)
                                  != pref_ops->end();
            const int succ_g = node.get_g() + get_adjusted_cost(op);

            if (succ_node.is_new()) {
                EvaluationContext succ_ctx(succ_state, succ_g, false, &statistics);
                statistics.inc_evaluated_states();

                if (open_list->is_dead_end(succ_ctx)) {
                    succ_node.mark_as_dead_end();
                    statistics.inc_dead_ends();
                    continue;
                }

                int fv = succ_ctx.get_evaluator_value_or_infinity(open_evaluator.get());
                int hf = succ_ctx.get_evaluator_value_or_infinity(focal_evaluator.get());
                f_value[succ_state]     = fv;
                h_focal_val[succ_state] = hf;

                min_h = min(min_h, hf);
                cands.push_back(Candidate{succ_state, op_id, true, succ_g,
                                          fv, hf, is_pref, move(succ_ctx)});
            } else {
                // Known sibling: cached h, no re-evaluation.
                int hf = h_focal_val[succ_state];
                min_h = min(min_h, hf);
                cands.push_back(Candidate{succ_state, op_id, false, succ_g,
                                          f_value[succ_state], hf, is_pref,
                                          nullopt});
            }
        }

        // ── Phase 2: assign discrepancies and insert ─────────────────────
        for (Candidate &c : cands) {
            SearchNode succ_node = search_space.get_node(c.state);
            const int delta = (c.hf > min_h) ? 1 : 0;
            const int d_new = parent_d + delta;
            OperatorProxy op = task_proxy.get_operators()[c.op_id];

            if (c.is_new) {
                d_value[c.state]           = d_new;
                generated_by_pref[c.state] = c.is_pref;
                succ_node.open(node, op, get_adjusted_cost(op));

                if (c.fv <= w_f_min) {
                    insert_focal(c.state, c.hf, c.g, d_new, c.is_pref);
                } else {
                    open_list->insert(*c.ctx, c.state.get_id());
                    in_focal[c.state] = false;
                }

                if (search_progress.check_progress(*c.ctx))
                    statistics.print_checkpoint_line(succ_node.get_g());

            } else if (succ_node.get_g() > c.g) {
                // Better path. Like g, the discrepancy follows the best path.
                if (succ_node.is_closed())
                    continue;  // reopening disabled

                const int g_old = succ_node.get_g();
                succ_node.update_parent(node, op, get_adjusted_cost(op));
                generated_by_pref[c.state] = c.is_pref;
                d_value[c.state]           = d_new;

                // open_eval = g + h with h fixed per state, so the new f
                // follows from the g improvement without re-evaluation.
                const int f_old = f_value[c.state];
                const int f_new = (f_old == INF) ? INF : f_old - (g_old - c.g);
                f_value[c.state] = f_new;

                if (in_focal[c.state]) {
                    count_f[f_old]--;
                    if (count_f[f_old] == 0)
                        count_f.erase(f_old);
                    count_f[f_new]++;
                    // Fresh entries under the new (g, d); old ones go stale.
                    push_views(c.state, c.hf, c.g, d_new, c.is_pref);
                } else if (f_new <= w_f_min) {
                    // Promoted from OPEN; its OPEN entry goes stale.
                    insert_focal(c.state, c.hf, c.g, d_new, c.is_pref);
                } else {
                    // Still in OPEN: re-insert under the improved f. The
                    // admissible h is cached per state, so this is cheap.
                    EvaluationContext new_ctx(c.state, c.g, false, &statistics);
                    open_list->insert(new_ctx, c.state.get_id());
                }
            }
        }
    }

    recompute_f_min_and_transfer();

    if (count_f.empty() && open_list->empty()) {
        log << "Completely explored state space -- no solution!" << endl;
        return FAILED;
    }

    return IN_PROGRESS;
}

// ---------------------------------------------------------------------------
// f_min maintenance and OPEN → FOCAL transfer
// ---------------------------------------------------------------------------

void DiscrepancyFocalSearch::clean_open_top() {
    while (!open_list->empty()) {
        StateID id_top = open_list->get_min();
        State s_top = state_registry.lookup_state(id_top);
        SearchNode node = search_space.get_node(s_top);
        if (node.is_closed() || in_focal[s_top]) {
            // Stale duplicate: expanded, or already promoted to FOCAL.
            open_list->remove_min();
            continue;
        }
        break;
    }
}

void DiscrepancyFocalSearch::recompute_f_min_and_transfer() {
    clean_open_top();

    f_min = INF;
    if (!count_f.empty())
        f_min = count_f.begin()->first;
    if (!open_list->empty()) {
        StateID id_top = open_list->get_min();
        f_min = min(f_min, f_value[state_registry.lookup_state(id_top)]);
    }
    if (f_min == INF)
        return;  // both empty: step() reports failure

    // Transfer unconditionally (not only when f_min grew): if FOCAL ran dry,
    // the OPEN minimum has f = f_min <= w * f_min and must move over.
    while (true) {
        clean_open_top();
        if (open_list->empty())
            break;
        StateID id_top = open_list->get_min();
        State s_top = state_registry.lookup_state(id_top);
        if (f_value[s_top] > w * f_min)
            break;
        open_list->remove_min();
        int g_top = search_space.get_node(s_top).get_g();
        insert_focal(s_top, h_focal_val[s_top], g_top,
                     d_value[s_top], generated_by_pref[s_top]);
    }
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

void DiscrepancyFocalSearch::print_statistics() const {
    statistics.print_detailed_statistics();
    search_space.print_statistics();
    long total = expanded_from_h + expanded_from_pref + expanded_from_disc;
    log << "Expansions by view: h=" << expanded_from_h
        << ", pref=" << expanded_from_pref
        << ", disc=" << expanded_from_disc
        << " (total " << total << ")" << endl;
}

void DiscrepancyFocalSearch::dump_search_space() const {
    search_space.dump(task_proxy);
}

void DiscrepancyFocalSearch::start_f_value_statistics(EvaluationContext &eval_context) {
    if (focal_evaluator) {
        int fv = eval_context.get_evaluator_value(focal_evaluator.get());
        statistics.report_f_value_progress(fv);
    }
}

void add_options_to_feature(plugins::Feature &feature) {
    SearchAlgorithm::add_pruning_option(feature);
    SearchAlgorithm::add_options_to_feature(feature);
}

}  // namespace discrepancy_focal_search
