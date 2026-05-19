#ifndef SEARCH_ALGORITHMS_TYPED_FOCAL_HG_SEARCH_H
#define SEARCH_ALGORITHMS_TYPED_FOCAL_HG_SEARCH_H

#include "../open_list.h"
#include "../search_algorithm.h"
#include "../utils/rng.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

class Evaluator;

namespace plugins {
class Feature;
}

namespace typed_focal_hg_search {

/*
 * Typed Focal HG Search (TFHG)
 *
 * Like Typed Focal Search but the bucket key is (h_focal, g) instead of
 * h_focal alone — matching the type definition used in Type WA*.
 * More buckets → broader diversity across both heuristic quality and depth.
 *
 * Extraction: a bucket is chosen uniformly at random (like TypeWA*'s typed
 * step), then a state is chosen uniformly at random within that bucket.
 * This ensures all (h,g) combinations in FOCAL get equal opportunity,
 * rather than always favouring the lowest-h bucket as TFS does.
 *
 * Suboptimality guarantee: only states with f(n) ≤ w·f_min enter FOCAL;
 * open_eval must be admissible.
 *
 * pref_balance: same semantics as TypedFocalSearch.
 *   0 — always typed_focal
 *   1 — alternate typed_focal / typed_pref
 *   2 — typed_pref first, fall back to typed_focal when empty
 */
class TypedFocalHGSearch : public SearchAlgorithm {
    using TypeKey = std::pair<int, int>;   // (h_focal, g)

    const int k;
    const int pref_balance;
    const double w;

    std::shared_ptr<Evaluator> open_evaluator;
    std::shared_ptr<Evaluator> focal_evaluator;
    std::shared_ptr<Evaluator> preferred_evaluator;

    // Typed FOCAL buckets: key = (h_focal, g)
    std::map<TypeKey, std::vector<StateID>> typed_focal;
    std::map<TypeKey, std::vector<StateID>> typed_pref;

    // Standard OPEN list for states with f > w·f_min, ordered by open_eval
    std::unique_ptr<StateOpenList> open_list;

    PerStateInformation<int>  f_value;
    PerStateInformation<int>  h_focal_val;
    PerStateInformation<bool> in_focal;
    PerStateInformation<bool> generated_by_pref;

    std::map<int, int> count_f;
    int f_min;

    bool next_from_pref;
    utils::RandomNumberGenerator rng;

    std::vector<Evaluator *> path_dependent_evaluators;

    bool use_pref_list(bool is_pref) const { return is_pref && pref_balance > 0; }

    void insert_typed(const State &s, int hf, int g, bool is_pref);
    void remove_typed(const State &s);
    StateID extract_typed(bool from_pref);

    void start_f_value_statistics(EvaluationContext &eval_context);

protected:
    virtual void initialize() override;
    virtual SearchStatus step() override;

public:
    explicit TypedFocalHGSearch(const plugins::Options &opts);
    virtual ~TypedFocalHGSearch() = default;

    virtual void print_statistics() const override;
    void dump_search_space() const;
};

extern void add_options_to_feature(plugins::Feature &feature);

}  // namespace typed_focal_hg_search

#endif
