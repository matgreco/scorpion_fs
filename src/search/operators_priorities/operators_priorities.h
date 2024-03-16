#ifndef OPERATORS_PRIORITIES_OPERATORS_PRIORITIES_H
#define OPERATORS_PRIORITIES_OPERATORS_PRIORITIES_H

#include "../heuristic.h"

#include "../plugins/plugin.h"
//#include "../global_state.h"
#include "../state_registry.h"
#include "../per_state_bitset.h"


#include "priorities_strategy.h"

#include <memory>

namespace successor_generator {
class SuccessorGenerator;
}


namespace OpPriorities {
class OpPrioritiesFunction;

class OpPrioritiesHeuristic : public Heuristic {
    std::vector<double> op_priorities;
    //PerStateInformation<float> priority; 
    PerStateInformation<double> cache_heuristics_priority;    
    //PerStateInformation<const State*> parent; // the parent state
    PerStateInformation<int> path_depth;
    PerStateInformation<double> sum_priorities_siblings; 
    PerStateInformation<bool> sum_priorities_siblings_ready; 
    std::shared_ptr<OpPriorities::PrioritiesStrategy> priority_strategy;
    
protected:
    std::unique_ptr<successor_generator::SuccessorGenerator> successor_generator;
    virtual int compute_heuristic(const State &ancestor_state) override;
    float path_heuristic(const State &state);
    float path_heuristic_normalized(const State &state);
    float instant_heuristic(const State &state);
    
public:
    virtual void get_path_dependent_evaluators(std::set<Evaluator *> &evals) override;
    explicit OpPrioritiesHeuristic(const plugins::Options &opts);
    // Define in .cc file to avoid include in header.
    ~OpPrioritiesHeuristic();
    virtual void notify_initial_state(const State &initial_state) override;
    virtual void notify_state_transition(const State &parent_state,
                                        OperatorID op_id,
                                        const State &state) override;
};

extern void OpPrioritiesFunction_options_to_feature(plugins::Feature &feature);

}

#endif
