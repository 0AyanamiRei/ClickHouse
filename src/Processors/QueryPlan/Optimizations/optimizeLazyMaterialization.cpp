#include <Processors/QueryPlan/Optimizations/Optimizations.h>
#include <Processors/QueryPlan/ExpressionStep.h>
#include <Processors/QueryPlan/FilterStep.h>
#include <Processors/QueryPlan/LazilyReadStep.h>
#include <Processors/QueryPlan/ReadFromMergeTree.h>
#include <Processors/QueryPlan/SortingStep.h>
#include <Interpreters/ActionsDAG.h>
#include "Storages/SelectQueryInfo.h"

namespace DB::QueryPlanOptimizations
{

constexpr size_t MAX_LIMIT_FOR_LAZY_MATERIALIZATION = 10;
using StepStack = std::vector<IQueryPlanStep *>;

static bool canUseLazyProjectionForReadingStep(ReadFromMergeTree * reading)
{
    if (reading->getLazilyReadInfo())
       return false;

    if (reading->hasAnalyzedResult())
       return false;

    if (reading->readsInOrder())
        return false;

    if (reading->isQueryWithFinal())
        return false;

    if (reading->isQueryWithSampling())
        return false;

    if (reading->isParallelReadingEnabled())
        return false;

    return true;
}

static void removeUsedColumnNames(
    const ActionsDAGPtr & actions,
    NameSet & lazily_read_column_name_set,
    AliasToNamePtr & alias_index)
{
    const auto & actions_outputs = actions->getOutputs();

    for (const auto * output_node : actions_outputs)
    {
        const auto * node = output_node;
        while (node && node->type == ActionsDAG::ActionType::ALIAS)
        {
            /// alias has only one child
            chassert(node->children.size() == 1);
            node = node->children.front();
        }

        if (!node)
            continue;

        if (node->type == ActionsDAG::ActionType::FUNCTION || node->type == ActionsDAG::ActionType::ARRAY_JOIN)
        {
            using ActionsNode = ActionsDAG::Node;

            std::unordered_set<const ActionsNode *> visited_nodes;
            std::stack<const ActionsNode *> stack;

            stack.push(node);
            while (!stack.empty())
            {
                const auto * current_node = stack.top();
                stack.pop();

                if (current_node->type == ActionsDAG::ActionType::INPUT)
                {
                    const auto it = alias_index->find(current_node->result_name);
                    if (it != alias_index->end())
                        lazily_read_column_name_set.erase(it->second);
                }

                for (const auto * child : current_node->children)
                {
                    if (!visited_nodes.contains(child))
                    {
                        stack.push(child);
                        visited_nodes.insert(child);
                    }
                }
             }
        }
    }

    /// Update alias name index.
    for (const auto * output_node : actions_outputs)
    {
        const auto * node = output_node;
        while (node && node->type == ActionsDAG::ActionType::ALIAS)
        {
            /// alias has only one child
            chassert(node->children.size() == 1);
            node = node->children.front();
        }
        if (node && node != output_node && node->type == ActionsDAG::ActionType::INPUT)
        {
            const auto it = alias_index->find(node->result_name);
            if (it != alias_index->end())
            {
                const auto real_column_name = it->second;
                alias_index->emplace(output_node->result_name, real_column_name);
                alias_index->erase(node->result_name);
            }
        }
    }
}

static void collectLazilyReadColumnNames(
    const StepStack & steps,
    Names & lazily_read_column_names,
    AliasToNamePtr & alias_index)
{
    auto * read_from_merge_tree = typeid_cast<ReadFromMergeTree *>(steps.back());
    const Names & real_column_names = read_from_merge_tree->getRealColumnNames();
    NameSet lazily_read_column_name_set(real_column_names.begin(), real_column_names.end());

    for (const auto & column_name : real_column_names)
        alias_index->emplace(column_name, column_name);

    if (const auto & prewhere_info = read_from_merge_tree->getPrewhereInfo())
    {
        if (prewhere_info->row_level_filter)
            removeUsedColumnNames(prewhere_info->row_level_filter, lazily_read_column_name_set, alias_index);

        if (prewhere_info->prewhere_actions)
            removeUsedColumnNames(prewhere_info->prewhere_actions, lazily_read_column_name_set, alias_index);
    }

    for (auto step_it = steps.rbegin(); step_it != steps.rend(); ++step_it)
    {
        auto * step = *step_it;

        if (lazily_read_column_name_set.empty())
            return;

        if (auto * expression_step = typeid_cast<ExpressionStep *>(step))
        {
            removeUsedColumnNames(expression_step->getExpression(), lazily_read_column_name_set, alias_index);
            continue;
        }

        if (auto * filter_step = typeid_cast<FilterStep *>(step))
        {
            removeUsedColumnNames(filter_step->getExpression(), lazily_read_column_name_set, alias_index);
            continue;
        }

        if (auto * sorting_step = typeid_cast<SortingStep *>(step))
        {
            const auto & sort_description = sorting_step->getSortDescription();
            for (const auto & sort_column_description : sort_description)
            {
                const auto it = alias_index->find(sort_column_description.column_name);
                if (it == alias_index->end())
                    continue;
                lazily_read_column_name_set.erase(it->second);
            }
            continue;
        }
    }

    lazily_read_column_names.insert(lazily_read_column_names.end(),
                                    lazily_read_column_name_set.begin(),
                                    lazily_read_column_name_set.end());
}

static ReadFromMergeTree * findReadingStep(QueryPlan::Node & node, StepStack & backward_path)
{
    IQueryPlanStep * step = node.step.get();
    backward_path.push_back(step);

    if (auto * read_from_merge_tree = typeid_cast<ReadFromMergeTree *>(step))
        return read_from_merge_tree;

    if (node.children.size() != 1)
        return nullptr;

    if (typeid_cast<ExpressionStep *>(step) || typeid_cast<FilterStep *>(step))
        return findReadingStep(*node.children.front(), backward_path);

    return nullptr;
}

static void updateStepsDataStreams(StepStack & steps_to_update)
{
    /// update output data stream for found transforms
    if (!steps_to_update.empty())
    {
        const DataStream * input_stream = &steps_to_update.back()->getOutputStream();
        chassert(dynamic_cast<ReadFromMergeTree *>(steps_to_update.back()));
        steps_to_update.pop_back();

        while (!steps_to_update.empty())
        {
            auto * transforming_step = dynamic_cast<ITransformingStep *>(steps_to_update.back());
            chassert(transforming_step);

            transforming_step->updateInputStream(*input_stream);
            input_stream = &steps_to_update.back()->getOutputStream();
            steps_to_update.pop_back();
        }
    }
}

void optimizeLazyProjection(Stack & stack, QueryPlan::Nodes & nodes)
{
    const auto & frame = stack.back();

    if (frame.node->children.size() != 1)
        return;

    auto * sorting = typeid_cast<SortingStep *>(frame.node->step.get());
    if (!sorting)
        return;

    if (sorting->getType() != SortingStep::Type::Full)
        return;

    const auto limit = sorting->getLimit();
    if (limit == 0 || limit > MAX_LIMIT_FOR_LAZY_MATERIALIZATION)
        return;

    StepStack steps_to_update;
    steps_to_update.push_back(sorting);
    auto * reading_step = findReadingStep(*frame.node->children.front(), steps_to_update);
    if (!reading_step)
        return;

    if (!canUseLazyProjectionForReadingStep(reading_step))
        return;

    LazilyReadInfoPtr lazily_read_info = std::make_shared<LazilyReadInfo>();
    AliasToNamePtr alias_index = std::make_shared<AliasToName>();
    collectLazilyReadColumnNames(steps_to_update, lazily_read_info->lazily_read_columns_names, alias_index);

    if (lazily_read_info->lazily_read_columns_names.empty())
        return;

    reading_step->updateLazilyReadInfo(lazily_read_info);

    QueryPlan::Node * sorting_node = frame.node;
    auto & replace_node = nodes.emplace_back();
    replace_node.children.emplace_back(sorting_node);

    QueryPlan::Node * sorting_parent_node = (stack.rbegin() + 1)->node;

    for (auto & sorting_parent_child : sorting_parent_node->children)
    {
        if (sorting_parent_child == sorting_node)
        {
            sorting_parent_child = &replace_node;
            break;
        }
    }

    updateStepsDataStreams(steps_to_update);

    auto lazily_read_step = std::make_unique<LazilyReadStep>(
        sorting->getOutputStream(),
        reading_step->getMergeTreeData(),
        reading_step->getStorageSnapshot(),
        lazily_read_info,
        reading_step->getContext(),
        alias_index);
    lazily_read_step->setStepDescription("Lazily Read");
    replace_node.step = std::move(lazily_read_step);
}

}
