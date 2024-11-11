#include "query_oracle.h"

#include <cstdio>

namespace buzzhouse
{

/*
Correctness query oracle
*/
/*
SELECT COUNT(*) FROM <FROM_CLAUSE> WHERE <PRED>;
or
SELECT COUNT(*) FROM <FROM_CLAUSE> WHERE <PRED1> GROUP BY <GROUP_BY CLAUSE> HAVING <PRED2>;
*/
int QueryOracle::GenerateCorrectnessTestFirstQuery(RandomGenerator & rg, StatementGenerator & gen, sql_query_grammar::SQLQuery & sq1)
{
    const std::filesystem::path & qfile = fc.db_file_path / "query.data";
    sql_query_grammar::TopSelect * ts = sq1.mutable_inner_query()->mutable_select();
    sql_query_grammar::SelectIntoFile * sif = ts->mutable_intofile();
    sql_query_grammar::SelectStatementCore * ssc = ts->mutable_sel()->mutable_select_core();
    const uint32_t combination = 0; //TODO fix this rg.NextLargeNumber() % 3; /* 0 WHERE, 1 HAVING, 2 WHERE + HAVING */

    gen.SetAllowNotDetermistic(false);
    gen.EnforceFinal(true);
    gen.levels[gen.current_level] = QueryLevel(gen.current_level);
    gen.GenerateFromStatement(rg, std::numeric_limits<uint32_t>::max(), ssc->mutable_from());

    const bool prev_allow_aggregates = gen.levels[gen.current_level].allow_aggregates,
               prev_allow_window_funcs = gen.levels[gen.current_level].allow_window_funcs;
    gen.levels[gen.current_level].allow_aggregates = gen.levels[gen.current_level].allow_window_funcs = false;
    if (combination != 1)
    {
        sql_query_grammar::BinaryExpr * bexpr
            = ssc->mutable_where()->mutable_expr()->mutable_expr()->mutable_comp_expr()->mutable_binary_expr();

        bexpr->set_op(sql_query_grammar::BinaryOperator::BINOP_EQ);
        bexpr->mutable_rhs()->mutable_lit_val()->set_special_val(sql_query_grammar::SpecialVal::VAL_TRUE);
        gen.GenerateWherePredicate(rg, bexpr->mutable_lhs());
    }
    if (combination != 0)
    {
        gen.GenerateGroupBy(rg, 1, true, true, ssc->mutable_groupby());
    }
    gen.levels[gen.current_level].allow_aggregates = prev_allow_aggregates;
    gen.levels[gen.current_level].allow_window_funcs = prev_allow_window_funcs;

    ssc->add_result_columns()->mutable_eca()->mutable_expr()->mutable_comp_expr()->mutable_func_call()->mutable_func()->set_catalog_func(
        sql_query_grammar::FUNCcount);
    gen.levels.erase(gen.current_level);
    gen.SetAllowNotDetermistic(true);
    gen.EnforceFinal(false);

    ts->set_format(sql_query_grammar::OutFormat::OUT_CSV);
    sif->set_path(qfile.generic_string());
    sif->set_step(sql_query_grammar::SelectIntoFile_SelectIntoFileStep::SelectIntoFile_SelectIntoFileStep_TRUNCATE);
    return 0;
}

/*
SELECT ifNull(SUM(PRED),0) FROM <FROM_CLAUSE>;
or
SELECT ifNull(SUM(PRED2),0) FROM <FROM_CLAUSE> WHERE <PRED1> GROUP BY <GROUP_BY CLAUSE>;
*/
int QueryOracle::GenerateCorrectnessTestSecondQuery(sql_query_grammar::SQLQuery & sq1, sql_query_grammar::SQLQuery & sq2)
{
    const std::filesystem::path & qfile = fc.db_file_path / "query.data";
    sql_query_grammar::TopSelect * ts = sq2.mutable_inner_query()->mutable_select();
    sql_query_grammar::SelectIntoFile * sif = ts->mutable_intofile();
    sql_query_grammar::SelectStatementCore & ssc1
        = const_cast<sql_query_grammar::SelectStatementCore &>(sq1.inner_query().select().sel().select_core());
    sql_query_grammar::SelectStatementCore * ssc2 = ts->mutable_sel()->mutable_select_core();
    sql_query_grammar::SQLFuncCall * sfc1
        = ssc2->add_result_columns()->mutable_eca()->mutable_expr()->mutable_comp_expr()->mutable_func_call();
    sql_query_grammar::SQLFuncCall * sfc2 = sfc1->add_args()->mutable_expr()->mutable_comp_expr()->mutable_func_call();

    sfc1->mutable_func()->set_catalog_func(sql_query_grammar::FUNCifNull);
    sfc1->add_args()->mutable_expr()->mutable_lit_val()->set_special_val(sql_query_grammar::SpecialVal::VAL_ZERO);
    sfc2->mutable_func()->set_catalog_func(sql_query_grammar::FUNCsum);

    ssc2->set_allocated_from(ssc1.release_from());
    if (ssc1.has_groupby())
    {
        sql_query_grammar::GroupByStatement & gbs = const_cast<sql_query_grammar::GroupByStatement &>(ssc1.groupby());

        sfc2->add_args()->set_allocated_expr(gbs.release_having_expr());
        ssc2->set_allocated_groupby(ssc1.release_groupby());
        ssc2->set_allocated_where(ssc1.release_where());
    }
    else
    {
        sql_query_grammar::ExprComparisonHighProbability & expr
            = const_cast<sql_query_grammar::ExprComparisonHighProbability &>(ssc1.where().expr());

        sfc2->add_args()->set_allocated_expr(expr.release_expr());
    }
    ts->set_format(sql_query_grammar::OutFormat::OUT_CSV);
    sif->set_path(qfile.generic_string());
    sif->set_step(sql_query_grammar::SelectIntoFile_SelectIntoFileStep::SelectIntoFile_SelectIntoFileStep_TRUNCATE);
    return 0;
}

/*
Dump and read table oracle
*/
int QueryOracle::DumpTableContent(RandomGenerator & rg, const SQLTable & t, sql_query_grammar::SQLQuery & sq1)
{
    bool first = true;
    const std::filesystem::path & qfile = fc.db_file_path / "query.data";
    sql_query_grammar::TopSelect * ts = sq1.mutable_inner_query()->mutable_select();
    sql_query_grammar::SelectIntoFile * sif = ts->mutable_intofile();
    sql_query_grammar::SelectStatementCore * sel = ts->mutable_sel()->mutable_select_core();
    sql_query_grammar::JoinedTable * jt = sel->mutable_from()->mutable_tos()->mutable_join_clause()->mutable_tos()->mutable_joined_table();
    sql_query_grammar::OrderByList * obs = sel->mutable_orderby()->mutable_olist();
    sql_query_grammar::ExprSchemaTable * est = jt->mutable_est();

    if (t.db)
    {
        est->mutable_database()->set_database("d" + std::to_string(t.db->dname));
    }
    est->mutable_table()->set_table("t" + std::to_string(t.tname));
    jt->set_final(t.SupportsFinal());
    for (const auto & entry : t.cols)
    {
        if (entry.second.CanBeInserted())
        {
            const std::string & cname = "c" + std::to_string(entry.first);
            sql_query_grammar::ExprOrderingTerm * eot = first ? obs->mutable_ord_term() : obs->add_extra_ord_terms();

            sel->add_result_columns()->mutable_etc()->mutable_col()->mutable_col()->set_column(cname);
            eot->mutable_expr()->mutable_comp_expr()->mutable_expr_stc()->mutable_col()->mutable_col()->set_column(cname);
            if (rg.NextBool())
            {
                eot->set_asc_desc(
                    rg.NextBool() ? sql_query_grammar::ExprOrderingTerm_AscDesc::ExprOrderingTerm_AscDesc_ASC
                                  : sql_query_grammar::ExprOrderingTerm_AscDesc::ExprOrderingTerm_AscDesc_DESC);
            }
            first = false;
        }
    }
    ts->set_format(sql_query_grammar::OutFormat::OUT_CSV);
    sif->set_path(qfile.generic_string());
    sif->set_step(sql_query_grammar::SelectIntoFile_SelectIntoFileStep::SelectIntoFile_SelectIntoFileStep_TRUNCATE);
    return 0;
}

static const std::map<sql_query_grammar::OutFormat, sql_query_grammar::InFormat> out_in{
    {sql_query_grammar::OutFormat::OUT_TabSeparated, sql_query_grammar::InFormat::IN_TabSeparated},
    {sql_query_grammar::OutFormat::OUT_TabSeparatedWithNames, sql_query_grammar::InFormat::IN_TabSeparatedWithNames},
    {sql_query_grammar::OutFormat::OUT_TabSeparatedWithNamesAndTypes, sql_query_grammar::InFormat::IN_TabSeparatedWithNamesAndTypes},
    {sql_query_grammar::OutFormat::OUT_CSV, sql_query_grammar::InFormat::IN_CSV},
    {sql_query_grammar::OutFormat::OUT_CSVWithNames, sql_query_grammar::InFormat::IN_CSVWithNames},
    {sql_query_grammar::OutFormat::OUT_CSVWithNamesAndTypes, sql_query_grammar::InFormat::IN_CSVWithNamesAndTypes},
    {sql_query_grammar::OutFormat::OUT_CustomSeparated, sql_query_grammar::InFormat::IN_CustomSeparated},
    {sql_query_grammar::OutFormat::OUT_CustomSeparatedWithNames, sql_query_grammar::InFormat::IN_CustomSeparatedWithNames},
    {sql_query_grammar::OutFormat::OUT_CustomSeparatedWithNamesAndTypes, sql_query_grammar::InFormat::IN_CustomSeparatedWithNamesAndTypes},
    {sql_query_grammar::OutFormat::OUT_Values, sql_query_grammar::InFormat::IN_Values},
    {sql_query_grammar::OutFormat::OUT_JSON, sql_query_grammar::InFormat::IN_JSON},
    {sql_query_grammar::OutFormat::OUT_JSONColumns, sql_query_grammar::InFormat::IN_JSONColumns},
    {sql_query_grammar::OutFormat::OUT_JSONColumnsWithMetadata, sql_query_grammar::InFormat::IN_JSONColumnsWithMetadata},
    {sql_query_grammar::OutFormat::OUT_JSONCompact, sql_query_grammar::InFormat::IN_JSONCompact},
    {sql_query_grammar::OutFormat::OUT_JSONCompactColumns, sql_query_grammar::InFormat::IN_JSONCompactColumns},
    {sql_query_grammar::OutFormat::OUT_JSONEachRow, sql_query_grammar::InFormat::IN_JSONEachRow},
    {sql_query_grammar::OutFormat::OUT_JSONStringsEachRow, sql_query_grammar::InFormat::IN_JSONStringsEachRow},
    {sql_query_grammar::OutFormat::OUT_JSONCompactEachRow, sql_query_grammar::InFormat::IN_JSONCompactEachRow},
    {sql_query_grammar::OutFormat::OUT_JSONCompactEachRowWithNames, sql_query_grammar::InFormat::IN_JSONCompactEachRowWithNames},
    {sql_query_grammar::OutFormat::OUT_JSONCompactEachRowWithNamesAndTypes,
     sql_query_grammar::InFormat::IN_JSONCompactEachRowWithNamesAndTypes},
    {sql_query_grammar::OutFormat::OUT_JSONCompactStringsEachRow, sql_query_grammar::InFormat::IN_JSONCompactStringsEachRow},
    {sql_query_grammar::OutFormat::OUT_JSONCompactStringsEachRowWithNames,
     sql_query_grammar::InFormat::IN_JSONCompactStringsEachRowWithNames},
    {sql_query_grammar::OutFormat::OUT_JSONCompactStringsEachRowWithNamesAndTypes,
     sql_query_grammar::InFormat::IN_JSONCompactStringsEachRowWithNamesAndTypes},
    {sql_query_grammar::OutFormat::OUT_JSONObjectEachRow, sql_query_grammar::InFormat::IN_JSONObjectEachRow},
    {sql_query_grammar::OutFormat::OUT_BSONEachRow, sql_query_grammar::InFormat::IN_BSONEachRow},
    {sql_query_grammar::OutFormat::OUT_TSKV, sql_query_grammar::InFormat::IN_TSKV},
    {sql_query_grammar::OutFormat::OUT_Protobuf, sql_query_grammar::InFormat::IN_Protobuf},
    {sql_query_grammar::OutFormat::OUT_ProtobufSingle, sql_query_grammar::InFormat::IN_ProtobufSingle},
    //{sql_query_grammar::OutFormat::OUT_ProtobufList, sql_query_grammar::InFormat::IN_ProtobufList},
    {sql_query_grammar::OutFormat::OUT_Avro, sql_query_grammar::InFormat::IN_Avro},
    {sql_query_grammar::OutFormat::OUT_Parquet, sql_query_grammar::InFormat::IN_Parquet},
    {sql_query_grammar::OutFormat::OUT_Arrow, sql_query_grammar::InFormat::IN_Arrow},
    {sql_query_grammar::OutFormat::OUT_ArrowStream, sql_query_grammar::InFormat::IN_ArrowStream},
    {sql_query_grammar::OutFormat::OUT_ORC, sql_query_grammar::InFormat::IN_ORC},
    {sql_query_grammar::OutFormat::OUT_Npy, sql_query_grammar::InFormat::IN_Npy},
    {sql_query_grammar::OutFormat::OUT_RowBinary, sql_query_grammar::InFormat::IN_RowBinary},
    {sql_query_grammar::OutFormat::OUT_RowBinaryWithNames, sql_query_grammar::InFormat::IN_RowBinaryWithNames},
    {sql_query_grammar::OutFormat::OUT_RowBinaryWithNamesAndTypes, sql_query_grammar::InFormat::IN_RowBinaryWithNamesAndTypes},
    {sql_query_grammar::OutFormat::OUT_Native, sql_query_grammar::InFormat::IN_Native},
    //{sql_query_grammar::OutFormat::OUT_RawBLOB, sql_query_grammar::InFormat::IN_RawBLOB}, outputs as a single value
    {sql_query_grammar::OutFormat::OUT_MsgPack, sql_query_grammar::InFormat::IN_MsgPack}};

int QueryOracle::GenerateExportQuery(RandomGenerator & rg, const SQLTable & t, sql_query_grammar::SQLQuery & sq2)
{
    bool first = true;
    sql_query_grammar::Insert * ins = sq2.mutable_inner_query()->mutable_insert();
    sql_query_grammar::FileFunc * ff = ins->mutable_tfunction()->mutable_file();
    sql_query_grammar::SelectStatementCore * sel = ins->mutable_select()->mutable_select_core();
    const std::filesystem::path & nfile = fc.db_file_path / "table.data";
    sql_query_grammar::OutFormat outf = rg.PickKeyRandomlyFromMap(out_in);

    if (std::filesystem::exists(nfile))
    {
        std::remove(nfile.generic_string().c_str()); //remove the file
    }
    ff->set_path(nfile.generic_string());

    buf.resize(0);
    for (const auto & entry : t.cols)
    {
        if (entry.second.CanBeInserted())
        {
            const std::string & cname = "c" + std::to_string(entry.first);

            if (!first)
            {
                buf += ", ";
            }
            buf += cname;
            buf += " ";
            entry.second.tp->TypeName(buf, true);
            if (entry.second.nullable.has_value())
            {
                buf += entry.second.nullable.value() ? "" : " NOT";
                buf += " NULL";
            }
            sel->add_result_columns()->mutable_etc()->mutable_col()->mutable_col()->set_column(std::move(cname));
            /* ArrowStream doesn't support UUID */
            if (outf == sql_query_grammar::OutFormat::OUT_ArrowStream && dynamic_cast<const UUIDType *>(entry.second.tp))
            {
                outf = sql_query_grammar::OutFormat::OUT_CSV;
            }
            first = false;
        }
    }
    ff->set_outformat(outf);
    ff->set_structure(buf);
    if (rg.NextSmallNumber() < 4)
    {
        ff->set_fcomp(static_cast<sql_query_grammar::FileCompression>(
            (rg.NextRandomUInt32() % static_cast<uint32_t>(sql_query_grammar::FileCompression_MAX)) + 1));
    }

    //Set the table on select
    sql_query_grammar::JoinedTable * jt = sel->mutable_from()->mutable_tos()->mutable_join_clause()->mutable_tos()->mutable_joined_table();
    sql_query_grammar::ExprSchemaTable * est = jt->mutable_est();

    if (t.db)
    {
        est->mutable_database()->set_database("d" + std::to_string(t.db->dname));
    }
    est->mutable_table()->set_table("t" + std::to_string(t.tname));
    jt->set_final(t.SupportsFinal());
    return 0;
}

int QueryOracle::GenerateClearQuery(const SQLTable & t, sql_query_grammar::SQLQuery & sq3)
{
    sql_query_grammar::Truncate * trunc = sq3.mutable_inner_query()->mutable_trunc();
    sql_query_grammar::ExprSchemaTable * est = trunc->mutable_est();

    if (t.db)
    {
        est->mutable_database()->set_database("d" + std::to_string(t.db->dname));
    }
    est->mutable_table()->set_table("t" + std::to_string(t.tname));
    return 0;
}

int QueryOracle::GenerateImportQuery(const SQLTable & t, const sql_query_grammar::SQLQuery & sq2, sql_query_grammar::SQLQuery & sq4)
{
    sql_query_grammar::Insert * ins = sq4.mutable_inner_query()->mutable_insert();
    sql_query_grammar::InsertIntoTable * iit = ins->mutable_itable();
    sql_query_grammar::InsertFromFile * iff = ins->mutable_ffile();
    const sql_query_grammar::FileFunc & ff = sq2.inner_query().insert().tfunction().file();
    sql_query_grammar::ExprSchemaTable * est = iit->mutable_est();

    if (t.db)
    {
        est->mutable_database()->set_database("d" + std::to_string(t.db->dname));
    }
    est->mutable_table()->set_table("t" + std::to_string(t.tname));
    for (const auto & entry : t.cols)
    {
        if (entry.second.CanBeInserted())
        {
            sql_query_grammar::ColumnPath * cp = iit->add_cols();

            cp->mutable_col()->set_column("c" + std::to_string(entry.first));
        }
    }
    iff->set_path(ff.path());
    iff->set_format(out_in.at(ff.outformat()));
    if (ff.has_fcomp())
    {
        iff->set_fcomp(ff.fcomp());
    }
    if (iff->format() == sql_query_grammar::IN_CSV)
    {
        sql_query_grammar::SettingValues * vals = iff->mutable_settings();
        sql_query_grammar::SetValue * sv = vals->mutable_set_value();

        sv->set_property("input_format_csv_detect_header");
        sv->set_value("0");
    }
    return 0;
}

/*
Run query with different settings oracle
*/
static const std::vector<TestSetting> test_settings{
    TestSetting("aggregate_functions_null_for_empty", {"0", "1"}),
    TestSetting("aggregation_in_order_max_block_bytes", {"0", "1"}),
    TestSetting("allow_aggregate_partitions_independently", {"0", "1"}),
    TestSetting("allow_introspection_functions", {"0", "1"}),
    TestSetting("allow_reorder_prewhere_conditions", {"0", "1"}),
    TestSetting("any_join_distinct_right_table_keys", {"0", "1"}),
    TestSetting("async_insert", {"0", "1"}),
    TestSetting("check_query_single_value_result", {"0", "1"}),
    TestSetting("compile_aggregate_expressions", {"0", "1"}),
    TestSetting("compile_expressions", {"0", "1"}),
    TestSetting("compile_sort_description", {"0", "1"}),
    TestSetting("cross_join_min_bytes_to_compress", {"0", "1"}),
    TestSetting("cross_join_min_rows_to_compress", {"0", "1"}),
    TestSetting("describe_include_subcolumns", {"0", "1"}),
    TestSetting("distributed_aggregation_memory_efficient", {"0", "1"}),
    TestSetting("enable_analyzer", {"0", "1"}),
    TestSetting("enable_memory_bound_merging_of_aggregation_results", {"0", "1"}),
    TestSetting("enable_multiple_prewhere_read_steps", {"0", "1"}),
    TestSetting("enable_named_columns_in_function_tuple", {"0", "1"}),
    TestSetting("enable_optimize_predicate_expression", {"0", "1"}),
    TestSetting("enable_optimize_predicate_expression_to_final_subquery", {"0", "1"}),
    TestSetting("enable_parsing_to_custom_serialization", {"0", "1"}),
    TestSetting("enable_reads_from_query_cache", {"0", "1"}),
    TestSetting("enable_scalar_subquery_optimization", {"0", "1"}),
    TestSetting("enable_sharing_sets_for_mutations", {"0", "1"}),
    TestSetting("enable_software_prefetch_in_aggregation", {"0", "1"}),
    TestSetting("enable_unaligned_array_join", {"0", "1"}),
    TestSetting("enable_vertical_final", {"0", "1"}),
    TestSetting("enable_writes_to_query_cache", {"0", "1"}),
    TestSetting("exact_rows_before_limit", {"0", "1"}),
    TestSetting("flatten_nested", {"0", "1"}),
    TestSetting("force_aggregate_partitions_independently", {"0", "1"}),
    TestSetting("force_optimize_projection", {"0", "1"}),
    TestSetting("fsync_metadata", {"0", "1"}),
    TestSetting("group_by_two_level_threshold", {"0", "1"}),
    TestSetting("group_by_two_level_threshold_bytes", {"0", "1"}),
    TestSetting("http_wait_end_of_query", {"0", "1"}),
    TestSetting("input_format_import_nested_json", {"0", "1"}),
    TestSetting("input_format_parallel_parsing", {"0", "1"}),
    TestSetting("insert_null_as_default", {"0", "1"}),
    TestSetting(
        "join_algorithm",
        {"'default'",
         "'grace_hash'",
         "'direct, hash'",
         "'hash'",
         "'parallel_hash'",
         "'partial_merge'",
         "'direct'",
         "'auto'",
         "'full_sorting_merge'",
         "'prefer_partial_merge'"}),
    TestSetting("join_any_take_last_row", {"0", "1"}),
    TestSetting("join_use_nulls", {"0", "1"}),
    TestSetting("local_filesystem_read_method", {"'read'", "'pread'", "'mmap'", "'pread_threadpool'", "'io_uring'"}),
    TestSetting("local_filesystem_read_prefetch", {"0", "1"}),
    TestSetting("log_queries", {"0", "1"}),
    TestSetting("log_query_threads", {"0", "1"}),
    TestSetting("low_cardinality_use_single_dictionary_for_part", {"0", "1"}),
    TestSetting("max_bytes_before_external_group_by", {"0", "100000000"}),
    TestSetting("max_bytes_before_external_sort", {"0", "100000000"}),
    TestSetting("max_bytes_before_remerge_sort", {"0", "1"}),
    TestSetting("max_final_threads", {"0", "1"}),
    TestSetting("max_threads", {"1", std::to_string(std::thread::hardware_concurrency())}),
    TestSetting("min_chunk_bytes_for_parallel_parsing", {"0", "1"}),
    TestSetting("min_external_table_block_size_bytes", {"0", "100000000"}),
    TestSetting("move_all_conditions_to_prewhere", {"0", "1"}),
    TestSetting("move_primary_key_columns_to_end_of_prewhere", {"0", "1"}),
    TestSetting("optimize_aggregation_in_order", {"0", "1"}),
    TestSetting("optimize_aggregators_of_group_by_keys", {"0", "1"}),
    TestSetting("optimize_append_index", {"0", "1"}),
    TestSetting("optimize_arithmetic_operations_in_aggregate_functions", {"0", "1"}),
    TestSetting("optimize_count_from_files", {"0", "1"}),
    TestSetting("optimize_distinct_in_order", {"0", "1"}),
    TestSetting("optimize_group_by_constant_keys", {"0", "1"}),
    TestSetting("optimize_group_by_function_keys", {"0", "1"}),
    TestSetting("optimize_functions_to_subcolumns", {"0", "1"}),
    TestSetting("optimize_if_chain_to_multiif", {"0", "1"}),
    TestSetting("optimize_if_transform_strings_to_enum", {"0", "1"}),
    TestSetting("optimize_injective_functions_in_group_by", {"0", "1"}),
    TestSetting("optimize_injective_functions_inside_uniq", {"0", "1"}),
    TestSetting("optimize_move_to_prewhere", {"0", "1"}),
    TestSetting("optimize_move_to_prewhere_if_final", {"0", "1"}),
    TestSetting("optimize_multiif_to_if", {"0", "1"}),
    TestSetting("optimize_normalize_count_variants", {"0", "1"}),
    TestSetting("optimize_on_insert", {"0", "1"}),
    TestSetting("optimize_or_like_chain", {"0", "1"}),
    TestSetting("optimize_read_in_order", {"0", "1"}),
    TestSetting("optimize_redundant_functions_in_order_by", {"0", "1"}),
    TestSetting("optimize_rewrite_aggregate_function_with_if", {"0", "1"}),
    TestSetting("optimize_rewrite_array_exists_to_has", {"0", "1"}),
    TestSetting("optimize_rewrite_sum_if_to_count_if", {"0", "1"}),
    TestSetting("optimize_skip_merged_partitions", {"0", "1"}),
    TestSetting("optimize_skip_unused_shards", {"0", "1"}),
    TestSetting("optimize_sorting_by_input_stream_properties", {"0", "1"}),
    TestSetting("optimize_substitute_columns", {"0", "1"}),
    TestSetting("optimize_syntax_fuse_functions", {"0", "1"}),
    TestSetting("optimize_time_filter_with_preimage", {"0", "1"}),
    TestSetting("optimize_trivial_approximate_count_query", {"0", "1"}),
    TestSetting("optimize_trivial_count_query", {"0", "1"}),
    TestSetting("optimize_trivial_insert_select", {"0", "1"}),
    TestSetting("optimize_uniq_to_count", {"0", "1"}),
    TestSetting("optimize_use_implicit_projections", {"0", "1"}),
    TestSetting("optimize_use_projections", {"0", "1"}),
    TestSetting("optimize_using_constraints", {"0", "1"}),
    TestSetting("output_format_parallel_formatting", {"0", "1"}),
    TestSetting("output_format_pretty_highlight_digit_groups", {"0", "1"}),
    TestSetting("output_format_pretty_row_numbers", {"0", "1"}),
    TestSetting("output_format_write_statistics", {"0", "1"}),
    TestSetting("page_cache_inject_eviction", {"0", "1"}),
    TestSetting("parallel_replicas_allow_in_with_subquery", {"0", "1"}),
    TestSetting("parallel_replicas_for_non_replicated_merge_tree", {"0", "1"}),
    TestSetting("parallel_replicas_local_plan", {"0", "1"}),
    TestSetting("parallel_replicas_prefer_local_join", {"0", "1"}),
    TestSetting("parallel_view_processing", {"0", "1"}),
    TestSetting("parallelize_output_from_storages", {"0", "1"}),
    TestSetting("partial_merge_join_optimizations", {"0", "1"}),
    TestSetting("precise_float_parsing", {"0", "1"}),
    TestSetting("prefer_external_sort_block_bytes", {"0", "1"}),
    TestSetting("prefer_localhost_replica", {"0", "1"}),
    TestSetting("query_plan_aggregation_in_order", {"0", "1"}),
    TestSetting("query_plan_convert_outer_join_to_inner_join", {"0", "1"}),
    TestSetting("query_plan_enable_multithreading_after_window_functions", {"0", "1"}),
    TestSetting("query_plan_enable_optimizations", {"0", "1"}),
    TestSetting("query_plan_execute_functions_after_sorting", {"0", "1"}),
    TestSetting("query_plan_filter_push_down", {"0", "1"}),
    TestSetting("query_plan_lift_up_array_join", {"0", "1"}),
    TestSetting("query_plan_lift_up_union", {"0", "1"}),
    TestSetting("query_plan_max_optimizations_to_apply", {"0", "1"}),
    TestSetting("query_plan_merge_expressions", {"0", "1"}),
    TestSetting("query_plan_merge_filters", {"0", "1"}),
    TestSetting("query_plan_optimize_prewhere", {"0", "1"}),
    TestSetting("query_plan_push_down_limit", {"0", "1"}),
    TestSetting("query_plan_read_in_order", {"0", "1"}),
    TestSetting("query_plan_remove_redundant_distinct", {"0", "1"}),
    TestSetting("query_plan_remove_redundant_sorting", {"0", "1"}),
    TestSetting("query_plan_reuse_storage_ordering_for_window_functions", {"0", "1"}),
    TestSetting("query_plan_split_filter", {"0", "1"}),
    TestSetting("read_from_filesystem_cache_if_exists_otherwise_bypass_cache", {"0", "1"}),
    TestSetting("read_in_order_use_buffering", {"0", "1"}),
    TestSetting("remote_filesystem_read_prefetch", {"0", "1"}),
    TestSetting("rows_before_aggregation", {"0", "1"}),
    TestSetting("throw_on_error_from_cache_on_write_operations", {"0", "1"}),
    TestSetting("transform_null_in", {"0", "1"}),
    TestSetting("update_insert_deduplication_token_in_dependent_materialized_views", {"0", "1"}),
    TestSetting("use_cache_for_count_from_files", {"0", "1"}),
    TestSetting("use_concurrency_control", {"0", "1"}),
    TestSetting("use_index_for_in_with_subqueries", {"0", "1"}),
    TestSetting("use_local_cache_for_remote_storage", {"0", "1"}),
    TestSetting("use_page_cache_for_disks_without_file_cache", {"0", "1"}),
    TestSetting(
        "use_query_cache",
        {"0, set_overflow_mode = 'break', group_by_overflow_mode = 'break', join_overflow_mode = 'break'",
         "1, set_overflow_mode = 'throw', group_by_overflow_mode = 'throw', join_overflow_mode = 'throw'"}),
    TestSetting("use_skip_indexes", {"0", "1"}),
    TestSetting("use_skip_indexes_if_final", {"0", "1"}),
    TestSetting("use_uncompressed_cache", {"0", "1"}),
    TestSetting("use_variant_as_common_type", {"0", "1"})};

int QueryOracle::GenerateFirstSetting(RandomGenerator & rg, sql_query_grammar::SQLQuery & sq1)
{
    const uint32_t nsets = rg.NextBool() ? 1 : ((rg.NextSmallNumber() % 3) + 1);
    sql_query_grammar::SettingValues * sv = sq1.mutable_inner_query()->mutable_setting_values();

    nsettings.clear();
    for (uint32_t i = 0; i < nsets; i++)
    {
        const TestSetting & ts = rg.PickRandomlyFromVector(test_settings);
        sql_query_grammar::SetValue * setv = i == 0 ? sv->mutable_set_value() : sv->add_other_values();

        setv->set_property(ts.tsetting);
        if (ts.options.size() == 2)
        {
            if (rg.NextBool())
            {
                setv->set_value(*ts.options.begin());
                nsettings.push_back(*std::next(ts.options.begin(), 1));
            }
            else
            {
                setv->set_value(*std::next(ts.options.begin(), 1));
                nsettings.push_back(*(ts.options.begin()));
            }
        }
        else
        {
            setv->set_value(rg.PickRandomlyFromSet(ts.options));
            nsettings.push_back(rg.PickRandomlyFromSet(ts.options));
        }
    }
    return 0;
}

int QueryOracle::GenerateSecondSetting(const sql_query_grammar::SQLQuery & sq1, sql_query_grammar::SQLQuery & sq3)
{
    const sql_query_grammar::SettingValues & osv = sq1.inner_query().setting_values();
    sql_query_grammar::SettingValues * sv = sq3.mutable_inner_query()->mutable_setting_values();

    for (size_t i = 0; i < nsettings.size(); i++)
    {
        const sql_query_grammar::SetValue & osetv = i == 0 ? osv.set_value() : osv.other_values(static_cast<int>(i - 1));
        sql_query_grammar::SetValue * setv = i == 0 ? sv->mutable_set_value() : sv->add_other_values();

        setv->set_property(osetv.property());
        setv->set_value(nsettings[i]);
    }
    return 0;
}

int QueryOracle::GenerateSettingQuery(RandomGenerator & rg, StatementGenerator & gen, sql_query_grammar::SQLQuery & sq2)
{
    const std::filesystem::path & qfile = fc.db_file_path / "query.data";
    sql_query_grammar::TopSelect * ts = sq2.mutable_inner_query()->mutable_select();
    sql_query_grammar::SelectIntoFile * sif = ts->mutable_intofile();

    gen.SetAllowNotDetermistic(false);
    gen.GenerateTopSelect(rg, std::numeric_limits<uint32_t>::max(), ts);
    gen.SetAllowNotDetermistic(true);

    sql_query_grammar::Select * osel = ts->release_sel();
    sql_query_grammar::SelectStatementCore * nsel = ts->mutable_sel()->mutable_select_core();
    nsel->mutable_from()->mutable_tos()->mutable_join_clause()->mutable_tos()->mutable_joined_derived_query()->set_allocated_select(osel);
    nsel->mutable_orderby()->set_oall(true);
    ts->set_format(sql_query_grammar::OutFormat::OUT_CSV);
    sif->set_path(qfile.generic_string());
    sif->set_step(sql_query_grammar::SelectIntoFile_SelectIntoFileStep::SelectIntoFile_SelectIntoFileStep_TRUNCATE);
    return 0;
}

int QueryOracle::ProcessOracleQueryResult(const bool first, const bool success, const std::string & oracle_name)
{
    bool & res = first ? first_success : second_sucess;

    if (success)
    {
        const std::filesystem::path & qfile = fc.db_file_path / "query.data";

        md5_hash.hashFile(qfile.generic_string(), first ? first_digest : second_digest);
    }
    res = success;
    if (!first && first_success && second_sucess
        && !std::equal(std::begin(first_digest), std::end(first_digest), std::begin(second_digest)))
    {
        throw std::runtime_error(oracle_name + " oracle failed");
    }
    return 0;
}

}
