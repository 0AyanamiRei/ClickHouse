-- Tags: no-fasttest
-- Tag no-fasttest: needs s3
--
INSERT INTO TABLE FUNCTION s3('http://localhost:11111/test/test-data-03644_object_storage.csv', 'clickhouse', 'clickhouse', 'CSV', 'number UInt64') SELECT number FROM numbers(10) SETTINGS s3_truncate_on_insert = 1;

SELECT n1.c1
FROM s3('http://localhost:11111/test/test-data-03644_object_storage.csv', 'test', 'testtest') AS n1
WHERE n1.c1 > (
    SELECT AVG(n2.c1)
    FROM s3('http://localhost:11111/test/test-data-03644_object_storage.csv', 'test', 'testtest') AS n2
    WHERE n2.c1 < n1.c1
)
SETTINGS allow_experimental_correlated_subqueries = 1;
