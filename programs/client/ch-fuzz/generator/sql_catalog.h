#pragma once

#include "sql_types.h"

namespace chfuzz {

using ColumnSpecial = enum ColumnSpecial {
	NONE = 0,
	SIGN = 1,
	VERSION = 2
};

struct SQLColumn {
public:
	std::optional<bool> nullable = std::nullopt;
	uint32_t cname = 0;
	SQLType *tp = nullptr;
	ColumnSpecial special = ColumnSpecial::NONE;

	SQLColumn() = default;
	SQLColumn(const SQLColumn& c) {
		this->nullable = c.nullable;
		this->cname = c.cname;
		this->special = c.special;
		this->tp = TypeDeepCopy(c.tp);
	}
	SQLColumn(SQLColumn&& c) {
		this->nullable = c.nullable;
		this->cname = c.cname;
		this->special = c.special;
		this->tp = c.tp;
		c.tp = nullptr;
	}
	SQLColumn& operator=(const SQLColumn& c) {
		this->nullable = c.nullable;
		this->cname = c.cname;
		this->special = c.special;
		this->tp = TypeDeepCopy(c.tp);
		return *this;
	}
	SQLColumn& operator=(SQLColumn&& c) {
		this->nullable = c.nullable;
		this->cname = c.cname;
		this->special = c.special;
		this->tp = c.tp;
		c.tp = nullptr;
		return *this;
	}
	~SQLColumn() {
		delete tp;
	}
};

struct SQLIndex {
public:
	uint32_t iname = 0;
};

struct SQLDatabase {
public:
	bool attached = true;
	uint32_t dname = 0, table_counter = 0;
	sql_query_grammar::DatabaseEngineValues deng;
};

struct SQLBase {
public:
	std::shared_ptr<SQLDatabase> db;
	bool attached = true, is_shared_engine = false;
	sql_query_grammar::TableEngineValues teng;

	bool IsMergeTreeFamily() const {
		return teng >= sql_query_grammar::TableEngineValues::MergeTree &&
			   teng <= sql_query_grammar::TableEngineValues::VersionedCollapsingMergeTree;
	}
};

struct SQLTable : SQLBase {
public:
	bool is_temp = false;
	uint32_t tname = 0, col_counter = 0, idx_counter = 0, proj_counter = 0, constr_counter = 0;
	std::map<uint32_t, SQLColumn> cols, staged_cols;
	std::map<uint32_t, SQLIndex> idxs, staged_idxs;
	std::set<uint32_t> projs, staged_projs, constrs, staged_constrs;

	size_t RealNumberOfColumns() const {
		size_t res = 0;
		NestedType *ntp = nullptr;

		for (const auto &entry : cols) {
			if ((ntp = dynamic_cast<NestedType*>(entry.second.tp))) {
				res += ntp->subtypes.size();
			} else {
				res++;
			}
		}
		return res;
	}

	bool SupportsFinal() const {
		return teng >= sql_query_grammar::TableEngineValues::ReplacingMergeTree &&
			   teng <= sql_query_grammar::TableEngineValues::VersionedCollapsingMergeTree;
	}

	bool HasSignColumn() const {
		return teng >= sql_query_grammar::TableEngineValues::CollapsingMergeTree &&
			   teng <= sql_query_grammar::TableEngineValues::VersionedCollapsingMergeTree;
	}

	bool HasVersionColumn() const {
		return teng == sql_query_grammar::TableEngineValues::VersionedCollapsingMergeTree;
	}
};

struct SQLView : SQLBase {
public:
	bool is_materialized = false, is_refreshable = false;
	uint32_t vname = 0, ncols = 1, staged_ncols = 1;
};

}
