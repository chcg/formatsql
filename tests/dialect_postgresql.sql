-- ═══════════════════════════════════════════════════════════════════════════════
-- FormatSQL test fixture: PostgreSQL-specific syntax and functions
-- Usage: format with dialect = PostgreSQL, or convert PostgreSQL -> another
-- dialect and check the rewritten function names in dialects.cpp's
-- RULES_POSTGRESQL table: SUBSTR->SUBSTRING, LENGTH->CHAR_LENGTH,
-- NOW()->CURRENT_TIMESTAMP.
-- Also covers: :: casts, ON CONFLICT ... DO UPDATE / RETURNING, JSONB
-- operators, ILIKE, ARRAY[] literals, generate_series, LATERAL join.
-- ═══════════════════════════════════════════════════════════════════════════════


-- ─── 1. Dialect-mapped functions: NOW(), SUBSTR, LENGTH ────────────────────

SELECT order_id, NOW() AS checked_at, SUBSTR(customer_name, 1, 10) AS short_name, LENGTH(customer_name) AS name_length FROM orders


-- ─── 2. :: cast operator, several forms ────────────────────────────────────

SELECT order_id, amount::NUMERIC(10,2) AS amount_num, customer_id::TEXT AS customer_id_str, order_date::DATE AS order_day, (amount * 1.21)::INT AS amount_with_vat FROM orders


-- ─── 3. INSERT ... ON CONFLICT ... DO UPDATE ... RETURNING ────────────────

INSERT INTO customers (customer_id, name, email) VALUES (42, 'Jane Doe', 'jane@example.com') ON CONFLICT (customer_id) DO UPDATE SET name = EXCLUDED.name, email = EXCLUDED.email RETURNING customer_id, name


-- ─── 4. JSONB operators: ->, ->>, @>, #> ───────────────────────────────────

SELECT order_id, payload->'customer'->>'name' AS customer_name, payload#>'{customer,address,city}' AS city, payload @> '{"status": "active"}' AS is_active FROM raw_orders WHERE payload->>'status' = 'active'


-- ─── 5. ILIKE (case-insensitive LIKE) and ARRAY[] literals ─────────────────

SELECT customer_id, name FROM customers WHERE name ILIKE '%smith%' AND region = ANY(ARRAY['EU', 'US', 'APAC'])


-- ─── 6. generate_series() used as a row source ─────────────────────────────

SELECT d::DATE AS calendar_date FROM generate_series('2024-01-01'::DATE, '2024-12-31'::DATE, '1 day'::INTERVAL) AS d


-- ─── 7. LATERAL join with a correlated subquery ────────────────────────────

SELECT c.customer_id, c.name, recent.order_id, recent.amount FROM customers c LEFT JOIN LATERAL (SELECT order_id, amount FROM orders o WHERE o.customer_id = c.customer_id ORDER BY o.order_date DESC LIMIT 1) recent ON TRUE


-- ─── 8. Window function with a FILTER clause ───────────────────────────────

SELECT customer_id, COUNT(*) FILTER (WHERE status = 'completed') AS completed_count, COUNT(*) FILTER (WHERE status = 'cancelled') AS cancelled_count FROM orders GROUP BY customer_id


-- ─── 9. $n positional parameters (prepared statement style) ───────────────

SELECT order_id, amount FROM orders WHERE customer_id = $1 AND order_date >= $2
