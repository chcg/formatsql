-- ─── 0. Conversietest: aanhalingstekens en getallen ──────────────────────────
-- Quotes → gebruik Convert > Quotes om enkels, dubbels en backticks te wisselen
-- Getallen → gebruik Convert > Number format om NL/EN-stijl te wisselen

SELECT "artikel_naam", `klant_naam`, 'orderstatus', 1.234.567,89 AS bedrag_nl, 1,234,567.89 AS bedrag_en FROM orders WHERE status = "actief" AND prijs > 1.000,00 AND naam != `onbekend`


-- ═══════════════════════════════════════════════════════════════════════════════
-- FormatSQL test queries
-- Gebruik: open in Notepad++, druk op Format SQL (Ctrl+Alt+F) en vergelijk output
-- ═══════════════════════════════════════════════════════════════════════════════


-- ─── 1. Basisquery: kw_case + fn_case + split_cols + comma_pos ────────────────
-- Verwacht met standaardinstellingen (kw_lower, split, comma before):
--   select count(order_id)
--        , sum(amount)
--        , customer_name
--        , order_date
--     from orders
--    where status = 'active'
--      and amount > 100

-- dit is een losse commentaarregel die bij strip_comments helemaal weg moet
SELECT COUNT(order_id), SUM(amount), customer_name -- totalen per klant
     , order_date FROM orders WHERE status = 'active' AND amount > 100 -- minimumfilter


-- ─── 2. JOINs: align_join_on + strip_inner_outer ─────────────────────────────
-- Met strip_inner_outer=aan: INNER JOIN → JOIN, LEFT OUTER JOIN → LEFT JOIN
-- Met align_join_on=aan: ON/AND uitgelijnd onder einde van het JOIN-sleutelwoord

SELECT o.order_id, c.name, p.product_name FROM orders o INNER JOIN customers c ON c.customer_id = o.customer_id AND c.active = 1 LEFT OUTER JOIN order_lines ol ON ol.order_id = o.order_id LEFT OUTER JOIN products p ON p.product_id = ol.product_id WHERE o.status = 'active'


-- ─── 3. Column aliases: alias_op + alias_case ────────────────────────────────
-- Met alias_op=Add + alias_case=lowercase:
--   select count(order_id) as order_count  (AS toegevoegd of behouden, lowercase)
-- Met alias_op=Remove: AS weggehaald
-- Met alias_op=Preserve: ongewijzigd

SELECT COUNT(order_id) AS order_count, SUM(amount) AS total_amount, customer_name cust_name, order_date FROM orders


-- ─── 4. CASE statements: case_stmt ───────────────────────────────────────────
-- Expand: WHEN/ELSE/END op eigen regels
-- Inline: alles op één regel
-- Preserve: ongewijzigd laten

SELECT order_id, CASE WHEN status = 'active' THEN 'Actief' WHEN status = 'pending' THEN 'In behandeling' ELSE 'Onbekend' END AS status_label, CASE WHEN amount > 1000 THEN 'Groot' WHEN amount > 100 THEN 'Middel' ELSE 'Klein' END AS grootte FROM orders


-- ─── 5. Semicolons: semi_pos ─────────────────────────────────────────────────
-- SameLine: puntkomma direct na laatste regel
-- OwnLine: puntkomma op eigen regel
-- Remove: puntkomma verwijderen
-- Preserve: ongewijzigd

SELECT customer_id, name FROM customers
;

SELECT order_id, amount FROM orders WHERE customer_id = 1
;


-- ─── 6. Spacing operators: spc_operators ─────────────────────────────────────
-- Add: spaties rondom =, <, >, <=, >=, <>, !=
-- Remove: spaties rondom operators verwijderen
-- Preserve: ongewijzigd

SELECT * FROM orders WHERE amount>=100 AND status='active' AND order_date<='2024-12-31' AND customer_id<>99


-- ─── 7. Blank lines: blank_between + max_blank ────────────────────────────────
-- blank_between=aan: lege regel voor elke clausule
-- max_blank=1: maximaal 1 lege regel tussen statements


SELECT a.id, a.name FROM table_a a WHERE a.active = 1



SELECT b.id, b.description FROM table_b b WHERE b.status = 'active'


