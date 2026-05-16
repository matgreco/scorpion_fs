#include "focal_search.h"

#include "../evaluation_context.h"
#include "../evaluator.h"
#include "../open_list_factory.h"
#include "../pruning_method.h"
#include "search_common.h"


#include "../algorithms/ordered_set.h"
#include "../plugins/options.h"
#include "../task_utils/successor_generator.h"
#include "../utils/logging.h"
#include "../open_lists/best_first_open_list.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <optional>
#include <set>

using namespace std;

namespace focal_search {

FocalSearch::FocalSearch(const plugins::Options &opts)
    : SearchAlgorithm(opts),
      reopen_closed_nodes(opts.get<bool>("reopen_closed")),
      k(opts.get<int>("k")),
      open_evaluator(opts.get<shared_ptr<Evaluator>>("open_eval", nullptr)),
      focal_evaluator(opts.get<shared_ptr<Evaluator>>("focal_eval", nullptr)),
      preferred_evaluator(opts.get<shared_ptr<Evaluator>>("pref_eval", nullptr)),
      w(opts.get<double>("w")) {

    // preferred_evaluator == nullptr means no preferred-operator prioritization

    plugins::Options focallist_opts(opts);
    focallist_opts.set("eval", focal_evaluator);
    focallist_opts.set("pref_only", false);
    shared_ptr<OpenListFactory> flf =
        make_shared<standard_scalar_open_list::BestFirstOpenListFactory>(focallist_opts);
    focal_list = flf->create_state_open_list();
    focal_pref = flf->create_state_open_list();

    plugins::Options openlist_opts(opts);
    openlist_opts.set("eval", open_evaluator);
    openlist_opts.set("pref_only", false);
    shared_ptr<OpenListFactory> olf =
        make_shared<standard_scalar_open_list::BestFirstOpenListFactory>(openlist_opts);
    open_list = olf->create_state_open_list();
}

void FocalSearch::initialize() {
    log << "Conducting FOCAL search (K=" << k << ")"
        << (reopen_closed_nodes ? " with" : " without")
        << " reopening closed nodes, (real) bound = " << bound
        << ", W suboptimality bound = " << w
        << ", preferred_evaluator = "
        << (!preferred_evaluator ? "No preferred evaluator" : preferred_evaluator == focal_evaluator ? "focal_eval" : "custom")
        << endl;
    assert(focal_list);
    assert(focal_pref);

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

    EvaluationContext eval_context(initial_state, 0, false, &statistics);
    statistics.inc_evaluated_states();

    if (open_list->is_dead_end(eval_context)) {
        log << "Initial state is a dead end." << endl;
    } else {
        if (search_progress.check_progress(eval_context))
            statistics.print_checkpoint_line(0);
        start_f_value_statistics(eval_context);
        SearchNode node = search_space.get_node(initial_state);
        node.open_initial();

        // Initial state has no parent operator — not preferred
        focal_list->insert(eval_context, initial_state.get_id());
        generated_by_pref[initial_state] = false;
        in_focal[initial_state] = true;

        int h_admissible = eval_context.get_evaluator_value_or_infinity(open_evaluator.get());
        f_value[initial_state] = 0 + h_admissible;

        count_f[f_value[initial_state]]++;
        f_min = f_value[initial_state];
    }
    print_initial_evaluator_values(eval_context);
}

void FocalSearch::print_statistics() const {
    statistics.print_detailed_statistics();
    search_space.print_statistics();
}

SearchStatus FocalSearch::step() {
    const int prev_f_min = f_min;
    // w_f_min is fixed for the entire batch of K expansions (K-FS invariant)
    const double w_f_min = w * f_min;

    for (int batch = 0; batch < k; ++batch) {
        bool found = false;

        // Try focal_pref first; fall back to focal_list
        for (StateOpenList *list : {focal_pref.get(), focal_list.get()}) {
            while (!list->empty()) {
                StateID id = list->remove_min();
                State s = state_registry.lookup_state(id);
                SearchNode node = search_space.get_node(s);

                if (node.is_closed())
                    continue;

                count_f[f_value[s]]--;
                if (count_f[f_value[s]] == 0)
                    count_f.erase(f_value[s]);
                in_focal[s] = false;
                node.close();
                assert(!node.is_dead_end());
                statistics.inc_expanded();
                found = true;

                if (check_goal_and_set_plan(s))
                    return SOLVED;

                // Preferred ops of s (empty vector if pref_eval not set)
                const vector<OperatorID> *pref_ops = nullptr;
                vector<OperatorID> empty_pref;
                optional<EvaluationContext> opt_parent_ctx;
                if (preferred_evaluator) {
                    opt_parent_ctx.emplace(s, node.get_g(), false, &statistics);
                    pref_ops = &opt_parent_ctx->get_preferred_operators(preferred_evaluator.get());
                } else {
                    pref_ops = &empty_pref;
                }

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

                    const bool is_pref = find(pref_ops->begin(), pref_ops->end(), op_id) != pref_ops->end();

                    if (succ_node.is_new()) {
                        int succ_g = node.get_g() + get_adjusted_cost(op);
                        EvaluationContext succ_eval_context(succ_state, succ_g, false, &statistics);

                        f_value[succ_state] = succ_eval_context.get_evaluator_value_or_infinity(open_evaluator.get());
                        statistics.inc_evaluated_states();

                        if (open_list->is_dead_end(succ_eval_context)) {
                            succ_node.mark_as_dead_end();
                            statistics.inc_dead_ends();
                            continue;
                        }
                        succ_node.open(node, op, get_adjusted_cost(op));
                        generated_by_pref[succ_state] = is_pref;

                        if (f_value[succ_state] <= w_f_min) {
                            if (is_pref)
                                focal_pref->insert(succ_eval_context, succ_state.get_id());
                            else
                                focal_list->insert(succ_eval_context, succ_state.get_id());
                            count_f[f_value[succ_state]]++;
                            in_focal[succ_state] = true;
                        } else {
                            open_list->insert(succ_eval_context, succ_state.get_id());
                            in_focal[succ_state] = false;
                        }

                        if (search_progress.check_progress(succ_eval_context))
                            statistics.print_checkpoint_line(succ_node.get_g());

                    } else if (succ_node.get_g() > node.get_g() + get_adjusted_cost(op)) {
                        if (reopen_closed_nodes) {
                            if (succ_node.is_closed())
                                statistics.inc_reopened();
                            succ_node.reopen(node, op, get_adjusted_cost(op));

                            EvaluationContext succ_eval_context(
                                succ_state, succ_node.get_g(), false, &statistics);

                            int old_f = f_value[succ_state];
                            bool was_in_focal = in_focal[succ_state];

                            f_value[succ_state] = succ_eval_context.get_evaluator_value_or_infinity(open_evaluator.get());
                            generated_by_pref[succ_state] = is_pref;

                            if (was_in_focal) {
                                count_f[old_f]--;
                                if (count_f[old_f] == 0)
                                    count_f.erase(old_f);
                                in_focal[succ_state] = false;
                            }

                            if (f_value[succ_state] <= w_f_min) {
                                if (is_pref)
                                    focal_pref->insert(succ_eval_context, succ_state.get_id());
                                else
                                    focal_list->insert(succ_eval_context, succ_state.get_id());
                                count_f[f_value[succ_state]]++;
                                in_focal[succ_state] = true;
                            } else {
                                open_list->insert(succ_eval_context, succ_state.get_id());
                                in_focal[succ_state] = false;
                            }
                        } else {
                            succ_node.update_parent(node, op, get_adjusted_cost(op));
                            generated_by_pref[succ_state] = is_pref;
                        }
                    }
                }
                break; // one expansion done; advance to next batch slot
            }
            if (found) break;
        }

        if (!found)
            break; // FOCAL exhausted; stop batch early
    }

