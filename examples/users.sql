CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL, age INTEGER, active BOOLEAN);
INSERT INTO users VALUES (1, 'Shivam', 24, TRUE), (2, 'Rahul', 23, FALSE), (3, 'Ada', 24, TRUE);
EXPLAIN SELECT * FROM users WHERE age = 24;
CREATE INDEX idx_users_age ON users(age);
EXPLAIN SELECT * FROM users WHERE age = 24;
SELECT id, name FROM users WHERE age = 24 AND active;
UPDATE users SET age = age + 1 WHERE id = 1;
DELETE FROM users WHERE id = 2;
SELECT * FROM users;
.schema
.stats
