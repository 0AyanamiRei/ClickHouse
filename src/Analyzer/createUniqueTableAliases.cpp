#include <unordered_map>
#include <Analyzer/createUniqueTableAliases.h>
#include <Analyzer/InDepthQueryTreeVisitor.h>
#include <Analyzer/IQueryTreeNode.h>
#include "Common/logger_useful.h"

namespace DB
{

namespace
{

class CreateUniqueTableAliasesVisitor : public InDepthQueryTreeVisitorWithContext<CreateUniqueTableAliasesVisitor>
{
public:
    using Base = InDepthQueryTreeVisitorWithContext<CreateUniqueTableAliasesVisitor>;

    explicit CreateUniqueTableAliasesVisitor(const ContextPtr & context)
        : Base(context)
    {}

    void enterImpl(QueryTreeNodePtr & node)
    {
        auto node_type = node->getNodeType();
        switch (node_type)
        {
            case QueryTreeNodeType::QUERY:
                [[fallthrough]];
            case QueryTreeNodeType::UNION:
            {
                /// Queries like `(SELECT 1) as t` have invalid syntax. To avoid creating such queries (e.g. in StorageDistributed)
                /// we need to remove aliases for top level queries.
                /// N.B. Subquery depth starts count from 1, so the following condition checks if it's a top level.
                if (getSubqueryDepth() == 1)
                {
                    node->removeAlias();
                    break;
                }
                [[fallthrough]];
            }
            case QueryTreeNodeType::TABLE:
                [[fallthrough]];
            case QueryTreeNodeType::TABLE_FUNCTION:
                [[fallthrough]];
            case QueryTreeNodeType::ARRAY_JOIN:
            {
                auto & alias = table_expression_to_alias[node];
                if (alias.empty())
                {
                    scope_to_nodes_with_aliases[scope_nodes_stack.back()].push_back(node);
                    alias = fmt::format("__table{}", ++next_id);
                    node->setAlias(alias);
                }
                break;
            }
            default:
                break;
        }

        switch (node_type)
        {
            case QueryTreeNodeType::QUERY:
                [[fallthrough]];
            case QueryTreeNodeType::UNION:
                [[fallthrough]];
            case QueryTreeNodeType::LAMBDA:
                scope_nodes_stack.push_back(node);
                break;
            default:
                break;
        }
    }

    void leaveImpl(QueryTreeNodePtr & node)
    {
        if (scope_nodes_stack.back() == node)
        {
            if (auto it = scope_to_nodes_with_aliases.find(scope_nodes_stack.back());
                it != scope_to_nodes_with_aliases.end())
            {
                for (const auto & node_with_alias : it->second)
                {
                    table_expression_to_alias.erase(node_with_alias);
                }
                scope_to_nodes_with_aliases.erase(it);
            }
            scope_nodes_stack.pop_back();
        }
    }

private:
    size_t next_id = 0;

    // Stack of nodes which create scopes: QUERY, UNION and LAMBDA.
    QueryTreeNodes scope_nodes_stack;

    std::unordered_map<QueryTreeNodePtr, QueryTreeNodes> scope_to_nodes_with_aliases;

    // We need to use raw pointer as a key, not a QueryTreeNodePtrWithHash.
    std::unordered_map<QueryTreeNodePtr, String> table_expression_to_alias;
};

}


void createUniqueTableAliases(QueryTreeNodePtr & node, const ContextPtr & context)
{
    CreateUniqueTableAliasesVisitor(context).visit(node);
}

}
