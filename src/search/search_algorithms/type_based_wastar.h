#ifndef SEARCH_ALGORITHMS_TYPE_BASED_WASTAR_H
#define SEARCH_ALGORITHMS_TYPE_BASED_WASTAR_H

/*
  Type-WA* (type-based weighted A*)

  Cohen, Valenzano and McIlraith: "Type-WA*: Using Exploration in Bounded
  Suboptimal Planning", IJCAI 2021 (Algorithm 1, Theorem 1 and the paragraph
  "Implementation Details" of Section 3.1) and its supplementary material
  (Technical Report CSRG-638, University of Toronto).

  Expansion policy (Algorithm 1):
    - Steps 1, 3, 5, ... are WA* steps: expand an OPEN node with minimum
      f_w(n) = g(n) + w * h(n). (This implementation uses the rounded variant
      g(n) + floor(w * h(n)) of the authors' experiments, see below.)
    - Steps 2, 4, 6, ... are exploration steps: with
      f_min = min_{n in OPEN} g(n) + h(n) and
      FOCAL = {n in OPEN | g(n) + h(n) <= w * f_min},
      choose a type (h, g) among the types that have nodes in FOCAL, then a
      node of that type. Algorithm 1 and the implementation paragraph say
      "randomly"; this implementation draws both uniformly, as in the
      Type-GBFS procedure the paper adapts (Section 2.2), so the probability
      of a type does not depend on how many nodes it holds.
    - The goal test happens when a node has been selected for expansion
      (lines 11-12); a selected goal node is therefore neither closed nor
      counted as expanded.
    - A strictly cheaper path updates a node that is still in OPEN and reopens
      a node that is in CLOSED (lines 18-24, footnote 2 of the paper).
    With an admissible heuristic the returned plan costs at most w times the
    optimal cost (Theorem 1).

  Data structures ("Implementation Details"):
    - OPEN ordered by f_w for the WA* steps: integer-keyed FIFO buckets with
      the same shape as Fast Downward's best-first open list (map<int, deque>,
      push_back on insertion, pop_front on removal). Every entry stores the g
      and h it was inserted with. The paper prescribes ignoring entries of
      nodes that are already closed when they are drawn; this implementation
      additionally skips entries superseded by a reinsertion of the same node
      with a cheaper g (or a re-evaluated h), so that a WA* step always
      expands a current entry with minimum f_w. Fast Downward's StateOpenList
      is not reused because its entries carry only a StateID: a superseded
      entry of a node that is still open could not be told apart from the
      node's current entry. (The BestFirstOpenList template is file-local, so
      it cannot be instantiated with a richer entry type, and the repository's
      weight evaluator takes an integer weight, so floor(w * h) with a
      fractional w cannot be expressed with the existing evaluators either.)
    - One bucket of StateIDs per type (h, g); the buckets are grouped by
      f = g + h in a map sorted by f. All nodes of a type share f, hence FOCAL
      is the set of buckets with f <= w * f_min. Stale entries are left in the
      buckets and discarded lazily: when they are drawn, and when f_min is
      computed by discarding stale entries from the back of the lowest-f
      buckets until an entry that is still in OPEN is found. Buckets that run
      out of entries are removed. A cheaper path inserts the node into a
      bucket with a smaller f, so f_min can also decrease.
    - The h value of the latest insertion of every state (PerStateInformation).
      An entry (id, g, h) is current iff the node is open with that g and its
      latest h is that h. Because a node is reinserted only with a strictly
      smaller g, the pair (id, g) alone identifies the latest insertion; the
      stored h is kept as a consistency check.

  Decisions not specified by the paper (documented in the plugin as well):
    - Tie-breaking among equal f_w in WA* steps: FIFO, the oldest insertion
      first; a reinsertion after a cheaper path counts as a new insertion.
      This was chosen by the maintainers of this repository; the paper does
      not specify any tie-breaking.
    - f_w uses floor(w * h), the rounding of the authors' experiments with
      fractional weights (Section 4); the FOCAL threshold w * f_min and
      f = g + h are not rounded. Both quantities are computed with exact
      integer arithmetic: w is interpreted with at most six decimal digits
      (scaled_w = w * 10^6) and the bounds 1 <= w <= 1000 keep every product
      within 64 bits.
    - h is recomputed through an EvaluationContext whenever a node is
      reinserted after a cheaper path (as eager_search does); path-dependent
      evaluators are notified of every state transition. As in eager_search,
      a reopened node whose recomputed estimate is infinite is left open but
      not reinserted.
    - random_seed is declared directly (default 0, dedicated
      utils::RandomNumberGenerator) instead of the repository's
      add_rng_options helper, so that the default is explicit and the draws
      cannot be perturbed by other components that use the global generator.
*/

