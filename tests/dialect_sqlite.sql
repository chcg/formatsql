-- ═══════════════════════════════════════════════════════════════════════════════
-- FormatSQL test fixture: SQLite-specific syntax and functions
-- Usage: format with dialect = SQLite, or convert SQLite -> another dialect
-- and check the rewritten function names in dialects.cpp's RULES_SQLITE
-- table: IFNULL->COALESCE, SUBSTR->SUBSTRING, LENGTH->CHAR_LENGTH,
-- IIF->IFF.
-- Also covers: INTEGER PRIMARY KEY AUTOINCREMENT, PRAGMA, INSERT OR REPLACE
-- / INSERT OR IGNORE, json_extract, ATTACH DATABASE, flexible typing.
-- ═══════════════════════════════════════════════════════════════════════════════


-- ─── 1. Dialect-mapped functions: IFNULL, SUBSTR, LENGTH, IIF ──────────────

SELECT order_id, IFNULL(discount, 0) AS discount, SUBSTR(customer_name, 1, 10) AS short_name, LENGTH(customer_name) AS name_length, IIF(amount > 1000, 'large', 'small') AS size_label FROM orders


-- ─── 2. CREATE TABLE with INTEGER PRIMARY KEY AUTOINCREMENT ────────────────
-- Expected: SQLite's rowid-alias autoincrement column is just an identifier
-- list; no special handling needed beyond normal CREATE TABLE formatting.

CREATE TABLE orders (order_id INTEGER PRIMARY KEY AUTOINCREMENT, customer_id INTEGER NOT NULL, amount REAL, status TEXT DEFAULT 'pending')


-- ─── 3. PRAGMA statements (SQLite-only configuration commands) ────────────

PRAGMA foreign_keys = ON
PRAGMA table_info(orders)
PRAGMA journal_mode = WAL


-- ─── 4. INSERT OR REPLACE / INSERT OR IGNORE (SQLite upsert shorthand) ────

INSERT OR REPLACE INTO customers (customer_id, name, email) VALUES (42, 'Jane Doe', 'jane@example.com')


INSERT OR IGNORE INTO customers (customer_id, name, email) VALUES (42, 'Jane Doe', 'jane@example.com')


-- ─── 5. json_extract / json_each (SQLite JSON1 extension) ──────────────────

SELECT order_id, json_extract(payload, '$.customer.name') AS customer_name, json_extract(payload, '$.customer.address.city') AS city FROM raw_orders


SELECT o.order_id, je.value AS item_sku FROM orders o, json_each(o.items) je


-- ─── 6. ATTACH DATABASE (SQLite multi-file databases) ──────────────────────

ATTACH DATABASE 'archive.db' AS archive
SELECT * FROM archive.orders WHERE order_date < '2020-01-01'
DETACH DATABASE archive


-- ─── 7. WITHOUT ROWID table (SQLite-only optimization) ─────────────────────

CREATE TABLE lookup (code TEXT PRIMARY KEY, label TEXT NOT NULL) WITHOUT ROWID
