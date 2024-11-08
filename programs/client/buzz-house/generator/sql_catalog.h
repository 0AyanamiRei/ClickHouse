#pragma once

#include "sql_types.h"

namespace buzzhouse
{

using ColumnSpecial = enum ColumnSpecial { NONE = 0, SIGN = 1, IS_DELETED = 2, VERSION = 3 };

using DetachStatus = enum DetachStatus { ATTACHED = 0, DETACHED = 1, PERM_DETACHED = 2 };

struct SQLColumn
{
public:
    uint32_t cname = 0;
    const SQLType * tp = nullptr;
    ColumnSpecial special = ColumnSpecial::NONE;
    std::optional<bool> nullable = std::nullopt;
    std::optional<sql_query_grammar::DModifier> dmod = std::nullopt;

    SQLColumn() = default;
    SQLColumn(const SQLColumn & c)
    {
        this->cname = c.cname;
        this->special = c.special;
        this->nullable = c.nullable;
        this->dmod = c.dmod;
        this->tp = TypeDeepCopy(c.tp);
    }
    SQLColumn(SQLColumn && c)
    {
        this->cname = c.cname;
        this->special = c.special;
        this->nullable = c.nullable;
        this->dmod = c.dmod;
        this->tp = c.tp;
        c.tp = nullptr;
    }
    SQLColumn & operator=(const SQLColumn & c)
    {
        this->cname = c.cname;
        this->special = c.special;
        this->nullable = c.nullable;
        this->dmod = c.dmod;
        this->tp = TypeDeepCopy(c.tp);
        return *this;
    }
    SQLColumn & operator=(SQLColumn && c)
    {
        this->cname = c.cname;
        this->special = c.special;
        this->nullable = c.nullable;
        this->dmod = std::optional<sql_query_grammar::DModifier>(c.dmod);
        this->tp = c.tp;
        c.tp = nullptr;
        return *this;
    }
    ~SQLColumn() { delete tp; }

    bool CanBeInserted() const { return !dmod.has_value() || dmod.value() == sql_query_grammar::DModifier::DEF_DEFAULT; }
};

struct SQLIndex
{
public:
    uint32_t iname = 0;
};

struct SQLDatabase
{
public:
    DetachStatus attached = ATTACHED;
    uint32_t dname = 0;
    sql_query_grammar::DatabaseEngineValues deng;
};

struct SQLBase
{
public:
    uint32_t tname = 0;
    std::shared_ptr<SQLDatabase> db = nullptr;
    DetachStatus attached = ATTACHED;
    std::optional<sql_query_grammar::TableEngineOption> toption;
    sql_query_grammar::TableEngineValues teng;

    bool IsMergeTreeFamily() const
    {
        return teng >= sql_query_grammar::TableEngineValues::MergeTree
            && teng <= sql_query_grammar::TableEngineValues::VersionedCollapsingMergeTree;
    }

    bool IsFileEngine() const { return teng == sql_query_grammar::TableEngineValues::File; }

    bool IsJoinEngine() const { return teng == sql_query_grammar::TableEngineValues::Join; }

    bool IsNullEngine() const { return teng == sql_query_grammar::TableEngineValues::Null; }

    bool IsSetEngine() const { return teng == sql_query_grammar::TableEngineValues::Set; }

    bool IsBufferEngine() const { return teng == sql_query_grammar::TableEngineValues::Buffer; }

    bool IsRocksEngine() const { return teng == sql_query_grammar::TableEngineValues::EmbeddedRocksDB; }

    bool IsMySQLEngine() const { return teng == sql_query_grammar::TableEngineValues::MySQL; }

    bool IsPostgreSQLEngine() const { return teng == sql_query_grammar::TableEngineValues::PostgreSQL; }

    bool IsSQLiteEngine() const { return teng == sql_query_grammar::TableEngineValues::SQLite; }

    bool IsMongoDBEngine() const { return teng == sql_query_grammar::TableEngineValues::MongoDB; }

    bool IsRedisEngine() const { return teng == sql_query_grammar::TableEngineValues::Redis; }

    bool IsS3Engine() const { return teng == sql_query_grammar::TableEngineValues::S3; }

    bool IsS3QueueEngine() const { return teng == sql_query_grammar::TableEngineValues::S3Queue; }

    bool IsAnyS3Engine() const { return IsS3Engine() || IsS3QueueEngine(); }

    bool IsHudiEngine() const { return teng == sql_query_grammar::TableEngineValues::Hudi; }

    bool IsDeltaLakeEngine() const { return teng == sql_query_grammar::TableEngineValues::DeltaLake; }

    bool IsIcebergEngine() const { return teng == sql_query_grammar::TableEngineValues::IcebergS3; }

    bool IsNotTruncableEngine() const
    {
        return IsNullEngine() || IsSetEngine() || IsMySQLEngine() || IsPostgreSQLEngine() || IsSQLiteEngine() || IsRedisEngine()
            || IsMongoDBEngine() || IsAnyS3Engine() || IsHudiEngine() || IsDeltaLakeEngine() || IsIcebergEngine();
    }
};

struct SQLTable : SQLBase
{
public:
    bool is_temp = false;
    uint32_t col_counter = 0, idx_counter = 0, proj_counter = 0, constr_counter = 0;
    std::map<uint32_t, SQLColumn> cols, staged_cols;
    std::map<uint32_t, SQLIndex> idxs, staged_idxs;
    std::set<uint32_t> projs, staged_projs, constrs, staged_constrs;

    size_t RealNumberOfColumns() const
    {
        size_t res = 0;
        const NestedType * ntp = nullptr;

        for (const auto & entry : cols)
        {
            if ((ntp = dynamic_cast<const NestedType *>(entry.second.tp)))
            {
                res += ntp->subtypes.size();
            }
            else
            {
                res++;
            }
        }
        return res;
    }

    bool SupportsFinal() const
    {
        return (teng >= sql_query_grammar::TableEngineValues::ReplacingMergeTree
                && teng <= sql_query_grammar::TableEngineValues::VersionedCollapsingMergeTree)
            || this->IsBufferEngine();
    }

    bool HasSignColumn() const
    {
        return teng >= sql_query_grammar::TableEngineValues::CollapsingMergeTree
            && teng <= sql_query_grammar::TableEngineValues::VersionedCollapsingMergeTree;
    }

    bool HasVersionColumn() const { return teng == sql_query_grammar::TableEngineValues::VersionedCollapsingMergeTree; }
};

struct SQLView : SQLBase
{
public:
    bool is_materialized = false, is_refreshable = false, is_deterministic = false;
    uint32_t ncols = 1, staged_ncols = 1;
};

struct SQLFunction
{
public:
    bool is_deterministic = false;
    uint32_t fname = 0, nargs = 0;
};

typedef struct InsertEntry
{
    std::optional<bool> nullable = std::nullopt;
    ColumnSpecial special = ColumnSpecial::NONE;
    uint32_t cname1 = 0;
    std::optional<uint32_t> cname2 = std::nullopt;
    const SQLType * tp = nullptr;
    std::optional<sql_query_grammar::DModifier> dmod = std::nullopt;

    InsertEntry(
        const std::optional<bool> nu,
        const ColumnSpecial cs,
        const uint32_t c1,
        const std::optional<uint32_t> c2,
        const SQLType * t,
        const std::optional<sql_query_grammar::DModifier> dm)
        : nullable(nu), special(cs), cname1(c1), cname2(c2), tp(t), dmod(dm)
    {
    }
} InsertEntry;

}
