#include "type_based_wastar.h"

#include "../plugins/plugin.h"

using namespace std;

namespace plugin_type_based_wastar {
class TypeBasedWAstarFeature
    : public plugins::TypedFeature<SearchAlgorithm, type_based_wastar::TypeBasedWAstar> {
public:
    TypeBasedWAstarFeature() : TypedFeature("type_based_wastar") {
        document_title("Type-WA* (type-based weighted A*)");
        document_synopsis(
            "Bounded suboptimal search that alternates weighted A* expansions "
            "with type-based exploration restricted to the focal list, following "
            "Algorithm 1 of Cohen, Valenzano and McIlraith, \"Type-WA*: Using "
            "Exploration in Bounded Suboptimal Planning\" (IJCAI 2021). "
            "Odd steps expand an OPEN node with minimum f_w = g + w*h (computed "
            "here as g + floor(w*h), see the note on the weighted f-value). "
            "Even steps compute f_min = min_{n in OPEN} g(n)+h(n), take "
            "FOCAL = {n in OPEN | g(n)+h(n) <= w*f_min}, choose uniformly at "
            "random one of the (h,g) types that have nodes in FOCAL and then "
            "uniformly at random a node of that type. The goal test is "
            "performed when a node is selected for expansion. Nodes are always "
            "reopened when a strictly cheaper path is found. With an admissible "
            "heuristic the solution cost is at most w times the optimal cost "
            "(Theorem 1).");

        add_option<shared_ptr<Evaluator>>(
            "h",
            "admissible heuristic; it defines f_w, f = g + h and the (h, g) type system");
        add_option<double>(
            "w",
            "weight and suboptimality bound, 1 <= w <= 1000, with at most six "
            "decimal digits (f_w and the focal test are computed with exact "
            "integer arithmetic)",
            "",
            plugins::Bounds("1.0", "1000.0"));
        add_option<int>(
            "random_seed",
            "seed of a dedicated random number generator that draws the type "
            "and the node in exploration steps. The global Fast Downward "
            "generator is deliberately not used, so runs with the same "
            "configuration, task and seed are reproducible.",
            "0",
            plugins::Bounds("0", "infinity"));
        type_based_wastar::add_options_to_feature(*this);

        document_note(
            "Tie-breaking in WA* steps",
            "Among OPEN nodes with equal f_w the WA* step expands the one that "
            "was inserted first (FIFO). A reinsertion caused by a cheaper path "
            "counts as a new insertion. The paper does not specify the "
            "tie-breaking; FIFO was chosen by the maintainers of this repository.");
        document_note(
            "Weighted f-value",
            "Algorithm 1 defines f_w(n) = g(n) + w*h(n). This implementation "
            "uses f_w(n) = g(n) + floor(w*h(n)), the rounding of the paper's "
            "experiments with fractional weights (Section 4), so that f_w stays "
            "integral; both coincide for integer w. The focal threshold w*f_min "
            "and f = g + h are not rounded.");
        document_note(
            "Implementation",
            "OPEN is kept as FIFO buckets ordered by f_w, plus one bucket of "
            "nodes per (h, g) type, grouped by f = g + h. The two structures "
            "are not synchronised: entries of closed nodes, and entries left "
            "behind by a cheaper path, are discarded only when they are drawn, "
            "and when f_min is determined by discarding stale entries from the "
            "lowest-f buckets until an entry that is still in OPEN is found, "
            "as described in Section 3.1, \"Implementation Details\".");
        document_note(
            "Differences from eager_search",
            "The goal test is applied to the selected node before it is "
            "expanded (Algorithm 1, lines 11-12), so the goal node is neither "
            "closed nor counted in the \"Expanded\" statistic; eager_search "
            "counts it. As in eager_search, a reopened node whose recomputed "
            "heuristic value is infinite is left open but not reinserted.");
    }
};

static plugins::FeaturePlugin<TypeBasedWAstarFeature> _plugin;
}
