#include "type_multi_focal_search.h"

#include "../plugins/plugin.h"

using namespace std;

namespace plugin_type_multi_focal_search {
class TypeMultiFocalSearchFeature
    : public plugins::TypedFeature<SearchAlgorithm, type_multi_focal_search::TypeMultiFocalSearch> {
public:
    TypeMultiFocalSearchFeature() : TypedFeature("type_mfs") {
        document_title("Type multi focal search (TypeMFS)");
        document_synopsis(
            "Bounded-suboptimal focal search: an admissible heuristic h defines "
            "f = g + h and FOCAL = {n in OPEN | f(n) <= w * f_min}; one or more "
            "inadmissible focal heuristics order FOCAL (multi focal search, used "
            "in rotation). Steps alternate strictly between a focal step and a "
            "type-based exploration step as in Type-WA* (Cohen, Valenzano and "
            "McIlraith, IJCAI 2021): choose uniformly at random one of the (h, g) "
            "types with nodes in FOCAL, then uniformly at random a node of that "
            "type. Closed nodes are reopened when a strictly cheaper path is "
            "found. Every expanded node satisfied f <= w * f_min when it entered "
            "FOCAL, so the plan costs at most w times the optimal cost.");

        add_option<shared_ptr<Evaluator>>(
            "h",
            "admissible heuristic; defines f = g + h, the focal bound and the (h, g) types");
        add_list_option<shared_ptr<Evaluator>>(
            "focal_evals",
            "evaluators ordering the focal lists (one list per evaluator, used in rotation on focal steps)");
        add_list_option<shared_ptr<Evaluator>>(
            "preferred",
            "evaluators whose preferred operators mark successors for the preferred "
            "list, a FOCAL list ordered by f that focal steps use before the focal "
            "lists. Empty (default): no preferred list.",
            "[]");
        add_option<double>(
            "w",
            "suboptimality bound, 1 <= w <= 1000, with at most six decimal digits",
            "",
            plugins::Bounds("1.0", "1000.0"));
        add_option<int>(
            "random_seed",
            "seed of the random number generator used by the type steps (dedicated generator)",
            "0",
            plugins::Bounds("0", "infinity"));
        type_multi_focal_search::add_options_to_feature(*this);

        document_note(
            "Alternation",
            "Odd steps are focal steps: the preferred list if it has a node, "
            "otherwise the next focal list in the rotation (lists are tried in "
            "order until one has a node). Even steps are type steps. Hence half of "
            "the expansions are type-based, also when the preferred list is used.");
        document_note(
            "Implementation",
            "All queues are integer-keyed FIFO buckets whose entries store the "
            "(g, h) of their insertion; entries of closed or reinserted nodes are "
            "skipped when drawn, never by scanning whole buckets. The type step "
            "and the computation of f_min inside FOCAL are those of "
            "type_based_wastar; f_min outside FOCAL is the first current entry of "
            "the open list. After every expansion the nodes of the open list with "
            "f <= w * f_min are moved into FOCAL (all focal lists, the type bucket "
            "and, if applicable, the preferred list).");
        document_note(
            "Bound",
            "A node that entered FOCAL stays there even if f_min later decreases "
            "through a reopened node (sticky FOCAL): f <= w * f_min <= w * C* "
            "held when it entered. The goal node is detected at selection and is "
            "not counted as expanded; the per-list and type expansion counters "
            "add up to the number of expanded states.");
        document_note(
            "Preferred list",
            "With preferred=[...], every focal step takes its node from the "
            "preferred list whenever it has one, so the focal lists only serve "
            "when no preferred node is left. The default (empty) disables it.");
    }
};

static plugins::FeaturePlugin<TypeMultiFocalSearchFeature> _plugin;
}
