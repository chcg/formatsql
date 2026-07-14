-- ─── 0. Conversion test: quotes and numbers ──────────────────────────
-- Quotes → use Convert > Quotes to switch between single, double and backticks
-- Numbers → use Convert > Number format to switch between NL/EN style

SELECT "product_name", `customer_name`, 'order_status', 1.234.567,89 AS amount_nl, 1,234,567.89 AS amount_en FROM orders WHERE status = "active" AND price > 1.000,00 AND name != `unknown`


-- ═══════════════════════════════════════════════════════════════════════════════
-- FormatSQL test queries
-- Usage: open in Notepad++, press Format SQL (Ctrl+Alt+F) and compare output
-- ═══════════════════════════════════════════════════════════════════════════════


-- ─── 1. Basic query: kw_case + fn_case + split_cols + comma_pos ────────────────
-- Expected with default settings (kw_lower, split, comma before):
--   select count(order_id)
--        , sum(amount)
--        , customer_name
--        , order_date
--     from orders
--    where status = 'active'
--      and amount > 100

-- this is a standalone comment line that strip_comments should remove entirely
SELECT COUNT(order_id), SUM(amount), customer_name -- totals per customer
     , order_date FROM orders WHERE status = 'active' AND amount > 100 -- minimum filter


-- ─── 2. JOINs: align_join_on + strip_inner_outer ─────────────────────────────
-- With strip_inner_outer=on: INNER JOIN → JOIN, LEFT OUTER JOIN → LEFT JOIN
-- With align_join_on=on: ON/AND aligned under the end of the JOIN keyword

SELECT o.order_id, c.name, p.product_name FROM orders o INNER JOIN customers c ON c.customer_id = o.customer_id AND c.active = 1 LEFT OUTER JOIN order_lines ol ON ol.order_id = o.order_id LEFT OUTER JOIN products p ON p.product_id = ol.product_id WHERE o.status = 'active'


-- ─── 3. Column aliases: alias_op + alias_case ────────────────────────────────
-- With alias_op=Add + alias_case=lowercase:
--   select count(order_id) as order_count  (AS added or kept, lowercase)
-- With alias_op=Remove: AS removed
-- With alias_op=Preserve: unchanged

SELECT COUNT(order_id) AS order_count, SUM(amount) AS total_amount, customer_name cust_name, order_date FROM orders


-- ─── 4. CASE statements: case_stmt ───────────────────────────────────────────
-- Expand: WHEN/ELSE/END each on their own line
-- Inline: everything on one line
-- Preserve: leave unchanged

SELECT order_id, CASE WHEN status = 'active' THEN 'Active' WHEN status = 'pending' THEN 'Processing' ELSE 'Unknown' END AS status_label, CASE WHEN amount > 1000 THEN 'Large' WHEN amount > 100 THEN 'Medium' ELSE 'Small' END AS size FROM orders


-- ─── 5. Semicolons: semi_pos ─────────────────────────────────────────────────
-- SameLine: semicolon directly after the last line
-- OwnLine: semicolon on its own line
-- Remove: remove the semicolon
-- Preserve: unchanged

SELECT customer_id, name FROM customers
;

SELECT order_id, amount FROM orders WHERE customer_id = 1
;


-- ─── 6. Spacing operators: spc_operators ─────────────────────────────────────
-- Add: spaces around =, <, >, <=, >=, <>, !=
-- Remove: remove spaces around operators
-- Preserve: unchanged

SELECT * FROM orders WHERE amount>=100 AND status='active' AND order_date<='2024-12-31' AND customer_id<>99


-- ─── 7. Blank lines: blank_between + max_blank ────────────────────────────────
-- blank_between=on: blank line before each clause
-- max_blank=1: at most 1 blank line between statements


SELECT a.id, a.name FROM table_a a WHERE a.active = 1



SELECT b.id, b.description FROM table_b b WHERE b.status = 'active'


-- ─── 8. Subquery in FROM clause ────────────────────────────────────────────
-- The SELECT inside the subquery should also be formatted

SELECT * FROM (SELECT customer_id, SUM(amount) AS total FROM orders GROUP BY customer_id) t WHERE t.total > 500


-- ─── 8b. Subquery in WHERE clause (IN) ─────────────────────────────────────

