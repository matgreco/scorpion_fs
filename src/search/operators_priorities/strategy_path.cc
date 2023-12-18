#include "../plugins/plugin.h"
#include "strategy_path.h"

using namespace std;

namespace OpPriorities{

StrategyPath::StrategyPath(const plugins::Options &opts)
    : PrioritiesStrategy(opts) {
}
    
double StrategyPath::compute_value(double parent_heuristic_priority, double op_priority) {
    double value = (parent_heuristic_priority + std::log10(op_priority));
    //cout << parent_heuristic_priority << " - " << op_priority << "(" << std::log10(op_priority) << ") = " << value << endl;
    return value;
}

std::string StrategyPath::get_name() const {
    return "Strategy path";
}
int StrategyPath::compute_heuristic_from_priority(double heuristic_priority, [[maybe_unused]] int path_lenght) {
    return (int) (heuristic_priority*10000)*-1;
}

class StrategyPathFeature : public plugins::TypedFeature<PrioritiesStrategy, StrategyPath> {
public:
    StrategyPathFeature() : TypedFeature("path") {
        document_title("Op priorities Path");
        document_synopsis("");
    }
};

static plugins::FeaturePlugin<StrategyPathFeature> _plugin;

}
