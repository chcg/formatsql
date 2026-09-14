-- ═══════════════════════════════════════════════════════════════════════════════
-- FormatSQL test fixture: dynamic SQL built in a SELECT/assignment, then EXEC'd
-- Usage: open in Notepad++, press Format SQL (Ctrl+Alt+F) per section and inspect.
-- Pattern: a SQL string is assembled via string concatenation (often inside a
-- SELECT @var = ... assignment) and then run with EXEC / EXEC sp_executesql /
-- PostgreSQL's EXECUTE. The interesting part for the formatter is the SELECT
-- that builds the string (it's a normal SELECT with string-concat expressions
-- and embedded quoted SQL fragments) versus the EXEC statement, which is a
-- keyword the formatter has never seen.
-- ═══════════════════════════════════════════════════════════════════════════════


-- ─── 1. Build a filter clause with SELECT @sql = ..., then EXEC(@sql) ─────
-- Expected: the embedded single-quoted SQL fragments inside the string
-- literal must never be touched (no keyword casing/splitting inside a
-- string), even though they look like SQL.

DECLARE @table_name VARCHAR(100) = 'orders'
DECLARE @sql NVARCHAR(MAX)
SELECT @sql = 'SELECT order_id, customer_id, amount FROM ' + @table_name + ' WHERE status = ''active'' AND amount > 100'
EXEC (@sql)


-- ─── 2. Multi-part concatenation across several variables ─────────────────

DECLARE @cols VARCHAR(200) = 'customer_id, name, email'
DECLARE @from_table VARCHAR(100) = 'customers'
DECLARE @where_clause VARCHAR(200) = 'active = 1'
DECLARE @dynamic_sql NVARCHAR(MAX)
SELECT @dynamic_sql = 'SELECT ' + @cols + ' FROM ' + @from_table + ' WHERE ' + @where_clause + ' ORDER BY name'
EXEC (@dynamic_sql)


-- ─── 3. sp_executesql with parameters (parameterized dynamic SQL) ─────────
-- Expected: the parameter-definition string (N'@id INT, @status VARCHAR(20)')
-- and the actual parameter bindings are just string/expression arguments to
-- EXEC sp_executesql; nothing inside the N'...' literal should be rewritten.

DECLARE @stmt NVARCHAR(MAX)
DECLARE @customer_id INT = 42
SELECT @stmt = 'SELECT order_id, amount FROM orders WHERE customer_id = @id AND status = @status'
EXEC sp_executesql @stmt, N'@id INT, @status VARCHAR(20)', @id = @customer_id, @status = 'active'


-- ─── 4. Dynamic ORDER BY column chosen at runtime ──────────────────────────

DECLARE @sort_column VARCHAR(50) = 'order_date'
DECLARE @sort_direction VARCHAR(4) = 'DESC'
DECLARE @query NVARCHAR(MAX)
SELECT @query = 'SELECT order_id, customer_id, amount, order_date FROM orders ORDER BY ' + @sort_column + ' ' + @sort_direction
EXEC (@query)


-- ─── 5. PostgreSQL EXECUTE ... format() inside a PL/pgSQL function ────────

CREATE OR REPLACE FUNCTION run_dynamic_report(table_name TEXT, min_amount NUMERIC) RETURNS VOID AS $$
DECLARE
    query TEXT;
BEGIN
    query := format('SELECT customer_id, SUM(amount) FROM %I WHERE amount > %L GROUP BY customer_id', table_name, min_amount);
    EXECUTE query;
END;
$$ LANGUAGE plpgsql;


-- ─── 6. Building dynamic SQL with a subquery embedded in the string ───────
-- Expected: the outer SELECT that assembles @sql is formatted normally; the
-- inner SQL text is opaque (it's inside a string literal), so it should stay
-- exactly as typed even though it contains SELECT/FROM/WHERE itself.

DECLARE @min_amount DECIMAL(10,2) = 500
DECLARE @report_sql NVARCHAR(MAX)
SELECT @report_sql = 'SELECT customer_id, name FROM customers WHERE customer_id IN (SELECT customer_id FROM orders WHERE amount > ' + CAST(@min_amount AS VARCHAR(20)) + ')'
EXEC (@report_sql)
