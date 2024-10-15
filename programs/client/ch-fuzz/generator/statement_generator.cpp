#include "statement_generator.h"
#include "random_settings.h"
#include "sql_catalog.h"
#include "sql_types.h"

#include <algorithm>
#include <optional>
#include <sys/types.h>

namespace chfuzz {

int StatementGenerator::GenerateSettingValues(RandomGenerator &rg,
											  const std::map<std::string, std::function<void(RandomGenerator&,std::string&)>> &settings,
											  const size_t nvalues, sql_query_grammar::SettingValues *vals) {
	for (size_t i = 0 ; i < nvalues ; i++) {
		sql_query_grammar::SetValue *sv = i == 0 ? vals->mutable_set_value() : vals->add_other_values();

		SetRandomSetting(rg, settings, this->buf, sv);
	}
	return 0;
}

int StatementGenerator::GenerateSettingValues(RandomGenerator &rg,
											  const std::map<std::string, std::function<void(RandomGenerator&,std::string&)>> &settings,
											  sql_query_grammar::SettingValues *vals) {
	return GenerateSettingValues(rg, settings, std::min<size_t>(settings.size(), static_cast<size_t>((rg.NextRandomUInt32() % 4) + 1)), vals);
}

int StatementGenerator::GenerateSettingList(RandomGenerator &rg,
											const std::map<std::string, std::function<void(RandomGenerator&,std::string&)>> &settings,
											sql_query_grammar::SettingList *sl) {
	const size_t nvalues = std::min<size_t>(settings.size(), static_cast<size_t>((rg.NextRandomUInt32() % 4) + 1));

	for (size_t i = 0 ; i < nvalues ; i++) {
		const std::string &next = rg.PickKeyRandomlyFromMap(settings);

		if (i == 0) {
			sl->set_setting(next);
		} else {
			sl->add_other_settings(next);
		}
	}
	return 0;
}

int StatementGenerator::GenerateNextCreateDatabase(RandomGenerator &rg, sql_query_grammar::CreateDatabase *cd) {
	SQLDatabase next;
	const uint32_t dname = this->database_counter++;
	sql_query_grammar::DatabaseEngine *deng = cd->mutable_dengine();
	sql_query_grammar::DatabaseEngineValues val = rg.NextBool() ?
		sql_query_grammar::DatabaseEngineValues::DAtomic : sql_query_grammar::DatabaseEngineValues::DReplicated;

	next.deng = val;
	deng->set_engine(val);
	if (val == sql_query_grammar::DatabaseEngineValues::DReplicated) {
		deng->set_zoo_path(this->zoo_path_counter++);
	}
	next.dname = dname;
	cd->mutable_database()->set_database("d" + std::to_string(dname));
	this->staged_databases[dname] = std::make_shared<SQLDatabase>(std::move(next));
	return 0;
}

void StatementGenerator::AddTableRelation(RandomGenerator &rg, const bool allow_internal_cols, const std::string &rel_name, const SQLTable &t) {
	NestedType *ntp = nullptr;
	SQLRelation rel(rel_name);

	for (const auto &entry : t.cols) {
		if ((ntp = dynamic_cast<NestedType*>(entry.second.tp))) {
			for (const auto &entry2 : ntp->subtypes) {
				rel.cols.push_back(SQLRelationCol(rel_name, "c" + std::to_string(entry.first),
												  std::optional<std::string>("c" + std::to_string(entry2.cname))));
			}
		} else {
			rel.cols.push_back(SQLRelationCol(rel_name, "c" + std::to_string(entry.first), std::nullopt));
		}
	}
	if (allow_internal_cols && t.IsMergeTreeFamily() && rg.NextSmallNumber() < 4) {
		rel.cols.push_back(SQLRelationCol(rel_name, "_block_number", std::nullopt));
		rel.cols.push_back(SQLRelationCol(rel_name, "_part", std::nullopt));
		rel.cols.push_back(SQLRelationCol(rel_name, "_part_data_version", std::nullopt));
		rel.cols.push_back(SQLRelationCol(rel_name, "_part_index", std::nullopt));
		rel.cols.push_back(SQLRelationCol(rel_name, "_part_offset", std::nullopt));
		rel.cols.push_back(SQLRelationCol(rel_name, "_part_uuid", std::nullopt));
		rel.cols.push_back(SQLRelationCol(rel_name, "_partition_id", std::nullopt));
		rel.cols.push_back(SQLRelationCol(rel_name, "_partition_value", std::nullopt));
		rel.cols.push_back(SQLRelationCol(rel_name, "_sample_factor", std::nullopt));
	}
	if (rel_name == "") {
		this->levels[this->current_level] = QueryLevel(this->current_level);
	}
	this->levels[this->current_level].rels.push_back(std::move(rel));
}

int StatementGenerator::GenerateNextStatistics(RandomGenerator &rg, sql_query_grammar::ColumnStatistics *cstats) {
	const size_t nstats = (rg.NextMediumNumber() % static_cast<uint32_t>(sql_query_grammar::ColumnStat_MAX)) + 1;

	for (uint32_t i = 1; i <= sql_query_grammar::ColumnStat_MAX; i++) {
		ids.push_back(i);
	}
	std::shuffle(ids.begin(), ids.end(), rg.gen);
	for (size_t i = 0; i < nstats; i++) {
		const sql_query_grammar::ColumnStat nstat = static_cast<sql_query_grammar::ColumnStat>(ids[i]);

		if (i == 0) {
			cstats->set_stat(nstat);
		} else {
			cstats->add_other_stats(nstat);
		}
	}
	ids.clear();
	return 0;
}

int StatementGenerator::PickUpNextCols(RandomGenerator &rg, const SQLTable &t, sql_query_grammar::ColumnList *clist) {
	NestedType *ntp = nullptr;
	const size_t ncols = (rg.NextMediumNumber() % std::min<uint32_t>(static_cast<uint32_t>(t.RealNumberOfColumns()), UINT32_C(3))) + 1;

	assert(this->ids.empty());
	for (const auto &entry : t.cols) {
		if ((ntp = dynamic_cast<NestedType*>(entry.second.tp))) {
			for (const auto &entry2 : ntp->subtypes) {
				ids.push_back(entry2.cname);
			}
		} else {
			ids.push_back(entry.first);
		}
	}
	std::shuffle(ids.begin(), ids.end(), rg.gen);
	for (size_t i = 0; i < ncols; i++) {
		sql_query_grammar::Column *col = i == 0 ? clist->mutable_col() : clist->add_other_cols();

		col->set_column("c" + std::to_string(ids[i]));
	}
	ids.clear();
	return 0;
}

int StatementGenerator::GenerateTableKey(RandomGenerator &rg, sql_query_grammar::TableKey *tkey) {
	if (!entries.empty() && rg.NextSmallNumber() < 7) {
		const size_t ocols = (rg.NextMediumNumber() % std::min<size_t>(entries.size(), UINT32_C(3))) + 1;

		std::shuffle(entries.begin(), entries.end(), rg.gen);
		for (size_t i = 0; i < ocols; i++) {
			const InsertEntry &entry = this->entries[i];
			sql_query_grammar::ExprColumn *ecol = tkey->add_exprs()->mutable_comp_expr()->mutable_expr_stc()->mutable_col();

			ecol->mutable_col()->set_column("c" + std::to_string(entry.cname1));
			if (entry.cname2.has_value()) {
				ecol->mutable_subcol()->set_column("c" + std::to_string(entry.cname2.value()));
			}
		}
	}
	return 0;
}

int StatementGenerator::GenerateEngineDetails(RandomGenerator &rg, sql_query_grammar::TableEngine *te) {
	GenerateTableKey(rg, te->mutable_order());
	if (te->order().exprs_size() && rg.NextSmallNumber() < 5) {
		//pkey is a subset of order by
		sql_query_grammar::TableKey *tkey = te->mutable_primary_key();
		std::uniform_int_distribution<uint32_t> table_order_by(1, te->order().exprs_size());
		const uint32_t pkey_size = table_order_by(rg.gen);

		for (uint32_t i = 0 ; i < pkey_size; i++) {
			const sql_query_grammar::ExprColumn &oecol = te->order().exprs(i).comp_expr().expr_stc().col();
			sql_query_grammar::ExprColumn *necol = tkey->add_exprs()->mutable_comp_expr()->mutable_expr_stc()->mutable_col();

			necol->mutable_col()->set_column(oecol.col().column());
			if (oecol.has_subcol()) {
				necol->mutable_subcol()->set_column(oecol.subcol().column());
			}
		}
	}
	if (rg.NextSmallNumber() < 5) {
		GenerateTableKey(rg, te->mutable_partition_by());
	}
	return 0;
}

int StatementGenerator::AddTableColumn(RandomGenerator &rg, SQLTable &t, const uint32_t cname, const bool staged,
									   const bool modify, const ColumnSpecial special, sql_query_grammar::ColumnDef *cd) {
	SQLColumn col;
	SQLType *tp = nullptr;
	LowCardinality *lc = nullptr;
	auto &to_add = staged ? t.staged_cols : t.cols;

	col.cname = cname;
	cd->mutable_col()->set_column("c" + std::to_string(cname));
	if (special) {
		tp = new IntType(8, special == ColumnSpecial::VERSION);
		cd->mutable_type()->mutable_type()->mutable_non_nullable()
		  ->set_integers(special == ColumnSpecial::VERSION ? sql_query_grammar::Integers::UInt8 : sql_query_grammar::Integers::Int8);
	} else {
		const uint32_t prev_max_depth = this->max_depth;

		this->max_depth = 4;
		tp = RandomNextType(rg, std::numeric_limits<uint32_t>::max(), t.col_counter, cd->mutable_type()->mutable_type());
		this->max_depth = prev_max_depth;
	}
	col.tp = tp;
	col.special = special;
	if (!modify && col.special == ColumnSpecial::NONE &&
		(dynamic_cast<IntType*>(tp) || dynamic_cast<FloatType*>(tp) || dynamic_cast<DateType*>(tp) || dynamic_cast<DecimalType*>(tp) ||
		 dynamic_cast<StringType*>(tp) || dynamic_cast<BoolType*>(tp) || dynamic_cast<UUIDType*>(tp) ||
		 ((lc = dynamic_cast<LowCardinality*>(tp)) && !dynamic_cast<Nullable*>(lc->subtype))) && rg.NextSmallNumber() < 4) {
		cd->set_nullable(rg.NextBool());
		col.nullable = std::optional<bool>(cd->nullable());
	}
	if (rg.NextSmallNumber() < 4) {
		GenerateNextStatistics(rg, cd->mutable_stats());
	}
	if (t.IsMergeTreeFamily()) {
		if (rg.NextSmallNumber() < 4) {
			const uint32_t ncodecs = (rg.NextMediumNumber() % UINT32_C(3)) + 1;

			for (uint32_t i = 0 ; i < ncodecs ; i++) {
				sql_query_grammar::CodecParam *cp = cd->add_codecs();
				sql_query_grammar::CompressionCodec cc =
					static_cast<sql_query_grammar::CompressionCodec>((rg.NextRandomUInt32() % static_cast<uint32_t>(sql_query_grammar::CompressionCodec_MAX)) + 1);

				cp->set_codec(cc);
				switch (cc) {
					case sql_query_grammar::COMPLZ4HC:
					case sql_query_grammar::COMPZSTD_QAT:
						if (rg.NextBool()) {
							std::uniform_int_distribution<uint32_t> next_dist(1, 12);
							cp->add_params(next_dist(rg.gen));
						}
						break;
					case sql_query_grammar::COMPZSTD:
						if (rg.NextBool()) {
							std::uniform_int_distribution<uint32_t> next_dist(1, 22);
							cp->add_params(next_dist(rg.gen));
						}
						break;
					case sql_query_grammar::COMPDelta:
					case sql_query_grammar::COMPDoubleDelta:
					case sql_query_grammar::COMPGorilla:
						if (rg.NextBool()) {
							std::uniform_int_distribution<uint32_t> next_dist(0, 3);
							cp->add_params(UINT32_C(1) << next_dist(rg.gen));
						}
						break;
					case sql_query_grammar::COMPFPC:
						if (rg.NextBool()) {
							std::uniform_int_distribution<uint32_t> next_dist1(1, 28);
							cp->add_params(next_dist1(rg.gen));
							cp->add_params(rg.NextBool() ? 4 : 9);
						}
						break;
					default:
						break;
				}
			}
		}
		if (rg.NextSmallNumber() < 4) {
			GenerateSettingValues(rg, MergeTreeColumnSettings, cd->mutable_settings());
		}
	}
	to_add[cname] = std::move(col);
	return 0;
}

#define PossibleForFullText(tpe) \
	(dynamic_cast<StringType*>(tpe) || \
	 ((at = dynamic_cast<ArrayType*>(tpe)) && dynamic_cast<StringType*>(at->subtype)) || \
	 ((lc = dynamic_cast<LowCardinality*>(tpe)) && dynamic_cast<StringType*>(lc->subtype)))

int StatementGenerator::AddTableIndex(RandomGenerator &rg, SQLTable &t, const bool staged, sql_query_grammar::IndexDef *idef) {
	SQLIndex idx;
	const uint32_t iname = t.idx_counter++;
	sql_query_grammar::Expr *expr = idef->mutable_expr();
	sql_query_grammar::IndexType itpe =
		static_cast<sql_query_grammar::IndexType>((rg.NextRandomUInt32() % static_cast<uint32_t>(sql_query_grammar::IndexType_MAX)) + 1);
	auto &to_add = staged ? t.staged_idxs : t.idxs;

	idx.iname = iname;
	idef->mutable_idx()->set_index("i" + std::to_string(iname));
	idef->set_type(itpe);
	if (rg.NextSmallNumber() < 9) {
		ArrayType *at = nullptr;
		NestedType *ntp = nullptr;
		LowCardinality *lc = nullptr;

		assert(this->entries.empty());
		for (const auto &entry : t.cols) {
			if ((ntp = dynamic_cast<NestedType*>(entry.second.tp))) {
				for (const auto &entry2 : ntp->subtypes) {
					if (itpe < sql_query_grammar::IndexType::IDXngrambf_v1 || PossibleForFullText(entry2.subtype)) {
						entries.push_back(InsertEntry(false, entry.second.cname, std::optional<uint32_t>(entry2.cname), entry2.array_subtype));
					}
				}
			} else if (itpe < sql_query_grammar::IndexType::IDXngrambf_v1 || PossibleForFullText(entry.second.tp)) {
				entries.push_back(InsertEntry(entry.second.special == ColumnSpecial::SIGN, entry.second.cname, std::nullopt, entry.second.tp));
			}
		}
	}
	if (!entries.empty()) {
		std::shuffle(entries.begin(), entries.end(), rg.gen);

		if (itpe == sql_query_grammar::IndexType::IDXhypothesis && entries.size() > 1 && rg.NextSmallNumber() < 9) {
			sql_query_grammar::BinaryExpr *bexpr = expr->mutable_comp_expr()->mutable_binary_expr();
			sql_query_grammar::Expr *expr1 = bexpr->mutable_lhs(), *expr2 = bexpr->mutable_rhs();
			sql_query_grammar::ExprSchemaTableColumn *estc1 = expr1->mutable_comp_expr()->mutable_expr_stc(),
													 *estc2 = expr2->mutable_comp_expr()->mutable_expr_stc();
			sql_query_grammar::ExprColumn *ecol1 = estc1->mutable_col(), *ecol2 = estc2->mutable_col();
			const InsertEntry &entry1 = this->entries[0], &entry2 = this->entries[1];

			bexpr->set_op(rg.NextSmallNumber() < 8 ? sql_query_grammar::BinaryOperator::BINOP_EQ :
				static_cast<sql_query_grammar::BinaryOperator>((rg.NextRandomUInt32() % static_cast<uint32_t>(sql_query_grammar::BinaryOperator::BINOP_LEGR)) + 1));
			ecol1->mutable_col()->set_column("c" + std::to_string(entry1.cname1));
			if (entry1.cname2.has_value()) {
				ecol1->mutable_subcol()->set_column("c" + std::to_string(entry1.cname2.value()));
			}
			ecol2->mutable_col()->set_column("c" + std::to_string(entry2.cname1));
			if (entry2.cname2.has_value()) {
				ecol2->mutable_subcol()->set_column("c" + std::to_string(entry2.cname2.value()));
			}
			AddFieldAccess(rg, expr1, 11);
			AddFieldAccess(rg, expr2, 11);
			AddColNestedAccess(rg, ecol1, 21);
			AddColNestedAccess(rg, ecol2, 21);
		} else {
			const InsertEntry &entry1 = this->entries[0];
			sql_query_grammar::ExprColumn *ecol = expr->mutable_comp_expr()->mutable_expr_stc()->mutable_col();

			ecol->mutable_col()->set_column("c" + std::to_string(entry1.cname1));
			if (entry1.cname2.has_value()) {
				ecol->mutable_subcol()->set_column("c" + std::to_string(entry1.cname2.value()));
			}
		}
		entries.clear();
	} else {
		AddTableRelation(rg, false, "", t);
		this->levels[this->current_level].allow_aggregates = this->levels[this->current_level].allow_window_funcs = false;
		GenerateExpression(rg, expr);
		this->levels[this->current_level].allow_aggregates = this->levels[this->current_level].allow_window_funcs = true;
		this->levels.clear();
	}
	switch (itpe) {
		case sql_query_grammar::IndexType::IDXset:
			if (rg.NextSmallNumber() < 7) {
				idef->add_params()->set_ival(0);
			} else {
				std::uniform_int_distribution<uint32_t> next_dist(1, 1000);
				idef->add_params()->set_ival(next_dist(rg.gen));
			}
			break;
		case sql_query_grammar::IndexType::IDXbloom_filter: {
			std::uniform_int_distribution<uint32_t> next_dist(1, 1000);
			idef->add_params()->set_dval(static_cast<double>(next_dist(rg.gen)) / static_cast<double>(1000));
		} break;
		case sql_query_grammar::IndexType::IDXngrambf_v1:
		case sql_query_grammar::IndexType::IDXtokenbf_v1: {
			std::uniform_int_distribution<uint32_t> next_dist1(1, 1000), next_dist2(1, 5);

			if (itpe == sql_query_grammar::IndexType::IDXngrambf_v1) {
				idef->add_params()->set_ival(next_dist1(rg.gen));
			}
			idef->add_params()->set_ival(next_dist1(rg.gen));
			idef->add_params()->set_ival(next_dist2(rg.gen));
			idef->add_params()->set_ival(next_dist1(rg.gen));
		} break;
		case sql_query_grammar::IndexType::IDXfull_text:
		case sql_query_grammar::IndexType::IDXinverted: {
			std::uniform_int_distribution<uint32_t> next_dist(0, 10);
			idef->add_params()->set_ival(next_dist(rg.gen));
		} break;
		case sql_query_grammar::IndexType::IDXminmax:
		case sql_query_grammar::IndexType::IDXhypothesis:
			break;
	}
	if (rg.NextSmallNumber() < 7) {
		std::uniform_int_distribution<uint32_t> next_dist(1, 1000);
		idef->set_granularity(next_dist(rg.gen));
	}
	to_add[iname] = std::move(idx);
	return 0;
}

int StatementGenerator::AddTableProjection(RandomGenerator &rg, SQLTable &t, const bool staged, sql_query_grammar::ProjectionDef *pdef) {
	const uint32_t pname = t.proj_counter++, ncols = std::max(std::min(this->max_width - this->width, (rg.NextMediumNumber() % UINT32_C(3)) + 1), UINT32_C(1));
	auto &to_add = staged ? t.staged_projs : t.projs;

	pdef->mutable_proj()->set_projection("p" + std::to_string(pname));
	this->inside_projection = true;
	AddTableRelation(rg, false, "", t);
	GenerateSelect(rg, true, ncols, allow_groupby|allow_orderby, pdef->mutable_select());
	this->levels.clear();
	this->inside_projection = false;
	to_add.insert(pname);
	return 0;
}

int StatementGenerator::AddTableConstraint(RandomGenerator &rg, SQLTable &t, const bool staged, sql_query_grammar::ConstraintDef *cdef) {
	const uint32_t crname = t.constr_counter++;
	auto &to_add = staged ? t.staged_constrs : t.constrs;

	cdef->mutable_constr()->set_constraint("c" + std::to_string(crname));
	cdef->set_ctype(static_cast<sql_query_grammar::ConstraintDef_ConstraintType>((rg.NextRandomUInt32() % static_cast<uint32_t>(sql_query_grammar::ConstraintDef::ConstraintType_MAX)) + 1));
	AddTableRelation(rg, false, "", t);
	this->levels[this->current_level].allow_aggregates = this->levels[this->current_level].allow_window_funcs = false;
	this->GenerateWherePredicate(rg, cdef->mutable_expr());
	this->levels[this->current_level].allow_aggregates = this->levels[this->current_level].allow_window_funcs = true;
	this->levels.clear();
	to_add.insert(crname);
	return 0;
}

const std::vector<sql_query_grammar::TableEngineValues> like_engs = {
  sql_query_grammar::TableEngineValues::Memory,
  sql_query_grammar::TableEngineValues::MergeTree,
  sql_query_grammar::TableEngineValues::ReplacingMergeTree,
  sql_query_grammar::TableEngineValues::SummingMergeTree,
  sql_query_grammar::TableEngineValues::AggregatingMergeTree,
  sql_query_grammar::TableEngineValues::StripeLog,
  sql_query_grammar::TableEngineValues::Log,
  sql_query_grammar::TableEngineValues::TinyLog};

int StatementGenerator::GenerateNextCreateTable(RandomGenerator &rg, sql_query_grammar::CreateTable *ct) {
	SQLTable next;
	uint32_t tname = 0;
	sql_query_grammar::TableEngine *te = ct->mutable_engine();
	sql_query_grammar::ExprSchemaTable *est = ct->mutable_est();
	const bool replace = CollectionCount<SQLTable>(attached_tables) > 3 && rg.NextMediumNumber() < 16;

	if (replace) {
		const SQLTable &t = rg.PickRandomlyFromVector(FilterCollection<SQLTable>(attached_tables));

		tname = next.tname = t.tname;
	} else {
		next.db = rg.PickRandomlyFromVector(FilterCollection<std::shared_ptr<SQLDatabase>>(attached_databases));
		tname = next.tname = next.db->table_counter++;
	}
	ct->set_replace(replace);
	next.is_temp = rg.NextMediumNumber() < 22;
	ct->set_is_temp(next.is_temp);
	est->mutable_database()->set_database("d" + std::to_string(next.db->dname));
	est->mutable_table()->set_table("t" + std::to_string(next.tname));
	if (!CollectionHas<SQLTable>(attached_tables) || rg.NextSmallNumber() < 9) {
		//create table with definition
		sql_query_grammar::TableDef *colsdef = ct->mutable_table_def();
		std::uniform_int_distribution<uint32_t> table_engine(1, sql_query_grammar::TableEngineValues_MAX);
		sql_query_grammar::TableEngineValues val = static_cast<sql_query_grammar::TableEngineValues>(table_engine(rg.gen));

		next.teng = val;
		te->set_engine(val);
		uint32_t added_cols = 0, added_idxs = 0, added_projs = 0, added_consts = 0, added_sign = 0, added_version = 0;
		const uint32_t to_addcols = (rg.NextMediumNumber() % 5) + 1,
					   to_addidxs = (rg.NextMediumNumber() % 4) * static_cast<uint32_t>(next.IsMergeTreeFamily() && rg.NextSmallNumber() < 4),
					   to_addprojs = (rg.NextMediumNumber() % 3) * static_cast<uint32_t>(next.IsMergeTreeFamily() && rg.NextSmallNumber() < 5),
					   to_addconsts = (rg.NextMediumNumber() % 3) * static_cast<uint32_t>(rg.NextSmallNumber() < 3),
					   to_add_sign = static_cast<uint32_t>(next.HasSignColumn()),
					   to_add_version = static_cast<uint32_t>(next.HasVersionColumn()),
					   total_to_add = to_addcols + to_addidxs + to_addprojs + to_addconsts + to_add_sign + to_add_version;

		for (uint32_t i = 0 ; i < total_to_add; i++) {
			const uint32_t add_idx = 4 * static_cast<uint32_t>(!next.cols.empty() && added_idxs < to_addidxs),
						   add_proj = 4 * static_cast<uint32_t>(!next.cols.empty() && added_projs < to_addprojs),
						   add_const = 4 * static_cast<uint32_t>(!next.cols.empty() && added_consts < to_addconsts),
						   add_col = 8 * static_cast<uint32_t>(added_cols < to_addcols),
						   add_sign = 2 * static_cast<uint32_t>(added_sign < to_add_sign),
						   add_version = 2 * static_cast<uint32_t>(added_version < to_add_version && added_sign == to_add_sign),
						   prob_space = add_idx + add_proj + add_const + add_col + add_sign + add_version;
			std::uniform_int_distribution<uint32_t> next_dist(1, prob_space);
			const uint32_t nopt = next_dist(rg.gen);

			if (add_idx && nopt < (add_idx + 1)) {
				AddTableIndex(rg, next, false, colsdef->add_other_defs()->mutable_idx_def());
				added_idxs++;
			} else if (add_proj && nopt < (add_idx + add_proj + 1)) {
				AddTableProjection(rg, next, false, colsdef->add_other_defs()->mutable_proj_def());
				added_projs++;
			} else if (add_const && nopt < (add_idx + add_proj + add_const + 1)) {
				AddTableConstraint(rg, next, false, colsdef->add_other_defs()->mutable_const_def());
				added_consts++;
			} else if (add_col && nopt < (add_idx + add_proj + add_const + add_col + 1)) {
				sql_query_grammar::ColumnDef *cd = i == 0 ? colsdef->mutable_col_def() : colsdef->add_other_defs()->mutable_col_def();

				AddTableColumn(rg, next, next.col_counter++, false, false, ColumnSpecial::NONE, cd);
				added_cols++;
			} else {
				const uint32_t cname = next.col_counter++;
				const bool add_version_col = add_version && nopt < (add_idx + add_proj + add_const + add_col + add_version + 1);
				sql_query_grammar::ColumnDef *cd = i == 0 ? colsdef->mutable_col_def() : colsdef->add_other_defs()->mutable_col_def();

				AddTableColumn(rg, next, cname, false, false, add_version_col ? ColumnSpecial::VERSION : ColumnSpecial::SIGN, cd);
				te->add_cols()->set_column("c" + std::to_string(cname));
				if (add_version_col) {
					added_version++;
				} else {
					added_sign++;
				}
			}
		}
		if (rg.NextSmallNumber() < 3) {
			this->levels[this->current_level] = QueryLevel(this->current_level);
			GenerateSelect(rg, true, static_cast<uint32_t>(next.RealNumberOfColumns()), std::numeric_limits<uint32_t>::max(), ct->mutable_as_select_stmt());
		}
	} else {
		//create table as
		sql_query_grammar::CreateTableAs *cta = ct->mutable_table_as();
		sql_query_grammar::ExprSchemaTable *aest = cta->mutable_est();
		const SQLTable &t = rg.PickRandomlyFromVector(FilterCollection<SQLTable>(attached_tables));
		std::uniform_int_distribution<size_t> table_engine(0, like_engs.size() - 1);
		sql_query_grammar::TableEngineValues val = like_engs[table_engine(rg.gen)];

		next.teng = val;
		te->set_engine(val);
		cta->set_clone(rg.NextBool());
		aest->mutable_database()->set_database("d" + std::to_string(t.db->dname));
		aest->mutable_table()->set_table("t" + std::to_string(t.tname));
		for (const auto &col : t.cols) {
			next.cols[col.first] = col.second;
		}
		for (const auto &idx : t.idxs) {
			next.idxs[idx.first] = idx.second;
		}
		next.projs.insert(t.projs.begin(), t.projs.end());
		next.constrs.insert(t.constrs.begin(), t.constrs.end());
		next.col_counter = t.col_counter;
		next.idx_counter = t.idx_counter;
		next.proj_counter = t.proj_counter;
		next.constr_counter = t.constr_counter;
		next.is_temp = t.is_temp;
	}
	if (next.IsMergeTreeFamily()) {
		NestedType *ntp = nullptr;

		if (rg.NextSmallNumber() < 4) {
			next.toption = supports_cloud_features && rg.NextBool() ?
				sql_query_grammar::TableEngineOption::TShared : sql_query_grammar::TableEngineOption::TReplicated;
		}
		assert(this->entries.empty());
		for (const auto &entry : next.cols) {
			if ((ntp = dynamic_cast<NestedType*>(entry.second.tp))) {
				for (const auto &entry2 : ntp->subtypes) {
					if (!dynamic_cast<JSONType*>(entry2.subtype)) {
						entries.push_back(InsertEntry(false, entry.second.cname, std::optional<uint32_t>(entry2.cname), entry2.array_subtype));
					}
				}
			} else if (!dynamic_cast<JSONType*>(entry.second.tp)) {
				entries.push_back(InsertEntry(entry.second.special == ColumnSpecial::SIGN, entry.second.cname, std::nullopt, entry.second.tp));
			}
		}
		GenerateEngineDetails(rg, te);
		this->entries.clear();

		sql_query_grammar::SettingValues *svs = ct->mutable_settings();
		if (rg.NextSmallNumber() < 5) {
			GenerateSettingValues(rg, MergeTreeTableSettings, svs);
		}
		sql_query_grammar::SetValue *sv = svs->has_set_value() ? svs->add_other_values() : svs->mutable_set_value();
		sv->set_property("allow_nullable_key");
		sv->set_value("1");

		if (next.toption.has_value() && next.toption.value() == sql_query_grammar::TableEngineOption::TShared) {
			sql_query_grammar::SetValue *sv2 = svs->add_other_values();
			sv2->set_property("storage_policy");
			sv2->set_value("'s3_with_keeper'");
		}
	}
	if (next.toption.has_value()) {
		te->set_toption(next.toption.value());
	}
	assert(!next.toption.has_value() || next.IsMergeTreeFamily());
	this->staged_tables[tname] = std::move(next);
	return 0;
}

static void
SetViewInterval(RandomGenerator &rg, sql_query_grammar::RefreshInterval *ri) {
	ri->set_interval(rg.NextSmallNumber() - 1);
	ri->set_unit(sql_query_grammar::RefreshInterval_RefreshUnit::RefreshInterval_RefreshUnit_SECOND);
}

int StatementGenerator::GenerateNextRefreshableView(RandomGenerator &rg, sql_query_grammar::RefreshableView *cv) {
	const sql_query_grammar::RefreshableView_RefreshPolicy pol = rg.NextBool() ?
		sql_query_grammar::RefreshableView_RefreshPolicy::RefreshableView_RefreshPolicy_EVERY :
		sql_query_grammar::RefreshableView_RefreshPolicy::RefreshableView_RefreshPolicy_AFTER;

	cv->set_policy(pol);
	SetViewInterval(rg, cv->mutable_interval());
	if (pol == sql_query_grammar::RefreshableView_RefreshPolicy::RefreshableView_RefreshPolicy_EVERY && rg.NextBool()) {
		SetViewInterval(rg, cv->mutable_offset());
	}
	SetViewInterval(rg, cv->mutable_randomize());
	cv->set_append(rg.NextBool());
	return 0;
}

int StatementGenerator::GenerateNextCreateView(RandomGenerator &rg, sql_query_grammar::CreateView *cv) {
	SQLView next;
	uint32_t vname = 0;
	sql_query_grammar::ExprSchemaTable *est = cv->mutable_est();
	const bool replace = CollectionCount<SQLView>(attached_views) > 3 && rg.NextMediumNumber() < 16;

	if (replace) {
		const SQLView &v = rg.PickRandomlyFromVector(FilterCollection<SQLView>(attached_views));

		vname = next.vname = v.vname;
	} else {
		next.db = rg.PickRandomlyFromVector(FilterCollection<std::shared_ptr<SQLDatabase>>(attached_databases));
		vname = next.vname = next.db->table_counter++;
	}
	cv->set_replace(replace);
	next.is_materialized = rg.NextBool();
	cv->set_materialized(next.is_materialized);
	next.ncols = (rg.NextMediumNumber() % 5) + 1;
	est->mutable_database()->set_database("d" + std::to_string(next.db->dname));
	est->mutable_table()->set_table("v" + std::to_string(next.vname));
	if (next.is_materialized) {
		sql_query_grammar::TableEngine *te = cv->mutable_engine();
		std::uniform_int_distribution<uint32_t> table_engine(1, sql_query_grammar::TableEngineValues_MAX);
		sql_query_grammar::TableEngineValues val = static_cast<sql_query_grammar::TableEngineValues>(table_engine(rg.gen));

		next.teng = val;
		te->set_engine(val);
		if (next.IsMergeTreeFamily()) {
			if (rg.NextSmallNumber() < 4) {
				next.toption = supports_cloud_features && rg.NextBool() ?
					sql_query_grammar::TableEngineOption::TShared : sql_query_grammar::TableEngineOption::TReplicated;
				te->set_toption(next.toption.value());
			}

			assert(this->entries.empty());
			for (uint32_t i = 0 ; i < next.ncols ; i++) {
				entries.push_back(InsertEntry(false, i, std::nullopt, nullptr));
			}
			GenerateEngineDetails(rg, te);
			this->entries.clear();
		}
		if (CollectionHas<SQLTable>(attached_tables) && rg.NextSmallNumber() < 5) {
			const SQLTable &t = rg.PickRandomlyFromVector(FilterCollection<SQLTable>(attached_tables));

			cv->mutable_to_est()->mutable_table()->set_table("t" + std::to_string(t.tname));
		}
		if ((next.is_refreshable = rg.NextBool())) {
			GenerateNextRefreshableView(rg, cv->mutable_refresh());
			cv->set_empty(rg.NextBool());
		} else {
			cv->set_populate(rg.NextSmallNumber() < 4);
		}
	}
	this->levels[this->current_level] = QueryLevel(this->current_level);
	GenerateSelect(rg, false, next.ncols, next.is_materialized ? (~allow_prewhere) : std::numeric_limits<uint32_t>::max(),
				   cv->mutable_select());
	assert(!next.toption.has_value() || next.IsMergeTreeFamily());
	this->staged_views[vname] = std::move(next);
	return 0;
}

int StatementGenerator::GenerateNextDrop(RandomGenerator &rg, sql_query_grammar::Drop *dp) {
	sql_query_grammar::SchemaOrTable *sot = dp->mutable_object();
	const uint32_t drop_table = 10 * static_cast<uint32_t>(CollectionHas<SQLTable>(attached_tables)),
				   drop_view = 10 * static_cast<uint32_t>(CollectionHas<SQLView>(attached_views)),
				   drop_database = 2 * static_cast<uint32_t>(CollectionHas<std::shared_ptr<SQLDatabase>>(attached_databases)),
				   prob_space = drop_table + drop_view + drop_database;
	std::uniform_int_distribution<uint32_t> next_dist(1, prob_space);
	const uint32_t nopt = next_dist(rg.gen);

	if (drop_table && nopt < (drop_table + 1)) {
		sql_query_grammar::ExprSchemaTable *est = sot->mutable_est();
		const SQLTable &t = rg.PickRandomlyFromVector(FilterCollection<SQLTable>(attached_tables));

		dp->set_is_temp(t.is_temp);
		dp->set_sobject(sql_query_grammar::SQLObject::TABLE);
		dp->set_if_empty(rg.NextSmallNumber() < 4);
		est->mutable_database()->set_database("d" + std::to_string(t.db->dname));
		est->mutable_table()->set_table("t" + std::to_string(t.tname));
	} else if (drop_view && nopt < (drop_table + drop_view + 1)) {
		sql_query_grammar::ExprSchemaTable *est = sot->mutable_est();
		const SQLView &v = rg.PickRandomlyFromVector(FilterCollection<SQLView>(attached_views));

		dp->set_sobject(sql_query_grammar::SQLObject::VIEW);
		est->mutable_database()->set_database("d" + std::to_string(v.db->dname));
		est->mutable_table()->set_table("v" + std::to_string(v.vname));
	} else if (drop_database) {
		const std::shared_ptr<SQLDatabase> &d = rg.PickRandomlyFromVector(FilterCollection<std::shared_ptr<SQLDatabase>>(attached_databases));

		dp->set_sobject(sql_query_grammar::SQLObject::DATABASE);
		sot->mutable_database()->set_database("d" + std::to_string(d->dname));
	} else {
		assert(0);
	}
	dp->set_sync(rg.NextSmallNumber() < 3);
	return 0;
}

int StatementGenerator::GenerateNextOptimizeTable(RandomGenerator &rg, sql_query_grammar::OptimizeTable *ot) {
	sql_query_grammar::ExprSchemaTable *est = ot->mutable_est();
	const SQLTable &t = rg.PickRandomlyFromVector(FilterCollection<SQLTable>(attached_tables));

	est->mutable_database()->set_database("d" + std::to_string(t.db->dname));
	est->mutable_table()->set_table("t" + std::to_string(t.tname));
	if (rg.NextSmallNumber() < 4) {
		sql_query_grammar::DeduplicateExpr *dde = ot->mutable_dedup();

		if (rg.NextSmallNumber() < 6) {
			NestedType *ntp = nullptr;
			sql_query_grammar::ExprColumnList *ecl = dde->mutable_col_list();
			const uint32_t ocols = (rg.NextMediumNumber() % std::min<uint32_t>(static_cast<uint32_t>(t.RealNumberOfColumns()), UINT32_C(4))) + 1;

			for (const auto &entry : t.cols) {
				if ((ntp = dynamic_cast<NestedType*>(entry.second.tp))) {
					for (const auto &entry2 : ntp->subtypes) {
						entries.push_back(InsertEntry(false, entry.second.cname, std::optional<uint32_t>(entry2.cname), entry2.array_subtype));
					}
				} else {
					entries.push_back(InsertEntry(entry.second.special == ColumnSpecial::SIGN, entry.second.cname, std::nullopt, entry.second.tp));
				}
			}
			std::shuffle(entries.begin(), entries.end(), rg.gen);
			for (uint32_t i = 0; i < ocols; i++) {
				const InsertEntry &entry = this->entries[i];
				sql_query_grammar::ExprColumn *ec = i == 0 ? ecl->mutable_col() : ecl->add_extra_cols();

				ec->mutable_col()->set_column("c" + std::to_string(entry.cname1));
				if (entry.cname2.has_value()) {
					ec->mutable_subcol()->set_column("c" + std::to_string(entry.cname2.value()));
				}
			}
			entries.clear();
		}
	}
	ot->set_final(t.SupportsFinal() && rg.NextSmallNumber() < 3);
	return 0;
}

int StatementGenerator::GenerateNextCheckTable(RandomGenerator &rg, sql_query_grammar::CheckTable *ct) {
	sql_query_grammar::ExprSchemaTable *est = ct->mutable_est();
	const SQLTable &t = rg.PickRandomlyFromVector(FilterCollection<SQLTable>(attached_tables));

	est->mutable_database()->set_database("d" + std::to_string(t.db->dname));
	est->mutable_table()->set_table("t" + std::to_string(t.tname));
	ct->set_single_result(rg.NextSmallNumber() < 4);
	return 0;
}

int StatementGenerator::GenerateNextDescTable(RandomGenerator &rg, sql_query_grammar::DescTable *dt) {
	sql_query_grammar::ExprSchemaTable *est = dt->mutable_est();
	const bool has_tables = CollectionHas<SQLTable>(attached_tables),
			   has_views = CollectionHas<SQLView>(attached_views);

	if (has_views && (!has_tables || rg.NextBool())) {
		const SQLView &v = rg.PickRandomlyFromVector(FilterCollection<SQLView>(attached_views));

		est->mutable_database()->set_database("d" + std::to_string(v.db->dname));
		est->mutable_table()->set_table("v" + std::to_string(v.vname));
	} else if (has_tables) {
		const SQLTable &t = rg.PickRandomlyFromVector(FilterCollection<SQLTable>(attached_tables));

		est->mutable_database()->set_database("d" + std::to_string(t.db->dname));
		est->mutable_table()->set_table("t" + std::to_string(t.tname));
	} else {
		assert(0);
	}
	dt->set_sub_cols(rg.NextSmallNumber() < 4);
	return 0;
}

int StatementGenerator::GenerateNextInsert(RandomGenerator &rg, sql_query_grammar::Insert *ins) {
	NestedType *ntp = nullptr;
	const uint32_t noption = rg.NextMediumNumber();
	sql_query_grammar::InsertIntoTable *iit = ins->mutable_itable();
	sql_query_grammar::ExprSchemaTable *est = iit->mutable_est();
	const SQLTable &t = rg.PickRandomlyFromVector(FilterCollection<SQLTable>(attached_tables));

	est->mutable_database()->set_database("d" + std::to_string(t.db->dname));
	est->mutable_table()->set_table("t" + std::to_string(t.tname));
	assert(this->entries.empty());
	for (const auto &entry : t.cols) {
		if ((ntp = dynamic_cast<NestedType*>(entry.second.tp))) {
			for (const auto &entry2 : ntp->subtypes) {
				this->entries.push_back(InsertEntry(false, entry.second.cname, std::optional<uint32_t>(entry2.cname), entry2.array_subtype));
			}
		} else {
			this->entries.push_back(InsertEntry(entry.second.special == ColumnSpecial::SIGN, entry.second.cname, std::nullopt, entry.second.tp));
		}
	}
	std::shuffle(this->entries.begin(), this->entries.end(), rg.gen);

	for (const auto &entry : this->entries) {
		sql_query_grammar::ColumnPath *cp = iit->add_cols();

		cp->mutable_col()->set_column("c" + std::to_string(entry.cname1));
		if (entry.cname2.has_value()) {
			cp->add_sub_cols()->set_column("c" + std::to_string(entry.cname2.value()));
		}
	}

	if (noption < 901) {
		const uint32_t nrows = rg.NextMediumNumber();

		buf.resize(0);
		for (uint32_t i = 0 ; i < nrows; i++) {
			uint32_t j = 0;

			if (i != 0) {
				buf += ", ";
			}
			buf += "(";
			for (const auto &entry : this->entries) {
				if (j != 0) {
					buf += ", ";
				}
				if (entry.is_sign) {
					buf += rg.NextBool() ? "1" : "-1";
				} else {
					StrAppendAnyValue(rg, buf, entry.tp);
				}
				j++;
			}
			buf += ")";
		}
		ins->set_query(buf);
	} else if (noption < 951) {
		this->levels[this->current_level] = QueryLevel(this->current_level);
		GenerateSelect(rg, true, static_cast<uint32_t>(this->entries.size()), std::numeric_limits<uint32_t>::max(), ins->mutable_select());
	} else {
		const uint32_t nrows = (rg.NextSmallNumber() % 3) + 1;
		sql_query_grammar::ValuesStatement *vs = ins->mutable_values();

		this->levels[this->current_level] = QueryLevel(this->current_level);
		this->levels[this->current_level].allow_aggregates = this->levels[this->current_level].allow_window_funcs = false;
		for (uint32_t i = 0 ; i < nrows; i++) {
			bool first = true;
			sql_query_grammar::ExprList *elist = i == 0 ? vs->mutable_expr_list() : vs->add_extra_expr_lists();

			for (const auto &entry : this->entries) {
				sql_query_grammar::Expr *expr = first ? elist->mutable_expr() : elist->add_extra_exprs();

				if (entry.is_sign) {
					expr->mutable_lit_val()->mutable_int_lit()->set_int_lit(rg.NextBool() ? 1 : -1);
				} else {
					GenerateExpression(rg, expr);
				}
				first = false;
			}
		}
		this->levels[this->current_level].allow_aggregates = this->levels[this->current_level].allow_window_funcs = true;
		this->levels.clear();
	}
	this->entries.clear();
	return 0;
}

int StatementGenerator::GenerateUptDelWhere(RandomGenerator &rg, const SQLTable &t, sql_query_grammar::Expr *expr) {
	if (rg.NextSmallNumber() < 10) {
		AddTableRelation(rg, true, "", t);
		this->levels[this->current_level].allow_aggregates = this->levels[this->current_level].allow_window_funcs = false;
		GenerateWherePredicate(rg, expr);
		this->levels[this->current_level].allow_aggregates = this->levels[this->current_level].allow_window_funcs = true;
		this->levels.clear();
	} else {
		expr->mutable_lit_val()->set_special_val(sql_query_grammar::SpecialVal::VAL_TRUE);
	}
	return 0;
}

int StatementGenerator::GenerateNextDelete(RandomGenerator &rg, sql_query_grammar::Delete *del) {
	sql_query_grammar::ExprSchemaTable *est = del->mutable_est();
	const SQLTable &t = rg.PickRandomlyFromVector(FilterCollection<SQLTable>(attached_tables));

	est->mutable_database()->set_database("d" + std::to_string(t.db->dname));
	est->mutable_table()->set_table("t" + std::to_string(t.tname));
	GenerateUptDelWhere(rg, t, del->mutable_where()->mutable_expr()->mutable_expr());
	return 0;
}

int StatementGenerator::GenerateNextTruncate(RandomGenerator &rg, sql_query_grammar::Truncate *trunc) {
	sql_query_grammar::ExprSchemaTable *est = trunc->mutable_est();
	const SQLTable &t = rg.PickRandomlyFromVector(FilterCollection<SQLTable>(attached_tables));

	est->mutable_database()->set_database("d" + std::to_string(t.db->dname));
	est->mutable_table()->set_table("t" + std::to_string(t.tname));
	return 0;
}

int StatementGenerator::GenerateNextExchangeTables(RandomGenerator &rg, sql_query_grammar::ExchangeTables *et) {
	sql_query_grammar::ExprSchemaTable *est1 = et->mutable_est1(), *est2 = et->mutable_est2();
	const auto &input = FilterCollection<SQLTable>(attached_tables);

	for (const auto &entry : input) {
		this->ids.push_back(entry.get().tname);
	}
	std::shuffle(this->ids.begin(), this->ids.end(), rg.gen);
	const SQLTable &t1 = this->tables[this->ids[0]], &t2 = this->tables[this->ids[1]];

	est1->mutable_database()->set_database("d" + std::to_string(t1.db->dname));
	est1->mutable_table()->set_table("t" + std::to_string(t1.tname));
	est2->mutable_database()->set_database("d" + std::to_string(t2.db->dname));
	est2->mutable_table()->set_table("t" + std::to_string(t2.tname));
	this->ids.clear();
	return 0;
}

int StatementGenerator::GenerateAlterTable(RandomGenerator &rg, sql_query_grammar::AlterTable *at) {
	sql_query_grammar::ExprSchemaTable *est = at->mutable_est();
	const uint32_t nalters = rg.NextBool() ? 1 : ((rg.NextMediumNumber() % 4) + 1);
	const bool has_tables = CollectionHas<SQLTable>(attached_tables),
			   has_views = CollectionHas<SQLView>(attached_views);

	if (has_views && (!has_tables || rg.NextBool())) {
		SQLView &v = const_cast<SQLView &>(rg.PickRandomlyFromVector(FilterCollection<SQLView>(attached_views)).get());

		est->mutable_database()->set_database("d" + std::to_string(v.db->dname));
		est->mutable_table()->set_table("v" + std::to_string(v.vname));
		for (uint32_t i = 0; i < nalters; i++) {
			const uint32_t alter_refresh = 1 * static_cast<uint32_t>(v.is_refreshable),
						   alter_query = 3,
						   prob_space = alter_refresh + alter_query;
			sql_query_grammar::AlterTableItem *ati = i == 0 ? at->mutable_alter() : at->add_other_alters();
			std::uniform_int_distribution<uint32_t> next_dist(1, prob_space);
			const uint32_t nopt = next_dist(rg.gen);

			if (alter_refresh && nopt < (alter_refresh + 1)) {
				GenerateNextRefreshableView(rg, ati->mutable_refresh());
			} else {
				v.staged_ncols = (rg.NextMediumNumber() % 5) + 1;
				this->levels[this->current_level] = QueryLevel(this->current_level);
				GenerateSelect(rg, false, v.staged_ncols, v.is_materialized ? (~allow_prewhere) : std::numeric_limits<uint32_t>::max(),
							   ati->mutable_modify_query());
			}
		}
	} else if (has_tables) {
		SQLTable &t = const_cast<SQLTable &>(rg.PickRandomlyFromVector(FilterCollection<SQLTable>(attached_tables)).get());

		est->mutable_database()->set_database("d" + std::to_string(t.db->dname));
		est->mutable_table()->set_table("t" + std::to_string(t.tname));
		for (uint32_t i = 0; i < nalters; i++) {
			const uint32_t alter_order_by = 3 * static_cast<uint32_t>(t.IsMergeTreeFamily()),
						   heavy_delete = 30,
						   heavy_update = 30,
						   add_column = 2 * static_cast<uint32_t>(t.cols.size() < 10),
						   materialize_column = 2,
						   drop_column = 2 * static_cast<uint32_t>(t.cols.size() > 1),
						   rename_column = 2,
						   modify_column = 2,
						   add_stats = 3 * static_cast<uint32_t>(t.IsMergeTreeFamily()),
						   mod_stats = 3 * static_cast<uint32_t>(t.IsMergeTreeFamily()),
						   drop_stats = 3 * static_cast<uint32_t>(t.IsMergeTreeFamily()),
						   clear_stats = 3 * static_cast<uint32_t>(t.IsMergeTreeFamily()),
						   mat_stats = 3 * static_cast<uint32_t>(t.IsMergeTreeFamily()),
						   delete_mask = 8 * static_cast<uint32_t>(t.IsMergeTreeFamily()),
						   add_idx = 2 * static_cast<uint32_t>(t.idxs.size() < 3),
						   materialize_idx = 2 * static_cast<uint32_t>(!t.idxs.empty()),
						   clear_idx = 2 * static_cast<uint32_t>(!t.idxs.empty()),
						   drop_idx = 2 * static_cast<uint32_t>(!t.idxs.empty()),
						   column_remove_property = 2,
						   column_modify_setting = 2 * static_cast<uint32_t>(t.IsMergeTreeFamily()),
						   column_remove_setting = 2 * static_cast<uint32_t>(t.IsMergeTreeFamily()),
						   table_modify_setting = 2 * static_cast<uint32_t>(t.IsMergeTreeFamily()),
						   table_remove_setting = 2 * static_cast<uint32_t>(t.IsMergeTreeFamily()),
						   add_projection = 2 * static_cast<uint32_t>(t.IsMergeTreeFamily()),
						   remove_projection = 2 * static_cast<uint32_t>(t.IsMergeTreeFamily() && !t.projs.empty()),
						   materialize_projection = 2 * static_cast<uint32_t>(t.IsMergeTreeFamily() && !t.projs.empty()),
						   clear_projection = 2 * static_cast<uint32_t>(t.IsMergeTreeFamily() && !t.projs.empty()),
						   add_constraint = 2 * static_cast<uint32_t>(t.constrs.size() < 4),
						   remove_constraint = 2 * static_cast<uint32_t>(!t.constrs.empty()),
						   prob_space = alter_order_by + heavy_delete + heavy_update + add_column + materialize_column + drop_column +
										rename_column + modify_column + delete_mask + add_stats + mod_stats + drop_stats + clear_stats +
										mat_stats + add_idx + materialize_idx + clear_idx + drop_idx + column_remove_property +
										column_modify_setting + column_remove_setting + table_modify_setting + table_remove_setting +
										add_projection + remove_projection + materialize_projection + clear_projection + add_constraint +
										remove_constraint;
			sql_query_grammar::AlterTableItem *ati = i == 0 ? at->mutable_alter() : at->add_other_alters();
			std::uniform_int_distribution<uint32_t> next_dist(1, prob_space);
			const uint32_t nopt = next_dist(rg.gen);

			if (alter_order_by && nopt < (alter_order_by + 1)) {
				NestedType *ntp = nullptr;

				assert(this->entries.empty());
				for (const auto &entry : t.cols) {
					if ((ntp = dynamic_cast<NestedType*>(entry.second.tp))) {
						for (const auto &entry2 : ntp->subtypes) {
							if (!dynamic_cast<JSONType*>(entry2.subtype)) {
								entries.push_back(InsertEntry(false, entry.second.cname, std::optional<uint32_t>(entry2.cname), entry2.array_subtype));
							}
						}
					} else if (!dynamic_cast<JSONType*>(entry.second.tp)) {
						entries.push_back(InsertEntry(entry.second.special == ColumnSpecial::SIGN, entry.second.cname, std::nullopt, entry.second.tp));
					}
				}
				GenerateTableKey(rg, ati->mutable_order());
				this->entries.clear();
			} else if (heavy_delete && nopt < (heavy_delete + alter_order_by + 1)) {
				GenerateUptDelWhere(rg, t, ati->mutable_del()->mutable_expr()->mutable_expr());
			} else if (add_column && nopt < (heavy_delete + alter_order_by + add_column + 1)) {
				const uint32_t next_option = rg.NextSmallNumber();
				sql_query_grammar::AddColumn *add_col = ati->mutable_add_column();

				AddTableColumn(rg, t, t.col_counter++, true, false, ColumnSpecial::NONE, add_col->mutable_new_col());
				if (next_option < 4) {
					const SQLColumn &ocol = rg.PickValueRandomlyFromMap(t.cols);
					add_col->mutable_add_where()->mutable_col()->set_column("c" + std::to_string(ocol.cname));
				} else if (next_option < 8) {
					add_col->mutable_add_where()->set_first(true);
				}
			} else if (materialize_column && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + 1)) {
				const SQLColumn &col = rg.PickValueRandomlyFromMap(t.cols);

				ati->mutable_materialize_column()->set_column("c" + std::to_string(col.cname));
			} else if (drop_column && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column + 1)) {
				const SQLColumn &col = rg.PickValueRandomlyFromMap(t.cols);

				ati->mutable_drop_column()->set_column("c" + std::to_string(col.cname));
			} else if (rename_column && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
												rename_column + 1)) {
				const uint32_t ncname = t.col_counter++;
				const SQLColumn &col = rg.PickValueRandomlyFromMap(t.cols);
				sql_query_grammar::RenameCol *rcol = ati->mutable_rename_column();

				rcol->mutable_old_name()->set_column("c" + std::to_string(col.cname));
				rcol->mutable_new_name()->set_column("c" + std::to_string(ncname));
			} else if (modify_column && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
												rename_column + modify_column + 1)) {
				const SQLColumn &ocol = rg.PickValueRandomlyFromMap(t.cols);
				const uint32_t next_option = rg.NextSmallNumber();
				sql_query_grammar::AddColumn *add_col = ati->mutable_modify_column();

				AddTableColumn(rg, t, ocol.cname, true, true, ColumnSpecial::NONE, add_col->mutable_new_col());
				if (next_option < 4) {
					const SQLColumn &col = rg.PickValueRandomlyFromMap(t.cols);
					add_col->mutable_add_where()->mutable_col()->set_column("c" + std::to_string(col.cname));
				} else if (next_option < 8) {
					add_col->mutable_add_where()->set_first(true);
				}
			} else if (delete_mask && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
											  rename_column + modify_column + delete_mask + 1)) {
				ati->set_delete_mask(true);
			} else if (heavy_update && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
											   rename_column + modify_column + delete_mask + heavy_update + 1)) {
				sql_query_grammar::Update *upt = ati->mutable_update();

				assert(this->entries.empty());
				for (const auto &entry : t.cols) {
					if (!dynamic_cast<NestedType*>(entry.second.tp)) {
						this->entries.push_back(InsertEntry(entry.second.special == ColumnSpecial::SIGN, entry.second.cname, std::nullopt, entry.second.tp));
					}
				}
				if (this->entries.empty()) {
					sql_query_grammar::UpdateSet *upset = upt->mutable_update();

					upset->mutable_col()->mutable_col()->set_column("c0");
					upset->mutable_expr()->mutable_lit_val()->mutable_int_lit()->set_int_lit(0);
				} else {
					const uint32_t nupdates = (rg.NextMediumNumber() % std::min<uint32_t>(static_cast<uint32_t>(this->entries.size()), UINT32_C(4))) + 1;

					std::shuffle(this->entries.begin(), this->entries.end(), rg.gen);
					for (uint32_t j = 0 ; j < nupdates; j++) {
						const InsertEntry &entry = this->entries[j];
						sql_query_grammar::ColumnPath *cp = j == 0 ? upt->mutable_update()->mutable_col() : upt->add_other_updates()->mutable_col();

						cp->mutable_col()->set_column("c" + std::to_string(entry.cname1));
						if (entry.cname2.has_value()) {
							cp->add_sub_cols()->set_column("c" + std::to_string(entry.cname2.value()));
						}
					}
					AddTableRelation(rg, true, "", t);
					this->levels[this->current_level].allow_aggregates = this->levels[this->current_level].allow_window_funcs = false;
					for (uint32_t j = 0 ; j < nupdates; j++) {
						const InsertEntry &entry = this->entries[j];
						sql_query_grammar::UpdateSet &uset = const_cast<sql_query_grammar::UpdateSet&>(j == 0 ? upt->update() : upt->other_updates(j - 1));
						sql_query_grammar::Expr *expr = uset.mutable_expr();

						if (rg.NextSmallNumber() < 9) {
							//set constant value
							sql_query_grammar::LiteralValue *lv = expr->mutable_lit_val();

							buf.resize(0);
							if (entry.is_sign) {
								buf += rg.NextBool() ? "1" : "-1";
							} else {
								StrAppendAnyValue(rg, buf, entry.tp);
							}
							lv->set_no_quote_str(buf);
						} else {
							GenerateExpression(rg, expr);
						}
					}
					this->levels[this->current_level].allow_aggregates = this->levels[this->current_level].allow_window_funcs = true;
					this->levels.clear();
					this->entries.clear();
				}

				GenerateUptDelWhere(rg, t, upt->mutable_where()->mutable_expr()->mutable_expr());
			} else if (add_stats && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
											rename_column + modify_column + delete_mask + heavy_update + add_stats + 1)) {
				sql_query_grammar::AddStatistics *ads = ati->mutable_add_stats();

				PickUpNextCols(rg, t, ads->mutable_cols());
				GenerateNextStatistics(rg, ads->mutable_stats());
			} else if (mod_stats && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
											rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats + 1)) {
				sql_query_grammar::AddStatistics *ads = ati->mutable_mod_stats();

				PickUpNextCols(rg, t, ads->mutable_cols());
				GenerateNextStatistics(rg, ads->mutable_stats());
			} else if (drop_stats && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
											 rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats +
											 drop_stats + 1)) {
				PickUpNextCols(rg, t, ati->mutable_drop_stats());
			} else if (clear_stats && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
											  rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats +
											  drop_stats + clear_stats + 1)) {
				PickUpNextCols(rg, t, ati->mutable_clear_stats());
			} else if (mat_stats && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
											rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats +
											drop_stats + clear_stats + mat_stats + 1)) {
				PickUpNextCols(rg, t, ati->mutable_mat_stats());
			} else if (add_idx && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
										  rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats +
										  drop_stats + clear_stats + mat_stats + add_idx + 1)) {
				sql_query_grammar::AddIndex *add_index = ati->mutable_add_index();

				AddTableIndex(rg, t, true, add_index->mutable_new_idx());
				if (!t.idxs.empty()) {
					const uint32_t next_option = rg.NextSmallNumber();

					if (next_option < 4) {
						const SQLIndex &oidx = rg.PickValueRandomlyFromMap(t.idxs);
						add_index->mutable_add_where()->mutable_idx()->set_index("c" + std::to_string(oidx.iname));
					} else if (next_option < 8) {
						add_index->mutable_add_where()->set_first(true);
					}
				}
			} else if (materialize_idx && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
												  rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats +
												  drop_stats + clear_stats + mat_stats + add_idx + materialize_idx + 1)) {
				const SQLIndex &idx = rg.PickValueRandomlyFromMap(t.idxs);
				ati->mutable_materialize_index()->set_index("i" + std::to_string(idx.iname));
			} else if (clear_idx && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
											rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats +
											drop_stats + clear_stats + mat_stats + add_idx + materialize_idx + clear_idx + 1)) {
				const SQLIndex &idx = rg.PickValueRandomlyFromMap(t.idxs);
				ati->mutable_clear_index()->set_index("i" + std::to_string(idx.iname));
			} else if (drop_idx && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
										   rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats +
										   drop_stats + clear_stats + mat_stats + add_idx + materialize_idx + clear_idx + drop_idx + 1)) {
				const SQLIndex &idx = rg.PickValueRandomlyFromMap(t.idxs);
				ati->mutable_drop_index()->set_index("i" + std::to_string(idx.iname));
			} else if (column_remove_property && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
														 rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats +
														 drop_stats + clear_stats + mat_stats + add_idx + materialize_idx + clear_idx + drop_idx +
														 column_remove_property + 1)) {
				sql_query_grammar::RemoveColumnProperty *rcs = ati->mutable_column_remove_property();
				const SQLColumn &col = rg.PickValueRandomlyFromMap(t.cols);

				rcs->mutable_col()->set_column("c" + std::to_string(col.cname));
				rcs->set_property(static_cast<sql_query_grammar::RemoveColumnProperty_ColumnProperties>((rg.NextRandomUInt32() % static_cast<uint32_t>(sql_query_grammar::RemoveColumnProperty::ColumnProperties_MAX)) + 1));
			} else if (column_modify_setting && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
														rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats +
														drop_stats + clear_stats + mat_stats + add_idx + materialize_idx + clear_idx + drop_idx +
														column_remove_property + column_modify_setting + 1)) {
				sql_query_grammar::ModifyColumnSetting *mcp = ati->mutable_column_modify_setting();
				const SQLColumn &col = rg.PickValueRandomlyFromMap(t.cols);

				mcp->mutable_col()->set_column("c" + std::to_string(col.cname));
				GenerateSettingValues(rg, MergeTreeColumnSettings, mcp->mutable_settings());
			} else if (column_remove_setting && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
														rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats +
														drop_stats + clear_stats + mat_stats + add_idx + materialize_idx + clear_idx + drop_idx +
														column_remove_property + column_modify_setting + column_remove_setting + 1)) {
				sql_query_grammar::RemoveColumnSetting *rcp = ati->mutable_column_remove_setting();
				const SQLColumn &col = rg.PickValueRandomlyFromMap(t.cols);

				rcp->mutable_col()->set_column("c" + std::to_string(col.cname));
				GenerateSettingList(rg, MergeTreeColumnSettings, rcp->mutable_settings());
			} else if (table_modify_setting && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
													   rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats +
													   drop_stats + clear_stats + mat_stats + add_idx + materialize_idx + clear_idx + drop_idx +
													   column_remove_property + column_modify_setting + column_remove_setting +
													   table_modify_setting + 1)) {
				GenerateSettingValues(rg, MergeTreeTableSettings, ati->mutable_table_modify_setting());
			} else if (table_remove_setting && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
													   rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats +
													   drop_stats + clear_stats + mat_stats + add_idx + materialize_idx + clear_idx + drop_idx +
													   column_remove_property + column_modify_setting + column_remove_setting +
													   table_modify_setting + table_remove_setting + 1)) {
				GenerateSettingList(rg, MergeTreeTableSettings, ati->mutable_table_remove_setting());
			} else if (add_projection && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
												 rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats +
												 drop_stats + clear_stats + mat_stats + add_idx + materialize_idx + clear_idx + drop_idx +
												 column_remove_property + column_modify_setting + column_remove_setting +
												 table_modify_setting + table_remove_setting + add_projection + 1)) {
				AddTableProjection(rg, t, true, ati->mutable_add_projection());
			} else if (remove_projection && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
													rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats +
													drop_stats + clear_stats + mat_stats + add_idx + materialize_idx + clear_idx + drop_idx +
													column_remove_property + column_modify_setting + column_remove_setting +
													table_modify_setting + table_remove_setting + add_projection + remove_projection + 1)) {
				const uint32_t &proj = rg.PickRandomlyFromSet(t.projs);
				ati->mutable_remove_projection()->set_projection("p" + std::to_string(proj));
			} else if (materialize_projection && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
														 rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats +
														 drop_stats + clear_stats + mat_stats + add_idx + materialize_idx + clear_idx + drop_idx +
														 column_remove_property + column_modify_setting + column_remove_setting +
														 table_modify_setting + table_remove_setting + add_projection + remove_projection +
														 materialize_projection + 1)) {
				const uint32_t &proj = rg.PickRandomlyFromSet(t.projs);
				ati->mutable_materialize_projection()->set_projection("p" + std::to_string(proj));
			} else if (clear_projection && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
												   rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats +
												   drop_stats + clear_stats + mat_stats + add_idx + materialize_idx + clear_idx + drop_idx +
												   column_remove_property + column_modify_setting + column_remove_setting +
												   table_modify_setting + table_remove_setting + add_projection + remove_projection +
												   materialize_projection + clear_projection + 1)) {
				const uint32_t &proj = rg.PickRandomlyFromSet(t.projs);
				ati->mutable_clear_projection()->set_projection("p" + std::to_string(proj));
			} else if (add_constraint && nopt < (heavy_delete + alter_order_by + add_column + materialize_column + drop_column +
												 rename_column + modify_column + delete_mask + heavy_update + add_stats + mod_stats +
												 drop_stats + clear_stats + mat_stats + add_idx + materialize_idx + clear_idx + drop_idx +
												 column_remove_property + column_modify_setting + column_remove_setting +
												 table_modify_setting + table_remove_setting + add_projection + remove_projection +
												 materialize_projection + clear_projection + add_constraint + 1)) {
				AddTableConstraint(rg, t, true, ati->mutable_add_constraint());
			} else {
				const uint32_t &constr = rg.PickRandomlyFromSet(t.constrs);
				ati->mutable_remove_constraint()->set_constraint("c" + std::to_string(constr));
			}
		}
	} else {
		assert(0);
	}
	return 0;
}

