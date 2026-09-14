-- ═══════════════════════════════════════════════════════════════════════════════
-- FormatSQL test fixture: MS SQL (T-SQL) -specific syntax and functions
-- Usage: format with dialect = MS SQL, or convert MS SQL -> another dialect
-- and check the rewritten function names in dialects.cpp's RULES_MSSQL
-- table: LEN->CHAR_LENGTH, ISNULL->COALESCE, IIF->IFF, GETDATE()->
-- CURRENT_TIMESTAMP.
-- Also covers: TOP N, [bracket] identifiers, WITH (NOLOCK), CROSS/OUTER
-- APPLY, MERGE, OFFSET-FETCH, OUTPUT clause.
-- ═══════════════════════════════════════════════════════════════════════════════


-- ─── 1. Dialect-mapped functions: LEN, ISNULL, IIF, GETDATE() ─────────────

SELECT order_id, LEN(customer_name) AS name_length, ISNULL(discount, 0) AS discount, IIF(amount > 1000, 'large', 'small') AS size_label, GETDATE() AS checked_at FROM orders


-- ─── 2. TOP N with [bracket] identifiers and a NOLOCK hint ─────────────────

SELECT TOP 10 [order id], [customer name], [order amount] FROM [dbo].[orders] WITH (NOLOCK) WHERE [order status] = 'active' ORDER BY [order amount] DESC


-- ─── 3. CROSS APPLY / OUTER APPLY with a table-valued function ─────────────

SELECT c.customer_id, c.name, recent.order_id FROM customers c CROSS APPLY (SELECT TOP 1 order_id, amount FROM orders o WHERE o.customer_id = c.customer_id ORDER BY o.order_date DESC) recent


SELECT c.customer_id, c.name, last_order.order_id FROM customers c OUTER APPLY (SELECT TOP 1 order_id FROM orders o WHERE o.customer_id = c.customer_id ORDER BY o.order_date DESC) last_order


-- ─── 4. MERGE with WHEN MATCHED / WHEN NOT MATCHED BY TARGET/SOURCE ────────

MERGE INTO customers AS target USING staging_customers AS source ON target.customer_id = source.customer_id WHEN MATCHED THEN UPDATE SET target.name = source.name WHEN NOT MATCHED BY TARGET THEN INSERT (customer_id, name) VALUES (source.customer_id, source.name) WHEN NOT MATCHED BY SOURCE THEN DELETE


-- ─── 5. OFFSET ... FETCH NEXT ... ROWS ONLY (T-SQL pagination) ────────────

SELECT order_id, customer_id, amount FROM orders ORDER BY order_date DESC OFFSET 20 ROWS FETCH NEXT 10 ROWS ONLY


-- ─── 6. OUTPUT clause on INSERT / UPDATE / DELETE ──────────────────────────

INSERT INTO orders (customer_id, amount, status) OUTPUT INSERTED.order_id, INSERTED.amount VALUES (42, 199.99, 'pending')


UPDATE orders SET status = 'shipped' OUTPUT deleted.status AS old_status, inserted.status AS new_status WHERE order_id = 100


-- ─── 7. Table variable + variable assignment via SELECT ────────────────────

DECLARE @order_count INT
SELECT @order_count = COUNT(*) FROM orders WHERE status = 'pending'
SELECT @order_count AS pending_orders