    // Recompute f_min once after all K expansions
    f_min = numeric_limits<int>::max();
    if (!open_list->empty()) {
        StateID id_min_open = open_list->get_min();
        State s_min_open = state_registry.lookup_state(id_min_open);
        f_min = min(f_min, f_value[s_min_open]);
    }
    if (!count_f.empty())
        f_min = min(f_min, count_f.begin()->first);

    if (focal_pref->empty() && focal_list->empty() && open_list->empty()) {
        log << "Completely explored state space -- no solution!" << endl;
        return FAILED;
    }

    assert(f_min < numeric_limits<int>::max());

    // Transfer nodes from OPEN to FOCAL for the new (larger) f_min
    if (f_min > prev_f_min) {
        while (!open_list->empty() &&
               f_value[state_registry.lookup_state(open_list->get_min())] <= w * f_min) {
            StateID id = open_list->remove_min();
            State s = state_registry.lookup_state(id);
            EvaluationContext update_eval_context(
                s, search_space.get_node(s).get_g(), false, &statistics);
            if (generated_by_pref[s])
                focal_pref->insert(update_eval_context, s.get_id());
            else
                focal_list->insert(update_eval_context, s.get_id());
            count_f[f_value[s]]++;
            in_focal[s] = true;
        }
    }

    return IN_PROGRESS;
}

void FocalSearch::dump_search_space() const {
    search_space.dump(task_proxy);
}

void FocalSearch::start_f_value_statistics(EvaluationContext &eval_context) {
    if (focal_evaluator) {
        int f_value = eval_context.get_evaluator_value(focal_evaluator.get());
        statistics.report_f_value_progress(f_value);
    }
}

void add_options_to_feature(plugins::Feature &feature) {
    SearchAlgorithm::add_pruning_option(feature);
    SearchAlgorithm::add_options_to_feature(feature);
}
}
