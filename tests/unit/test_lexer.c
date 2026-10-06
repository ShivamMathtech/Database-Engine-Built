#include "cdb/lexer.h"
#include "test.h"
bool test_lexer(void) {
    CdbError e = {0};
    CdbLexer l;
    CdbToken t;
    cdb_lexer_init(&l, "SELECT id, name FROM users WHERE age >= 18;");
    const CdbTokenType expected[] = {TK_SELECT, TK_IDENT, TK_COMMA, TK_IDENT,   TK_FROM, TK_IDENT,
                                     TK_WHERE,  TK_IDENT, TK_GE,    TK_INTEGER, TK_SEMI, TK_EOF};
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        OK(cdb_lexer_next(&l, &t, &e));
        CHECK(t.type == expected[i]);
        if (i == 1)
            CHECK(t.position == 7);
    }
    cdb_lexer_init(&l, "-- comment\n /* block */ 'it''s' .5 1.25e-3 != <> <= + - * / ( )");
    const CdbTokenType rest[] = {TK_STRING, TK_FLOAT,  TK_FLOAT, TK_NE,   TK_NE,
                                 TK_LE,     TK_PLUS,   TK_MINUS, TK_STAR, TK_SLASH,
                                 TK_LPAREN, TK_RPAREN, TK_EOF};
    for (size_t i = 0; i < sizeof(rest) / sizeof(rest[0]); i++) {
        OK(cdb_lexer_next(&l, &t, &e));
        CHECK(t.type == rest[i]);
    }
    const char *bad[] = {"'unterminated", "1e+", "@", "!", "/* unfinished"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        cdb_lexer_init(&l, bad[i]);
        CHECK(!cdb_lexer_next(&l, &t, &e));
        CHECK(e.code == CDB_ERR_SYNTAX);
    }
    return true;
}
