#ifndef CDB_LEXER_H
#define CDB_LEXER_H
#include "error.h"
typedef enum {
    TK_EOF,
    TK_IDENT,
    TK_INTEGER,
    TK_FLOAT,
    TK_STRING,
    TK_LPAREN,
    TK_RPAREN,
    TK_COMMA,
    TK_SEMI,
    TK_STAR,
    TK_PLUS,
    TK_MINUS,
    TK_SLASH,
    TK_EQ,
    TK_NE,
    TK_LT,
    TK_LE,
    TK_GT,
    TK_GE,
    TK_CREATE,
    TK_TABLE,
    TK_DROP,
    TK_INSERT,
    TK_INTO,
    TK_VALUES,
    TK_SELECT,
    TK_FROM,
    TK_WHERE,
    TK_UPDATE,
    TK_SET,
    TK_DELETE,
    TK_AND,
    TK_OR,
    TK_NOT,
    TK_IS,
    TK_NULL,
    TK_TRUE,
    TK_FALSE,
    TK_INT_TYPE,
    TK_FLOAT_TYPE,
    TK_TEXT_TYPE,
    TK_BOOL_TYPE,
    TK_PRIMARY,
    TK_KEY,
    TK_INDEX,
    TK_ON,
    TK_BEGIN,
    TK_COMMIT,
    TK_ROLLBACK,
    TK_EXPLAIN,
    TK_LIMIT
} CdbTokenType;
typedef struct {
    CdbTokenType type;
    const char *start;
    size_t length, position;
} CdbToken;
typedef struct {
    const char *sql;
    size_t length, cursor;
} CdbLexer;
void cdb_lexer_init(CdbLexer *l, const char *sql);
bool cdb_lexer_next(CdbLexer *l, CdbToken *token, CdbError *e);
#endif
