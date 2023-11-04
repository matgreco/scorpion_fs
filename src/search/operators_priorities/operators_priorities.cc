#include <fstream>
#include "operators_priorities.h"


using namespace std;

namespace OpPriorities {

OpPrioritiesHeuristic::OpPrioritiesHeuristic(const plugins::Options &opts)  : Heuristic(opts) {
    cout << "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ OP PRIORITIES CREATED " << endl;
    std::fstream in("workspace/action_probabilities.txt");
    std::string line;
    int i = 0;

    if (!std::ifstream("workspace/action_probabilities.txt")){
        std::cerr << "Error: El archivo de prioridades no existe" ;
    }

    while (std::getline(in, line))
    {
        std::stringstream ss(line);
        op_priorities.push_back(std::stod(line));       
        ++i;
    }

    for(int i=0; i<op_priorities.size(); i++ ){
        cout << op_priorities[i] << endl;
    }


    
}
void OpPrioritiesHeuristic::get_path_dependent_evaluators(
        std::set<Evaluator *> &evals) {
        evals.insert(this);
}
OpPrioritiesHeuristic::~OpPrioritiesHeuristic() {
}
// heuristic which return the multiplication of the priorities in the path
float OpPrioritiesHeuristic::path_heuristic(const State &state) {
    cache_heuristics_priority[state] = cache_heuristics_priority[*parent[state]] * priority[state] ;
    return cache_heuristics_priority[state];
}

// heuristic which return the priority of the applied operator
float OpPrioritiesHeuristic::instant_heuristic(const State &state) {
    return round((1-priority[state])*10000);
}

int OpPrioritiesHeuristic::compute_heuristic(const State &state) {
    return round(1-instant_heuristic(state))*10000;
    //return (1-path_heuristic(state))*10000;
}
void OpPrioritiesHeuristic::notify_initial_state(const State &initial_state) {
    cout << "INITIAL STATE" << endl ; 
    cache_heuristics_priority[initial_state] = 1;
}

void OpPrioritiesHeuristic::notify_state_transition(const State &parent_state, [[maybe_unused]] OperatorID op_id, const State &state) {
    priority[state] = op_priorities[op_id.get_index()];
    parent[state] = &parent_state;
    
    cout << "Notificando " << state.get_id() << "el operador id " << op_id.get_index() << ":" << priority[state] << endl;     
}   
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
        Heuristic::add_options_to_feature(*this);

    }
};

static plugins::FeaturePlugin<OpPrioritiesFeature> _plugin;


