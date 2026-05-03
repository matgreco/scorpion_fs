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

#include <cassert>
#include <cstdlib>
#include <memory>
#include <optional>
#include <set>

using namespace std;

namespace focal_search {
/*
FocalSearch::FocalSearch(const plugins::Options &opts)
    : SearchAlgorithm(opts),
      reopen_closed_nodes(opts.get<bool>("reopen_closed")),
      open_list(opts.get<shared_ptr<OpenListFactory>>("open")->
                create_state_open_list()),
      f_evaluator(opts.get<shared_ptr<Evaluator>>("f_eval", nullptr)),
      preferred_operator_evaluators(opts.get_list<shared_ptr<Evaluator>>("preferred")),
      lazy_evaluator(opts.get<shared_ptr<Evaluator>>("lazy_evaluator", nullptr)),
      pruning_method(opts.get<shared_ptr<PruningMethod>>("pruning")) {
    if (lazy_evaluator && !lazy_evaluator->does_cache_estimates()) {
        cerr << "lazy_evaluator must cache its estimates" << endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
}
*/
FocalSearch::FocalSearch(const plugins::Options &opts)
    : SearchAlgorithm(opts),
      reopen_closed_nodes(opts.get<bool>("reopen_closed")),
      open_evaluator(opts.get<shared_ptr<Evaluator>>("open_eval", nullptr)),
      focal_evaluator(opts.get<shared_ptr<Evaluator>>("focal_eval", nullptr)),
      w(opts.get<double>("w")){

        cout << opts.get_unparsed_config() << endl;        
        
        plugins::Options focallist_opts(opts);
        focallist_opts.set("eval", focal_evaluator);
        std::shared_ptr<OpenListFactory> flf = std::make_shared<standard_scalar_open_list::BestFirstOpenListFactory>(focallist_opts);
        focal_list = flf->create_state_open_list();

        plugins::Options openlist_opts(opts);
        openlist_opts.set("eval", open_evaluator);
        std::shared_ptr<OpenListFactory> olf = std::make_shared<standard_scalar_open_list::BestFirstOpenListFactory>(openlist_opts);
        open_list = olf->create_state_open_list();
}

void FocalSearch::initialize() {
    log << "Conducting FOCAL search"
        << (reopen_closed_nodes ? " with" : " without")
        << " reopening closed nodes, (real) bound = " << bound
        << "W suboptimality bound " << w 
        << endl;
    assert(focal_list);
    log << "OK ASSERT" << endl;

    set<Evaluator *> evals;
    open_list->get_path_dependent_evaluators(evals);
    focal_evaluator->get_path_dependent_evaluators(evals);
    path_dependent_evaluators.assign(evals.begin(), evals.end());
 

    State initial_state = state_registry.get_initial_state();
    open_evaluator->notify_initial_state(initial_state);
    focal_evaluator->notify_initial_state(initial_state);

    log << "Notifies initial state done" << endl;
    EvaluationContext eval_context(initial_state, 0, false, &statistics);

    log << "INITIAL STATE IN FOCAL EVALUATOR" << eval_context.get_evaluator_value_or_infinity(focal_evaluator.get()) << endl;
    log << "SIZE OF FOCAL LIST" << focal_list->empty() << endl;

    statistics.inc_evaluated_states();

    if (open_list->is_dead_end(eval_context)) {
        log << "Initial state is a dead end." << endl;
    } else {
        if (search_progress.check_progress(eval_context))
            statistics.print_checkpoint_line(0);
        start_f_value_statistics(eval_context);
        SearchNode node = search_space.get_node(initial_state);
        node.open_initial();

        focal_list->insert(eval_context, initial_state.get_id());
        in_focal[initial_state] = true;

        int h_admissible = eval_context.get_evaluator_value_or_infinity(open_evaluator.get());
        f_value[initial_state] = 0 + h_admissible;

        count_f[f_value[initial_state]] ++;
        f_min = f_value[initial_state];
    }
    cout << "** INITIALIZED OK" << endl;
    print_initial_evaluator_values(eval_context);

    //pruning_method->initialize(task);
}

void FocalSearch::print_statistics() const {
    statistics.print_detailed_statistics();
    search_space.print_statistics();
    //pruning_method->print_statistics();
}

SearchStatus FocalSearch::step() {
    const int prev_f_min = f_min;
    optional<SearchNode> node;
    while (true) {
        if (focal_list->empty()) {
            log << "Completely explored state space -- no solution!" << endl;
            return FAILED;
        }       
        StateID id = focal_list->remove_min();
        State s = state_registry.lookup_state(id);
        //cout << "-- el nodo extraido es " << s.get_id() << " con f " << f_value[s] << endl;

        f_min = count_f.begin()->first; // the min key
        node.emplace(search_space.get_node(s));

        if (node->is_closed())
            continue;

        /*
          We can pass calculate_preferred=false here since preferred
          operators are computed when the state is expanded.
        */
        EvaluationContext eval_context(s, node->get_g(), false, &statistics);

        // Decrement count_f here, after confirming the entry is not stale.
        // Stale entries (node already closed due to reopen) are skipped above;
        // their count_f contribution was already removed at the reopen site.
        count_f[f_value[s]]--;
        if (count_f[f_value[s]] == 0)
            count_f.erase(f_value[s]);
        in_focal[s] = false;

        node->close();
        assert(!node->is_dead_end());
        //update_f_value_statistics(eval_context);
        statistics.inc_expanded();
        break;
    }

    const State &s = node->get_state();
    if (check_goal_and_set_plan(s))
        return SOLVED;

    const double w_f_min = w * f_min;
    vector<OperatorID> applicable_ops;
    successor_generator.generate_applicable_ops(s, applicable_ops);

    /*
      TODO: When preferred operators are in use, a preferred operator will be
      considered by the preferred operator queues even when it is pruned.
    */
    // NOT NEEDED BY NOW
    //pruning_method->prune_operators(s, applicable_ops);

    // This evaluates the expanded state (again) to get preferred ops
    // NOT NEEDED BY NOW
    /*
    EvaluationContext eval_context(s, node->get_g(), false, &statistics, true);
    ordered_set::OrderedSet<OperatorID> preferred_operators;
    for (const shared_ptr<Evaluator> &preferred_operator_evaluator : preferred_operator_evaluators) {
        collect_preferred_operators(eval_context,
                                    preferred_operator_evaluator.get(),
                                    preferred_operators);
    }
    */

    for (OperatorID op_id : applicable_ops) {
        OperatorProxy op = task_proxy.get_operators()[op_id];
        if ((node->get_real_g() + op.get_cost()) >= bound)
            continue;
        
        State succ_state = state_registry.get_successor_state(s, op);
        statistics.inc_generated();
        bool is_preferred = false; //because the current implementation does not use pref opp

        SearchNode succ_node = search_space.get_node(succ_state);

        // check how it's works
        for (Evaluator *evaluator : path_dependent_evaluators) {
            evaluator->notify_state_transition(s, op_id, succ_state);
        }
        


        // Previously encountered dead end. Don't re-evaluate.
        if (succ_node.is_dead_end())
            continue;

        if (succ_node.is_new()) {
            // We have not seen this state before.
            // Evaluate and create a new node.

            // Careful: succ_node.get_g() is not available here yet,
            // hence the stupid computation of succ_g.
            // TODO: Make this less fragile.
            int succ_g = node->get_g() + get_adjusted_cost(op);


                 
            EvaluationContext succ_eval_context(
                succ_state, succ_g, is_preferred, &statistics);

            f_value[succ_state] = succ_eval_context.get_evaluator_value_or_infinity(open_evaluator.get());
            //log << "** OPEN f value = " << f_value[succ_state] << " for the succ state" << succ_state.get_id() << endl;
            
            statistics.inc_evaluated_states();


            if (open_list->is_dead_end(succ_eval_context)) {
                succ_node.mark_as_dead_end();
                statistics.inc_dead_ends();
                continue;
            }
            succ_node.open(*node, op, get_adjusted_cost(op));

            if(f_value[succ_state] <= w_f_min){
                focal_list->insert(succ_eval_context, succ_state.get_id());
                count_f[f_value[succ_state]]++;
                in_focal[succ_state] = true;
                //log << "-- Node inserted into FOCAL with f=" << f_value[succ_state] << " and fmin=" << f_min << endl;
            }
            else {
                open_list->insert(succ_eval_context, succ_state.get_id());
                in_focal[succ_state] = false;
                //log << "++ Node inserted into OPEN with f=" << f_value[succ_state] << " and fmin=" << f_min << endl;
            }


            if (search_progress.check_progress(succ_eval_context)) {
                statistics.print_checkpoint_line(succ_node.get_g());
                //reward_progress();
            }
        } else if (succ_node.get_g() > node->get_g() + get_adjusted_cost(op)) {
            // We found a new cheapest path to an open or closed state.
            if (reopen_closed_nodes) {
                if (succ_node.is_closed()) {
                    /*
                      TODO: It would be nice if we had a way to test
                      that reopening is expected behaviour, i.e., exit
                      with an error when this is something where
                      reopening should not occur (e.g. A* with a
                      consistent heuristic).
                    */
                    statistics.inc_reopened();
                }
                succ_node.reopen(*node, op, get_adjusted_cost(op));

                EvaluationContext succ_eval_context(
                    succ_state, succ_node.get_g(), is_preferred, &statistics);

                /*
                  Note: our old code used to retrieve the h value from
                  the search node here. Our new code recomputes it as
                  necessary, thus avoiding the incredible ugliness of
                  the old "set_evaluator_value" approach, which also
                  did not generalize properly to settings with more
                  than one evaluator.

                  Reopening should not happen all that frequently, so
                  the performance impact of this is hopefully not that
                  large. In the medium term, we want the evaluators to
                  remember evaluator values for states themselves if
                  desired by the user, so that such recomputations
                  will just involve a look-up by the Evaluator object
                  rather than a recomputation of the evaluator value
                  from scratch.
                */
                // Bug 3 fix: save old f and focal status before overwriting f_value.
                // If the node was open and tracked in count_f (in_focal=true), its stale
                // focal entry will be skipped when popped (node will be closed by then),
                // so we remove its count_f contribution here instead of at pop time.
                int old_f = f_value[succ_state];
                bool was_in_focal = in_focal[succ_state];

                f_value[succ_state] = succ_eval_context.get_evaluator_value_or_infinity(open_evaluator.get());

                if (was_in_focal) {
                    count_f[old_f]--;
                    if (count_f[old_f] == 0)
                        count_f.erase(old_f);
                    in_focal[succ_state] = false;
                }

                if(f_value[succ_state] <= w_f_min){
                    focal_list->insert(succ_eval_context, succ_state.get_id());
                    count_f[f_value[succ_state]]++;
                    in_focal[succ_state] = true;
                }
                else {
                    open_list->insert(succ_eval_context, succ_state.get_id());
                    in_focal[succ_state] = false;
                }
            } else {
                // If we do not reopen closed nodes, we just update the parent pointers.
                // Note that this could cause an incompatibility between
                // the g-value and the actual path that is traced back.
                succ_node.update_parent(*node, op, get_adjusted_cost(op));
            }
        }
    }
    // CHECK THIS BECAUSE IN THIS PART OF THE CODE I NEED TO ACCESS TO THE MIN F VALUE IN THE OPEN (WHICH DOEST'N HAVE THE METHOD TO ACCESS THE MIN, ONLY POP)
    //fmin = std::min(count_f.begin()->first, open_list-> )
    f_min = numeric_limits<int>::max();
    if (!open_list->empty()){
        StateID id_min_open = open_list->get_min();
        State s_min_open = state_registry.lookup_state(id_min_open);
        f_min = std::min(f_min, f_value[s_min_open]); // the minimum betweet open and count_f
    }
    if(!count_f.empty()){
        f_min = std::min(f_min, count_f.begin()->first);
    }
    //cout << "**El fmin al final del step es " << f_min << endl;
    // while open not empty and f(head(open)) < w*fmin 
    assert(f_min < numeric_limits<int>::max());
    if (f_min > prev_f_min) {
        while(!open_list->empty() && f_value[state_registry.lookup_state(open_list->get_min())] <= w*f_min){
            StateID id = open_list->remove_min();
            State s = state_registry.lookup_state(id);
            EvaluationContext update_eval_context(
                s, search_space.get_node(s).get_g(), false, &statistics);
            focal_list->insert(update_eval_context, s.get_id());
            count_f[f_value[s]]++;
            in_focal[s] = true;
        }
    }


    return IN_PROGRESS;
}

// void FocalSearch::reward_progress() {
//     // Boost the "preferred operator" open lists somewhat whenever
//     // one of the heuristics finds a state with a new best h value.
//     focal_list->boost_preferred();
// }

void FocalSearch::dump_search_space() const {
    search_space.dump(task_proxy);
}

void FocalSearch::start_f_value_statistics(EvaluationContext &eval_context) {
    if (focal_evaluator) {
        int f_value = eval_context.get_evaluator_value(focal_evaluator.get());
        statistics.report_f_value_progress(f_value);
    }
}

// /* TODO: HACK! This is very inefficient for simply looking up an h value.
//    Also, if h values are not saved it would recompute h for each and every state. */
// void FocalSearch::update_f_value_statistics(EvaluationContext &eval_context) {
    // if (f_evaluator) {
        // int f_value = eval_context.get_evaluator_value(f_evaluator.get());
        // statistics.report_f_value_progress(f_value);
    // }
// }

void add_options_to_feature(plugins::Feature &feature) {
    SearchAlgorithm::add_pruning_option(feature);
    SearchAlgorithm::add_options_to_feature(feature);
}
}