SELECT order_id, amount FROM orders WHERE customer_id IN (SELECT customer_id FROM customers WHERE active = 1) AND amount > 100


-- ─── 9. CTEs (cte_nl / cte_indent not implemented) ──────────────────────
-- CTE formatting is currently not adjusted

WITH active_customers AS (SELECT customer_id, name FROM customers WHERE active = 1), large_orders AS (SELECT customer_id, SUM(amount) AS total FROM orders GROUP BY customer_id HAVING SUM(amount) > 1000) SELECT c.name, o.total FROM active_customers c JOIN large_orders o ON o.customer_id = c.customer_id


-- ─── 10. Function casing: fn_case ──────────────────────────────────────────────
-- fn_case=Lowercase: all functions lowercase (count, sum, coalesce, ...)
-- fn_case=Uppercase: COUNT, SUM, COALESCE, ...
-- fn_case=Preserve: keep original casing
-- Note: only identifiers followed by ( are treated as functions

SELECT COALESCE(phone, email, 'unknown') AS contact, UPPER(name) AS name_upper, TRIM(description) AS clean_desc, NVL(discount, 0) AS disc, ROW_NUMBER() OVER (PARTITION BY customer_id ORDER BY order_date DESC) AS rn FROM orders


-- ─── 11. Kw_case=Preserve: keep original casing ────────────────────
-- With kw_case=Preserve, SELECT, FROM, WHERE etc. remain unchanged

Select Order_Id, Customer_Name From Orders Where Status = 'active' Order By Order_Date Desc


-- ─── 12. Combination: everything at once ──────────────────────────────────────

SELECT O.ORDER_ID,C.NAME AS CUST,CASE WHEN O.AMOUNT>1000 THEN 'LARGE' ELSE 'SMALL' END AS SIZE,COUNT(OL.LINE_ID) AS LINES FROM ORDERS O INNER JOIN CUSTOMERS C ON C.CUSTOMER_ID=O.CUSTOMER_ID LEFT OUTER JOIN ORDER_LINES OL ON OL.ORDER_ID=O.ORDER_ID WHERE O.STATUS='active' AND O.ORDER_DATE>='2024-01-01' GROUP BY O.ORDER_ID,C.NAME,O.AMOUNT ORDER BY O.ORDER_ID;


-- ─── 13. INSERT: column list + VALUES ─────────────────────────────────────────
-- Expected:
--   insert into orders (customer_id
--                     , product_id
--                     , quantity
--                     , price
--                     , status)
--   values ( 1
--          , 42
--          , 3
--          , 19.99
--          , 'pending'
--          )

INSERT INTO orders (customer_id, product_id, quantity, price, status) VALUES (1, 42, 3, 19.99, 'pending')


-- ─── 13b. INSERT ... SELECT (insert from select) ─────────────────────────────
-- Expected:
--   insert into order_archive (order_id
--                            , customer_id
--                            , amount
--                            , status)
--   select order_id
--        , customer_id
--        , amount
--        , status
--     from orders
--    where status = 'completed'
--      and order_date < '2024-01-01'

INSERT INTO order_archive (order_id, customer_id, amount, status) SELECT order_id, customer_id, amount, status FROM orders WHERE status = 'completed' AND order_date < '2024-01-01'


-- ─── 14. UPDATE: SET columns and WHERE ───────────────────────────────────────
-- Expected:
--   update orders
--      set status = 'shipped'
--        , shipped_date = '2024-01-15'
--        , tracking_number = 'TRACK123'
--    where order_id = 42
--      and customer_id = 1

UPDATE orders SET status = 'shipped', shipped_date = '2024-01-15', tracking_number = 'TRACK123' WHERE order_id = 42 AND customer_id = 1


-- ─── 14b. UPDATE with subquery in WHERE ───────────────────────────────────────

UPDATE products SET price = price * 1.1 WHERE category_id IN (SELECT id FROM categories WHERE premium = 1)


-- ─── 15. No-format pragma ────────────────────────────────────────────────────
-- Everything between @formatter:off and @formatter:on stays unchanged

-- @formatter:off
SELECT a,b,c FROM t WHERE x=1 AND y=2
-- @formatter:on

SELECT d, e FROM other_table WHERE y = 2


-- ─── 16. DELETE ──────────────────────────────────────────────────────────────
-- Expected:
--   delete from orders
--    where status = 'cancelled'
--      and order_date < '2024-01-01'

DELETE FROM orders WHERE status = 'cancelled' AND order_date < '2024-01-01'


-- ─── 17. CREATE TABLE ────────────────────────────────────────────────────────
-- Expected: column definitions expanded, aligned under the first column

CREATE TABLE products (product_id INT NOT NULL, name VARCHAR(100) NOT NULL, price DECIMAL(10,2), category_id INT, status VARCHAR(20) DEFAULT 'active')


-- ─── 17b. CREATE VIEW ────────────────────────────────────────────────────────
-- Expected: SELECT on its own lines after the AS

CREATE VIEW active_orders AS SELECT order_id, customer_id, amount FROM orders WHERE status = 'active'


-- ─── 17c. CREATE OR REPLACE VIEW ─────────────────────────────────────────────
-- Expected: 'or' is not split off separately, thanks to "create or replace" being treated as a combined keyword

CREATE OR REPLACE VIEW active_orders AS SELECT order_id, customer_id, amount FROM orders WHERE status = 'active'


-- ─── 18. DROP ────────────────────────────────────────────────────────────────
-- Expected: drop table/view/index without extra indentation

DROP TABLE IF EXISTS order_archive

DROP VIEW active_orders


-- ─── 19. Window functions ─────────────────────────────────────────────────────
-- Expected: OVER (...) expanded with PARTITION BY / ORDER BY on their own lines,
-- closing ) on its own line, directly under the opening bracket