-- ─── 8. Subquery in FROM-clausule ────────────────────────────────────────────
-- De SELECT binnen de subquery hoort ook opgemaakt te worden

SELECT * FROM (SELECT customer_id, SUM(amount) AS total FROM orders GROUP BY customer_id) t WHERE t.total > 500


-- ─── 8b. Subquery in WHERE-clausule (IN) ─────────────────────────────────────

SELECT order_id, amount FROM orders WHERE customer_id IN (SELECT customer_id FROM customers WHERE active = 1) AND amount > 100


-- ─── 9. CTEs (cte_nl / cte_indent niet geïmplementeerd) ──────────────────────
-- CTE-opmaak wordt momenteel niet aangepast

WITH active_customers AS (SELECT customer_id, name FROM customers WHERE active = 1), large_orders AS (SELECT customer_id, SUM(amount) AS total FROM orders GROUP BY customer_id HAVING SUM(amount) > 1000) SELECT c.name, o.total FROM active_customers c JOIN large_orders o ON o.customer_id = c.customer_id


-- ─── 10. Functiecasing: fn_case ──────────────────────────────────────────────
-- fn_case=Lowercase: alle functies lowercase (count, sum, coalesce, ...)
-- fn_case=Uppercase: COUNT, SUM, COALESCE, ...
-- fn_case=Preserve: originele schrijfwijze bewaren
-- Let op: alleen identifiers gevolgd door ( worden als functie beschouwd

SELECT COALESCE(phone, email, 'onbekend') AS contact, UPPER(name) AS name_upper, TRIM(description) AS clean_desc, NVL(discount, 0) AS disc, ROW_NUMBER() OVER (PARTITION BY customer_id ORDER BY order_date DESC) AS rn FROM orders


-- ─── 11. Kw_case=Preserve: originele schrijfwijze bewaren ────────────────────
-- Met kw_case=Preserve blijven SELECT, FROM, WHERE etc. ongewijzigd

Select Order_Id, Customer_Name From Orders Where Status = 'active' Order By Order_Date Desc


-- ─── 12. Combinatie: alles tegelijk ──────────────────────────────────────────

SELECT O.ORDER_ID,C.NAME AS CUST,CASE WHEN O.AMOUNT>1000 THEN 'GROOT' ELSE 'KLEIN' END AS GROOTTE,COUNT(OL.LINE_ID) AS REGELS FROM ORDERS O INNER JOIN CUSTOMERS C ON C.CUSTOMER_ID=O.CUSTOMER_ID LEFT OUTER JOIN ORDER_LINES OL ON OL.ORDER_ID=O.ORDER_ID WHERE O.STATUS='active' AND O.ORDER_DATE>='2024-01-01' GROUP BY O.ORDER_ID,C.NAME,O.AMOUNT ORDER BY O.ORDER_ID;


-- ─── 13. INSERT: kolomlijst + VALUES ─────────────────────────────────────────
-- Verwacht:
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
-- Verwacht:
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


-- ─── 14. UPDATE: SET-kolommen en WHERE ───────────────────────────────────────
-- Verwacht:
--   update orders
--      set status = 'shipped'
--        , shipped_date = '2024-01-15'
--        , tracking_number = 'TRACK123'
--    where order_id = 42
--      and customer_id = 1

UPDATE orders SET status = 'shipped', shipped_date = '2024-01-15', tracking_number = 'TRACK123' WHERE order_id = 42 AND customer_id = 1


-- ─── 14b. UPDATE met subquery in WHERE ───────────────────────────────────────

UPDATE products SET price = price * 1.1 WHERE category_id IN (SELECT id FROM categories WHERE premium = 1)


-- ─── 15. No-format pragma ────────────────────────────────────────────────────
-- Alles tussen @formatter:off en @formatter:on blijft ongewijzigd

-- @formatter:off
SELECT a,b,c FROM t WHERE x=1 AND y=2
-- @formatter:on

SELECT d, e FROM other_table WHERE y = 2


-- ─── 16. DELETE ──────────────────────────────────────────────────────────────
-- Verwacht:
--   delete from orders
--    where status = 'cancelled'
--      and order_date < '2024-01-01'

DELETE FROM orders WHERE status = 'cancelled' AND order_date < '2024-01-01'


-- ─── 17. CREATE TABLE ────────────────────────────────────────────────────────
-- Verwacht: kolomdefinities uitgevouwen, aligned onder eerste kolom

CREATE TABLE products (product_id INT NOT NULL, name VARCHAR(100) NOT NULL, price DECIMAL(10,2), category_id INT, status VARCHAR(20) DEFAULT 'active')


-- ─── 17b. CREATE VIEW ────────────────────────────────────────────────────────
-- Verwacht: SELECT op eigen regels na de AS

CREATE VIEW active_orders AS SELECT order_id, customer_id, amount FROM orders WHERE status = 'active'


-- ─── 17c. CREATE OR REPLACE VIEW ─────────────────────────────────────────────
-- Verwacht: 'or' wordt niet los gesplitst dankzij "create or replace" als gecombineerd keyword

CREATE OR REPLACE VIEW active_orders AS SELECT order_id, customer_id, amount FROM orders WHERE status = 'active'


-- ─── 18. DROP ────────────────────────────────────────────────────────────────
-- Verwacht: drop tabel/view/index zonder extra inspringing

DROP TABLE IF EXISTS order_archive

DROP VIEW active_orders


-- ─── 19. Window functions ─────────────────────────────────────────────────────
-- Verwacht: OVER (...) uitgevouwen met PARTITION BY / ORDER BY op eigen regels,
-- afsluitende ) op eigen regel, recht onder de openingshaak

