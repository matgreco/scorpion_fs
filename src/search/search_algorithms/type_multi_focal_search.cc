#include "type_multi_focal_search.h"

#include "../evaluation_context.h"
#include "../evaluator.h"
#include "../plugins/options.h"
#include "../plugins/plugin.h"
#include "../algorithms/ordered_set.h"
#include "../task_utils/successor_generator.h"
#include "../utils/logging.h"
#include "../utils/system.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <set>

using namespace std;

namespace type_multi_focal_search {
// w is interpreted with at most six decimal digits (see type_based_wastar).
static const long long WEIGHT_SCALE = 1000000;

TypeMultiFocalSearch::TypeMultiFocalSearch(const plugins::Options &opts)
    : SearchAlgorithm(opts),
      h_evaluator(opts.get<shared_ptr<Evaluator>>("h")),
      focal_evaluators(opts.get_list<shared_ptr<Evaluator>>("focal_evals")),
      preferred_operator_evaluators(opts.get_list<shared_ptr<Evaluator>>("preferred")),
      w(opts.get<double>("w")),
      scaled_w(0),
      random_seed(opts.get<int>("random_seed")),
      rng(random_seed),
      focal_lists(focal_evaluators.size()),
      latest_h(-1),
      in_focal(false),
      preferred_state(false),
      f_min(-1),
      step_counter(0),
      next_focal_list(0),
      preferred_expansions(0),
      focal_expansions(focal_evaluators.size(), 0),
      type_expansions(0),
      stale_entries_skipped(0),
      stale_type_entries(0) {
    if (focal_evaluators.empty()) {
        cerr << "type_mfs: focal_evals must contain at least one evaluator." << endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
    if (!isfinite(w) || w < 1.0 || w > 1000.0) {
        cerr << "type_mfs: w must be a finite weight with 1 <= w <= 1000." << endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
    scaled_w = llround(w * WEIGHT_SCALE);
    if (fabs(w * WEIGHT_SCALE - static_cast<double>(scaled_w)) > 1e-3) {
        cerr << "type_mfs: w must have at most six decimal digits." << endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
}

// FOCAL bound f <= w * f_min, exact in integers.
bool TypeMultiFocalSearch::within_bound(int f) const {
    assert(f_min >= 0);
    return static_cast<long long>(f) * WEIGHT_SCALE <= scaled_w * f_min;
}

/*
  An entry is current iff its node is still open, has the g and h it was
  inserted with, and is on the side (FOCAL or not) the entry belongs to. A
  node is reinserted only with a strictly smaller g, so (id, g) identifies
  its latest insertion; h and the FOCAL flag are consistency checks.
*/
bool TypeMultiFocalSearch::is_current_entry(
    const SearchNode &node, const State &state, int g, int h, bool expected_in_focal) const {
    return node.is_open() && node.get_g() == g && latest_h[state] == h &&
           in_focal[state] == expected_in_focal;
}

void TypeMultiFocalSearch::insert_node(
    const State &state, int g, int h, EvaluationContext &eval_context) {
    latest_h[state] = h;
    if (within_bound(g + h)) {
        insert_into_focal(state, g, h, eval_context);
    } else {
        in_focal[state] = false;
        open_list[g + h].push_back({state.get_id(), g, h});
    }
}

void TypeMultiFocalSearch::insert_into_focal(
    const State &state, int g, int h, EvaluationContext &eval_context) {
    StateID id = state.get_id();
    in_focal[state] = true;
    latest_h[state] = h;
    for (size_t i = 0; i < focal_evaluators.size(); ++i) {
        // An infinite focal estimate is not reliable: the node stays in FOCAL
        // with the lowest priority instead of being dropped.
        int key = eval_context.get_evaluator_value_or_infinity(focal_evaluators[i].get());
        focal_lists[i][key].push_back({id, g, h});
    }
    get_or_create_bucket(h, g).entries.push_back(id);
    if (use_preferred() && preferred_state[state])
        preferred_list[g + h].push_back({id, g, h});
    if (log.is_at_least_debug()) {
        log << "FOCAL <- state " << id << " g=" << g << " h=" << h
            << " f=" << g + h << " f_min=" << f_min << endl;
    }
}

TypeMultiFocalSearch::TypeBucket &TypeMultiFocalSearch::get_or_create_bucket(int h, int g) {
    TypeKey key(h, g);
    vector<TypeBucket> &buckets = buckets_by_f[g + h];
    auto it = bucket_position.find(key);
    if (it != bucket_position.end())
        return buckets[it->second];
    bucket_position[key] = static_cast<int>(buckets.size());
    buckets.push_back({h, g, {}});
    return buckets.back();
}

void TypeMultiFocalSearch::remove_bucket(int f, int position) {
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

// Discard stale entries from the front of the queue; return the first current entry.
optional<TypeMultiFocalSearch::Entry> TypeMultiFocalSearch::current_front(
    Queue &queue, bool expected_in_focal) {
    while (!queue.empty()) {
        auto it = queue.begin();
        deque<Entry> &bucket = it->second;
        Entry entry = bucket.front();
        State state = state_registry.lookup_state(entry.id);
        SearchNode node = search_space.get_node(state);
        if (is_current_entry(node, state, entry.g, entry.h, expected_in_focal))
            return entry;
        bucket.pop_front();
        ++stale_entries_skipped;
        if (bucket.empty())
            queue.erase(it);
    }
    return nullopt;
}

/*
  Minimum f inside FOCAL, obtained as in Type-WA*: stale entries are
  discarded from the back of the lowest-f type bucket until a current entry
  is found; exhausted buckets are removed. Returns -1 if FOCAL is empty.
*/
int TypeMultiFocalSearch::clean_and_get_focal_f_min() {
    while (!buckets_by_f.empty()) {
        auto it = buckets_by_f.begin();
        int f = it->first;
        TypeBucket &bucket = it->second.front();
        while (!bucket.entries.empty()) {
            StateID id = bucket.entries.back();
            State state = state_registry.lookup_state(id);
            SearchNode node = search_space.get_node(state);
            if (is_current_entry(node, state, bucket.g, bucket.h, true))
                return f;
            bucket.entries.pop_back();
            ++stale_type_entries;
        }
        remove_bucket(f, 0);
    }
    return -1;
}

/*
  f_min = min(f of the first current entry of open_list, min f inside FOCAL);
  then every node of open_list with f <= w * f_min is moved into FOCAL.
*/
void TypeMultiFocalSearch::update_f_min_and_fill_focal() {
    optional<Entry> open_front = current_front(open_list, false);
    int focal_min = clean_and_get_focal_f_min();
    f_min = -1;
    if (open_front)
        f_min = open_front->g + open_front->h;
    if (focal_min != -1 && (f_min == -1 || focal_min < f_min))
        f_min = focal_min;
    if (f_min == -1)
        return;
    while (true) {
        open_front = current_front(open_list, false);
        if (!open_front || !within_bound(open_front->g + open_front->h))
            break;
        Entry entry = *open_front;
        open_list.begin()->second.pop_front();
        if (open_list.begin()->second.empty())
            open_list.erase(open_list.begin());
        State state = state_registry.lookup_state(entry.id);
        EvaluationContext eval_context(state, entry.g, false, &statistics);
        insert_into_focal(state, entry.g, entry.h, eval_context);
    }
}

// Focal step: preferred list first (if enabled), then the focal lists in rotation.
optional<SearchNode> TypeMultiFocalSearch::select_focal_node(string &origin) {
    if (use_preferred()) {
        optional<Entry> entry = current_front(preferred_list, true);
        if (entry) {
            preferred_list.begin()->second.pop_front();
            if (preferred_list.begin()->second.empty())
                preferred_list.erase(preferred_list.begin());
            origin = "preferred";
            ++preferred_expansions;
            State state = state_registry.lookup_state(entry->id);
            return search_space.get_node(state);
        }
    }
    size_t num_lists = focal_lists.size();
    for (size_t attempt = 0; attempt < num_lists; ++attempt) {
        size_t i = (next_focal_list + attempt) % num_lists;
        optional<Entry> entry = current_front(focal_lists[i], true);
        if (entry) {
            focal_lists[i].begin()->second.pop_front();
            if (focal_lists[i].begin()->second.empty())
                focal_lists[i].erase(focal_lists[i].begin());
            next_focal_list = (i + 1) % num_lists;
            origin = "focal list " + to_string(i);
            ++focal_expansions[i];
            State state = state_registry.lookup_state(entry->id);
            return search_space.get_node(state);
        }
    }
    return nullopt;
}

/*
  Type step, identical to the exploration step of Type-WA*: uniform choice of
  a type among the (h, g) buckets of FOCAL, then uniform choice of a node of
  that type; a stale entry that is drawn is discarded and the draw repeated;
  a bucket without current entries is removed and the type is drawn again.
*/
optional<SearchNode> TypeMultiFocalSearch::select_type_node() {
    while (!buckets_by_f.empty()) {
        int num_types = 0;
        for (const auto &[f, buckets] : buckets_by_f)
            num_types += static_cast<int>(buckets.size());
        int choice = rng.random(num_types);
        auto chosen = buckets_by_f.begin();
        while (choice >= static_cast<int>(chosen->second.size())) {
            choice -= static_cast<int>(chosen->second.size());
            ++chosen;
        }
        int f = chosen->first;
        int position = choice;
        TypeBucket &bucket = chosen->second[position];
        while (!bucket.entries.empty()) {
            int k = rng.random(static_cast<int>(bucket.entries.size()));
            StateID id = bucket.entries[k];
            State state = state_registry.lookup_state(id);
            SearchNode node = search_space.get_node(state);
            if (is_current_entry(node, state, bucket.g, bucket.h, true)) {
                if (log.is_at_least_debug()) {
                    log << "Type step " << step_counter << ": types=" << num_types
                        << " type=(h=" << bucket.h << ",g=" << bucket.g << ")"
                        << " state " << id << endl;
                }
                ++type_expansions;
                return node;
            }
            bucket.entries[k] = bucket.entries.back();
            bucket.entries.pop_back();
            ++stale_type_entries;
        }
        remove_bucket(f, position);
    }
    return nullopt;
}

void TypeMultiFocalSearch::initialize() {
    log << "Conducting type multi focal search with w = " << w
        << ", " << focal_evaluators.size() << " focal list(s)"
        << (use_preferred() ? ", preferred list" : ", no preferred list")
        << ", random seed " << random_seed
        << ", reopening closed nodes, (real) bound = " << bound << endl;

    set<Evaluator *> evals;
    h_evaluator->get_path_dependent_evaluators(evals);
    for (const shared_ptr<Evaluator> &evaluator : focal_evaluators)
        evaluator->get_path_dependent_evaluators(evals);
    for (const shared_ptr<Evaluator> &evaluator : preferred_operator_evaluators)
        evaluator->get_path_dependent_evaluators(evals);
    path_dependent_evaluators.assign(evals.begin(), evals.end());

    State initial_state = state_registry.get_initial_state();
    for (Evaluator *evaluator : path_dependent_evaluators)
        evaluator->notify_initial_state(initial_state);

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
        f_min = h;   // f of the only open node
        insert_node(initial_state, 0, h, eval_context);
    }
    print_initial_evaluator_values(eval_context);
}

SearchStatus TypeMultiFocalSearch::step() {
    ++step_counter;
    bool type_step = (step_counter % 2 == 0);   // odd: focal step, even: type step
    string origin;
    optional<SearchNode> node = type_step ? select_type_node() : select_focal_node(origin);
    if (!node) {
        // FOCAL has no current node: refill it from open_list and try again.
        update_f_min_and_fill_focal();
        optional<SearchNode> retry = type_step ? select_type_node() : select_focal_node(origin);
        if (!retry) {
            log << "Completely explored state space -- no solution!" << endl;
            return FAILED;
        }
        node.emplace(*retry);   // SearchNode is copy-constructible but not assignable
    }

    const State &s = node->get_state();
    if (log.is_at_least_debug() && !type_step) {
        log << "Focal step " << step_counter << " (" << origin << "): state " << s.get_id()
            << " g=" << node->get_g() << " h=" << latest_h[s] << endl;
    }
    if (check_goal_and_set_plan(s))
        return SOLVED;

    node->close();
    statistics.inc_expanded();

    // Preferred operators of the expanded state (only if the preferred list is used).
    ordered_set::OrderedSet<OperatorID> preferred_operators;
    if (use_preferred()) {
        EvaluationContext eval_context(s, node->get_g(), false, &statistics, true);
        for (const shared_ptr<Evaluator> &evaluator : preferred_operator_evaluators)
            collect_preferred_operators(eval_context, evaluator.get(), preferred_operators);
    }

    vector<OperatorID> applicable_ops;
    successor_generator.generate_applicable_ops(s, applicable_ops);

    for (OperatorID op_id : applicable_ops) {
        OperatorProxy op = task_proxy.get_operators()[op_id];
        if ((node->get_real_g() + op.get_cost()) >= bound)
            continue;

        State succ_state = state_registry.get_successor_state(s, op);
        statistics.inc_generated();
        SearchNode succ_node = search_space.get_node(succ_state);
        bool is_preferred = preferred_operators.contains(op_id);

        for (Evaluator *evaluator : path_dependent_evaluators)
            evaluator->notify_state_transition(s, op_id, succ_state);

        if (succ_node.is_dead_end())
            continue;

        int succ_g = node->get_g() + get_adjusted_cost(op);

        if (succ_node.is_new()) {
            EvaluationContext succ_eval_context(succ_state, succ_g, is_preferred, &statistics);
            statistics.inc_evaluated_states();
            if (succ_eval_context.is_evaluator_value_infinite(h_evaluator.get())) {
                succ_node.mark_as_dead_end();
                statistics.inc_dead_ends();
                continue;
            }
            int succ_h = succ_eval_context.get_evaluator_value(h_evaluator.get());
            succ_node.open(*node, op, get_adjusted_cost(op));
            preferred_state[succ_state] = is_preferred;
            insert_node(succ_state, succ_g, succ_h, succ_eval_context);
            if (search_progress.check_progress(succ_eval_context))
                statistics.print_checkpoint_line(succ_node.get_g());
        } else if (succ_node.get_g() > succ_g) {
            // Strictly cheaper path: update an open node or reopen a closed one
            // (as type_based_wastar); h is recomputed and the node is
            // reinserted into FOCAL or open_list, making its old entries stale.
            bool was_closed = succ_node.is_closed();
            int old_g = succ_node.get_g();
            if (was_closed)
                statistics.inc_reopened();
            succ_node.reopen(*node, op, get_adjusted_cost(op));
            EvaluationContext succ_eval_context(
                succ_state, succ_node.get_g(), is_preferred, &statistics);
            if (succ_eval_context.is_evaluator_value_infinite(h_evaluator.get()))
                continue;   // as eager_search: stays open, not reinserted
            int succ_h = succ_eval_context.get_evaluator_value(h_evaluator.get());
            if (log.is_at_least_debug()) {
                log << "Cheaper path to " << (was_closed ? "closed" : "open")
                    << " state " << succ_state.get_id() << ": g " << old_g
                    << " -> " << succ_node.get_g() << " (h " << latest_h[succ_state]
                    << " -> " << succ_h << ")" << endl;
            }
            preferred_state[succ_state] = is_preferred;
            insert_node(succ_state, succ_node.get_g(), succ_h, succ_eval_context);
        }
    }

    update_f_min_and_fill_focal();
    return IN_PROGRESS;
}

void TypeMultiFocalSearch::print_statistics() const {
    statistics.print_detailed_statistics();
    search_space.print_statistics();
    for (size_t i = 0; i < focal_expansions.size(); ++i)
        log << "Focal list " << i << " expansions: " << focal_expansions[i] << endl;
    log << "Preferred list expansions: " << preferred_expansions << endl;
    log << "Type expansions: " << type_expansions << endl;
    log << "Stale queue entries skipped: " << stale_entries_skipped << endl;
    log << "Stale type bucket entries discarded: " << stale_type_entries << endl;
}

void add_options_to_feature(plugins::Feature &feature) {
    SearchAlgorithm::add_options_to_feature(feature);
}
}
