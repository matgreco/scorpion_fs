#ifndef SEARCH_ALGORITHMS_DISCREPANCY_FOCAL_SEARCH_H
#define SEARCH_ALGORITHMS_DISCREPANCY_FOCAL_SEARCH_H

#include "../open_list.h"
#include "../search_algorithm.h"
#include "../utils/rng.h"

#include <map>
#include <memory>
#include <queue>
#include <string>
#include <tuple>
#include <vector>

class Evaluator;

namespace plugins {
class Feature;
}

namespace discrepancy_focal_search {

/*
 * Round-robin order over the FOCAL views: h = min-h_focal heap,
 * p = preferred-operator heap, d = discrepancy buckets.
 */
enum class Rotation {
    H, P, D, HP, HD, PD, HPD
};

/*
 * Discrepancy Focal Search (DFS)
 *
 * Bounded-suboptimal search that alternates between three views of the same
 * FOCAL list (f(n) <= w * f_min, open_eval must be admissible):
 *
 *   - focal_h:    min-heap ordered by h_focal (classic focal exploitation)
 *   - focal_pref: min-heap ordered by h_focal, restricted to states generated
 *                 by a preferred operator (on their current best path)
 *   - focal_disc: buckets keyed by discrepancy d(n); a bucket is chosen
 *                 uniformly at random, then the min-h state within is taken
 *
 * Discrepancy is an edge property accumulated along the current best path:
 *   d(root) = 0
 *   d(succ) = d(parent) + 1  iff  h_focal(succ) > min h_focal among the
 *                                 siblings generated when parent was expanded
 * Ties with the sibling minimum count as no discrepancy. Like g, d is
 * path-dependent and is updated whenever a cheaper path to a state is found.
 *
 * All three views are kept consistent via lazy deletion: entries store the
 * (key, g) under which they were pushed and are discarded on extraction if
 * the state was closed, its g changed, or (for focal_disc) its d changed.
 *
 * Suboptimality guarantee: every expanded state satisfies f(n) <= w * f_min,
 * hence any solution returned costs at most w * C*. The guarantee is
 * independent of the alternation policy.
 *
 * rotation: string over {h, p, d} giving the round-robin expansion order,
 * e.g. "hpd" (default). If a view is empty (or pref_eval is not given),
 * the remaining views are tried as fallback.
 */
class DiscrepancyFocalSearch : public SearchAlgorithm {
    struct HeapEntry {
        int h;
        int g;
        StateID id;
    };
    struct HeapEntryGreater {
        bool operator()(const HeapEntry &a, const HeapEntry &b) const {
            return std::tie(a.h, a.g) > std::tie(b.h, b.g);
        }
    };
    using MinHHeap =
        std::priority_queue<HeapEntry, std::vector<HeapEntry>, HeapEntryGreater>;

    const int k;
    const double w;
    const std::string rotation;

    std::shared_ptr<Evaluator> open_evaluator;
    std::shared_ptr<Evaluator> focal_evaluator;
    std::shared_ptr<Evaluator> preferred_evaluator;

    // FOCAL views (lazy deletion)
    MinHHeap focal_h;
    MinHHeap focal_pref;
    std::map<int, MinHHeap> focal_disc;   // key: discrepancy d

    // Standard OPEN list for states with f > w·f_min, ordered by open_eval
    std::unique_ptr<StateOpenList> open_list;

    PerStateInformation<int>  f_value;
    PerStateInformation<int>  h_focal_val;
    PerStateInformation<int>  d_value;
    PerStateInformation<bool> in_focal;
    PerStateInformation<bool> generated_by_pref;

    // f values of states currently in FOCAL (for f_min maintenance)
    std::map<int, int> count_f;
    int f_min;

    size_t rotation_idx;
    utils::RandomNumberGenerator rng;

    // Per-view expansion statistics
    long expanded_from_h;
    long expanded_from_pref;
    long expanded_from_disc;

    std::vector<Evaluator *> path_dependent_evaluators;

    // Push entries for s into the views it belongs to (h always, pref if
    // is_pref, disc bucket d). Does NOT touch count_f / in_focal.
    void push_views(const State &s, int hf, int g, int d, bool is_pref);
    // First-time admission of s into FOCAL: push_views + count_f + in_focal.
    void insert_focal(const State &s, int hf, int g, int d, bool is_pref);

    StateID extract_heap(MinHHeap &heap, bool require_pref);
    StateID extract_disc();
    // Extract from the view named by src ('h'/'p'/'d'), falling back to the
    // other views if empty. Sets used_src to the view that actually provided
    // the state.
    StateID extract_source(char src, char &used_src);

    // Drop closed / already-transferred entries from the top of OPEN.
    void clean_open_top();
    void recompute_f_min_and_transfer();

    void start_f_value_statistics(EvaluationContext &eval_context);

protected:
    virtual void initialize() override;
    virtual SearchStatus step() override;

public:
    explicit DiscrepancyFocalSearch(const plugins::Options &opts);
    virtual ~DiscrepancyFocalSearch() = default;

    virtual void print_statistics() const override;
    void dump_search_space() const;
};

extern void add_options_to_feature(plugins::Feature &feature);

}  // namespace discrepancy_focal_search

#endif
