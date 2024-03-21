#include "../plugins/plugin.h"
#include "strategy_path_min.h"

using namespace std;

namespace OpPriorities{

StrategyPathMin::StrategyPathMin(const plugins::Options &opts)
    : PrioritiesStrategy(opts) {
}
    
double StrategyPathMin::compute_value(double parent_heuristic_priority, double op_priority, [[maybe_unused]] double exp_sum_siblings) {
    double value = min(op_priority, parent_heuristic_priority);
    //cout << parent_heuristic_priority << " - " << op_priority << " value min = " << value << endl;
    return value;
}

std::string StrategyPathMin::get_name() const {
    return "Strategy path min"; 
}
int StrategyPathMin::compute_heuristic_from_priority(double heuristic_priority, [[maybe_unused]] int path_lenght) {
    //cout << "- H path min = " << (int)((1 - heuristic_priority)*10000) << " (value )" << heuristic_priority << endl;
    return (int)((1 - heuristic_priority)*10000);

}

class StrategyPathMinFeature : public plugins::TypedFeature<PrioritiesStrategy, StrategyPathMin> {
public:
    StrategyPathMinFeature() : TypedFeature("path_min") {
        document_title("Op priorities Path Min");
        document_synopsis("");
    }
};

static plugins::FeaturePlugin<StrategyPathMinFeature> _plugin;

}
