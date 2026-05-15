#include "type_based_wastar.h"

#include "../plugins/plugin.h"

using namespace std;

namespace plugin_type_based_wastar {

class TypeBasedWAstarFeature
    : public plugins::TypedFeature<SearchAlgorithm, type_based_wastar::TypeBasedWAstar> {
public:
    TypeBasedWAstarFeature() : TypedFeature("type_based_wastar") {
        document_title("Type-based Weighted A* (TYPE WA*)");
        document_synopsis(
            "Alternates between WA* expansions (odd steps) and type-based "
            "focal expansions (even steps). States are grouped into types by "
            "(h-value, g-value) pairs. At each even step, a type is drawn "
            "uniformly at random from FOCAL = {n | f(n) <= w * f_min}, then "
            "a state is drawn uniformly at random from that type. "
            "Guarantees w-admissible solutions when h is admissible. "
            "Reference: Cohen, Valenzano, McIlraith, IJCAI 2021.");

        add_option<shared_ptr<Evaluator>>(
            "h",
            "admissible heuristic (used for both WA* ordering and FOCAL bound)",
            plugins::ArgumentInfo::NO_DEFAULT);

        add_option<double>(
            "w",
            "suboptimality bound w >= 1.0 (higher w = faster but worse solutions)",
            "2.0");

        add_option<bool>(
            "reopen_closed",
            "reopen closed nodes when a shorter path is found",
            "true");

        add_option<int>(
            "random_seed",
            "seed for the random number generator (type and state selection)",
            "0");

        type_based_wastar::add_options_to_feature(*this);
    }

    virtual shared_ptr<type_based_wastar::TypeBasedWAstar>
    create_component(const plugins::Options &options,
                     const utils::Context &) const override {
        return make_shared<type_based_wastar::TypeBasedWAstar>(options);
    }
};

static plugins::FeaturePlugin<TypeBasedWAstarFeature> _plugin;

}  // namespace plugin_type_based_wastar
