#include "../plugins/plugin.h"
#include "strategy_instant.h"

using namespace std;

namespace OpPriorities{

StrategyInstant::StrategyInstant(const plugins::Options &opts)
    : PrioritiesStrategy(opts) { 
}

double StrategyInstant::compute_value([[maybe_unused]] double parent_heuristic_priority, double op_priority) {
    double value = op_priority;
    return value;
}

std::string StrategyInstant::get_name() const {
    return "Strategy Instant";
}

int StrategyInstant::compute_heuristic_from_priority(double heuristic_priority, [[maybe_unused]] int path_lenght) {
    return (int)((1 - heuristic_priority)*10000);
}



class StrategyInstantFeature : public plugins::TypedFeature<PrioritiesStrategy, StrategyInstant> {
public:
    StrategyInstantFeature() : TypedFeature("instant") {
        document_title("Op priorities Instant");
        document_synopsis("");

        //ShrinkBucketBased::add_options_to_feature(*this);
    }
};

static plugins::FeaturePlugin<StrategyInstantFeature> _plugin;

}
