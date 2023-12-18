#include "../plugins/plugin.h"
#include "strategy_path_normalized.h"

using namespace std;

namespace OpPriorities{

StrategyPathNormalized::StrategyPathNormalized(const plugins::Options &opts)
    : PrioritiesStrategy(opts) {
}

double StrategyPathNormalized::compute_value(double parent_heuristic_priority, double op_priority) {
    double value = (parent_heuristic_priority + std::log10(op_priority));
    //cout << parent_heuristic_priority << " - " << op_priority << "(" << std::log10(op_priority) << ") = " << value << endl;
    return value;
}

std::string StrategyPathNormalized::get_name() const {
    return "Strategy path normalized";
}

int StrategyPathNormalized::compute_heuristic_from_priority(double heuristic_priority, int path_lenght) {
    return (int) ((heuristic_priority*10000)*-1)/path_lenght;
}



class StrategyPathNormalizedFeature : public plugins::TypedFeature<PrioritiesStrategy, StrategyPathNormalized> {
public:
    StrategyPathNormalizedFeature() : TypedFeature("path_norm") {
        document_title("Op priorities Path Normalized");
        document_synopsis("");
    }
};

static plugins::FeaturePlugin<StrategyPathNormalizedFeature> _plugin;

}
