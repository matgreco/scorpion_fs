#include <cmath>
#include "strategy_path.h"


namespace OpPriorities {


class StrategyPathNormalized : public PrioritiesStrategy {
    public:
        bool sum_succ_dependent_evaluator = false;
        StrategyPathNormalized(const plugins::Options &opts); 
        double compute_value(double parent_heuristic_priority, double op_priority, double exp_sum_siblings) override;
        std::string get_name() const override;
        int compute_heuristic_from_priority(double heuristic_priority, int path_lenght) override; 

};
}