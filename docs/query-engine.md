# Query engine, grammar, and SQL semantics

## Frontend and representation

`cdb_lexer_next` returns tokens with type, input slice, byte length, and starting byte offset. Keywords are recognized case-insensitively. Numbers accept a decimal point and exponent; integer literals are checked against signed 64-bit limits, including `-9223372036854775808`. Strings escape quotes by doubling them. Both `--` line comments and nonnested `/* ... */` comments are recognized.

The parser owns a `CdbQuery`: statement kind, table name, schema definitions, literal INSERT rows, projection names, assignments, and a `CdbExpr` tree. The precedence parser makes extension local: adding a token is not sufficient; its precedence, binding rule, evaluation, and tests must agree.

## Grammar (implemented subset)

```ebnf
statement = create_table | drop_table | create_index | insert | select
          | update | delete | "BEGIN" | "COMMIT" | "ROLLBACK" ;
create_table = "CREATE TABLE", name, "(", column, {",", column}, ")" ;
column = name, type, ["PRIMARY KEY"], ["NOT NULL"] ;
type = "INTEGER" | "INT" | "FLOAT" | "REAL" | "TEXT" | "BOOLEAN" | "BOOL" ;
drop_table = "DROP TABLE", name ;
create_index = "CREATE INDEX", name, "ON", name, "(", name, ")" ;
insert = "INSERT INTO", name, ["(", names, ")"], "VALUES", tuple, {",", tuple} ;
tuple = "(", literal, {",", literal}, ")" ;
select = ["EXPLAIN"], "SELECT", ("*" | names), "FROM", name,
         ["WHERE", expression], ["LIMIT", nonnegative_integer] ;
update = "UPDATE", name, "SET", assignment, {",", assignment}, ["WHERE", expression] ;
assignment = name, "=", expression ;
delete = "DELETE FROM", name, ["WHERE", expression] ;
names = name, {",", name} ;
literal = signed_number | quoted_text | "TRUE" | "FALSE" | "NULL" ;
```

One optional trailing `;` is accepted by the C API. More than one statement in a call is rejected. The REPL splits a stream into individual statements while respecting strings/comments. Column constraints can be written in either order; redundant `NOT NULL` has no additional effect.

Expression precedence, tightest first:

1. Literal, column, parenthesized expression
2. Unary `+`, `-`
3. `*`, `/`
4. `+`, `-`
5. `=`, `!=`, `<>`, `<`, `<=`, `>`, `>=`, `IS NULL`, `IS NOT NULL`
6. `NOT`
7. `AND`
8. `OR`

The parser limits recursion to 64 and total expression nodes to 256 per statement, so untrusted nesting cannot consume an unbounded stack. Numeric overflows and oversized text/identifiers are errors.

## Binding and types

Before scanning, the executor resolves every column name to a schema index and checks operand types. Unknown WHERE/projection/SET columns therefore fail even on an empty table. INSERT permits an explicit column list; omitted fields become NULL. INTEGER values may be widened to FLOAT for storage; other implicit coercions are rejected. A primary key must be INTEGER and is implicitly NOT NULL.

WHERE requires BOOLEAN or NULL; integer truthiness is not accepted. Arithmetic supports numbers, returns NULL for NULL inputs, checks integer overflow and division by zero, and rejects non-finite FLOAT results. Integer division truncates toward zero. Mixed integer/float comparisons avoid rounding the integer to double before deciding ordering. TEXT comparisons use bytewise `strcmp`, not linguistic collation.

NULL uses three-valued logic: comparison with NULL yields NULL; WHERE admits only TRUE. `FALSE AND NULL` is FALSE; `TRUE OR NULL` is TRUE. Use `IS NULL`, not `= NULL`, to test for missing values. Both sides of AND/OR are evaluated; there is no short-circuit contract.

## Planning

The planner uses a primary/secondary INTEGER index when a predicate is `indexed_column = integer_literal` (or reversed). It can find such a predicate inside AND, then evaluate the entire predicate on fetched rows. It does not use a single equality index to answer OR, range comparisons, floating literals, or arbitrary expressions. Those queries scan the table.

```sql
EXPLAIN SELECT * FROM users WHERE age = 25;
-- INDEX LOOKUP users.age (idx_users_age), if that integer index exists
```

`CdbResult.plan` stores the choice; `examined` counts visited candidate rows. EXPLAIN binds and plans without scanning or mutating. SELECT returns independent value copies. Row order is deliberately unspecified and may differ between scan and index plans. LIMIT caps output only; the current iterator continues the traversal.

## Mutation safety

UPDATE/DELETE collect matching RIDs before writing. This prevents a changed index key or a relocated row from being processed twice. SET expressions see the original row; `SET a=b, b=a` swaps two non-key values rather than reading an already-updated field. Primary-key uniqueness is checked per changed row, so a multiple-row key swap may be rejected.

Any failed write execution rolls back the entire open transaction. SQL syntax errors occur before execution and leave an existing transaction active. Read/type errors during SELECT likewise leave it active. Automatic write transactions commit on success or roll back on failure.

## Errors and result ownership

Errors carry a code, byte position when relevant, and bounded descriptive message. Byte positions are relative to the statement passed to `cdb_execute`, not a cumulative file line number. Reusable layers return booleans/errors; the shell presents them.

Call `cdb_result_free` once per result before reusing it. The result may outlive the connection. The current API materializes at most 100,000 rows; future cursors will allow streaming. There are no prepared/parameterized statements in this version, so it is not suitable as a web application's direct unescaped SQL endpoint.