int StatementGenerator::GenerateAttach(RandomGenerator &rg, sql_query_grammar::Attach *att) {
	sql_query_grammar::SchemaOrTable *sot = att->mutable_object();
	const uint32_t attach_table = 10 * static_cast<uint32_t>(CollectionHas<SQLTable>(detached_tables)),
				   attach_view = 10 * static_cast<uint32_t>(CollectionHas<SQLView>(detached_views)),
				   attach_database = 2 * static_cast<uint32_t>(CollectionHas<std::shared_ptr<SQLDatabase>>(detached_databases)),
				   prob_space = attach_table + attach_view + attach_database;
	std::uniform_int_distribution<uint32_t> next_dist(1, prob_space);
	const uint32_t nopt = next_dist(rg.gen);

	if (attach_table && nopt < (attach_table + 1)) {
		sql_query_grammar::ExprSchemaTable *est = sot->mutable_est();
		const SQLTable &t = rg.PickRandomlyFromVector(FilterCollection<SQLTable>(detached_tables));

		att->set_sobject(sql_query_grammar::SQLObject::TABLE);
		est->mutable_database()->set_database("d" + std::to_string(t.db->dname));
		est->mutable_table()->set_table("t" + std::to_string(t.tname));
	} else if (attach_view && nopt < (attach_table + attach_view + 1)) {
		sql_query_grammar::ExprSchemaTable *est = sot->mutable_est();
		const SQLView &v = rg.PickRandomlyFromVector(FilterCollection<SQLView>(detached_views));

		att->set_sobject(sql_query_grammar::SQLObject::TABLE);
		est->mutable_database()->set_database("d" + std::to_string(v.db->dname));
		est->mutable_table()->set_table("v" + std::to_string(v.vname));
	} else if (attach_database) {
		const std::shared_ptr<SQLDatabase> &d = rg.PickRandomlyFromVector(FilterCollection<std::shared_ptr<SQLDatabase>>(detached_databases));

		att->set_sobject(sql_query_grammar::SQLObject::DATABASE);
		sot->mutable_database()->set_database("d" + std::to_string(d->dname));
	} else {
		assert(0);
	}
	return 0;
}

