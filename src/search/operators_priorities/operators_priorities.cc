#include <fstream>
#include <cmath>
#include "operators_priorities.h"
#include "../task_utils/successor_generator.h"


using namespace std;

namespace OpPriorities {

OpPrioritiesHeuristic::OpPrioritiesHeuristic(
    const plugins::Options &opts)  
    : Heuristic(opts), 
      priority_strategy(opts.get<shared_ptr<PrioritiesStrategy>> ("priority")),
      successor_generator(nullptr){

    string strategy_name = priority_strategy->get_name();
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
        double oppriority = std::stod(line);
        if (oppriority == 0.0)
            oppriority = 0.000001;
        op_priorities.push_back(oppriority);       
        ++i;
    }
    cout << "*********************************** OP PRIORITIES TYPE " << strategy_name << endl;
    successor_generator =
            utils::make_unique_ptr<successor_generator::SuccessorGenerator>(
                task_proxy);
}

void OpPrioritiesHeuristic::get_path_dependent_evaluators(
        std::set<Evaluator *> &evals) {
        evals.insert(this);
}

OpPrioritiesHeuristic::~OpPrioritiesHeuristic() {
}

int OpPrioritiesHeuristic::compute_heuristic(const State &state) {
    int value = priority_strategy->compute_heuristic_from_priority(cache_heuristics_priority[state], path_depth[state]);
    cout << " * state " << state.get_id() << " heuristic = " << value << " - cache heuristic priority " << cache_heuristics_priority[state] << endl;
    return value;
}

void OpPrioritiesHeuristic::notify_initial_state(const State &initial_state) {
    cout << "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ OP PRIORITIES HAS " << op_priorities.size()  << " OPERATORS." << endl;
    cout << "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ THE SAS TASK HAS " << initial_state.get_task().get_operators().size() << "OPERATORS" << endl;
    assert( op_priorities.size() != initial_state.get_task().get_operators().size());

    //priority[initial_state] = 1.0;
    path_depth[initial_state] = 1;   
    cache_heuristics_priority[initial_state] = 0.0;
    //parent[initial_state] = &initial_state; // this is just for the normalized heuristic because need to accesss to the parent depth

    sum_priorities_siblings[initial_state] = 0;
    // only if the priority strategy needs to has the sum of the siblings and if is not already calculated 
    if (priority_strategy->sum_succ_dependent_evaluator && !sum_priorities_siblings_ready[initial_state]){
        vector<OperatorID> applicable_ops;
        successor_generator->generate_applicable_ops(initial_state, applicable_ops);

        sum_priorities_siblings_ready[initial_state] = true;
        for (OperatorID op_id_succs : applicable_ops) {
            sum_priorities_siblings[initial_state] += exp(op_priorities[op_id_succs.get_index()]);
        }
    }
    
}

void OpPrioritiesHeuristic::notify_state_transition(const State &parent_state, OperatorID op_id, const State &state) {
    path_depth[state] = path_depth[parent_state]+1;
    // only if the priority strategy needs to has the sum of the siblings and if is not already calculated 
    if (priority_strategy->sum_succ_dependent_evaluator && !sum_priorities_siblings_ready[parent_state]){
        vector<OperatorID> applicable_ops;
        successor_generator->generate_applicable_ops(parent_state, applicable_ops);
        sum_priorities_siblings[parent_state] = 0;
        sum_priorities_siblings_ready[parent_state] = true;
        for (OperatorID op_id_succs : applicable_ops) {
            sum_priorities_siblings[parent_state] += exp(op_priorities[op_id_succs.get_index()]);
        }
    } 
    
    cout << "  - calculando para el op con priority " << op_priorities[op_id.get_index()] << " y sum " << sum_priorities_siblings[parent_state] << endl;
    double value = priority_strategy->compute_value(cache_heuristics_priority[parent_state], op_priorities[op_id.get_index()], sum_priorities_siblings[parent_state]);  //<--- cambiar
    cache_heuristics_priority[state] = value;

    if (cache_heuristics_priority[state] < value) 
        cache_heuristics_priority[state] = value;
    
    // 2) CACHE HEURISTIC PRIORITY AS THE FINAL HEURISTIC FOR THE STATE
    // 3) UPDATE THE PRIORITY OF THE STATE IF THE NEW PRIORITY IS LOWER (AS HEURISTIC VALUE, LOWER IS BETTER)
    // 4) OPTIONS (AS I WRITED IN THE FEATURE METHOD)
    
    
}   
}


class OpPrioritiesFeature : public plugins::TypedFeature<Evaluator, OpPriorities::OpPrioritiesHeuristic> {
public:
    OpPrioritiesFeature() : TypedFeature("operator_priorities") {
        document_title("Priorities evaluator");
        document_synopsis("Returns the priorities of the state.");
        
        // implement this as merge_and_shrink_algoritm.cc     feature.add_option<shared_ptr<LabelReduction>>
        add_option<shared_ptr<OpPriorities::PrioritiesStrategy>>(
            "priority",
            "Select the priority strategy");

        Heuristic::add_options_to_feature(*this);

    }
};

static plugins::FeaturePlugin<OpPrioritiesFeature> _plugin;


