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

    // WA* open list: min-heap on (f_w = g + w*h, g_at_push, state_id).
    // Storing g_at_push lets us skip stale entries after g-updates.
    struct WAEntry {
        double fw;
        int g;
        StateID id;
        // Min-heap: smaller fw (and g as tiebreak) has higher priority.
        bool operator>(const WAEntry &o) const {
            if (fw != o.fw) return fw > o.fw;
            return g > o.g;
        }
    };
    std::priority_queue<WAEntry, std::vector<WAEntry>, std::greater<WAEntry>> wa_open;

    // Type buckets: (h, g) -> StateIDs currently believed to be in OPEN.
    // Closed states are pruned lazily when a bucket is sampled.
    std::unordered_map<TypeKey, std::vector<StateID>, TypeKeyHash> type_buckets;

    // Exact count of open states per type key (for efficient FOCAL enumeration).
    std::unordered_map<TypeKey, int, TypeKeyHash> type_open_count;

    // Exact count of open states per f = g+h value (for f_min).
    std::map<int, int> count_f;

    // Per-state data: h and f=g+h cached at the time of last insertion into OPEN.
    PerStateInformation<int> cached_h;
    PerStateInformation<int> cached_f;
    // True iff the state is currently tracked as OPEN (in type_open_count / count_f).
    PerStateInformation<bool> in_open;

    long long step_counter;
    utils::RandomNumberGenerator rng;
    std::vector<Evaluator *> path_dependent_evaluators;

    // Add state to OPEN data structures (wa_open + type bucket + counts).
    void add_to_open(const State &state, int g, int h);

    // Remove state from type bucket counts / count_f (lazy bucket vectors persist).
    // Must be called exactly once per state, just before node.close().
    void remove_from_open_tracking(const State &state);

    // Return a StateID selected uniformly-at-random from FOCAL, or StateID::no_state.
    StateID select_from_focal();

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
