-- ═══════════════════════════════════════════════════════════════════════════════
-- FormatSQL test fixture: cursors (procedural T-SQL / PL/pgSQL blocks)
-- Usage: open in Notepad++, press Format SQL (Ctrl+Alt+F) per section and inspect.
-- The formatter is primarily a DQL/DML text formatter, not a full procedural
-- parser, so this file is as much a stress test as a coverage gap: DECLARE,
-- CURSOR, OPEN/FETCH/CLOSE/DEALLOCATE and WHILE/BEGIN...END are keywords it
-- has never been shown before. Expect this section to surface real bugs.
-- ═══════════════════════════════════════════════════════════════════════════════


-- ─── 1. T-SQL cursor: DECLARE ... CURSOR FOR, OPEN, FETCH NEXT, WHILE loop ──
-- Classic pattern: iterate customers, accumulate something per row.

DECLARE @customer_id INT, @customer_name VARCHAR(100), @total_spent DECIMAL(10,2)
DECLARE customer_cursor CURSOR FOR SELECT customer_id, name FROM customers WHERE active = 1 ORDER BY name
OPEN customer_cursor
FETCH NEXT FROM customer_cursor INTO @customer_id, @customer_name
WHILE @@FETCH_STATUS = 0
BEGIN
    SELECT @total_spent = SUM(amount) FROM orders WHERE customer_id = @customer_id
    PRINT @customer_name + ': ' + CAST(@total_spent AS VARCHAR(20))
    FETCH NEXT FROM customer_cursor INTO @customer_id, @customer_name
END
CLOSE customer_cursor
DEALLOCATE customer_cursor


-- ─── 2. T-SQL cursor with explicit options (LOCAL, FAST_FORWARD) ──────────

DECLARE order_cursor CURSOR LOCAL FAST_FORWARD FOR SELECT order_id, amount FROM orders WHERE status = 'pending' ORDER BY order_date
OPEN order_cursor
FETCH NEXT FROM order_cursor INTO @order_id, @order_amount
WHILE @@FETCH_STATUS = 0
BEGIN
    UPDATE orders SET status = 'processing' WHERE order_id = @order_id
    FETCH NEXT FROM order_cursor INTO @order_id, @order_amount
END
CLOSE order_cursor
DEALLOCATE order_cursor


-- ─── 3. PL/pgSQL cursor inside a DO block, FOR ... IN cursor LOOP ──────────
-- PostgreSQL style: no explicit FETCH statement, the FOR loop drives the
-- cursor implicitly.

DO $$
DECLARE
    cur CURSOR FOR SELECT customer_id, name FROM customers WHERE active = TRUE;
    rec RECORD;
BEGIN
    FOR rec IN cur LOOP
        RAISE NOTICE 'Customer: % (%)', rec.name, rec.customer_id;
    END LOOP;
END $$;


-- ─── 4. Nested cursor loop (outer customers, inner orders per customer) ───

DECLARE @cid INT, @cname VARCHAR(100)
DECLARE @oid INT, @oamount DECIMAL(10,2)
DECLARE outer_cursor CURSOR FOR SELECT customer_id, name FROM customers WHERE active = 1
OPEN outer_cursor
FETCH NEXT FROM outer_cursor INTO @cid, @cname
WHILE @@FETCH_STATUS = 0
BEGIN
    DECLARE inner_cursor CURSOR FOR SELECT order_id, amount FROM orders WHERE customer_id = @cid
    OPEN inner_cursor
    FETCH NEXT FROM inner_cursor INTO @oid, @oamount
    WHILE @@FETCH_STATUS = 0
    BEGIN
        PRINT CAST(@cid AS VARCHAR) + ' - ' + CAST(@oid AS VARCHAR)
        FETCH NEXT FROM inner_cursor INTO @oid, @oamount
    END
    CLOSE inner_cursor
    DEALLOCATE inner_cursor
    FETCH NEXT FROM outer_cursor INTO @cid, @cname
END
CLOSE outer_cursor
DEALLOCATE outer_cursor
