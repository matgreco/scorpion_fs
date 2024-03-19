#include <cmath>
#include "../plugins/plugin.h"
#include "strategy_policy_path_normalized.h"

using namespace std;

namespace OpPriorities{

StrategyPolicyPathNormalized::StrategyPolicyPathNormalized(const plugins::Options &opts)
    : PrioritiesStrategy(opts) { 
}

double StrategyPolicyPathNormalized::compute_value([[maybe_unused]] double parent_heuristic_priority, double op_priority, double exp_sum_siblings) {
    double instant = exp(op_priority)/exp_sum_siblings;
    double value = (parent_heuristic_priority + std::log10(instant));
    return value;
}

std::string StrategyPolicyPathNormalized::get_name() const {
    return "Strategy Policy Instant";
}

int StrategyPolicyPathNormalized::compute_heuristic_from_priority(double heuristic_priority, [[maybe_unused]] int path_lenght) {
    return (int) (heuristic_priority*10000)*-1/path_lenght;;
}



class StrategyPolicyPathNormalizedFeature : public plugins::TypedFeature<PrioritiesStrategy, StrategyPolicyPathNormalized> {
public:
    StrategyPolicyPathNormalizedFeature() : TypedFeature("policy_path_norm") {
        document_title("Op priorities policy path norm");
        document_synopsis("");

        //ShrinkBucketBased::add_options_to_feature(*this);
    }
};

static plugins::FeaturePlugin<StrategyPolicyPathNormalizedFeature> _plugin;

}
