CREATE TABLE students (id INTEGER PRIMARY KEY, name TEXT, age INTEGER);
INSERT INTO students VALUES (1, 'Shivam', 24), (2, 'Rahul', 23);
SELECT * FROM students;
SELECT name FROM students WHERE age > 23;
UPDATE students SET age = age + 1 WHERE id = 1;
SELECT * FROM students WHERE id = 1;
