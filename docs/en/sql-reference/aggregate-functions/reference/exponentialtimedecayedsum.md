---
slug: /en/sql-reference/aggregate-functions/reference/exponentialtimedecayedsum
sidebar_position: 108
sidebar_title: exponentialTimeDecayedSum
---

## exponentialTimeDecayedSum

Calculates the exponential moving average of values for the determined time.

**Syntax**

```sql
exponentialTimeDecayedSum(x)(value, timeunit)
```

**Arguments**

- `value` — Value. [Integer](../../../sql-reference/data-types/int-uint.md), [Float](../../../sql-reference/data-types/float.md) or [Decimal](../../../sql-reference/data-types/decimal.md).
- `timeunit` — Timeunit. [Integer](../../../sql-reference/data-types/int-uint.md), [Float](../../../sql-reference/data-types/float.md) or [Decimal](../../../sql-reference/data-types/decimal.md), [DateTime](../../data-types/datetime.md), [DateTime64](../../data-types/datetime64.md).

**Parameters**

- `x` — Half-life period. [Integer](../../../sql-reference/data-types/int-uint.md), [Float](../../../sql-reference/data-types/float.md) or [Decimal](../../../sql-reference/data-types/decimal.md).

**Returned values**

- Returns an [exponentially smoothed moving average](https://en.wikipedia.org/wiki/Moving_average#Exponential_moving_average) of the values for the past `x` time at the latest point of time.

**Example**

Query:

```sql
SELECT
    value,
    time,
    round(exp_smooth, 3),
    bar(exp_smooth, 0, 1, 50) AS bar
FROM
    (
    SELECT
    (number % 5) = 0 AS value,
    number AS time,
    exponentialTimeDecayedSum(1)(value, time) OVER (ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS exp_smooth
    FROM numbers(50)
    );
```

Result:

```response
    ┌─value─┬─time─┬─round(exp_smooth, 3)─┬─bar────────────────────────────────────────────────┐
 1. │     1 │    0 │                    1 │ ██████████████████████████████████████████████████ │
 2. │     0 │    1 │                0.368 │ ██████████████████▍                                │
 3. │     0 │    2 │                0.135 │ ██████▊                                            │
 4. │     0 │    3 │                 0.05 │ ██▍                                                │
 5. │     0 │    4 │                0.018 │ ▉                                                  │
 6. │     1 │    5 │                1.007 │ ██████████████████████████████████████████████████ │
 7. │     0 │    6 │                 0.37 │ ██████████████████▌                                │
 8. │     0 │    7 │                0.136 │ ██████▊                                            │
 9. │     0 │    8 │                 0.05 │ ██▌                                                │
10. │     0 │    9 │                0.018 │ ▉                                                  │
11. │     1 │   10 │                1.007 │ ██████████████████████████████████████████████████ │
12. │     0 │   11 │                 0.37 │ ██████████████████▌                                │
13. │     0 │   12 │                0.136 │ ██████▊                                            │
14. │     0 │   13 │                 0.05 │ ██▌                                                │
15. │     0 │   14 │                0.018 │ ▉                                                  │
16. │     1 │   15 │                1.007 │ ██████████████████████████████████████████████████ │
17. │     0 │   16 │                 0.37 │ ██████████████████▌                                │
18. │     0 │   17 │                0.136 │ ██████▊                                            │
19. │     0 │   18 │                 0.05 │ ██▌                                                │
20. │     0 │   19 │                0.018 │ ▉                                                  │
21. │     1 │   20 │                1.007 │ ██████████████████████████████████████████████████ │
22. │     0 │   21 │                 0.37 │ ██████████████████▌                                │
23. │     0 │   22 │                0.136 │ ██████▊                                            │
24. │     0 │   23 │                 0.05 │ ██▌                                                │
25. │     0 │   24 │                0.018 │ ▉                                                  │
26. │     1 │   25 │                1.007 │ ██████████████████████████████████████████████████ │
27. │     0 │   26 │                 0.37 │ ██████████████████▌                                │
28. │     0 │   27 │                0.136 │ ██████▊                                            │
29. │     0 │   28 │                 0.05 │ ██▌                                                │
30. │     0 │   29 │                0.018 │ ▉                                                  │
31. │     1 │   30 │                1.007 │ ██████████████████████████████████████████████████ │
32. │     0 │   31 │                 0.37 │ ██████████████████▌                                │
33. │     0 │   32 │                0.136 │ ██████▊                                            │
34. │     0 │   33 │                 0.05 │ ██▌                                                │
35. │     0 │   34 │                0.018 │ ▉                                                  │
36. │     1 │   35 │                1.007 │ ██████████████████████████████████████████████████ │
37. │     0 │   36 │                 0.37 │ ██████████████████▌                                │
38. │     0 │   37 │                0.136 │ ██████▊                                            │
39. │     0 │   38 │                 0.05 │ ██▌                                                │
40. │     0 │   39 │                0.018 │ ▉                                                  │
41. │     1 │   40 │                1.007 │ ██████████████████████████████████████████████████ │
42. │     0 │   41 │                 0.37 │ ██████████████████▌                                │
43. │     0 │   42 │                0.136 │ ██████▊                                            │
44. │     0 │   43 │                 0.05 │ ██▌                                                │
45. │     0 │   44 │                0.018 │ ▉                                                  │
46. │     1 │   45 │                1.007 │ ██████████████████████████████████████████████████ │
47. │     0 │   46 │                 0.37 │ ██████████████████▌                                │
48. │     0 │   47 │                0.136 │ ██████▊                                            │
49. │     0 │   48 │                 0.05 │ ██▌                                                │
50. │     0 │   49 │                0.018 │ ▉                                                  │
    └───────┴──────┴──────────────────────┴────────────────────────────────────────────────────┘
```