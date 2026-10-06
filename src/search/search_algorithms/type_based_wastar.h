#ifndef SEARCH_ALGORITHMS_TYPE_BASED_WASTAR_H
#define SEARCH_ALGORITHMS_TYPE_BASED_WASTAR_H

/*
 * Type-based Weighted A* (TYPE WA*)
 *
 * Alternates between two expansion strategies:
 *   Odd steps  : WA* — expand the node with minimum f_w = g + w*h
 *   Even steps : Type-based focal — compute FOCAL = {n | f(n) <= w*f_min},
 *                randomly select a type (h,g bucket) from FOCAL, then
 *                randomly select a state from that type.
 *
 * The type system partitions states by (h-value, g-value) pairs.
 * Guarantees w-admissible solutions when h is admissible.
 *
 * Reference: Cohen, Valenzano, McIlraith — IJCAI 2021.
 *
 * Two implementation options are exposed for evaluation:
 *
 *   focal_selection = scan (original implementation, kept to reproduce old
 *     results): every exploration step scans the whole
 *     selected bucket to drop stale entries before sampling. This costs
 *     O(|bucket|) per step, which is quadratic overall when the type system
 *     has few types (small h and g ranges, e.g. ged, openstacks).
 *   focal_selection = lazy (default; paper, Sec. 3 "Implementation Details"): sample a
 *     type uniformly among the FOCAL types, then sample a random bucket entry;
 *     a stale entry (closed, or moved to another bucket after a g-update) is
 *     removed with swap-and-pop and sampling is repeated. Expected O(1)
 *     amortized per step. f_min stays exact through count_f.
 *
 *   wa_tiebreaking = low_g (original): among equal f_w, prefer lower g.
 *   wa_tiebreaking = low_h: among equal f_w, prefer lower h, then FIFO.
 *   wa_tiebreaking = fifo: among equal f_w, oldest entry first (this is what
 *     Fast Downward's eager_wastar does: a single bucket-based open list on
 *     g + w*h with FIFO buckets).
 *   (Enum values avoid the names "g" and "h" because those are commonly used
 *   as let-variables in the search string and would be parsed as evaluators.)
 */

#include "../per_state_information.h"
#include "../search_algorithm.h"
#include "../utils/rng.h"

#include <map>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

class Evaluator;

namespace plugins {
class Feature;
}

namespace type_based_wastar {

enum class FocalSelection {
    SCAN,
    LAZY
};

enum class WATieBreaking {
    LOW_G,
    LOW_H,
    FIFO
};

class TypeBasedWAstar : public SearchAlgorithm {
    // Type key: (h-value, g-value) — defines a type bucket.
    using TypeKey = std::pair<int, int>;

    struct TypeKeyHash {
        std::size_t operator()(const TypeKey &k) const noexcept {
            return std::hash<long long>()(
                static_cast<long long>(k.first) * 1000003LL + k.second);
        }
    };

    std::shared_ptr<Evaluator> h_evaluator;
    double w;
    bool reopen_closed_nodes;
    FocalSelection focal_selection;
    WATieBreaking wa_tiebreaking;

    // WA* open list: min-heap on f_w = g + w*h with configurable tie-breaking.
    // Storing g_at_push lets us skip stale entries after g-updates.
    struct WAEntry {
        double fw;
        int g;
        int h;
        unsigned long long seq;
        StateID id;
    };

    // Returns true iff a has LOWER priority than b (std::priority_queue semantics).
    struct WAEntryCompare {
        WATieBreaking mode;
        bool operator()(const WAEntry &a, const WAEntry &b) const {
            if (a.fw != b.fw)
                return a.fw > b.fw;
            switch (mode) {
            case WATieBreaking::LOW_H:
                if (a.h != b.h)
                    return a.h > b.h;      // prefer lower h
                return a.seq > b.seq;      // then FIFO
            case WATieBreaking::FIFO:
                return a.seq > b.seq;      // oldest entry first
            case WATieBreaking::LOW_G:
            default:
                return a.g > b.g;          // original: prefer lower g
            }
        }
    };
    std::priority_queue<WAEntry, std::vector<WAEntry>, WAEntryCompare> wa_open;
    unsigned long long wa_seq;

    // Type buckets: (h, g) -> StateIDs currently believed to be in OPEN.
    // Closed states are pruned lazily when a bucket is sampled.
    std::unordered_map<TypeKey, std::vector<StateID>, TypeKeyHash> type_buckets;

    // Exact count of open states per type key (for efficient FOCAL enumeration).
    std::unordered_map<TypeKey, int, TypeKeyHash> type_open_count;

    // LAZY mode only: types with at least one open state, grouped by f = h + g.
    // FOCAL types are exactly the entries with f <= w * f_min.
    std::map<int, std::vector<TypeKey>> focal_types_by_f;

    // Exact count of open states per f = g+h value (for f_min).
    std::map<int, int> count_f;

    // Per-state data: h and f=g+h cached at the time of last insertion into OPEN.
    PerStateInformation<int> cached_h;
    PerStateInformation<int> cached_f;
    // True iff the state is currently tracked as OPEN (in type_open_count / count_f).
    PerStateInformation<bool> in_open;

    long long step_counter;
    long long stale_entries_dropped;
    utils::RandomNumberGenerator rng;
    std::vector<Evaluator *> path_dependent_evaluators;

    // Add state to OPEN data structures (wa_open + type bucket + counts).
    void add_to_open(const State &state, int g, int h);

    // Remove state from type bucket counts / count_f (lazy bucket vectors persist).
    // Must be called exactly once per state, just before node.close().
    void remove_from_open_tracking(const State &state);

    // LAZY mode bookkeeping for focal_types_by_f.
    void focal_types_add(const TypeKey &key);
    void focal_types_remove(const TypeKey &key);

    // Return a StateID selected uniformly-at-random from FOCAL, or StateID::no_state.
    StateID select_from_focal();
    StateID select_from_focal_scan();
    StateID select_from_focal_lazy();

    // Shared expansion logic: close node, generate successors, insert into OPEN.
    SearchStatus do_expansion(const State &state, SearchNode &node);

protected:
    virtual void initialize() override;
    virtual SearchStatus step() override;

public:
    explicit TypeBasedWAstar(const plugins::Options &opts);
    virtual void print_statistics() const override;
};

void add_options_to_feature(plugins::Feature &feature);

}  // namespace type_based_wastar

#endif
