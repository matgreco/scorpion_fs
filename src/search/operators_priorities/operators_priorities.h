#ifndef POTENTIALS_POTENTIAL_HEURISTIC_H
#define POTENTIALS_POTENTIAL_HEURISTIC_H

#include "../heuristic.h"

#include "../plugins/plugin.h"
//#include "../global_state.h"
#include "../state_registry.h"
#include "../per_state_bitset.h"


#include <memory>

namespace OpPriorities {
//class OpPrioritiesFunction;

class OpPrioritiesHeuristic : public Heuristic {
    //std::unique_ptr<OpPrioritiesFunction> function;
    std::vector<float> op_priorities;
    PerStateInformation<float> priority; 
    PerStateInformation<float> cache_heuristics_priority;    
    PerStateInformation<const State*> parent; // the parent state
   
    //unordered_map<int, float> 
       

protected:
    virtual int compute_heuristic(const State &ancestor_state) override;
    float path_heuristic(const State &state);
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
}

#endif
