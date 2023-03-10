DROP TABLE IF EXISTS src_table;
DROP TABLE IF EXISTS mv;

CREATE TABLE src_table (`a` UInt32, `b` UInt32) ENGINE = MergeTree ORDER BY a;
CREATE MATERIALIZED VIEW mv UUID '2bad6d75-86fe-4da0-815b-2c7410253941' (`a` UInt32) ENGINE = MergeTree ORDER BY a AS SELECT a FROM src_table;

INSERT INTO src_table (a, b) VALUES (1, 1), (2, 2);

SELECT * FROM mv;

SET allow_experimental_alter_materialized_view_structure = 1;
ALTER TABLE mv MODIFY QUERY SELECT a, b FROM src_table; -- {serverError QUERY_IS_NOT_SUPPORTED_IN_MATERIALIZED_VIEW}

DROP TABLE src_table;
DROP TABLE mv;