int StatementGenerator::GenerateDetach(RandomGenerator &rg, sql_query_grammar::Detach *det) {
	sql_query_grammar::SchemaOrTable *sot = det->mutable_object();
	const uint32_t detach_table = 10 * static_cast<uint32_t>(CollectionHas<SQLTable>(attached_tables)),
				   detach_view = 10 * static_cast<uint32_t>(CollectionHas<SQLView>(attached_views)),
				   detach_database = 2 * static_cast<uint32_t>(CollectionHas<std::shared_ptr<SQLDatabase>>(attached_databases)),
				   prob_space = detach_table + detach_view + detach_database;
	std::uniform_int_distribution<uint32_t> next_dist(1, prob_space);
	const uint32_t nopt = next_dist(rg.gen);

	if (detach_table && nopt < (detach_table + 1)) {
		sql_query_grammar::ExprSchemaTable *est = sot->mutable_est();
		const SQLTable &t = rg.PickRandomlyFromVector(FilterCollection<SQLTable>(attached_tables));

		det->set_sobject(sql_query_grammar::SQLObject::TABLE);
		est->mutable_database()->set_database("d" + std::to_string(t.db->dname));
		est->mutable_table()->set_table("t" + std::to_string(t.tname));
	} else if (detach_view && nopt < (detach_table + detach_view + 1)) {
		sql_query_grammar::ExprSchemaTable *est = sot->mutable_est();
		const SQLView &v = rg.PickRandomlyFromVector(FilterCollection<SQLView>(attached_views));

		det->set_sobject(sql_query_grammar::SQLObject::TABLE);
		est->mutable_database()->set_database("d" + std::to_string(v.db->dname));
		est->mutable_table()->set_table("v" + std::to_string(v.vname));
	} else if (detach_database) {
		const std::shared_ptr<SQLDatabase> &d = rg.PickRandomlyFromVector(FilterCollection<std::shared_ptr<SQLDatabase>>(attached_databases));

		det->set_sobject(sql_query_grammar::SQLObject::DATABASE);
		sot->mutable_database()->set_database("d" + std::to_string(d->dname));
	} else {
		assert(0);
	}
	det->set_permanentely(rg.NextBool());
	det->set_sync(rg.NextBool());
	return 0;
}

