#!/bin/sh
set -eu
cdb=${1:-./bin/cdb}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat > "$tmp/input.sql" <<'SQL'
-- semicolons ; inside comments must not execute
CREATE TABLE students (
 id INTEGER PRIMARY KEY,
 name TEXT,
 age INTEGER
);
INSERT INTO students VALUES (1,'Shivam; it''s CDB',24),(2,'Rahul',23);
/* a comment ; */ SELECT * FROM students WHERE age>23;
BEGIN; DELETE FROM students; ROLLBACK;
.schema
.check
.quit
SQL
"$cdb" "$tmp/university.cdb" --file "$tmp/input.sql" > "$tmp/first.txt"
printf '%s\n' 'SELECT * FROM students;' '.quit' | "$cdb" "$tmp/university.cdb" > "$tmp/reopen.txt"
grep -q '2 rows returned' "$tmp/reopen.txt"
grep -q 'Shivam' "$tmp/reopen.txt"
printf '%s\n' 'BEGIN;' 'DELETE FROM students;' '.quit' | "$cdb" "$tmp/university.cdb" > /dev/null
printf '%s\n' 'SELECT * FROM students;' | "$cdb" "$tmp/university.cdb" > "$tmp/rollback.txt"
grep -q '2 rows returned' "$tmp/rollback.txt"
if printf '%s\n' 'SELECT missing FROM students;' | "$cdb" "$tmp/university.cdb" > /dev/null 2>&1; then
 echo 'Expected nonzero batch error status' >&2; exit 1
fi
printf '%s\n' 'CLI multiline, quoted semicolons, metadata, restart and exit-rollback: PASS'
