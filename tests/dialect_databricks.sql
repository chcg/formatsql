-- ═══════════════════════════════════════════════════════════════════════════════
-- FormatSQL test fixture: Databricks (Spark SQL) -specific syntax and functions
-- Usage: format with dialect = Databricks, or convert Databricks -> another
-- dialect and check the rewritten function names in dialects.cpp's
-- RULES_DATABRICKS table: COLLECT_LIST->ARRAY_AGG, COLLECT_SET->ARRAY_AGG
-- (one-way only), DATE_FORMAT->TO_CHAR, FROM_UNIXTIME->TO_TIMESTAMP.
-- Also covers: MERGE INTO a Delta table, OPTIMIZE ... ZORDER BY, VACUUM,
-- LATERAL VIEW EXPLODE, CREATE TABLE USING DELTA.
-- ═══════════════════════════════════════════════════════════════════════════════


-- ─── 1. Dialect-mapped functions: COLLECT_LIST, COLLECT_SET ────────────────

SELECT customer_id, COLLECT_LIST(order_id) AS all_order_ids, COLLECT_SET(status) AS distinct_statuses FROM orders GROUP BY customer_id


-- ─── 2. Dialect-mapped functions: DATE_FORMAT, FROM_UNIXTIME ──────────────

SELECT order_id, DATE_FORMAT(order_date, 'yyyy-MM-dd') AS formatted_date, FROM_UNIXTIME(created_ts) AS created_at FROM raw_orders


-- ─── 3. MERGE INTO a Delta table ────────────────────────────────────────────

MERGE INTO customers AS target USING staging_customers AS source ON target.customer_id = source.customer_id WHEN MATCHED THEN UPDATE SET target.name = source.name, target.email = source.email WHEN NOT MATCHED THEN INSERT (customer_id, name, email) VALUES (source.customer_id, source.name, source.email)


-- ─── 4. OPTIMIZE ... ZORDER BY (Delta Lake file compaction) ────────────────

OPTIMIZE orders ZORDER BY (customer_id, order_date)


-- ─── 5. VACUUM (Delta Lake retention cleanup) ──────────────────────────────

VACUUM orders RETAIN 168 HOURS


-- ─── 6. LATERAL VIEW EXPLODE (flatten an array column, Hive/Spark syntax) ──

SELECT o.order_id, item_sku FROM orders o LATERAL VIEW EXPLODE(o.item_skus) exploded_items AS item_sku


-- ─── 7. CREATE TABLE ... USING DELTA with PARTITIONED BY ──────────────────

CREATE TABLE orders (order_id BIGINT, customer_id BIGINT, amount DECIMAL(10,2), order_date DATE) USING DELTA PARTITIONED BY (order_date)


-- ─── 8. Window function combined with COLLECT_LIST over a partition ──────

SELECT customer_id, order_date, COLLECT_LIST(product_id) OVER (PARTITION BY customer_id ORDER BY order_date ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS products_so_far FROM order_lines
