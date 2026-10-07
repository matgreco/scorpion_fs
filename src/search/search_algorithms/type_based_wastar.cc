#include "type_based_wastar.h"

#include "../evaluation_context.h"
#include "../evaluator.h"
#include "../plugins/options.h"
#include "../plugins/plugin.h"
#include "../task_utils/successor_generator.h"
#include "../utils/logging.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <set>

using namespace std;

namespace type_based_wastar {
/*
  Tolerance for the floating-point products w * h and w * f_min. For weights
  with at most six decimal digits, a product that is not an integer is at
  least 1e-6 away from the nearest integer, so the tolerance only repairs
  representation errors such as 1.7 * 10 = 16.999999999999996.
*/
static const double EPSILON = 1e-9;

TypeBasedWAstar::TypeBasedWAstar(const plugins::Options &opts)
    : SearchAlgorithm(opts),
      h_evaluator(opts.get<shared_ptr<Evaluator>>("h")),
      w(opts.get<double>("w")),
      random_seed(opts.get<int>("random_seed")),
      rng(random_seed),
      latest_h(-1),
      step_counter(0),
      wastar_expansions(0),
      exploration_expansions(0),
      stale_open_entries(0),
      stale_type_entries(0) {
    // w >= 1 is enforced by the option bounds in the plugin.
    assert(w >= 1.0);
}

/*
  f_w(n) = g(n) + floor(w * h(n)): the rounding used by the authors'
  experimental implementation for fractional weights (Section 4 of the paper).
  For integer weights it coincides with the theoretical g(n) + w * h(n).
*/
int TypeBasedWAstar::compute_fw(int g, int h) const {
    return g + static_cast<int>(floor(w * h + EPSILON));
}

/*
  An entry (type bucket (h, g), or OPEN entry inserted with g and h) is
  current iff its node is still open with that g and the h of the latest
  insertion of the state is that h. Entries of closed nodes and entries left
  behind by a reinsertion with a cheaper g (or a re-evaluated h) are stale.
*/
bool TypeBasedWAstar::is_current_entry(
    const SearchNode &node, const State &state, int g, int h) const {
    return node.is_open() && node.get_g() == g && latest_h[state] == h;
}

void TypeBasedWAstar::insert(const State &state, int g, int h) {
    StateID id = state.get_id();
    // FIFO: the new entry goes to the back of its f_w bucket.
    wastar_open[compute_fw(g, h)].push_back({id, g, h});
    get_or_create_bucket(h, g).entries.push_back(id);
    latest_h[state] = h;
}

TypeBasedWAstar::TypeBucket &TypeBasedWAstar::get_or_create_bucket(int h, int g) {
    TypeKey key(h, g);
    vector<TypeBucket> &buckets = buckets_by_f[g + h];
    auto it = bucket_position.find(key);
    if (it != bucket_position.end())
        return buckets[it->second];
    bucket_position[key] = static_cast<int>(buckets.size());
    buckets.push_back({h, g, {}});
    return buckets.back();
}

void TypeBasedWAstar::remove_bucket(int f, int position) {
    auto it = buckets_by_f.find(f);
    assert(it != buckets_by_f.end());
    vector<TypeBucket> &buckets = it->second;
    assert(position >= 0 && position < static_cast<int>(buckets.size()));
    bucket_position.erase(TypeKey(buckets[position].h, buckets[position].g));
    int last = static_cast<int>(buckets.size()) - 1;
    if (position != last) {
        buckets[position] = move(buckets[last]);
        bucket_position[TypeKey(buckets[position].h, buckets[position].g)] = position;
    }
    buckets.pop_back();
    if (buckets.empty())
        buckets_by_f.erase(it);
}

/*
  "Before selecting a node for an exploratory expansion, we remove nodes from
  the lowest f-cost bucket until we find one that is open." Stale entries are
  discarded from the back of the first bucket with the lowest f; a bucket
  that runs out of entries is removed, and so is an f value without buckets.
  The first entry that is still current gives the exact f_min. Current
  entries are never removed here. A cheaper path to a node inserts a new
  entry in a bucket with a smaller f, so f_min can also decrease.
*/
int TypeBasedWAstar::clean_and_get_f_min() {
    while (!buckets_by_f.empty()) {
        auto it = buckets_by_f.begin();
        int f = it->first;
        vector<TypeBucket> &buckets = it->second;
        assert(!buckets.empty());
        TypeBucket &bucket = buckets.front();
        while (!bucket.entries.empty()) {
            StateID id = bucket.entries.back();
            State state = state_registry.lookup_state(id);
            SearchNode node = search_space.get_node(state);
            if (is_current_entry(node, state, bucket.g, bucket.h))
                return f;
            bucket.entries.pop_back();
            ++stale_type_entries;
        }
        // No current entry in this bucket: remove it and look at the next
        // bucket with the lowest f (the map iterator is re-fetched because
        // remove_bucket may erase the f value).
        remove_bucket(f, 0);
    }
    return -1;
}

// Minimum f over all current entries of all type buckets (assertions only).
int TypeBasedWAstar::brute_force_f_min() {
    int f_min = -1;
    for (const auto &[f, buckets] : buckets_by_f) {
        for (const TypeBucket &bucket : buckets) {
            for (StateID id : bucket.entries) {
                State state = state_registry.lookup_state(id);
                SearchNode node = search_space.get_node(state);
                if (is_current_entry(node, state, bucket.g, bucket.h)) {
                    if (f_min == -1 || f < f_min)
                        f_min = f;
                }
            }
        }
    }
    return f_min;
}

/*
  WA* step: the oldest entry of the lowest f_w bucket (FIFO). Entries that
  are no longer current are skipped, as the paper prescribes for closed nodes
  in the unsynchronised queues.
*/
optional<SearchNode> TypeBasedWAstar::select_wastar_node() {
    while (!wastar_open.empty()) {
        auto it = wastar_open.begin();
        deque<OpenEntry> &bucket = it->second;
        OpenEntry entry = bucket.front();
        bucket.pop_front();
        if (bucket.empty())
            wastar_open.erase(it);
        State state = state_registry.lookup_state(entry.id);
        SearchNode node = search_space.get_node(state);
        if (is_current_entry(node, state, entry.g, entry.h)) {
            if (log.is_at_least_debug()) {
                log << "WA* step " << step_counter << ": state " << entry.id
                    << " g=" << entry.g << " h=" << entry.h
                    << " f_w=" << compute_fw(entry.g, entry.h) << endl;
            }
            return node;
        }
        ++stale_open_entries;
    }
    return nullopt;
}

/*
  Exploration step: uniform choice of a type among the types with nodes in
  FOCAL, then uniform choice of a node of that type. The type buckets may
  still hold stale entries: a stale entry that is drawn is discarded and the
  draw within the type is repeated; a type that turns out to have no current
  entry is removed and the type is drawn again, uniformly among the remaining
  types of FOCAL. Every repetition removes at least one entry or bucket, and
  clean_and_get_f_min has just verified that a bucket of FOCAL contains a
  current entry, so the procedure terminates with a node whenever OPEN is
  not empty.
*/
optional<SearchNode> TypeBasedWAstar::select_exploration_node() {
    int f_min = clean_and_get_f_min();
    if (f_min == -1)
        return nullopt;
    assert(f_min == brute_force_f_min());
    double threshold = w * f_min + EPSILON;   // FOCAL: g + h <= w * f_min

    while (true) {
        int num_focal_types = 0;
        for (const auto &[f, buckets] : buckets_by_f) {
            if (f > threshold)
                break;
            num_focal_types += static_cast<int>(buckets.size());
        }
        assert(num_focal_types > 0);

        int choice = rng.random(num_focal_types);
        int f = -1;
        int position = -1;
        for (const auto &[f_value, buckets] : buckets_by_f) {
            if (f_value > threshold)
                break;
            if (choice < static_cast<int>(buckets.size())) {
                f = f_value;
                position = choice;
                break;
            }
            choice -= static_cast<int>(buckets.size());
        }
        assert(f != -1);
        TypeBucket &bucket = buckets_by_f[f][position];

        while (!bucket.entries.empty()) {
            int k = rng.random(static_cast<int>(bucket.entries.size()));
            StateID id = bucket.entries[k];
            State state = state_registry.lookup_state(id);
            SearchNode node = search_space.get_node(state);
            if (is_current_entry(node, state, bucket.g, bucket.h)) {
                if (log.is_at_least_debug()) {
                    log << "Exploration step " << step_counter
                        << ": f_min=" << f_min << " threshold=" << w * f_min
                        << " focal_types=" << num_focal_types
                        << " type=(h=" << bucket.h << ",g=" << bucket.g << ")"
                        << " state " << id << endl;
                }
                return node;
            }
            // Stale entry: discard it (order within a bucket is irrelevant).
            bucket.entries[k] = bucket.entries.back();
            bucket.entries.pop_back();
            ++stale_type_entries;
        }
        // The type has no node in OPEN any more: drop it and draw again.
        remove_bucket(f, position);
    }
}

void TypeBasedWAstar::initialize() {
    log << "Conducting Type-WA* search with w = " << w
        << ", random seed " << random_seed
        << ", reopening closed nodes, (real) bound = " << bound << endl;
    assert(h_evaluator);

    set<Evaluator *> evals;
    h_evaluator->get_path_dependent_evaluators(evals);
    path_dependent_evaluators.assign(evals.begin(), evals.end());

    State initial_state = state_registry.get_initial_state();
    for (Evaluator *evaluator : path_dependent_evaluators) {
        evaluator->notify_initial_state(initial_state);
    }

    // As in eager_search, the initial state counts as reached by a preferred operator.
    EvaluationContext eval_context(initial_state, 0, true, &statistics);
    statistics.inc_evaluated_states();

    if (eval_context.is_evaluator_value_infinite(h_evaluator.get())) {
        log << "Initial state is a dead end." << endl;
    } else {
        if (search_progress.check_progress(eval_context))
            statistics.print_checkpoint_line(0);
        int h = eval_context.get_evaluator_value(h_evaluator.get());
        SearchNode node = search_space.get_node(initial_state);
        node.open_initial();
        insert(initial_state, 0, h);
    }

    print_initial_evaluator_values(eval_context);
}

SearchStatus TypeBasedWAstar::step() {
    ++step_counter;
    // Algorithm 1: step 1 is a WA* step; WA* and exploration steps alternate.
    bool wastar_step = (step_counter % 2 == 1);

    optional<SearchNode> node =
        wastar_step ? select_wastar_node() : select_exploration_node();
    if (!node) {
        log << "Completely explored state space -- no solution!" << endl;
        return FAILED;
    }

    const State &s = node->get_state();
    // Algorithm 1, line 11: the goal test is performed on the selected node.
    if (check_goal_and_set_plan(s))
        return SOLVED;

    node->close();
    statistics.inc_expanded();
    if (wastar_step)
        ++wastar_expansions;
    else
        ++exploration_expansions;

    vector<OperatorID> applicable_ops;
    successor_generator.generate_applicable_ops(s, applicable_ops);

    for (OperatorID op_id : applicable_ops) {
        OperatorProxy op = task_proxy.get_operators()[op_id];
        if ((node->get_real_g() + op.get_cost()) >= bound)
            continue;

        State succ_state = state_registry.get_successor_state(s, op);
        statistics.inc_generated();

        SearchNode succ_node = search_space.get_node(succ_state);

        for (Evaluator *evaluator : path_dependent_evaluators) {
            evaluator->notify_state_transition(s, op_id, succ_state);
        }

        // Previously encountered dead end. Don't re-evaluate.
        if (succ_node.is_dead_end())
            continue;

        int succ_g = node->get_g() + get_adjusted_cost(op);

        if (succ_node.is_new()) {
            // Algorithm 1, lines 14-17: a new state.
            EvaluationContext succ_eval_context(
                succ_state, succ_g, false, &statistics);
            statistics.inc_evaluated_states();

            if (succ_eval_context.is_evaluator_value_infinite(h_evaluator.get())) {
                succ_node.mark_as_dead_end();
                statistics.inc_dead_ends();
                continue;
            }
            int succ_h = succ_eval_context.get_evaluator_value(h_evaluator.get());
            succ_node.open(*node, op, get_adjusted_cost(op));
            insert(succ_state, succ_g, succ_h);

            if (search_progress.check_progress(succ_eval_context))
                statistics.print_checkpoint_line(succ_node.get_g());
        } else if (succ_node.get_g() > succ_g) {
            /*
              Algorithm 1, lines 18-24: a strictly cheaper path to a node in
              OPEN (update it) or in CLOSED (reopen it). SearchNode::reopen
              covers both cases, as in eager_search. The heuristic is
              recomputed through a fresh EvaluationContext: admissibility
              does not imply path independence, and whether a cached estimate
              is reused is the evaluator's decision. The new entry makes the
              older entries of the state stale.
            */
            bool was_closed = succ_node.is_closed();
            int old_g = succ_node.get_g();
            if (was_closed)
                statistics.inc_reopened();
            succ_node.reopen(*node, op, get_adjusted_cost(op));

            EvaluationContext succ_eval_context(
                succ_state, succ_node.get_g(), false, &statistics);
            if (succ_eval_context.is_evaluator_value_infinite(h_evaluator.get())) {
                /*
                  Not specified by the paper. With an admissible heuristic an
                  infinite estimate proves that no plan passes through the
                  state, so it is marked as a dead end (its stale entries are
                  skipped). eager_search would leave such a node open but
                  never reinsert it, which has the same effect on the search.
                */
                succ_node.mark_as_dead_end();
                statistics.inc_dead_ends();
                continue;
            }
            int succ_h = succ_eval_context.get_evaluator_value(h_evaluator.get());
            if (log.is_at_least_debug()) {
                log << "Cheaper path to " << (was_closed ? "closed" : "open")
                    << " state " << succ_state.get_id()
                    << ": g " << old_g << " -> " << succ_node.get_g()
                    << " (h " << latest_h[succ_state] << " -> " << succ_h << ")" << endl;
            }
            insert(succ_state, succ_node.get_g(), succ_h);
        }
    }

    return IN_PROGRESS;
}

void TypeBasedWAstar::print_statistics() const {
    statistics.print_detailed_statistics();
    search_space.print_statistics();
    log << "WA* expansions: " << wastar_expansions << endl;
    log << "Exploration expansions: " << exploration_expansions << endl;
    log << "Stale OPEN entries skipped: " << stale_open_entries << endl;
    log << "Stale type bucket entries discarded: " << stale_type_entries << endl;
}

void add_options_to_feature(plugins::Feature &feature) {
    SearchAlgorithm::add_options_to_feature(feature);
}
}
