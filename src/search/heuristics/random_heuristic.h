#ifndef HEURISTICS_RANDOM_HEURISTIC_H
#define HEURISTICS_RANDOM_HEURISTIC_H

#include "../heuristic.h"
//#include "../utils/rng.h"
#include "../plugins/plugin.h"
#include "../task_utils/task_properties.h"

#include <cstddef>
#include <limits>
#include <utility>

namespace random_heuristic {
class RandomHeuristic : public Heuristic {
    int max_random_value;
    int random_seed;
    std::shared_ptr<utils::RandomNumberGenerator> rng;
protected:
    virtual int compute_heuristic(const State &global_state);
public:
    RandomHeuristic(const plugins::Options &opts);
    ~RandomHeuristic();
};

extern void RandomHeuristicFunction_options_to_feature(plugins::Feature &feature);
}

#endif