#include "focal_search.h"

#include "../evaluation_context.h"
#include "../evaluator.h"
#include "../algorithms/ordered_set.h"
#include "../plugins/options.h"
#include "../plugins/plugin.h"
#include "../task_utils/successor_generator.h"
#include "../utils/logging.h"
#include "../utils/system.h"

#include <cassert>
#include <cmath>
#include <set>

using namespace std;

namespace focal_search {
// w is interpreted with at most six decimal digits (see type_based_wastar).
static const long long WEIGHT_SCALE = 1000000;

FocalSearch::FocalSearch(const plugins::Options &opts)
    : SearchAlgorithm(opts),
      reopen_closed_nodes(opts.get<bool>("reopen_closed")),
      k(opts.get<int>("k")),
      open_evaluator(opts.get<shared_ptr<Evaluator>>("open_eval")),
      focal_evaluator(opts.get<shared_ptr<Evaluator>>("focal_eval")),
      preferred_evaluator(opts.get<shared_ptr<Evaluator>>("pref_eval", nullptr)),
      w(opts.get<double>("w")),
      scaled_w(0),
      f_value(-1),
      in_focal(false),
      generated_by_pref(false),
      f_min(-1),
      preferred_expansions(0),
      stale_entries_skipped(0) {
    if (!isfinite(w) || w < 1.0 || w > 1000.0) {
        cerr << "focal_search: w must be a finite weight with 1 <= w <= 1000." << endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
    scaled_w = llround(w * WEIGHT_SCALE);
    if (fabs(w * WEIGHT_SCALE - static_cast<double>(scaled_w)) > 1e-3) {
        cerr << "focal_search: w must have at most six decimal digits." << endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
}

// FOCAL bound f <= w * f_min, exact in integers.
bool FocalSearch::within_bound(int f) const {
    assert(f_min >= 0);
    return static_cast<long long>(f) * WEIGHT_SCALE <= scaled_w * f_min;
}

/*
  An entry is current iff its node is still open, has the g and f it was
  inserted with, and is on the side (FOCAL or not) the queue belongs to. A
  node is reinserted only with a strictly smaller g, so (id, g) identifies
  its latest insertion; f and the FOCAL flag are consistency checks.
*/
bool FocalSearch::is_current_entry(
    const SearchNode &node, const State &state, const Entry &entry, bool expected_in_focal) const {
    return node.is_open() && node.get_g() == entry.g && f_value[state] == entry.f &&
           in_focal[state] == expected_in_focal;
}

optional<FocalSearch::Entry> FocalSearch::current_front(Queue &queue, bool expected_in_focal) {
    while (!queue.empty()) {
        auto it = queue.begin();
        Entry entry = it->second.front();
        State state = state_registry.lookup_state(entry.id);
        SearchNode node = search_space.get_node(state);
        if (is_current_entry(node, state, entry, expected_in_focal))
            return entry;
        it->second.pop_front();
        ++stale_entries_skipped;
        if (it->second.empty())
            queue.erase(it);
    }
    return nullopt;
}

void FocalSearch::pop_front(Queue &queue) {
    assert(!queue.empty());
    auto it = queue.begin();
    it->second.pop_front();
    if (it->second.empty())
        queue.erase(it);
}

void FocalSearch::insert_node(const State &state, int g, int f, EvaluationContext &eval_context) {
    f_value[state] = f;
    if (within_bound(f)) {
        insert_into_focal(state, g, f, eval_context);
    } else {
        in_focal[state] = false;
        open_list[f].push_back({state.get_id(), g, f});
    }
}

void FocalSearch::insert_into_focal(const State &state, int g, int f, EvaluationContext &eval_context) {
    in_focal[state] = true;
    f_value[state] = f;
    // Only open_eval decides dead ends: an infinite focal estimate gets the
    // lowest priority instead of being dropped.
    int key = eval_context.get_evaluator_value_or_infinity(focal_evaluator.get());
    Queue &queue = generated_by_pref[state] ? focal_pref : focal_list;
    queue[key].push_back({state.get_id(), g, f});
    ++count_f[f];
    if (log.is_at_least_debug()) {
        log << "FOCAL <- state " << state.get_id() << " g=" << g << " f=" << f
            << " f_min=" << f_min << (generated_by_pref[state] ? " (preferred)" : "") << endl;
    }
}

// The node leaves FOCAL: it is expanded or reinserted with a cheaper path.
void FocalSearch::remove_from_focal(const State &state) {
    assert(in_focal[state]);
    auto it = count_f.find(f_value[state]);
    assert(it != count_f.end() && it->second > 0);
    if (--it->second == 0)
        count_f.erase(it);
    in_focal[state] = false;
}

/*
  f_min = min(f of the first current entry of open_list, min f inside FOCAL);
  then every node of open_list with f <= w * f_min is moved into FOCAL.
  Afterwards FOCAL is empty only if OPEN is empty.
*/
void FocalSearch::update_f_min_and_fill_focal() {
    optional<Entry> open_front = current_front(open_list, false);
    f_min = -1;
    if (open_front)
        f_min = open_front->f;
    if (!count_f.empty() && (f_min == -1 || count_f.begin()->first < f_min))
        f_min = count_f.begin()->first;
    if (f_min == -1)
        return;
    while (true) {
        open_front = current_front(open_list, false);
        if (!open_front || !within_bound(open_front->f))
            break;
        Entry entry = *open_front;
        pop_front(open_list);
        State state = state_registry.lookup_state(entry.id);
        EvaluationContext eval_context(state, entry.g, false, &statistics);
        insert_into_focal(state, entry.g, entry.f, eval_context);
    }
}

// Best node of FOCAL: the preferred sub-list first, then the focal list.
optional<SearchNode> FocalSearch::select_node(bool &from_preferred) {
    for (Queue *queue : {&focal_pref, &focal_list}) {
        optional<Entry> entry = current_front(*queue, true);
        if (entry) {
            pop_front(*queue);
            from_preferred = (queue == &focal_pref);
            State state = state_registry.lookup_state(entry->id);
            return search_space.get_node(state);
        }
    }
    return nullopt;
}

void FocalSearch::initialize() {
    log << "Conducting focal search (k = " << k << ") with w = " << w
        << (reopen_closed_nodes ? ", reopening closed nodes" : ", not reopening closed nodes")
        << (preferred_evaluator ? ", preferred-operator sub-list" : ", no preferred-operator sub-list")
        << ", (real) bound = " << bound << endl;

    set<Evaluator *> evals;
    open_evaluator->get_path_dependent_evaluators(evals);
    focal_evaluator->get_path_dependent_evaluators(evals);
    if (preferred_evaluator)
        preferred_evaluator->get_path_dependent_evaluators(evals);
    path_dependent_evaluators.assign(evals.begin(), evals.end());

    State initial_state = state_registry.get_initial_state();
    for (Evaluator *evaluator : path_dependent_evaluators)
        evaluator->notify_initial_state(initial_state);

    EvaluationContext eval_context(initial_state, 0, true, &statistics);
    statistics.inc_evaluated_states();
    if (eval_context.is_evaluator_value_infinite(open_evaluator.get())) {
        log << "Initial state is a dead end." << endl;
    } else {
        if (search_progress.check_progress(eval_context))
            statistics.print_checkpoint_line(0);
        int f = eval_context.get_evaluator_value(open_evaluator.get());
        SearchNode node = search_space.get_node(initial_state);
        node.open_initial();
        f_min = f;   // f of the only open node
        insert_node(initial_state, 0, f, eval_context);
    }
    print_initial_evaluator_values(eval_context);
}

SearchStatus FocalSearch::step() {
    // w * f_min is fixed during the k expansions of the batch (K-focal search).
    for (int expansion = 0; expansion < k; ++expansion) {
        bool from_preferred = false;
        optional<SearchNode> node = select_node(from_preferred);
        if (!node) {
            if (expansion > 0)
                break;   // FOCAL exhausted within the batch: refill below
            // After update_f_min_and_fill_focal, FOCAL is empty only if OPEN is empty.
            log << "Completely explored state space -- no solution!" << endl;
            return FAILED;
        }

        const State &s = node->get_state();
        if (log.is_at_least_debug()) {
            log << "Expand (" << (from_preferred ? "preferred" : "focal") << "): state "
                << s.get_id() << " g=" << node->get_g() << " f=" << f_value[s] << endl;
        }
        if (check_goal_and_set_plan(s))
            return SOLVED;

        remove_from_focal(s);
        node->close();
        statistics.inc_expanded();
        if (from_preferred)
            ++preferred_expansions;

        ordered_set::OrderedSet<OperatorID> preferred_operators;
        if (preferred_evaluator) {
            EvaluationContext eval_context(s, node->get_g(), false, &statistics, true);
            collect_preferred_operators(eval_context, preferred_evaluator.get(), preferred_operators);
        }

        vector<OperatorID> applicable_ops;
        successor_generator.generate_applicable_ops(s, applicable_ops);

        for (OperatorID op_id : applicable_ops) {
            OperatorProxy op = task_proxy.get_operators()[op_id];
            if ((node->get_real_g() + op.get_cost()) >= bound)
                continue;

            State succ_state = state_registry.get_successor_state(s, op);
            statistics.inc_generated();
            SearchNode succ_node = search_space.get_node(succ_state);
            bool is_preferred = preferred_operators.contains(op_id);

            for (Evaluator *evaluator : path_dependent_evaluators)
                evaluator->notify_state_transition(s, op_id, succ_state);

            if (succ_node.is_dead_end())
                continue;

            int succ_g = node->get_g() + get_adjusted_cost(op);

            if (succ_node.is_new()) {
                EvaluationContext succ_eval_context(succ_state, succ_g, is_preferred, &statistics);
                statistics.inc_evaluated_states();
                if (succ_eval_context.is_evaluator_value_infinite(open_evaluator.get())) {
                    succ_node.mark_as_dead_end();
                    statistics.inc_dead_ends();
                    continue;
                }
                int succ_f = succ_eval_context.get_evaluator_value(open_evaluator.get());
                succ_node.open(*node, op, get_adjusted_cost(op));
                generated_by_pref[succ_state] = is_preferred;
                insert_node(succ_state, succ_g, succ_f, succ_eval_context);
                if (search_progress.check_progress(succ_eval_context))
                    statistics.print_checkpoint_line(succ_node.get_g());
            } else if (succ_node.get_g() > succ_g) {
                // Strictly cheaper path to an open or closed node.
                bool was_closed = succ_node.is_closed();
                if (was_closed && !reopen_closed_nodes) {
                    // As eager_search: parent pointers only (the w bound no longer holds).
                    succ_node.update_parent(*node, op, get_adjusted_cost(op));
                    continue;
                }
                int old_g = succ_node.get_g();
                int old_f = f_value[succ_state];
                if (was_closed)
                    statistics.inc_reopened();
                else if (in_focal[succ_state])
                    remove_from_focal(succ_state);
                // The old queue entries of the node become stale (different g).
                succ_node.reopen(*node, op, get_adjusted_cost(op));
                generated_by_pref[succ_state] = is_preferred;
                EvaluationContext succ_eval_context(
                    succ_state, succ_node.get_g(), is_preferred, &statistics);
                if (succ_eval_context.is_evaluator_value_infinite(open_evaluator.get()))
                    continue;   // as eager_search: stays open, not reinserted
                int succ_f = succ_eval_context.get_evaluator_value(open_evaluator.get());
                if (log.is_at_least_debug()) {
                    log << "Cheaper path to " << (was_closed ? "closed" : "open") << " state "
                        << succ_state.get_id() << ": g " << old_g << " -> " << succ_node.get_g()
                        << " (f " << old_f << " -> " << succ_f << ")" << endl;
                }
                insert_node(succ_state, succ_node.get_g(), succ_f, succ_eval_context);
            }
        }
    }

    update_f_min_and_fill_focal();
    return IN_PROGRESS;
}

void FocalSearch::print_statistics() const {
    statistics.print_detailed_statistics();
    search_space.print_statistics();
    log << "Preferred sub-list expansions: " << preferred_expansions << endl;
    log << "Stale queue entries skipped: " << stale_entries_skipped << endl;
}

void add_options_to_feature(plugins::Feature &feature) {
    SearchAlgorithm::add_options_to_feature(feature);
}
}
