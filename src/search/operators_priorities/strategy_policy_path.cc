#include <cmath>
#include "../plugins/plugin.h"
#include "strategy_policy_instant.h"

using namespace std;

namespace OpPriorities{

StrategyPolicyInstant::StrategyPolicyInstant(const plugins::Options &opts)
    : PrioritiesStrategy(opts) { 
}

double StrategyPolicyInstant::compute_value([[maybe_unused]] double parent_heuristic_priority, double op_priority, double exp_sum_siblings) {
    double instant = exp(op_priority)/exp_sum_siblings;
    double value = (parent_heuristic_priority + std::log10(instant));
    return value;
}

std::string StrategyPolicyInstant::get_name() const {
    return "Strategy Policy Instant";
}

int StrategyPolicyInstant::compute_heuristic_from_priority(double heuristic_priority, [[maybe_unused]] int path_lenght) {
    return (int) (heuristic_priority*10000)*-1;
}



class StrategyPolicyInstantFeature : public plugins::TypedFeature<PrioritiesStrategy, StrategyPolicyInstant> {
public:
    StrategyPolicyInstantFeature() : TypedFeature("policy_path_norm") {
        document_title("Op priorities Instant");
        document_synopsis("");

        //ShrinkBucketBased::add_options_to_feature(*this);
    }
};

static plugins::FeaturePlugin<StrategyPolicyInstantFeature> _plugin;

}
