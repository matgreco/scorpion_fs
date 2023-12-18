#include <cmath>
#include "strategy_path.h"


namespace OpPriorities {


class StrategyPathNormalized : public PrioritiesStrategy {
    public:
        StrategyPathNormalized(const plugins::Options &opts); 
        double compute_value(double parent_heuristic_priority, double op_priority) override;
        std::string get_name() const override;
        int compute_heuristic_from_priority(double heuristic_priority, int path_lenght) override; 
};
}