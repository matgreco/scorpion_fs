#ifndef HEURISTICS_RANDOM_HEURISTIC_H
#define HEURISTICS_RANDOM_HEURISTIC_H

#include "../heuristic.h"

namespace random_heuristic {
class RandomHeuristic : public Heuristic {
    int max_random_value;
    int random_seed;
protected:
    virtual int compute_heuristic(const State &global_state);
public:
    RandomHeuristic(const plugins::Options &opts);
    ~RandomHeuristic();
};
}

#endif