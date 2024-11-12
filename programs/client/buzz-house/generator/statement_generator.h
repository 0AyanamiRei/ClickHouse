#pragma once

#include "external_integrations.h"
#include "random_generator.h"
#include "random_settings.h"
#include "sql_catalog.h"


namespace buzzhouse
{

class QueryOracle;

class SQLRelationCol
{
public:
    std::string rel_name, name;
    std::optional<std::string> name2;

    SQLRelationCol() { }

    SQLRelationCol(const std::string rname, const std::string cname, std::optional<std::string> cname2)
        : rel_name(rname), name(cname), name2(cname2)
    {
    }
};

class SQLRelation
{
public:
    std::string name;
    std::vector<SQLRelationCol> cols;

    SQLRelation() { }

    SQLRelation(const std::string n) : name(n) { }
};

class GroupCol
{
public:
    SQLRelationCol col;
    sql_query_grammar::Expr * gexpr = nullptr;

    GroupCol() { }
    GroupCol(SQLRelationCol c, sql_query_grammar::Expr * g) : col(c), gexpr(g) { }
};

class QueryLevel
{
public:
    bool global_aggregate = false, inside_aggregate = false, allow_aggregates = true, allow_window_funcs = true, group_by_all = false;
    uint32_t level, aliases_counter = 0;
    std::vector<GroupCol> gcols;
    std::vector<SQLRelation> rels;
    std::vector<uint32_t> projections;

    QueryLevel() { }

    QueryLevel(const uint32_t n) : level(n) { }
};

const constexpr uint32_t allow_set = (1 << 0), allow_cte = (1 << 1), allow_distinct = (1 << 2), allow_from = (1 << 3),
                         allow_prewhere = (1 << 4), allow_where = (1 << 5), allow_groupby = (1 << 6), allow_groupby_settings = (1 << 7),
                         allow_orderby = (1 << 8), allow_orderby_settings = (1 << 9), allow_limit = (1 << 10);

class StatementGenerator
{
private:
    const FuzzConfig & fc;
    ExternalIntegrations & connections;
    const bool supports_cloud_features;

    std::string buf;

    bool in_transaction = false, inside_projection = false, allow_not_deterministic = true, allow_in_expression_alias = true,
         allow_subqueries = true, enforce_final = false;
    uint32_t depth = 0, width = 0, database_counter = 0, table_counter = 0, zoo_path_counter = 0, function_counter = 0, current_level = 0;
    std::map<uint32_t, std::shared_ptr<SQLDatabase>> staged_databases, databases;
    std::map<uint32_t, SQLTable> staged_tables, tables;
    std::map<uint32_t, SQLView> staged_views, views;
    std::map<uint32_t, SQLFunction> staged_functions, functions;

    std::vector<std::string> enum_values
        = {"'-1'",
           "'0'",
           "'1'",
           "'10'",
           "'1000'",
           "'is'",
           "'was'",
           "'are'",
           "'be'",
           "'have'",
           "'had'",
           "'were'",
           "'can'",
           "'said'",
           "'use'",
           "','",
           "'😀'",
           "'😀😀😀😀'"
           "'名字'",
           "'兄弟姐妹'",
           "''",
           "'\\n'",
           "x'c328'",
           "x'e28228'",
           "x'ff'",
           "b'101'",
           "b'100'",
           "b'10001'",
           "' '",
           "'c0'",
           "'c1'",
           "'11'"};

    std::vector<uint32_t> ids;
    std::vector<InsertEntry> entries;
    std::vector<std::reference_wrapper<const SQLTable>> filtered_tables;
    std::vector<std::reference_wrapper<const SQLView>> filtered_views;
    std::vector<std::reference_wrapper<const std::shared_ptr<SQLDatabase>>> filtered_databases;
    std::vector<std::reference_wrapper<const SQLFunction>> filtered_functions;

    std::map<uint32_t, std::map<std::string, SQLRelation>> ctes;
    std::map<uint32_t, QueryLevel> levels;

    void SetAllowNotDetermistic(const bool value) { allow_not_deterministic = value; }
    void EnforceFinal(const bool value) { enforce_final = value; }

    template <typename T>
    const std::map<uint32_t, T> & GetNextCollection() const
    {
        if constexpr (std::is_same<T, SQLTable>::value)
        {
            return tables;
        }
        else if constexpr (std::is_same<T, SQLView>::value)
        {
            return views;
        }
        else if constexpr (std::is_same<T, SQLFunction>::value)
        {
            return functions;
        }
        else
        {
            return databases;
        }
    }

public:
    template <typename T>
    bool CollectionHas(const std::function<bool(const T &)> func) const
    {
        const auto & input = GetNextCollection<T>();

        for (const auto & entry : input)
        {
            if (func(entry.second))
            {
                return true;
            }
        }
        return false;
    }

private:
    template <typename T>
    uint32_t CollectionCount(const std::function<bool(const T &)> func) const
    {
        uint32_t res = 0;
        const auto & input = GetNextCollection<T>();

        for (const auto & entry : input)
        {
            res += func(entry.second) ? 1 : 0;
        }
        return res;
    }

