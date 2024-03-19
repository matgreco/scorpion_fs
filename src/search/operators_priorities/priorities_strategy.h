#ifndef OPERATORS_PRIORITIES_PRIORITIES_STRATEGY_H
#define OPERATORS_PRIORITIES_PRIORITIES_STRATEGY_H

#include <string>
#include <vector>
#include "../plugins/plugin.h"
#include "../state_registry.h"
#include "../per_state_bitset.h"

namespace OpPriorities {

class PrioritiesStrategy {
public:
    bool sum_succ_dependent_evaluator;
    PrioritiesStrategy([[maybe_unused]] const plugins::Options &opts) {} 
    virtual ~PrioritiesStrategy() = default;

    virtual double compute_value(double parent_heuristic_priority, double op_priority, double exp_sum_siblings) = 0 ;

    virtual std::string get_name() const = 0;

    virtual int compute_heuristic_from_priority(double heuristic_priority, int path_lenght) = 0; 

    //virtual int compute_heuristic_from_priority(double heuristic_priority, int path_lenght, ) ; 

     

};
}

#endif