SELECT customer_id, order_id, amount, ROW_NUMBER() OVER (PARTITION BY customer_id ORDER BY order_date DESC) AS rn, SUM(amount) OVER (PARTITION BY customer_id ORDER BY order_date ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS running_total, DENSE_RANK() OVER (ORDER BY amount DESC) AS rnk FROM orders


-- ─── 20. Alias alignment ─────────────────────────────────────────────────────
-- Settings > Columns > checkbox "Align AS keywords in SELECT lists" ON
-- Expected: all AS keywords in the block aligned to the widest expression.
--
--   select customer_id            as cust_id
--        , name                   as cust_name
--        , coalesce(phone, email) as contact
--        , order_date             as first_order
--     from customers

SELECT customer_id AS cust_id, name AS cust_name, COALESCE(phone, email) AS contact, order_date AS first_order FROM customers


-- ─── 21. IN list wrapping ───────────────────────────────────────────────────
-- Settings > Spacing > "Wrap IN lists at column" e.g. 60
-- Expected: the IN list is aligned vertically if the line is too long.
--
--   select order_id
--     from orders
--    where status in ('pending'
--                   , 'processing'
--                   , 'awaiting_payment'
--                   , 'on_hold'
--                   , 'shipped'
--                   )

SELECT order_id FROM orders WHERE status IN ('pending', 'processing', 'awaiting_payment', 'on_hold', 'shipped')


-- ─── 22. CASE WHEN alignment ────────────────────────────────────────────────
-- Settings > Structure > CASE = Expand + checkbox "Align THEN columns" ON
-- Expected: THEN keywords aligned to the longest WHEN condition.
--
--   case
--       when status = 'active'  then 'Active'
--       when status = 'pending' then 'Processing'
--       else                         'Unknown'
--   end as label

SELECT order_id, CASE WHEN status = 'active' THEN 'Active' WHEN status = 'pending' THEN 'Processing' ELSE 'Unknown' END AS label FROM orders


-- ─── 23. Jinja/dbt support ──────────────────────────────────────────────────
-- {{ expr }} and {# comment #} are protected; {% %} lines stay unchanged.
-- Expected: SQL formatted correctly, Jinja tokens intact.

{% set my_status = 'active' %}
SELECT order_id, customer_id, {{ amount_col }}, {# inline jinja comment #} amount AS total_amount FROM {{ ref('orders') }} WHERE status = '{{ my_status }}' AND amount > 100


-- ─── 23b. dbt FQDN conventions ───────────────────────────────────────────────
-- Settings > FQDN: database = 'mydb', schema = 'dbo', qualify ON.
-- {{ ref('orders') }} starts with '{' → NO qualification (correct behavior).
-- Regular tables ARE qualified.

SELECT o.order_id, c.name FROM orders o JOIN customers c ON c.customer_id = o.customer_id