#include "../per_state_information.h"
#include "../search_algorithm.h"
#include "../utils/rng.h"

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

class Evaluator;

namespace plugins {
class Feature;
}

namespace type_based_wastar {
class TypeBasedWAstar : public SearchAlgorithm {
    std::shared_ptr<Evaluator> h_evaluator;
    const double w;
    // w * WEIGHT_SCALE as an integer; w is interpreted with six decimal digits.
    long long scaled_w;
    const int random_seed;
    utils::RandomNumberGenerator rng;

    /*
      OPEN for the WA* steps: f_w -> FIFO bucket (same shape as
      standard_scalar_open_list::BestFirstOpenList). An entry is current iff
      its node is open with the same g and the state's latest h is entry.h.
    */
    struct OpenEntry {
        StateID id;
        int g;
        int h;
    };
    std::map<int, std::deque<OpenEntry>> wastar_open;

    /* Type buckets for the exploration steps. */
    using TypeKey = std::pair<int, int>;   // (h, g)
    struct TypeKeyHash {
        std::size_t operator()(const TypeKey &key) const noexcept {
            return std::hash<long long>()(
                (static_cast<long long>(key.first) << 32) ^
                static_cast<unsigned int>(key.second));
        }
    };
    struct TypeBucket {
        int h;
        int g;
        std::vector<StateID> entries;
    };
    // f = g + h -> the buckets of the types with that f (never empty vectors).
    std::map<int, std::vector<TypeBucket>> buckets_by_f;
    // (h, g) -> position of its bucket within buckets_by_f[h + g].
    std::unordered_map<TypeKey, int, TypeKeyHash> bucket_position;

    // h value of the latest insertion of each state (-1: never inserted).
    PerStateInformation<int> latest_h;

    std::vector<Evaluator *> path_dependent_evaluators;

    // Number of calls to step(); odd steps are WA* steps (Algorithm 1).
    long long step_counter;

    // Additional statistics.
    long long wastar_expansions;
    long long exploration_expansions;
    long long stale_open_entries;
    long long stale_type_entries;

    int compute_fw(int g, int h) const;
    bool in_focal(int f, int f_min) const;
    bool is_current_entry(
        const SearchNode &node, const State &state, int g, int h) const;

    // Add an entry for the state to OPEN (f_w bucket and type bucket).
    void insert(const State &state, int g, int h);
    TypeBucket &get_or_create_bucket(int h, int g);
    void remove_bucket(int f, int position);

    /*
      Discard stale entries from the back of the lowest-f buckets until an
      entry that is still in OPEN is found. Return its f, which is the exact
      f_min, or -1 if OPEN contains no node.
    */
    int clean_and_get_f_min();
    /*
      Minimum f over all current entries, by scanning every bucket. Only used
      by an assertion in debug builds with verbosity=debug (it makes every
      exploration step linear in the number of stored entries).
    */
    int brute_force_f_min();

    std::optional<SearchNode> select_wastar_node();
    std::optional<SearchNode> select_exploration_node();

protected:
    virtual void initialize() override;
    virtual SearchStatus step() override;

public:
    explicit TypeBasedWAstar(const plugins::Options &opts);
    virtual ~TypeBasedWAstar() = default;

    virtual void print_statistics() const override;
};

extern void add_options_to_feature(plugins::Feature &feature);
}

#endif
