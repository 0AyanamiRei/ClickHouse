#!/usr/bin/env bash
# Tags: no-fasttest, long

CUR_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# reset --log_comment
CLICKHOUSE_LOG_COMMENT=
# shellcheck source=../shell_config.sh
. "$CUR_DIR"/../shell_config.sh

CH_CLIENT="$CLICKHOUSE_CLIENT --allow_experimental_json_type=1 --allow_experimental_variant_type=1 --use_variant_as_common_type=1"

function insert()
{
    echo "insert"
    $CH_CLIENT -q "truncate table test"
    $CH_CLIENT -q "insert into test select number, '{}' from numbers(5)"
    $CH_CLIENT -q "insert into test select number, toJSONString(map('a.b', arrayMap(x -> map('b.c.d_' || toString(x), number::UInt32, 'c.d.e', range((number + x) % 5 + 1)), range(number % 5 + 1)))) from numbers(5, 5)"
    $CH_CLIENT -q "insert into test select number, toJSONString(map('a.r', arrayMap(x -> map('b.c.d_' || toString(x), number::UInt32, 'c.d.e', range((number + x) % 5 + 1)), range(number % 5 + 1)))) from numbers(10, 5)"
    $CH_CLIENT -q "insert into test select number, toJSONString(map('a.a1', number, 'a.a2', number, 'a.a3', number, 'a.a4', number, 'a.a5', number, 'a.a6', number, 'a.a7', number, 'a.a8', number, 'a.r', arrayMap(x -> map('b.c.d_' || toString(x), number::UInt32, 'c.d.e', range((number + x) % 5 + 1)), range(number % 5 + 1)))) from numbers(15, 5)"
}

function test()
{
    echo "test"
    $CH_CLIENT -q "select distinct arrayJoin(JSONAllPathsWithTypes(json)) as paths_with_types from test order by paths_with_types"
    $CH_CLIENT -q "select distinct arrayJoin(JSONAllPathsWithTypes(arrayJoin(json.a.b))) as paths_with_types from test order by paths_with_types"
    $CH_CLIENT -q "select distinct arrayJoin(JSONAllPathsWithTypes(arrayJoin(json.a.r[]))) as paths_with_types from test order by paths_with_types"

    $CH_CLIENT -q "select json, json.a.b, json.a.b.c, json.a.b.c.d.e, json.a.b.b.c.d_0, json.a.b.b.c.d_1, json.a.b.b.c.d_2, json.a.b.b.c.d_3, json.a.b.b.c.d_4, json.a.r, json.a.r[], json.a.r[].c.d.e, json.a.r[].b.c.d_0, json.a.r[].b.c.d_1, json.a.r[].b.c.d_2, json.a.r[].b.c.d_3, json.a.r[].b.c.d_4, json.^a, json.a.b.^b.c, json.a.r[].^b.c from test order by id format JSONColumns"
    $CH_CLIENT -q "select json.a.b, json.a.b.c, json.a.b.c.d.e, json.a.b.b.c.d_0, json.a.b.b.c.d_1, json.a.b.b.c.d_2, json.a.b.b.c.d_3, json.a.b.b.c.d_4, json.a.r, json.a.r[], json.a.r[].c.d.e, json.a.r[].b.c.d_0, json.a.r[].b.c.d_1, json.a.r[].b.c.d_2, json.a.r[].b.c.d_3, json.a.r[].b.c.d_4, json.^a, json.a.b.^b.c, json.a.r[].^b.c from test order by id format JSONColumns"

    $CH_CLIENT -q "select json.a.r[].c.d.e, json.a.r[].b.c.d_0, json.a.r[].b.c.d_1 from test order by id format JSONColumns"
    $CH_CLIENT -q "select json.a.r[].c.d.e.:\`Array(Nullable(Int64))\`, json.a.r[].b.c.d_0.:Int64, json.a.r[].b.c.d_1.:Int64 from test order by id format JSONColumns"
    $CH_CLIENT -q "select json.a.r, json.a.r[].c.d.e, json.a.r[].b.c.d_0, json.a.r[].b.c.d_1 from test order by id format JSONColumns"
    $CH_CLIENT -q "select json.a.r, json.a.r[].c.d.e.:\`Array(Nullable(Int64))\`, json.a.r[].b.c.d_0.:Int64, json.a.r[].b.c.d_1.:Int64 from test order by id format JSONColumns"

    $CH_CLIENT -q "select json.a.r[].^b, json.a.r[].^b.c, json.a.r[].b.c.d_0 from test order by id format JSONColumns"
    $CH_CLIENT -q "select json.a.r[].^b, json.a.r[].^b.c, json.a.r[].b.c.d_0.:Int64 from test order by id format JSONColumns"
    $CH_CLIENT -q "select json.a.r, json.a.r[].^b, json.a.r[].^b.c, json.a.r[].b.c.d_0 from test order by id format JSONColumns"
    $CH_CLIENT -q "select json.a.r, json.a.r[].^b, json.a.r[].^b.c, json.a.r[].b.c.d_0.:Int64 from test order by id format JSONColumns"
}

$CH_CLIENT -q "drop table if exists test;"

echo "Memory"
$CH_CLIENT -q "create table test (id UInt64, json JSON(max_dynamic_paths=8, a.b Array(JSON))) engine=Memory"
insert
test
$CH_CLIENT -q "drop table test;"

echo "MergeTree compact"
$CH_CLIENT -q "create table test (id UInt64, json JSON(max_dynamic_paths=8, a.b Array(JSON))) engine=MergeTree order by id settings min_rows_for_wide_part=1000000000, min_bytes_for_wide_part=10000000000;"
echo "No merges"
$CH_CLIENT -q "system stop merges test"
insert
test
echo "With merges"
$CH_CLIENT -q "system start merges test"
test
$CH_CLIENT -q "drop table test;"

echo "MergeTree wide"
$CH_CLIENT -q "create table test (id UInt64, json JSON(max_dynamic_paths=8, a.b Array(JSON))) engine=MergeTree order by id settings min_rows_for_wide_part=1, min_bytes_for_wide_part=1;"
echo "No merges"
$CH_CLIENT -q "system stop merges test"
insert
test
echo "With merges"
$CH_CLIENT -q "system start merges test"
test
$CH_CLIENT -q "drop table test;"
