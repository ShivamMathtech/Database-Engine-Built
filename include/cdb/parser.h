#ifndef CDB_PARSER_H
#define CDB_PARSER_H
#include "ast.h"
/* Exactly one SQL statement; optional trailing semicolon. Result owns its AST. */ bool
cdb_parse(const char *sql, CdbQuery *query, CdbError *e);
#endif
