#ifndef SEARCH_ALGORITHMS_FOCAL_SEARCH_H
#define SEARCH_ALGORITHMS_FOCAL_SEARCH_H

#include "../open_list.h"
#include "../search_algorithm.h"


#include <memory>
#include <vector>
#include <map>

class Evaluator;
class PruningMethod;

namespace plugins {
class Feature;
}

namespace focal_search {
class FocalSearch : public SearchAlgorithm {
    const bool reopen_closed_nodes;
    const int k;

    std::unique_ptr<StateOpenList> focal_list;
    std::unique_ptr<StateOpenList> focal_pref;
    std::unique_ptr<StateOpenList> open_list;
    std::map<int, int> count_f;
    std::shared_ptr<Evaluator> open_evaluator;
    std::shared_ptr<Evaluator> focal_evaluator;
    std::shared_ptr<Evaluator> preferred_evaluator;

    PerStateInformation<int> f_value;
    PerStateInformation<bool> in_focal;
    PerStateInformation<bool> generated_by_pref;
    int f_min;
    double w;

    std::vector<Evaluator *> path_dependent_evaluators;

    //std::shared_ptr<PruningMethod> pruning_method;

    void start_f_value_statistics(EvaluationContext &eval_context);
    //void update_f_value_statistics(EvaluationContext &eval_context);
    //void reward_progress();

protected:
    virtual void initialize() override;
    virtual SearchStatus step() override;

public:
    explicit FocalSearch(const plugins::Options &opts);
    virtual ~FocalSearch() = default;

    virtual void print_statistics() const override;

    void dump_search_space() const;
};

extern void add_options_to_feature(plugins::Feature &feature);
}

#endif
