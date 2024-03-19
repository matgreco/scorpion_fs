#include <cmath>
#include "priorities_strategy.h"

//#include "../state_registry.h"
#include "../per_state_bitset.h"


namespace OpPriorities {

class StrategyPolicyPath : public PrioritiesStrategy {

    public:
        bool sum_succ_dependent_evaluator = true;
        StrategyPolicyPath(const plugins::Options &opts); 
        double compute_value(double parent_heuristic_priority, double op_priority, double exp_sum_siblings) override; // this require the sum of the siblings
        std::string get_name() const override;

        int compute_heuristic_from_priority(double heuristic_priority, int path_lenght) override;  /// IMPLEMENTAR ESTA FUNCION QUE HACE LA NORMALIZACION ES DECIR MULTIPLICA POR UNA AMPLIFICACION

        
};
}