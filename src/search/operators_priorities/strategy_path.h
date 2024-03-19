#include <cmath>
#include "priorities_strategy.h"


namespace OpPriorities {


class StrategyPath :  public PrioritiesStrategy {
    public:
        bool sum_succ_dependent_evaluator = false;
        StrategyPath(const plugins::Options &opts); 
        double compute_value(double parent_heuristic_priority, double op_priority, double exp_sum_siblings) override;
        std::string get_name() const override;
        int compute_heuristic_from_priority(double heuristic_priority, int path_lenght) override; 

   
};
}