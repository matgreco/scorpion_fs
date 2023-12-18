#include <cmath>
#include "priorities_strategy.h"


namespace OpPriorities {

class StrategyInstant : public PrioritiesStrategy {
    public:
        StrategyInstant(const plugins::Options &opts); 
        double compute_value(double parent_heuristic_priority, double op_priority) override;
        std::string get_name() const override;

        int compute_heuristic_from_priority(double heuristic_priority, int path_lenght) override;  /// IMPLEMENTAR ESTA FUNCION QUE HACE LA NORMALIZACION ES DECIR MULTIPLICA POR UNA AMPLIFICACION
};
}