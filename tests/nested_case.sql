-- ═══════════════════════════════════════════════════════════════════════════════
-- FormatSQL test fixture: nested / combined CASE expressions
-- Usage: open in Notepad++, press Format SQL (Ctrl+Alt+F) per section and inspect.
-- test_queries.sql only has flat CASE WHEN...THEN...ELSE END (#4, #22). This
-- file adds: CASE nested inside a WHEN/THEN/ELSE branch, CASE inside an
-- aggregate, simple CASE (CASE expr WHEN val THEN...) vs searched CASE, CASE
-- in ORDER BY, CASE in WHERE, a subquery inside a WHEN condition, and three
-- levels of nesting.
-- ═══════════════════════════════════════════════════════════════════════════════


-- ─── 1. CASE nested inside another CASE's ELSE branch ──────────────────────
-- Expected: the inner CASE...END is indented one level deeper than the outer
-- one; its own WHEN/THEN/ELSE align with each other, not with the outer CASE.

SELECT order_id, amount, CASE WHEN amount > 1000 THEN 'large' WHEN amount > 100 THEN CASE WHEN customer_id IN (SELECT customer_id FROM vip_customers) THEN 'medium-vip' ELSE 'medium' END ELSE 'small' END AS size_label FROM orders


-- ─── 2. CASE nested three levels deep ──────────────────────────────────────

SELECT order_id, CASE WHEN status = 'completed' THEN CASE WHEN amount > 1000 THEN CASE WHEN customer_id IN (SELECT customer_id FROM vip_customers) THEN 'vip-large' ELSE 'large' END ELSE 'small-completed' END ELSE 'not-completed' END AS classification FROM orders


-- ─── 3. CASE inside an aggregate function (conditional sum) ────────────────
-- Expected: SUM(CASE WHEN ... END) keeps the CASE fully expanded inside the
-- function's parentheses, same indentation rules as a standalone CASE.

SELECT customer_id, SUM(CASE WHEN status = 'completed' THEN amount ELSE 0 END) AS completed_total, SUM(CASE WHEN status = 'cancelled' THEN amount ELSE 0 END) AS cancelled_total, COUNT(CASE WHEN status = 'pending' THEN 1 END) AS pending_count FROM orders GROUP BY customer_id


-- ─── 4. Simple CASE (CASE expr WHEN value THEN ...) vs searched CASE ──────
-- Expected: both forms are recognized as CASE...END, not just the searched
-- (CASE WHEN condition THEN) form.

SELECT order_id, CASE status WHEN 'pending' THEN 'Awaiting processing' WHEN 'shipped' THEN 'On the way' WHEN 'delivered' THEN 'Complete' ELSE 'Unknown status' END AS status_description FROM orders


-- ─── 5. CASE used inside WHERE ──────────────────────────────────────────────

SELECT order_id, amount, status FROM orders WHERE (CASE WHEN status = 'completed' THEN amount ELSE 0 END) > 500


-- ─── 6. CASE used inside ORDER BY ───────────────────────────────────────────

SELECT order_id, status, amount FROM orders ORDER BY CASE status WHEN 'urgent' THEN 1 WHEN 'high' THEN 2 WHEN 'normal' THEN 3 ELSE 4 END, amount DESC


-- ─── 7. Subquery inside a WHEN condition ────────────────────────────────────

SELECT customer_id, name, CASE WHEN (SELECT COUNT(*) FROM orders o WHERE o.customer_id = c.customer_id) > 10 THEN 'frequent' WHEN (SELECT COUNT(*) FROM orders o WHERE o.customer_id = c.customer_id) > 0 THEN 'occasional' ELSE 'none' END AS purchase_frequency FROM customers c


-- ─── 8. Two independent CASE expressions in the same SELECT, one nested ───

SELECT order_id, CASE WHEN amount > 1000 THEN 'large' ELSE 'small' END AS size_label, CASE WHEN status = 'completed' THEN CASE WHEN amount > 1000 THEN 'big win' ELSE 'closed' END ELSE 'open' END AS deal_state FROM orders
