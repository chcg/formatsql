-- ═══════════════════════════════════════════════════════════════════════════════
-- FormatSQL test fixture: CTEs (WITH clauses) not covered by basic_test_queries.sql
-- Usage: open in Notepad++, press Format SQL (Ctrl+Alt+F) per section and inspect.
-- Gaps this fills: WITH RECURSIVE, CTE with an explicit column list, chained
-- CTEs (each depending on the previous), a CTE referenced twice (self-join on
-- a CTE), and a recursive CTE whose recursive branch is a UNION ALL.
-- ═══════════════════════════════════════════════════════════════════════════════


-- ─── 1. WITH RECURSIVE: classic employee/manager hierarchy ─────────────────
-- Expected: RECURSIVE is kept attached to WITH; the UNION ALL between the
-- anchor and recursive branch is formatted like any other UNION ALL.

WITH RECURSIVE org_chart AS (SELECT employee_id, name, manager_id, 1 AS depth FROM employees WHERE manager_id IS NULL UNION ALL SELECT e.employee_id, e.name, e.manager_id, oc.depth + 1 FROM employees e JOIN org_chart oc ON e.manager_id = oc.employee_id) SELECT * FROM org_chart ORDER BY depth, name


-- ─── 2. CTE with an explicit column list ───────────────────────────────────
-- Expected: the (col1, col2) list right after the CTE name is preserved and
-- not confused with a function call.

WITH monthly_totals(month_name, total_amount) AS (SELECT DATE_TRUNC('month', order_date), SUM(amount) FROM orders GROUP BY DATE_TRUNC('month', order_date)) SELECT month_name, total_amount FROM monthly_totals WHERE total_amount > 1000 ORDER BY month_name


-- ─── 3. Chained CTEs: each one depends on the previous ─────────────────────

WITH base_orders AS (SELECT order_id, customer_id, amount, order_date FROM orders WHERE status = 'completed'), customer_totals AS (SELECT customer_id, SUM(amount) AS total_spent, COUNT(*) AS order_count FROM base_orders GROUP BY customer_id), top_tier AS (SELECT customer_id, total_spent, order_count FROM customer_totals WHERE total_spent > 10000) SELECT c.name, t.total_spent, t.order_count FROM top_tier t JOIN customers c ON c.customer_id = t.customer_id ORDER BY t.total_spent DESC


-- ─── 4. Same CTE referenced twice (a self-join against a CTE) ──────────────
-- Expected: both references use the same CTE name; no re-materialisation
-- syntax, just two aliases in the FROM/JOIN.

WITH ranked_orders AS (SELECT order_id, customer_id, amount, ROW_NUMBER() OVER (PARTITION BY customer_id ORDER BY amount DESC) AS rn FROM orders) SELECT a.customer_id, a.order_id AS biggest_order, b.order_id AS second_biggest FROM ranked_orders a JOIN ranked_orders b ON b.customer_id = a.customer_id AND b.rn = 2 WHERE a.rn = 1


-- ─── 5. Recursive CTE building a number sequence (no base table) ───────────

WITH RECURSIVE nums AS (SELECT 1 AS n UNION ALL SELECT n + 1 FROM nums WHERE n < 100) SELECT n FROM nums WHERE n % 10 = 0


-- ─── 6. Multiple independent CTEs combined with UNION in the final query ──

WITH new_customers AS (SELECT customer_id, name, 'new' AS segment FROM customers WHERE created_date >= '2024-01-01'), returning_customers AS (SELECT customer_id, name, 'returning' AS segment FROM customers WHERE created_date < '2024-01-01' AND customer_id IN (SELECT customer_id FROM orders)) SELECT customer_id, name, segment FROM new_customers UNION ALL SELECT customer_id, name, segment FROM returning_customers ORDER BY name


-- ─── 7. CTE whose body itself contains a correlated subquery ──────────────

WITH customer_last_order AS (SELECT c.customer_id, c.name, (SELECT MAX(o.order_date) FROM orders o WHERE o.customer_id = c.customer_id) AS last_order_date FROM customers c) SELECT customer_id, name, last_order_date FROM customer_last_order WHERE last_order_date < '2023-06-01' OR last_order_date IS NULL
