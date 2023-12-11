#include <cmath>
#include "priorities_strategy.h"


namespace OpPriorities {


class StrategyPath : public PrioritiesStrategy {
    public:
        StrategyPath(const plugins::Options &opts); 
        double compute_value(double parent_heuristic_priority, double op_priority, int path_lenght) override;
        std::string get_name() const override;
        int compute_heuristic_from_priority(double heuristic_priority) override; 
};
}