#include "focal_search.h"
#include "search_common.h"

#include "../plugins/plugin.h"

using namespace std;

namespace plugin_focal {
class FocalSearchFeature : public plugins::TypedFeature<SearchAlgorithm, focal_search::FocalSearch> {
public:
    FocalSearchFeature() : TypedFeature("focal_search") {
        document_title("Focal search");
        document_synopsis("");

        //add_option<shared_ptr<OpenListFactory>>("open", "open list");
        add_list_option<shared_ptr<Evaluator>>("evals", "evaluators","[]");
        
        add_option<shared_ptr<Evaluator>>(
            "open_eval",
            "set evaluator for jump statistics. "
            "(Optional; if no evaluator is used, jump statistics will not be displayed.)",
            plugins::ArgumentInfo::NO_DEFAULT);
        add_option<shared_ptr<Evaluator>>(
            "focal_eval",
            "set evaluator for jump statistics. "
            "(Optional; if no evaluator is used, jump statistics will not be displayed.)",
            plugins::ArgumentInfo::NO_DEFAULT);
        add_list_option<shared_ptr<Evaluator>>(
            "preferred",
            "use preferred operators of these evaluators", "[]");
        
        add_option<int>(
            "boost",
            "boost value for preferred operator open lists", 
            "0");
        add_option<double>(
            "w",
            "w suboptimality bound", 
            "2.0");
        focal_search::add_options_to_feature(*this);
        

    }
    virtual shared_ptr<focal_search::FocalSearch> create_component(const plugins::Options &options, const utils::Context &context) const override {
        /*
        plugins::Options options_copy(options);
        auto temp = search_common::create_astar_open_list_factory_and_f_eval(options);
        options_copy.set("open", temp.first);
        options_copy.set("f_eval", temp.second);
        */
        
        //plugins::verify_list_non_empty<shared_ptr<Evaluator>>(context, options, "evals");
        plugins::Options options_copy(options);
        options_copy.set("open", search_common::create_greedy_open_list_factory(options_copy));
        //options_copy.set("eval", focal_evaluator);
        options_copy.set("reopen_closed", false);
        options_copy.set("pref_only", false);
        options_copy.set("boost", 0);

        shared_ptr<Evaluator> evaluator = nullptr;
        options_copy.set("evals", evaluator);

        
        return make_shared<focal_search::FocalSearch>(options_copy);
    }
};

static plugins::FeaturePlugin<FocalSearchFeature> _plugin;
}
