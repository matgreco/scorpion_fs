#include <fstream>
#include <cmath>
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
        float oppriority = std::stod(line);
        if (oppriority == 0.0)
            oppriority = 0.000001;
        op_priorities.push_back(oppriority);       
        ++i;
    }
    /*
    for(int i=0; i<op_priorities.size(); i++ ){
        cout << op_priorities[i] << endl;
    }
    */
    type_opprior = opts.get<int>("path_likelihood");
    
    cout << "*********************************** OP PRIORITIES TYPE " << type_opprior << endl;


}
void OpPrioritiesHeuristic::get_path_dependent_evaluators(
        std::set<Evaluator *> &evals) {
        evals.insert(this);
}
OpPrioritiesHeuristic::~OpPrioritiesHeuristic() {
}
// heuristic which return the multiplication of the priorities in the path
float OpPrioritiesHeuristic::path_heuristic(const State &state) {
    //cout << "++++ PATH HEURISTIC" << cache_heuristics_priority[*parent[state]] << " + " << std::log(priority[state]) << endl;
    //cache_heuristics_priority[state] = cache_heuristics_priority[*parent[state]] * priority[state] ;
    
    cache_heuristics_priority[state] = cache_heuristics_priority[*parent[state]] + std::log(priority[state]) ;
    return -1*cache_heuristics_priority[state];
}

float OpPrioritiesHeuristic::path_heuristic_normalized(const State &state) {
    //cout << "PATH HEURISTIC NORM en state " << endl;
    //cache_heuristics_priority[state] = cache_heuristics_priority[*parent[state]] * priority[state] ;
    cache_heuristics_priority[state] = cache_heuristics_priority[*parent[state]] + std::log(priority[state]) ;
    return -1*cache_heuristics_priority[state]/path_depth[state];
}

// heuristic which return the priority of the applied operator
float OpPrioritiesHeuristic::instant_heuristic(const State &state) {
    //cout << "instante HEURISTIC" << endl;
    return round((1-priority[state])*10000);
}

// generate a new priority function which use the priority of the parent

int OpPrioritiesHeuristic::compute_heuristic(const State &state) {
    //cout << "compute HEURISTIC" << endl;
    //return instant_heuristic(state);
    //return round(path_heuristic(state)*10000);
    //cout << "-- Computando h de state " << round(path_heuristic(state) ) << endl;
    int h_value;
    if(type_opprior == 0)
        h_value = lround((path_heuristic_normalized(state)*10000.0));
    else if (type_opprior == 1)
        h_value = lround(path_heuristic(state)*10000.0);
    else 
        h_value = lround(instant_heuristic(state));
    //cout << "compute heuristic state " << state.get_id() << " - h: " << h_value << endl;

    return h_value;
}
void OpPrioritiesHeuristic::notify_initial_state(const State &initial_state) {
    cout << "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ OP PRIORITIES HAS " << op_priorities.size()  << " OPERATORS." << endl;
    cout << "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ THE SAS TASK HAS " << initial_state.get_task().get_operators().size() << "OPERATORS" << endl;
    assert( op_priorities.size() != initial_state.get_task().get_operators().size());

    priority[initial_state] = 1.0;
    path_depth[initial_state] = 1;   
    cache_heuristics_priority[initial_state] = 0.0;
    parent[initial_state] = &initial_state; // this is just for the normalized heuristic because need to accesss to the parent depth
    cout << "OK";
}

void OpPrioritiesHeuristic::notify_state_transition(const State &parent_state, OperatorID op_id, const State &state) {
    //cout << "NOTIFY STATE TRANSITION HEURISTIC" << endl;
    // 1) PUT THE COMPUTE HEURISTIC CODE HERE
    priority[state] = op_priorities[op_id.get_index()];

    if(type_opprior == 0){

    }
    else if (type_opprior == 1)
        h_value = lround(path_heuristic(state)*10000.0);
    else 
        h_value = lround(instant_heuristic(state));

    // 2) CACHE HEURISTIC PRIORITY AS THE FINAL HEURISTIC FOR THE STATE
    // 3) UPDATE THE PRIORITY OF THE STATE IF THE NEW PRIORITY IS LOWER (AS HEURISTIC VALUE, LOWER IS BETTER)
    // 4) OPTIONS (AS I WRITED IN THE FEATURE METHOD)
    path_depth[state] = path_depth[parent_state]+1;
    
    //priority[state] = op_priorities[op_id.get_index()];
    //parent[state] = &parent_state;
    
    //cout << "Notificando " << state.get_id() << "el operador id " << op_id.get_index() << ":" << priority[state] << endl;     
}   
}


class OpPrioritiesFeature : public plugins::TypedFeature<Evaluator, OpPriorities::OpPrioritiesHeuristic> {
public:
    OpPrioritiesFeature() : TypedFeature("opprio") {
        document_title("Priorities evaluator");
        document_synopsis("Returns the priorities of the state.");
        
        // implement this as merge_and_shrink_algoritm.cc     feature.add_option<shared_ptr<LabelReduction>>
        add_option<int>(
            "path_likelihood",
            "instant (consider the likelihood of the operator) or path (consider the log-likelihood of all the path)",
            "0",
            plugins::Bounds("0", "2"));
        
        add_option<int>(
            "value",
            "the constant value--",
            "1",
            plugins::Bounds("0", "infinity"));
        
        Heuristic::add_options_to_feature(*this);

    }
};

static plugins::FeaturePlugin<OpPrioritiesFeature> _plugin;


