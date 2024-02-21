#include "random_heuristic.h"

using namespace std;

namespace random_heuristic {
RandomHeuristic::RandomHeuristic(const plugins::Options &opts)
    : Heuristic(opts) {
    
    int mv = opts.get<int>("max_value");
    int rs = opts.get<int>("seed");

    this->random_seed = rs;
    this->max_random_value = mv;
    srand(this->random_seed);
    this->rng = make_shared<utils::RandomNumberGenerator>(random_seed);
}

RandomHeuristic::~RandomHeuristic() {
}

int RandomHeuristic::compute_heuristic(const State &state) {
    if (task_properties::is_goal_state(task_proxy, state))
        return 0;
    else {
        return this->rng->random(this->max_random_value);
    }
}

class RandomHeuristicFeature : public plugins::TypedFeature<Evaluator, RandomHeuristic> {
public:
    RandomHeuristicFeature() : TypedFeature("random_heuristic") {
        //document_subcategory("evaluators_basic");
        //document_title("Constant evaluator");
        //document_synopsis("Returns a constant value.");

        add_option<int>(
            "max_value",
            "the random max value",
            "100",
            plugins::Bounds("0", "infinity"));
        add_option<int>(
            "seed",
            "random seed",
            "123",
            plugins::Bounds("0", "infinity"));
        //add_evaluator_options_to_feature(*this);
        Heuristic::add_options_to_feature(*this);

    }
};

static plugins::FeaturePlugin<RandomHeuristicFeature> _plugin;
}

