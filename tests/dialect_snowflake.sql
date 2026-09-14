-- ═══════════════════════════════════════════════════════════════════════════════
-- FormatSQL test fixture: Snowflake-specific syntax and functions
-- Usage: format with dialect = Snowflake, or convert Snowflake -> another
-- dialect and check the rewritten function names in dialects.cpp's
-- RULES_SNOWFLAKE table: SUBSTR->SUBSTRING, LENGTH->CHAR_LENGTH,
-- NVL->COALESCE, IFNULL->COALESCE (one-way only).
-- Also covers: QUALIFY, semi-structured (:field) access, FLATTEN/LATERAL,
-- OBJECT_CONSTRUCT/ARRAY_CONSTRUCT, TRY_CAST/TRY_TO_*, MERGE INTO.
-- ═══════════════════════════════════════════════════════════════════════════════


-- ─── 1. Dialect-mapped functions together: NVL, IFNULL, SUBSTR, LENGTH ────

SELECT order_id, NVL(discount, 0) AS discount, IFNULL(notes, 'n/a') AS notes, SUBSTR(customer_name, 1, 10) AS short_name, LENGTH(customer_name) AS name_length FROM orders


-- ─── 2. QUALIFY: filter on a window function without a subquery ───────────

SELECT order_id, customer_id, amount, ROW_NUMBER() OVER (PARTITION BY customer_id ORDER BY amount DESC) AS rn FROM orders QUALIFY ROW_NUMBER() OVER (PARTITION BY customer_id ORDER BY amount DESC) = 1


-- ─── 3. Semi-structured data access with the colon operator ───────────────

SELECT order_id, payload:customer.name AS customer_name, payload:customer.address.city AS city, payload:items[0].sku AS first_item_sku FROM raw_orders


-- ─── 4. LATERAL FLATTEN on a VARIANT / ARRAY column ─────────────────────────

SELECT o.order_id, item.value:sku AS sku, item.value:qty AS quantity FROM orders o, LATERAL FLATTEN(input => o.items) item


-- ─── 5. OBJECT_CONSTRUCT / ARRAY_CONSTRUCT to build semi-structured data ──

SELECT order_id, OBJECT_CONSTRUCT('id', order_id, 'amount', amount, 'items', ARRAY_CONSTRUCT('a', 'b', 'c')) AS payload FROM orders


-- ─── 6. TRY_CAST / TRY_TO_NUMBER / TRY_TO_DATE (safe conversion) ──────────

SELECT raw_id, TRY_CAST(raw_id AS INT) AS parsed_id, TRY_TO_NUMBER(raw_amount) AS parsed_amount, TRY_TO_DATE(raw_date, 'YYYY-MM-DD') AS parsed_date FROM staging_orders WHERE TRY_CAST(raw_id AS INT) IS NOT NULL


-- ─── 7. MERGE INTO (Snowflake supports full MERGE) ─────────────────────────

MERGE INTO customers AS target USING staging_customers AS source ON target.customer_id = source.customer_id WHEN MATCHED THEN UPDATE SET target.name = source.name, target.email = source.email WHEN NOT MATCHED THEN INSERT (customer_id, name, email) VALUES (source.customer_id, source.name, source.email)


-- ─── 8. Time travel / AT|BEFORE syntax (Snowflake-only) ────────────────────

SELECT * FROM orders AT (TIMESTAMP => '2024-01-01 00:00:00'::TIMESTAMP) WHERE customer_id = 42
