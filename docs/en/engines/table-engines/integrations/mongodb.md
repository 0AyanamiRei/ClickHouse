---
slug: /en/engines/table-engines/integrations/mongodb
sidebar_position: 135
sidebar_label: MongoDB
---

# MongoDB

MongoDB engine is read-only table engine which allows to read data from remote MongoDB collection.

Only MongoDB v3.6+ servers are supported.

## Types mappings

| MongoDB   | ClickHouse                 |
|-----------|----------------------------|
| bool      | UInt8                      |
| int32     | Int32                      |
| int64     | Int64                      |
| double    | Float64                    |
| date      | Date, DateTime, DateTime64 |
| timestamp | Date, DateTime, DateTime64 |
| string    | String, UUID               |
| document  | String(as JSON)            |
| array     | String(as JSON)            |
| oid       | String                     |

If key not found in MongoDB document, default value or null(if the column is nullable) will be inserted.

## Supported conditions
### WHERE
Only constant literals are allowed.

Types that can be used in WHERE section:
 * Null
 * UInt64
 * Int64
 * Float64
 * String
 * Array
 * Tuple
 * Map
 * UUID
 * Bool
 * Object

Prewhere is not supported.

### LIMIT and OFFSET
Only `LIMIT` is supported.

### ORDER BY
Simple expressions only are supported, without any modification like COLLATE, WITH, TO, etc.

### WINDOW
Not supported.

### GROUP BY
Not supported.

### Aggregation functions
Not supported.


## Creating a Table {#creating-a-table}

``` sql
CREATE TABLE [IF NOT EXISTS] [db.]table_name
(
    name1 [type1],
    name2 [type2],
    ...
) ENGINE = MongoDB(host:port, database, collection, user, password [, options]);
```

**Engine Parameters**

- `host:port` — MongoDB server address.

- `database` — Remote database name.

- `collection` — Remote collection name.

- `user` — MongoDB user.

- `password` — User password.

- `options` — MongoDB connection string options (optional parameter).

:::tip
If you are using the MongoDB Atlas cloud offering please add these options:

```
'connectTimeoutMS=10000&ssl=true&authSource=admin'
```

:::

## Usage Example {#usage-example}

Create a table in ClickHouse which allows to read data from MongoDB collection:

``` sql
CREATE TABLE mongo_table
(
    key UInt64,
    data String
) ENGINE = MongoDB('mongo1:27017', 'test', 'simple_table', 'testuser', 'clickhouse');
```

To read from an SSL secured MongoDB server:

``` sql
CREATE TABLE mongo_table_ssl
(
    key UInt64,
    data String
) ENGINE = MongoDB('mongo2:27017', 'test', 'simple_table', 'testuser', 'clickhouse', 'ssl=true');
```

Query:

``` sql
SELECT COUNT() FROM mongo_table;
```

``` text
┌─count()─┐
│       4 │
└─────────┘
```

You can also adjust connection timeout:

``` sql
CREATE TABLE mongo_table
(
    key UInt64,
    data String
) ENGINE = MongoDB('mongo2:27017', 'test', 'simple_table', 'testuser', 'clickhouse', 'connectTimeoutMS=100000');
```

## Troubleshooting
You can see the generated MongoDB query in DEBUG level logs.
