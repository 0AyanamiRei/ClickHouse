DROP TABLE IF EXISTS test_table;

CREATE TABLE test_table
(
    uint64 UInt64,
    float64 Float64,
    decimal32 Decimal32(5),
) ENGINE=MergeTree ORDER BY uint64;

INSERT INTO test_table VALUES (1, 1.1, 1.11);
INSERT INTO test_table VALUES (2, 2.2, 2.22);
INSERT INTO test_table VALUES (3, 3.3, 3.33);
INSERT INTO test_table VALUES (4, 4.4, 4.44);
INSERT INTO test_table VALUES (5, 5.5, 5.55);


SELECT sum(uint64 + 2) From test_table;
SELECT sum(uint64) + 2 * count(uint64) From test_table;
EXPLAIN SYNTAX (SELECT sum(uint64 + 2) From test_table);
EXPLAIN SYNTAX (SELECT sum(uint64) + 2 * count(uint64) From test_table);
SELECT '--';

SELECT sum(float64 + 2) From test_table;
SELECT sum(float64) + 2 * count(float64) From test_table;
EXPLAIN SYNTAX (SELECT sum(float64 + 2) From test_table);
EXPLAIN SYNTAX (SELECT sum(float64) + 2 * count(float64) From test_table);
SELECT '--';

SELECT sum(decimal32 + 2) From test_table;
SELECT sum(decimal32) + 2 * count(decimal32) From test_table;
EXPLAIN SYNTAX (SELECT sum(decimal32 + 2) From test_table);
EXPLAIN SYNTAX (SELECT sum(decimal32) + 2 * count(decimal32) From test_table);
SELECT '--';

SELECT sum(uint64 + 2) + sum(uint64 + 3) From test_table;
SELECT sum(uint64) + 2 * count(uint64) + sum(uint64) + 3 * count(uint64) From test_table;
EXPLAIN SYNTAX (SELECT sum(uint64 + 2) + sum(uint64 + 3) From test_table);
EXPLAIN SYNTAX (SELECT sum(uint64) + 2 * count(uint64) + sum(uint64) + 3 * count(uint64) From test_table);
SELECT '--';

SELECT sum(float64 + 2) + sum(float64 + 3) From test_table;
SELECT sum(float64) + 2 * count(float64) + sum(float64) + 3 * count(float64) From test_table;
EXPLAIN SYNTAX (SELECT sum(float64 + 2) + sum(float64 + 3) From test_table);
EXPLAIN SYNTAX (SELECT sum(float64) + 2 * count(float64) + sum(float64) + 3 * count(float64) From test_table);
SELECT '--';

SELECT sum(decimal32 + 2) + sum(decimal32 + 3) From test_table;
SELECT sum(decimal32) + 2 * count(decimal32) + sum(decimal32) + 3 * count(decimal32) From test_table;
EXPLAIN SYNTAX (SELECT sum(decimal32 + 2) + sum(decimal32 + 3) From test_table);
EXPLAIN SYNTAX (SELECT sum(decimal32) + 2 * count(decimal32) + sum(decimal32) + 3 * count(decimal32) From test_table);

DROP TABLE IF EXISTS test_table;
