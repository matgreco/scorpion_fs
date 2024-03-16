#include <cmath>
#include "../plugins/plugin.h"
#include "strategy_policy_instant.h"

using namespace std;

namespace OpPriorities{

StrategyPolicyInstant::StrategyPolicyInstant(const plugins::Options &opts)
    : PrioritiesStrategy(opts) { 
}

double StrategyPolicyInstant::compute_value([[maybe_unused]] double parent_heuristic_priority, double op_priority, double exp_sum_siblings) {
    double value = exp(op_priority)/exp_sum_siblings;
    return value;
}

std::string StrategyPolicyInstant::get_name() const {
    return "Strategy Policy Instant";
}

// NO LISTO
int StrategyPolicyInstant::compute_heuristic_from_priority(double heuristic_priority, [[maybe_unused]] int path_lenght) {
    return (int)((1 - heuristic_priority)*10000);
}



class StrategyPolicyInstantFeature : public plugins::TypedFeature<PrioritiesStrategy, StrategyPolicyInstant> {
public:
    StrategyPolicyInstantFeature() : TypedFeature("policy_instant") {
        document_title("Op priorities Instant");
        document_synopsis("");

        //ShrinkBucketBased::add_options_to_feature(*this);
    }
};

static plugins::FeaturePlugin<StrategyPolicyInstantFeature> _plugin;

}