    template <typename T>
    std::vector<std::reference_wrapper<const T>> & GetNextCollectionResult()
    {
        if constexpr (std::is_same<T, SQLTable>::value)
        {
            return filtered_tables;
        }
        else if constexpr (std::is_same<T, SQLView>::value)
        {
            return filtered_views;
        }
        else if constexpr (std::is_same<T, SQLFunction>::value)
        {
            return filtered_functions;
        }
        else
        {
            return filtered_databases;
        }
    }

public:
    template <typename T>
    std::vector<std::reference_wrapper<const T>> & FilterCollection(const std::function<bool(const T &)> func)
    {
        const auto & input = GetNextCollection<T>();
        auto & res = GetNextCollectionResult<T>();

        res.clear();
        for (const auto & entry : input)
        {
            if (func(entry.second))
            {
                res.push_back(std::ref<const T>(entry.second));
            }
        }
        return res;
    }

private:
    void InsertEntryRef(const InsertEntry & entry, sql_query_grammar::Expr * expr);
    void InsertEntryRefCP(const InsertEntry & entry, sql_query_grammar::ColumnPath * cp);
    void AddTableRelation(RandomGenerator & rg, const bool allow_internal_cols, const std::string & rel_name, const SQLTable & t);

    void StrAppendBottomValue(RandomGenerator & rg, std::string & ret, const SQLType * tp);
    void StrAppendMap(RandomGenerator & rg, std::string & ret, const MapType * mt);
    void StrAppendArray(RandomGenerator & rg, std::string & ret, const ArrayType * at);
    void StrAppendTuple(RandomGenerator & rg, std::string & ret, const TupleType * at);
    void StrAppendVariant(RandomGenerator & rg, std::string & ret, const VariantType * vtp);
    void StrAppendAnyValueInternal(RandomGenerator & rg, std::string & ret, const SQLType * tp);
    void StrAppendAnyValue(RandomGenerator & rg, std::string & ret, const SQLType * tp);

    int GenerateNextStatistics(RandomGenerator & rg, sql_query_grammar::ColumnStatistics * cstats);
    int PickUpNextCols(RandomGenerator & rg, const SQLTable & t, sql_query_grammar::ColumnList * clist);
    int AddTableColumn(
        RandomGenerator & rg,
        SQLTable & t,
        const uint32_t cname,
        const bool staged,
        const bool modify,
        const bool is_pk,
        const ColumnSpecial special,
        sql_query_grammar::ColumnDef * cd);
    int AddTableIndex(RandomGenerator & rg, SQLTable & t, const bool staged, sql_query_grammar::IndexDef * idef);
    int AddTableProjection(RandomGenerator & rg, SQLTable & t, const bool staged, sql_query_grammar::ProjectionDef * pdef);
    int AddTableConstraint(RandomGenerator & rg, SQLTable & t, const bool staged, sql_query_grammar::ConstraintDef * cdef);
    int GenerateTableKey(RandomGenerator & rg, sql_query_grammar::TableKey * tkey);
    int GenerateMergeTreeEngineDetails(
        RandomGenerator & rg, const sql_query_grammar::TableEngineValues teng, const bool add_pkey, sql_query_grammar::TableEngine * te);
    int GenerateEngineDetails(RandomGenerator & rg, SQLBase & b, const bool add_pkey, sql_query_grammar::TableEngine * te);