int StatementGenerator::GenerateNextQuery(RandomGenerator &rg, sql_query_grammar::SQLQueryInner *sq) {
	const uint32_t create_table = 6 * static_cast<uint32_t>(CollectionHas<std::shared_ptr<SQLDatabase>>(attached_databases) &&
															tables.size() < this->max_tables),
				   create_view = 10 * static_cast<uint32_t>(CollectionHas<std::shared_ptr<SQLDatabase>>(attached_databases) &&
															views.size() < this->max_views),
				   drop = 1 * static_cast<uint32_t>(CollectionHas<SQLTable>(attached_tables) ||
													CollectionHas<SQLView>(attached_views) ||
													CollectionHas<std::shared_ptr<SQLDatabase>>(attached_databases)),
				   insert = 100 * static_cast<uint32_t>(CollectionHas<SQLTable>(attached_tables)),
				   light_delete = 6 * static_cast<uint32_t>(CollectionHas<SQLTable>(attached_tables)),
				   truncate = 2 * static_cast<uint32_t>(CollectionHas<SQLTable>(attached_tables)),
				   optimize_table = 2 * static_cast<uint32_t>(CollectionHas<SQLTable>(attached_tables)),
				   check_table = 2 * static_cast<uint32_t>(CollectionHas<SQLTable>(attached_tables)),
				   desc_table = 2 * static_cast<uint32_t>(CollectionHas<SQLTable>(attached_tables) ||
														  CollectionHas<SQLView>(attached_views)),
				   exchange_tables = 1 * static_cast<uint32_t>(CollectionCount<SQLTable>(attached_tables) > 1),
				   alter_table = 6 * static_cast<uint32_t>(CollectionHas<SQLTable>(attached_tables) ||
														   CollectionHas<SQLView>(attached_views)),
				   set_values = 5,
				   attach = 2 * static_cast<uint32_t>(CollectionHas<SQLTable>(detached_tables) ||
													  CollectionHas<SQLView>(detached_views) ||
													  CollectionHas<std::shared_ptr<SQLDatabase>>(detached_databases)),
				   dettach = 2 * static_cast<uint32_t>(CollectionHas<SQLTable>(attached_tables) ||
													   CollectionHas<SQLView>(attached_views) ||
													   CollectionHas<std::shared_ptr<SQLDatabase>>(attached_databases)),
				   create_database = 2 * static_cast<uint32_t>(databases.size() < this->max_databases),
				   select_query = 300,
				   prob_space = create_table + create_view + drop + insert + light_delete + truncate + optimize_table +
								check_table + desc_table + exchange_tables + alter_table + set_values + attach +
								dettach + create_database + select_query;
	std::uniform_int_distribution<uint32_t> next_dist(1, prob_space);
	const uint32_t nopt = next_dist(rg.gen);

	assert(this->ids.empty());
	if (create_table && nopt < (create_table + 1)) {
		return GenerateNextCreateTable(rg, sq->mutable_create_table());
	} else if (create_view && nopt < (create_table + create_view + 1)) {
		return GenerateNextCreateView(rg, sq->mutable_create_view());
	} else if (drop && nopt < (create_table + create_view + drop + 1)) {
		return GenerateNextDrop(rg, sq->mutable_drop());
	} else if (insert && nopt < (create_table + create_view + drop + insert + 1)) {
		return GenerateNextInsert(rg, sq->mutable_insert());
	} else if (light_delete && nopt < (create_table + create_view + drop + insert + light_delete + 1)) {
		return GenerateNextDelete(rg, sq->mutable_del());
	} else if (truncate && nopt < (create_table + create_view + drop + insert + light_delete + truncate + 1)) {
		return GenerateNextTruncate(rg, sq->mutable_trunc());
	} else if (optimize_table && nopt < (create_table + create_view + drop + insert + light_delete + truncate + optimize_table + 1)) {
		return GenerateNextOptimizeTable(rg, sq->mutable_opt());
	} else if (check_table && nopt < (create_table + create_view + drop + insert + light_delete + truncate + optimize_table + check_table + 1)) {
		return GenerateNextCheckTable(rg, sq->mutable_check());
	} else if (desc_table && nopt < (create_table + create_view + drop + insert + light_delete + truncate + optimize_table + check_table +
									 desc_table + 1)) {
		return GenerateNextDescTable(rg, sq->mutable_desc());
	} else if (exchange_tables && nopt < (create_table + create_view + drop + insert + light_delete + truncate + optimize_table + check_table +
										  desc_table + exchange_tables + 1)) {
		return GenerateNextExchangeTables(rg, sq->mutable_exchange());
	} else if (alter_table && nopt < (create_table + create_view + drop + insert + light_delete + truncate + optimize_table + check_table +
									  desc_table + exchange_tables + alter_table + 1)) {
		return GenerateAlterTable(rg, sq->mutable_alter_table());
	} else if (set_values && nopt < (create_table + create_view + drop + insert + light_delete + truncate + optimize_table + check_table +
									 desc_table + exchange_tables + alter_table + set_values + 1)) {
		return GenerateSettingValues(rg, ServerSettings, sq->mutable_setting_values());
	} else if (attach && nopt < (create_table + create_view + drop + insert + light_delete + truncate + optimize_table + check_table +
								 desc_table + exchange_tables + alter_table + set_values + attach + 1)) {
		return GenerateAttach(rg, sq->mutable_attach());
	} else if (dettach && nopt < (create_table + create_view + drop + insert + light_delete + truncate + optimize_table + check_table +
								  desc_table + exchange_tables + alter_table + set_values + attach + dettach + 1)) {
		return GenerateDetach(rg, sq->mutable_detach());
	} else if (create_database && nopt < (create_table + create_view + drop + insert + light_delete + truncate + optimize_table + check_table +
										  desc_table + exchange_tables + alter_table + set_values + attach + dettach + create_database + 1)) {
		return GenerateNextCreateDatabase(rg, sq->mutable_create_database());
	}
	return GenerateTopSelect(rg, sq->mutable_select());
}

