-- ═══════════════════════════════════════════════════════════════════════════════
-- FormatSQL test fixture: subqueries not covered by basic_test_queries.sql
-- Usage: open in Notepad++, press Format SQL (Ctrl+Alt+F) per section and inspect.
-- Gaps this fills: scalar subquery in the SELECT list, correlated subquery in
-- WHERE, subquery inside a JOIN's ON, ANY/ALL/SOME comparisons, EXISTS/NOT
-- EXISTS as a boolean expression (not just top-level WHERE), a derived table
-- with an explicit column alias list, subquery in HAVING, and a subquery
-- inside a subquery inside a JOIN.
-- ═══════════════════════════════════════════════════════════════════════════════


-- ─── 1. Scalar subquery in the SELECT list ─────────────────────────────────
-- Expected: the parenthesized subquery is treated like any other expression
-- in the column list; its own SELECT/FROM/WHERE still get formatted inside.

SELECT o.order_id, o.customer_id, o.amount, (SELECT c.name FROM customers c WHERE c.customer_id = o.customer_id) AS customer_name FROM orders o WHERE o.amount > 100


-- ─── 2. Correlated subquery in WHERE (references the outer alias) ─────────

SELECT o.order_id, o.amount FROM orders o WHERE o.amount > (SELECT AVG(o2.amount) FROM orders o2 WHERE o2.customer_id = o.customer_id)


-- ─── 3. Subquery inside a JOIN's ON clause ─────────────────────────────────

SELECT o.order_id, c.name FROM orders o JOIN customers c ON c.customer_id = o.customer_id AND c.customer_id IN (SELECT customer_id FROM vip_customers WHERE active = 1)


-- ─── 4. ANY / SOME / ALL with a subquery ───────────────────────────────────

SELECT product_id, product_name, price FROM products WHERE price > ANY (SELECT price FROM products WHERE category_id = 3)


SELECT product_id, product_name, price FROM products WHERE price >= ALL (SELECT price FROM products WHERE category_id = 3 AND discontinued = 0)


-- ─── 5. EXISTS used as a boolean expression, not a bare WHERE clause ───────
-- Expected: EXISTS(...) inside a CASE / boolean expression still formats its
-- inner SELECT correctly rather than being treated as a plain function call.

SELECT customer_id, name, CASE WHEN EXISTS (SELECT 1 FROM orders o WHERE o.customer_id = c.customer_id AND o.status = 'completed') THEN 'has orders' ELSE 'no orders' END AS order_status FROM customers c


-- ─── 6. Derived table with an explicit column alias list ──────────────────
-- Expected: (SELECT ...) AS t(col1, col2, col3) keeps the column list intact
-- and distinct from the table alias.

SELECT t.customer_id, t.total, t.rank_in_segment FROM (SELECT customer_id, SUM(amount), RANK() OVER (ORDER BY SUM(amount) DESC) FROM orders GROUP BY customer_id) AS t(customer_id, total, rank_in_segment) WHERE t.rank_in_segment <= 10


-- ─── 7. Subquery in HAVING ──────────────────────────────────────────────────

SELECT customer_id, SUM(amount) AS total FROM orders GROUP BY customer_id HAVING SUM(amount) > (SELECT AVG(amount) * 10 FROM orders)


-- ─── 8. Subquery nested inside a subquery, inside a JOIN ───────────────────
-- Expected: each level re-indents relative to its own parent (same rule as
-- basic_test_queries.sql #31, but this time the subquery is a JOIN target, not a
-- FROM target).

SELECT o.order_id, ranked.customer_id, ranked.total_rank FROM orders o JOIN (SELECT customer_id, RANK() OVER (ORDER BY total DESC) AS total_rank FROM (SELECT customer_id, SUM(amount) AS total FROM orders GROUP BY customer_id) inner_totals) ranked ON ranked.customer_id = o.customer_id WHERE ranked.total_rank <= 5


-- ─── 9. IN-subquery with multiple columns (row value constructor) ─────────

SELECT order_id, customer_id, product_id FROM order_lines WHERE (customer_id, product_id) IN (SELECT customer_id, product_id FROM vip_product_access)


-- ─── 10. UPDATE ... SET column = (correlated scalar subquery) ─────────────

UPDATE customers SET lifetime_value = (SELECT SUM(o.amount) FROM orders o WHERE o.customer_id = customers.customer_id AND o.status = 'completed') WHERE customer_id IN (SELECT customer_id FROM orders WHERE order_date >= '2024-01-01')
