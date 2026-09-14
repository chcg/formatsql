-- ═══════════════════════════════════════════════════════════════════════════════
-- FormatSQL test fixture: MySQL / MariaDB -specific syntax and functions
-- Usage: format with dialect = MySQL, or convert MySQL -> another dialect and
-- check the rewritten function names in dialects.cpp's RULES_MYSQL table:
-- IFNULL->COALESCE, IF->IFF, NOW()->CURRENT_TIMESTAMP.
-- Also covers: backtick identifiers, LIMIT/OFFSET, ON DUPLICATE KEY UPDATE,
-- GROUP_CONCAT, STR_TO_DATE/DATE_FORMAT, AUTO_INCREMENT.
-- ═══════════════════════════════════════════════════════════════════════════════


-- ─── 1. Dialect-mapped functions: IFNULL, IF, NOW() ────────────────────────

SELECT order_id, IFNULL(discount, 0) AS discount, IF(amount > 1000, 'large', 'small') AS size_label, NOW() AS checked_at FROM orders


-- ─── 2. Backtick identifiers and LIMIT/OFFSET ──────────────────────────────

SELECT `order_id`, `customer_name`, `order_amount` FROM `orders` WHERE `order_status` = 'active' ORDER BY `order_amount` DESC LIMIT 10 OFFSET 20


-- ─── 3. INSERT ... ON DUPLICATE KEY UPDATE (MySQL upsert) ──────────────────

INSERT INTO customers (customer_id, name, email, visit_count) VALUES (42, 'Jane Doe', 'jane@example.com', 1) ON DUPLICATE KEY UPDATE name = VALUES(name), email = VALUES(email), visit_count = visit_count + 1


-- ─── 4. GROUP_CONCAT with SEPARATOR and ORDER BY inside the aggregate ─────

SELECT customer_id, GROUP_CONCAT(product_name ORDER BY order_date SEPARATOR ', ') AS purchased_products FROM order_lines ol JOIN products p ON p.product_id = ol.product_id GROUP BY customer_id


-- ─── 5. STR_TO_DATE / DATE_FORMAT round trip ────────────────────────────────

SELECT order_id, STR_TO_DATE(raw_date, '%d-%m-%Y') AS parsed_date, DATE_FORMAT(order_date, '%Y-%m-%d') AS formatted_date FROM raw_orders


-- ─── 6. CREATE TABLE with AUTO_INCREMENT and ENGINE clause ─────────────────

CREATE TABLE orders (order_id INT AUTO_INCREMENT PRIMARY KEY, customer_id INT NOT NULL, amount DECIMAL(10,2), status VARCHAR(20) DEFAULT 'pending') ENGINE=InnoDB DEFAULT CHARSET=utf8mb4


-- ─── 7. LIMIT without OFFSET, and the LIMIT offset, count short form ──────

SELECT order_id, amount FROM orders ORDER BY amount DESC LIMIT 5


SELECT order_id, amount FROM orders ORDER BY amount DESC LIMIT 20, 10