    sql_query_grammar::TableEngineValues GetNextTableEngine(RandomGenerator & rg, const bool use_external_integrations);
    int GenerateNextRefreshableView(RandomGenerator & rg, sql_query_grammar::RefreshableView * cv);
    int GenerateNextCreateView(RandomGenerator & rg, sql_query_grammar::CreateView * cv);
    int GenerateNextDrop(RandomGenerator & rg, sql_query_grammar::Drop * sq);
    int GenerateNextInsert(RandomGenerator & rg, sql_query_grammar::Insert * sq);
    int GenerateNextDelete(RandomGenerator & rg, sql_query_grammar::LightDelete * sq);
    int GenerateNextTruncate(RandomGenerator & rg, sql_query_grammar::Truncate * sq);
    int GenerateNextOptimizeTable(RandomGenerator & rg, sql_query_grammar::OptimizeTable * sq);
    int GenerateNextCheckTable(RandomGenerator & rg, sql_query_grammar::CheckTable * sq);
    int GenerateNextDescTable(RandomGenerator & rg, sql_query_grammar::DescTable * sq);
    int GenerateNextExchangeTables(RandomGenerator & rg, sql_query_grammar::ExchangeTables * sq);
    int GenerateUptDelWhere(RandomGenerator & rg, const SQLTable & t, sql_query_grammar::Expr * expr);
    int GenerateAlterTable(RandomGenerator & rg, sql_query_grammar::AlterTable * at);
    int GenerateSettingValues(
        RandomGenerator & rg,
        const std::map<std::string, std::function<void(RandomGenerator &, std::string &)>> & settings,
        sql_query_grammar::SettingValues * vals);
    int GenerateSettingValues(
        RandomGenerator & rg,
        const std::map<std::string, std::function<void(RandomGenerator &, std::string &)>> & settings,
        const size_t nvalues,
        sql_query_grammar::SettingValues * vals);
    int GenerateSettingList(
        RandomGenerator & rg,
        const std::map<std::string, std::function<void(RandomGenerator &, std::string &)>> & settings,
        sql_query_grammar::SettingList * pl);
    int GenerateAttach(RandomGenerator & rg, sql_query_grammar::Attach * att);
    int GenerateDetach(RandomGenerator & rg, sql_query_grammar::Detach * det);
    int GenerateNextCreateFunction(RandomGenerator & rg, sql_query_grammar::CreateFunction * cf);

    int AddFieldAccess(RandomGenerator & rg, sql_query_grammar::Expr * expr, const uint32_t nested_prob);
    int AddColNestedAccess(RandomGenerator & rg, sql_query_grammar::ExprColumn * expr, const uint32_t nested_prob);
    int RefColumn(RandomGenerator & rg, const GroupCol & gcol, sql_query_grammar::Expr * expr);
    int GenerateSubquery(RandomGenerator & rg, sql_query_grammar::Select * sel);
    int GenerateColRef(RandomGenerator & rg, sql_query_grammar::Expr * expr);
    int GenerateLiteralValue(RandomGenerator & rg, sql_query_grammar::Expr * expr);
    int GeneratePredicate(RandomGenerator & rg, sql_query_grammar::Expr * expr);
    int GenerateFrameBound(RandomGenerator & rg, sql_query_grammar::Expr * expr);
    int GenerateExpression(RandomGenerator & rg, sql_query_grammar::Expr * expr);
    int GenerateLambdaCall(RandomGenerator & rg, const uint32_t nparams, sql_query_grammar::LambdaExpr * lexpr);
    int GenerateFuncCall(RandomGenerator & rg, const bool allow_funcs, const bool allow_aggr, sql_query_grammar::SQLFuncCall * expr);

    int GenerateOrderBy(RandomGenerator & rg, const uint32_t ncols, const bool allow_settings, sql_query_grammar::OrderByStatement * ob);

    int GenerateLimitExpr(RandomGenerator & rg, sql_query_grammar::Expr * expr);
    int GenerateLimit(RandomGenerator & rg, const bool has_order_by, const uint32_t ncols, sql_query_grammar::LimitStatement * ls);
    int GenerateOffset(RandomGenerator & rg, sql_query_grammar::OffsetStatement * off);
    int GenerateGroupByExpr(
        RandomGenerator & rg,
        const bool enforce_having,
        const uint32_t offset,
        const uint32_t ncols,
        const std::vector<SQLRelationCol> & available_cols,
        std::vector<GroupCol> & gcols,
        sql_query_grammar::Expr * expr);
    int GenerateGroupBy(
        RandomGenerator & rg,
        const uint32_t ncols,
        const bool enforce_having,
        const bool allow_settings,
        sql_query_grammar::GroupByStatement * gb);
    int AddWhereSide(RandomGenerator & rg, const std::vector<GroupCol> & available_cols, sql_query_grammar::Expr * expr);
    int AddWhereFilter(RandomGenerator & rg, const std::vector<GroupCol> & available_cols, sql_query_grammar::Expr * expr);
    int GenerateWherePredicate(RandomGenerator & rg, sql_query_grammar::Expr * expr);
    int AddJoinClause(RandomGenerator & rg, sql_query_grammar::BinaryExpr * bexpr);
    int GenerateArrayJoin(RandomGenerator & rg, sql_query_grammar::ArrayJoin * aj);
    int GenerateFromElement(RandomGenerator & rg, const uint32_t allowed_clauses, sql_query_grammar::TableOrSubquery * tos);
    int GenerateJoinConstraint(RandomGenerator & rg, const bool allow_using, sql_query_grammar::JoinConstraint * jc);
    int GenerateDerivedTable(RandomGenerator & rg, SQLRelation & rel, const uint32_t allowed_clauses, sql_query_grammar::Select * sel);
    int GenerateFromStatement(RandomGenerator & rg, const uint32_t allowed_clauses, sql_query_grammar::FromStatement * ft);
    int AddCTEs(RandomGenerator & rg, const uint32_t allowed_clauses, sql_query_grammar::CTEs * qctes);
    int GenerateSelect(
        RandomGenerator & rg, const bool top, const uint32_t ncols, const uint32_t allowed_clauses, sql_query_grammar::Select * sel);

