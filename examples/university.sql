-- Run on a NEW database path. All changes below are committed as one unit.
BEGIN;
CREATE TABLE students (id INTEGER PRIMARY KEY, name TEXT NOT NULL, age INTEGER, course_id INTEGER, active BOOLEAN);
CREATE TABLE courses (id INTEGER PRIMARY KEY, title TEXT NOT NULL, credits INTEGER);
CREATE TABLE faculty (id INTEGER PRIMARY KEY, name TEXT NOT NULL, department TEXT);
INSERT INTO courses VALUES (101, 'C Systems Programming', 4), (102, 'Database Internals', 4), (103, 'Numerical Methods', 3);
INSERT INTO faculty VALUES (1, 'Professor Rao', 'Computer Science'), (2, 'Professor Mehta', 'Mathematics');
INSERT INTO students VALUES (1, 'Shivam', 24, 101, TRUE), (2, 'Rahul', 23, 102, TRUE), (3, 'Ada', 25, 102, TRUE), (4, 'Grace', 22, 101, TRUE), (5, 'Alan', 24, 103, FALSE);
CREATE INDEX idx_students_age ON students(age);
CREATE INDEX idx_students_course ON students(course_id);
COMMIT;
SELECT * FROM students;
SELECT name, age FROM students WHERE age > 23;
EXPLAIN SELECT * FROM students WHERE age = 24;
BEGIN;
UPDATE students SET age = age + 1 WHERE id = 1;
DELETE FROM students WHERE id = 5;
ROLLBACK;
.tables
.schema
.check