int StatementGenerator::GenerateNextExplain(RandomGenerator &rg, sql_query_grammar::ExplainQuery *eq) {

	if (rg.NextSmallNumber() < 10) {
		eq->set_expl(static_cast<sql_query_grammar::ExplainQuery_ExplainValues>((rg.NextRandomUInt32() % static_cast<uint32_t>(sql_query_grammar::ExplainQuery::ExplainValues_MAX)) + 1));
	}
	return GenerateNextQuery(rg, eq->mutable_inner_query());
}

int StatementGenerator::GenerateNextStatement(RandomGenerator &rg, sql_query_grammar::SQLQuery &sq) {
	const uint32_t noption = rg.NextMediumNumber();

	if (noption < 11) {
		return GenerateNextExplain(rg, sq.mutable_explain());
	} /*else if (this->in_transaction && noption < 41) {
		if (rg.NextBool()) {
			sq.set_commit_trans(true);
		} else {
			sq.set_rollback_trans(true);
		}
		this->in_transaction = false;
		return 0;
	} else if (!this->in_transaction && noption < 16) {
		sq.set_start_trans(true);
		this->in_transaction = true;
		return 0;
	}*/ else {
		return GenerateNextQuery(rg, sq.mutable_inner_query());
	}
}

void StatementGenerator::UpdateGenerator(const sql_query_grammar::SQLQuery &sq, const bool success) {
	const sql_query_grammar::SQLQueryInner &query = sq.has_inner_query() ? sq.inner_query() : sq.explain().inner_query();

	if (sq.has_inner_query() && query.has_create_table()) {
		const uint32_t tname = static_cast<uint32_t>(std::stoul(query.create_table().est().table().table().substr(1)));

		if (success) {
			if (query.create_table().replace()) {
				this->tables.erase(tname);
			}
			this->tables[tname] = std::move(this->staged_tables[tname]);
		}
		this->staged_tables.erase(tname);
	} else if (sq.has_inner_query() && query.has_create_view()) {
		const uint32_t vname = static_cast<uint32_t>(std::stoul(query.create_view().est().table().table().substr(1)));

		if (success) {
			if (query.create_view().replace()) {
				this->views.erase(vname);
			}
			this->views[vname] = std::move(this->staged_views[vname]);
		}
		this->staged_views.erase(vname);
	} else if (sq.has_inner_query() && query.has_drop() && success) {
		const sql_query_grammar::Drop &drp = query.drop();

		if (drp.sobject() == sql_query_grammar::SQLObject::TABLE) {
			this->tables.erase(static_cast<uint32_t>(std::stoul(drp.object().est().table().table().substr(1))));
		} else if (drp.sobject() == sql_query_grammar::SQLObject::VIEW) {
			this->views.erase(static_cast<uint32_t>(std::stoul(drp.object().est().table().table().substr(1))));
		} else if (drp.sobject() == sql_query_grammar::SQLObject::DATABASE) {
			const uint32_t dname = static_cast<uint32_t>(std::stoul(drp.object().database().database().substr(1)));

			for (auto it = this->tables.cbegin(); it != this->tables.cend();) {
				if (it->second.db->dname == dname) {
					this->tables.erase(it++);
				} else {
					++it;
				}
			}
			for (auto it = this->views.cbegin(); it != this->views.cend();) {
				if (it->second.db->dname == dname) {
					this->views.erase(it++);
				} else {
					++it;
				}
			}
			this->databases.erase(dname);
		}
	} else if (sq.has_inner_query() && query.has_exchange() && success) {
		const uint32_t tname1 = static_cast<uint32_t>(std::stoul(query.exchange().est1().table().table().substr(1))),
					   tname2 = static_cast<uint32_t>(std::stoul(query.exchange().est2().table().table().substr(1)));
		SQLTable tx = std::move(this->tables[tname1]), ty = std::move(this->tables[tname2]);

		tx.tname = tname2;
		ty.tname = tname1;
		this->tables[tname2] = std::move(tx);
		this->tables[tname1] = std::move(ty);
	} else if (sq.has_inner_query() && query.has_alter_table()) {
		const sql_query_grammar::AlterTable &at = sq.inner_query().alter_table();
		const bool isview = at.est().table().table()[0] == 'v';
		const uint32_t tname = static_cast<uint32_t>(std::stoul(at.est().table().table().substr(1)));

		if (isview) {
			SQLView &v = this->views[tname];

			for (int i = 0 ; i < at.other_alters_size() + 1; i++) {
				const sql_query_grammar::AlterTableItem &ati = i == 0 ? at.alter() : at.other_alters(i - 1);

				if (success && ati.has_add_column()) {
					v.ncols = v.staged_ncols;
				}
			}
		} else {
			SQLTable &t = this->tables[tname];

			for (int i = 0 ; i < at.other_alters_size() + 1; i++) {
				const sql_query_grammar::AlterTableItem &ati = i == 0 ? at.alter() : at.other_alters(i - 1);

				assert(!ati.has_modify_query() && !ati.has_refresh());
				if (ati.has_add_column()) {
					const uint32_t cname = static_cast<uint32_t>(std::stoul(ati.add_column().new_col().col().column().substr(1)));

					if (success) {
						t.cols[cname] = std::move(t.staged_cols[cname]);
					}
					t.staged_cols.erase(cname);
				} else if (ati.has_drop_column() && success) {
					const uint32_t cname = static_cast<uint32_t>(std::stoul(ati.drop_column().column().substr(1)));

					t.cols.erase(cname);
				} else if (ati.has_rename_column() && success) {
					const uint32_t old_cname = static_cast<uint32_t>(std::stoul(ati.rename_column().old_name().column().substr(1))),
								   new_cname = static_cast<uint32_t>(std::stoul(ati.rename_column().new_name().column().substr(1)));

					t.cols[new_cname] = std::move(t.cols[old_cname]);
					t.cols[new_cname].cname = new_cname;
					t.cols.erase(old_cname);
				} else if (ati.has_modify_column()) {
					const uint32_t cname = static_cast<uint32_t>(std::stoul(ati.modify_column().new_col().col().column().substr(1)));

					if (success) {
						t.cols.erase(cname);
						t.cols[cname] = std::move(t.staged_cols[cname]);
					}
					t.staged_cols.erase(cname);
				} else if (ati.has_add_index()) {
					const uint32_t cname = static_cast<uint32_t>(std::stoul(ati.add_index().new_idx().idx().index().substr(1)));

					if (success) {
						t.idxs[cname] = std::move(t.staged_idxs[cname]);
					}
					t.staged_idxs.erase(cname);
				} else if (ati.has_drop_index() && success) {
					const uint32_t cname = static_cast<uint32_t>(std::stoul(ati.drop_index().index().substr(1)));

					t.idxs.erase(cname);
				} else if (ati.has_add_projection()) {
					const uint32_t pname = static_cast<uint32_t>(std::stoul(ati.add_projection().proj().projection().substr(1)));

					if (success) {
						t.projs.insert(pname);
					}
					t.staged_projs.erase(pname);
				} else if (ati.has_remove_projection() && success) {
					const uint32_t pname = static_cast<uint32_t>(std::stoul(ati.remove_projection().projection().substr(1)));

					t.projs.erase(pname);
				} else if (ati.has_add_constraint()) {
					const uint32_t pname = static_cast<uint32_t>(std::stoul(ati.add_constraint().constr().constraint().substr(1)));

					if (success) {
						t.constrs.insert(pname);
					}
					t.staged_constrs.erase(pname);
				} else if (ati.has_remove_constraint() && success) {
					const uint32_t pname = static_cast<uint32_t>(std::stoul(ati.remove_constraint().constraint().substr(1)));

					t.constrs.erase(pname);
				}
			}
		}
	} else if (sq.has_inner_query() && query.has_attach() && success) {
		const sql_query_grammar::Attach &att = sq.inner_query().attach();
		const bool istable = att.object().has_est() && att.object().est().table().table()[0] == 't',
				   isview = att.object().has_est() && att.object().est().table().table()[0] == 'v',
				   isdatabase = att.object().has_database();

		if (isview) {
			this->views[static_cast<uint32_t>(std::stoul(att.object().est().table().table().substr(1)))].attached = true;
		} else if (istable) {
			this->tables[static_cast<uint32_t>(std::stoul(att.object().est().table().table().substr(1)))].attached = true;
		} else if (isdatabase) {
			this->databases[static_cast<uint32_t>(std::stoul(att.object().database().database().substr(1)))]->attached = true;
		}
	} else if (sq.has_inner_query() && query.has_detach() && success) {
		const sql_query_grammar::Detach &det = sq.inner_query().detach();
		const bool istable = det.object().has_est() && det.object().est().table().table()[0] == 't',
				   isview = det.object().has_est() && det.object().est().table().table()[0] == 'v',
				   isdatabase = det.object().has_database();

		if (isview) {
			this->views[static_cast<uint32_t>(std::stoul(det.object().est().table().table().substr(1)))].attached = false;
		} else if (istable) {
			this->tables[static_cast<uint32_t>(std::stoul(det.object().est().table().table().substr(1)))].attached = false;
		} else if (isdatabase) {
			this->databases[static_cast<uint32_t>(std::stoul(det.object().database().database().substr(1)))]->attached = false;
		}
	} else if (sq.has_inner_query() && query.has_create_database()) {
		const uint32_t dname = static_cast<uint32_t>(std::stoul(query.create_database().database().database().substr(1)));

		if (success) {
			this->databases[dname] = std::move(this->staged_databases[dname]);
		}
		this->staged_databases.erase(dname);
	}
}

}
