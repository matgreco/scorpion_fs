#include <cmath>
#include "../plugins/plugin.h"
#include "strategy_policy_path.h"

using namespace std;

namespace OpPriorities{

StrategyPolicyPath::StrategyPolicyPath(const plugins::Options &opts)
    : PrioritiesStrategy(opts) { 
}

double StrategyPolicyPath::compute_value([[maybe_unused]] double parent_heuristic_priority, double op_priority, double exp_sum_siblings) {
    double instant = exp(op_priority)/exp_sum_siblings;
    double value = (parent_heuristic_priority + std::log10(instant));
    return value;
}

std::string StrategyPolicyPath::get_name() const {
    return "Strategy Policy Instant";
}

int StrategyPolicyPath::compute_heuristic_from_priority(double heuristic_priority, [[maybe_unused]] int path_lenght) {
    return (int) (heuristic_priority*10000)*-1;
}



class StrategyPolicyPathFeature : public plugins::TypedFeature<PrioritiesStrategy, StrategyPolicyPath> {
public:
    StrategyPolicyPathFeature() : TypedFeature("policy_path") {
        document_title("Op priorities policy path");
        document_synopsis("");

        //ShrinkBucketBased::add_options_to_feature(*this);
    }
};

static plugins::FeaturePlugin<StrategyPolicyPathFeature> _plugin;

}