SELECT customer_id, order_id, amount, ROW_NUMBER() OVER (PARTITION BY customer_id ORDER BY order_date DESC) AS rn, SUM(amount) OVER (PARTITION BY customer_id ORDER BY order_date ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS running_total, DENSE_RANK() OVER (ORDER BY amount DESC) AS rnk FROM orders


-- ─── 20. Alias-uitlijning ─────────────────────────────────────────────────────
-- Settings > Columns > vinkje "Align AS keywords in SELECT lists" AAN
-- Verwacht: alle AS-sleutelwoorden in het blok uitlijnen op de breedste expressie.
--
--   select customer_id            as cust_id
--        , name                   as cust_name
--        , coalesce(phone, email) as contact
--        , order_date             as first_order
--     from customers

SELECT customer_id AS cust_id, name AS cust_name, COALESCE(phone, email) AS contact, order_date AS first_order FROM customers


-- ─── 21. IN-lijst wrapping ───────────────────────────────────────────────────
-- Settings > Spacing > "Wrap IN lists at column" bijv. 60
-- Verwacht: de IN-lijst wordt verticaal uitgelijnd als de regel te lang is.
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


-- ─── 22. CASE WHEN-uitlijning ────────────────────────────────────────────────
-- Settings > Structure > CASE = Expand + vinkje "Align THEN columns" AAN
-- Verwacht: THEN-sleutelwoorden uitlijnen op de langste WHEN-conditie.
--
--   case
--       when status = 'active'  then 'Actief'
--       when status = 'pending' then 'In behandeling'
--       else                         'Onbekend'
--   end as label

SELECT order_id, CASE WHEN status = 'active' THEN 'Actief' WHEN status = 'pending' THEN 'In behandeling' ELSE 'Onbekend' END AS label FROM orders


-- ─── 23. Jinja/dbt-support ──────────────────────────────────────────────────
-- {{ expr }} en {# commentaar #} worden beschermd; {% %} regels ongewijzigd.
-- Verwacht: SQL correct opgemaakt, Jinja-tokens intact.

{% set my_status = 'active' %}
SELECT order_id, customer_id, {{ amount_col }}, {# inline jinja comment #} amount AS bedrag FROM {{ ref('orders') }} WHERE status = '{{ my_status }}' AND amount > 100


-- ─── 23b. dbt FQDN-conventies ───────────────────────────────────────────────
-- Settings > FQDN: database = 'mydb', schema = 'dbo', kwalificeer AAN.
-- {{ ref('orders') }} start met '{' → GEEN kwalificatie (correct gedrag).
-- Gewone tabellen WEL gekwalificeerd.

SELECT o.order_id, c.name FROM orders o JOIN customers c ON c.customer_id = o.customer_id
