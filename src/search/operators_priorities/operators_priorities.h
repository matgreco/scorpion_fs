#ifndef POTENTIALS_POTENTIAL_HEURISTIC_H
#define POTENTIALS_POTENTIAL_HEURISTIC_H

#include "../heuristic.h"

#include <memory>

namespace OpPriorities {
//class OpPrioritiesFunction;

/*
*/
class OpPrioritiesHeuristic : public Heuristic {
    //std::unique_ptr<OpPrioritiesFunction> function;
    std::vector<float> priorities;

protected:
    virtual int compute_heuristic(const State &ancestor_state) override;

public:
    explicit OpPrioritiesHeuristic(const plugins::Options &opts);
    // Define in .cc file to avoid include in header.
    ~OpPrioritiesHeuristic();
};
}

#endif
