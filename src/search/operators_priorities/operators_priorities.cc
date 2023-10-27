#include "operators_priorities.h"

#include "../plugins/plugin.h"

using namespace std;

namespace OpPriorities {

OpPrioritiesHeuristic::OpPrioritiesHeuristic(const plugins::Options &opts)   : Heuristic(opts) {
    cout << "OK" << endl;
}

OpPrioritiesHeuristic::~OpPrioritiesHeuristic() {
}

int OpPrioritiesHeuristic::compute_heuristic(const State &ancestor_state) {
    /*
    State state = convert_ancestor_state(ancestor_state);
    return max(0, function->get_value(state));
    */
   return 10000;
}




class OpPrioritiesFeature : public plugins::TypedFeature<Evaluator, OpPriorities::OpPrioritiesHeuristic> {
public:
    OpPrioritiesFeature() : TypedFeature("opprio") {
        document_title("Priorities evaluator");
        document_synopsis("Returns the priorities of the state.");

        add_option<int>(
            "value",
            "the constant value--",
            "1",
            plugins::Bounds("0", "infinity"));
        //add_evaluator_options_to_feature(*this);
        Heuristic::add_options_to_feature(*this);

    }
};

static plugins::FeaturePlugin<OpPrioritiesFeature> _plugin;
}

