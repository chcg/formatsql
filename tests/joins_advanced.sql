-- ═══════════════════════════════════════════════════════════════════════════════
-- FormatSQL test fixture: JOINs not covered by test_queries.sql
-- Usage: open in Notepad++, press Format SQL (Ctrl+Alt+F) per section and inspect.
-- Gaps this fills: RIGHT/FULL OUTER JOIN, self-join, multi-condition ON with OR,
-- join to a derived table, join to a CTE, semi/anti-join via EXISTS, LATERAL /
-- CROSS APPLY / OUTER APPLY, mixed join types in one query.
-- ═══════════════════════════════════════════════════════════════════════════════


-- ─── 1. RIGHT [OUTER] JOIN ──────────────────────────────────────────────────
-- Expected: RIGHT OUTER JOIN -> RIGHT JOIN when strip_inner_outer is on.

SELECT c.customer_id, c.name, o.order_id, o.amount FROM orders o RIGHT OUTER JOIN customers c ON c.customer_id = o.customer_id WHERE c.active = 1


-- ─── 2. FULL [OUTER] JOIN ───────────────────────────────────────────────────

SELECT c.customer_id, c.name, o.order_id FROM customers c FULL OUTER JOIN orders o ON o.customer_id = c.customer_id WHERE c.region = 'EU' OR o.status = 'pending'


-- ─── 3. Self-join (same table, two aliases) ────────────────────────────────
-- Expected: aliasing keeps both sides of the join unambiguous; ON references
-- both aliases of the same table.

SELECT e.employee_id, e.name AS employee, m.name AS manager FROM employees e LEFT JOIN employees m ON m.employee_id = e.manager_id WHERE e.active = 1 ORDER BY m.name, e.name


-- ─── 4. Multi-condition ON with mixed AND / OR, parenthesized ──────────────
-- Expected: the OR-group stays inside its own parens; AND/OR precedence in
-- the ON clause is never rewritten, only re-spaced/re-indented.

SELECT o.order_id, p.product_name, p.category_id FROM orders o JOIN order_lines ol ON ol.order_id = o.order_id JOIN products p ON p.product_id = ol.product_id AND (p.category_id = 3 OR p.category_id = 7) AND p.discontinued = 0


-- ─── 5. JOIN to a derived table (subquery) with alias ──────────────────────

SELECT o.order_id, o.customer_id, top_customers.total_spent FROM orders o JOIN (SELECT customer_id, SUM(amount) AS total_spent FROM orders GROUP BY customer_id HAVING SUM(amount) > 5000) top_customers ON top_customers.customer_id = o.customer_id


-- ─── 6. JOIN to a CTE ───────────────────────────────────────────────────────

WITH recent_orders AS (SELECT order_id, customer_id, amount FROM orders WHERE order_date >= '2024-01-01') SELECT c.name, ro.order_id, ro.amount FROM customers c JOIN recent_orders ro ON ro.customer_id = c.customer_id


-- ─── 7. Semi-join: WHERE EXISTS (correlated subquery) ──────────────────────
-- Expected: EXISTS keeps its subquery on its own indented block, same
-- treatment as an IN-subquery.

SELECT c.customer_id, c.name FROM customers c WHERE EXISTS (SELECT 1 FROM orders o WHERE o.customer_id = c.customer_id AND o.status = 'completed')


-- ─── 8. Anti-join: WHERE NOT EXISTS ─────────────────────────────────────────

SELECT c.customer_id, c.name FROM customers c WHERE NOT EXISTS (SELECT 1 FROM orders o WHERE o.customer_id = c.customer_id) AND c.created_date < '2023-01-01'


-- ─── 9. CROSS APPLY / OUTER APPLY (MS SQL table-valued function join) ──────
-- Expected: APPLY is treated like a JOIN keyword for indentation purposes.

SELECT c.customer_id, c.name, top_orders.order_id, top_orders.amount FROM customers c CROSS APPLY (SELECT TOP 3 order_id, amount FROM orders o WHERE o.customer_id = c.customer_id ORDER BY o.amount DESC) top_orders


SELECT c.customer_id, c.name, last_order.order_id FROM customers c OUTER APPLY (SELECT TOP 1 order_id FROM orders o WHERE o.customer_id = c.customer_id ORDER BY o.order_date DESC) last_order


-- ─── 10. LATERAL join (PostgreSQL / ANSI spelling of CROSS APPLY) ──────────

SELECT c.customer_id, recent.order_id, recent.amount FROM customers c, LATERAL (SELECT order_id, amount FROM orders o WHERE o.customer_id = c.customer_id ORDER BY o.order_date DESC LIMIT 1) recent


-- ─── 11. Five-way join mixing INNER / LEFT / RIGHT in one query ────────────
-- Expected: each JOIN keyword and its ON get their own line regardless of
-- join type; strip_inner_outer normalizes INNER/OUTER away consistently.

SELECT o.order_id, c.name, p.product_name, w.warehouse_name, s.shipper_name FROM orders o INNER JOIN customers c ON c.customer_id = o.customer_id LEFT JOIN order_lines ol ON ol.order_id = o.order_id LEFT JOIN products p ON p.product_id = ol.product_id RIGHT JOIN warehouses w ON w.warehouse_id = p.warehouse_id LEFT OUTER JOIN shippers s ON s.shipper_id = o.shipper_id WHERE o.status <> 'cancelled'


-- ─── 12. CROSS JOIN with an explicit keyword (not the comma form) ──────────

SELECT s.size_name, col.color_name FROM sizes s CROSS JOIN colors col ORDER BY s.size_name, col.color_name
