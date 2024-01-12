/*
#include "random_heuristic.h"

#include "../plugins/plugin.h"

#include "../utils/logging.h"

#include "../task_utils/task_properties.h"

#include <cstddef>
#include <limits>
#include <utility>

using namespace std;

namespace random_heuristic {
RandomHeuristic::RandomHeuristic(const Options &opts)
    : Heuristic(opts){

    string mv = opts.get<string>("max_value");
    string rs = opts.get<string>("random_seed");
    cout << "Initializing random heuristic with random seed " << rs << " and max val " << mv << endl;
    this->random_seed = stoi(rs);
    this->max_random_value = stoi(mv);
    srand(this->random_seed);

}

RandomHeuristic::~RandomHeuristic() {
}

int RandomHeuristic::compute_heuristic(const State &global_state) {
    State state = convert_global_state(global_state);    
    if (task_properties::is_goal_state(task_proxy, state))
        return 0;
    else
        return rand() % this->max_random_value;
}

static shared_ptr<Heuristic> _parse(OptionParser &parser) {
    parser.document_synopsis("Random heuristic",
                             "Returns random value for "
                             "non-goal states, "
                             "0 for goal states");
    parser.document_language_support("action costs", "supported");
    parser.document_language_support("conditional effects", "supported");
    parser.document_language_support("axioms", "supported");
    parser.document_property("admissible", "no");
    parser.document_property("consistent", "no");
    parser.document_property("safe", "no");
    parser.document_property("preferred operators", "no");

    Heuristic::add_options_to_parser(parser);
    parser.add_option<string>("random_seed", "Random Seed", "1");
    parser.add_option<string>("max_value", "Max Value", "100");
    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<RandomHeuristic>(opts);
}

static Plugin<Evaluator> _plugin("random", _parse);
}
*/