    int GenerateTopSelect(RandomGenerator & rg, const uint32_t allowed_clauses, sql_query_grammar::TopSelect * sq);
    int GenerateNextExplain(RandomGenerator & rg, sql_query_grammar::ExplainQuery * sq);
    int GenerateNextQuery(RandomGenerator & rg, sql_query_grammar::SQLQueryInner * sq);

    std::tuple<const SQLType *, sql_query_grammar::Integers> RandomIntType(RandomGenerator & rg, const uint32_t allowed_types);
    std::tuple<const SQLType *, sql_query_grammar::FloatingPoints> RandomFloatType(RandomGenerator & rg);
    std::tuple<const SQLType *, sql_query_grammar::Dates> RandomDateType(RandomGenerator & rg, const uint32_t allowed_types);
    const SQLType * RandomDateTimeType(RandomGenerator & rg, const uint32_t allowed_types, sql_query_grammar::DateTimeTp * dt);
    const SQLType *
    BottomType(RandomGenerator & rg, const uint32_t allowed_types, const bool low_card, sql_query_grammar::BottomTypeName * tp);
    const SQLType * GenerateArraytype(RandomGenerator & rg, const uint32_t allowed_types);
    const SQLType *
    GenerateArraytype(RandomGenerator & rg, const uint32_t allowed_types, uint32_t & col_counter, sql_query_grammar::TopTypeName * tp);

    const SQLType * RandomNextType(RandomGenerator & rg, const uint32_t allowed_types);
    const SQLType *
    RandomNextType(RandomGenerator & rg, const uint32_t allowed_types, uint32_t & col_counter, sql_query_grammar::TopTypeName * tp);

    void DropDatabase(const uint32_t dname);

public:
    const std::function<bool(const std::shared_ptr<SQLDatabase> &)> attached_databases
        = [](const std::shared_ptr<SQLDatabase> & d) { return d->attached == DetachStatus::ATTACHED; };
    const std::function<bool(const SQLTable &)> attached_tables
        = [](const SQLTable & t) { return (!t.db || t.db->attached == DetachStatus::ATTACHED) && t.attached == DetachStatus::ATTACHED; };
    const std::function<bool(const SQLView &)> attached_views
        = [](const SQLView & v) { return (!v.db || v.db->attached == DetachStatus::ATTACHED) && v.attached == DetachStatus::ATTACHED; };

    const std::function<bool(const SQLTable &)> attached_tables_for_oracle = [](const SQLTable & t)
    { return (!t.db || t.db->attached == DetachStatus::ATTACHED) && t.attached == DetachStatus::ATTACHED && !t.IsNotTruncableEngine(); };


    const std::function<bool(const std::shared_ptr<SQLDatabase> &)> detached_databases
        = [](const std::shared_ptr<SQLDatabase> & d) { return d->attached != DetachStatus::ATTACHED; };
    const std::function<bool(const SQLTable &)> detached_tables
        = [](const SQLTable & t) { return (t.db && t.db->attached != DetachStatus::ATTACHED) || t.attached != DetachStatus::ATTACHED; };
    const std::function<bool(const SQLView &)> detached_views
        = [](const SQLView & v) { return (v.db && v.db->attached != DetachStatus::ATTACHED) || v.attached != DetachStatus::ATTACHED; };

    StatementGenerator(const FuzzConfig & fuzzc, ExternalIntegrations & conn, const bool scf)
        : fc(fuzzc), connections(conn), supports_cloud_features(scf)
    {
        buf.reserve(2048);
    }

    int GenerateNextCreateTable(RandomGenerator & rg, sql_query_grammar::CreateTable * sq);
    int GenerateNextCreateDatabase(RandomGenerator & rg, sql_query_grammar::CreateDatabase * cd);
    int GenerateNextStatement(RandomGenerator & rg, sql_query_grammar::SQLQuery & sq);

    void UpdateGenerator(const sql_query_grammar::SQLQuery & sq, ExternalIntegrations & ei, bool success);

    friend class QueryOracle;
};

}
