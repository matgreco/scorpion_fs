#ifndef OPERATORS_PRIORITIES_PRIORITIES_STRATEGY_H
#define OPERATORS_PRIORITIES_PRIORITIES_STRATEGY_H

#include <string>
#include <vector>
#include "../plugins/plugin.h"

namespace OpPriorities {

class PrioritiesStrategy {
public:
    PrioritiesStrategy([[maybe_unused]] const plugins::Options &opts) {} 
    virtual ~PrioritiesStrategy() = default;

    virtual double compute_value(double parent_heuristic_priority, double op_priority, int path_lenght) = 0 ;

    virtual std::string get_name() const = 0;

    virtual int compute_heuristic_from_priority(double heuristic_priority) = 0; 
};
}

#endif
