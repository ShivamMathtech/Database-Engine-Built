#include "cdb/lexer.h"
#include <ctype.h>
#include <string.h>
typedef struct {
    const char *word;
    CdbTokenType type;
} Keyword;
static const Keyword keywords[] = {{"create", TK_CREATE},     {"table", TK_TABLE},
                                   {"drop", TK_DROP},         {"insert", TK_INSERT},
                                   {"into", TK_INTO},         {"values", TK_VALUES},
                                   {"select", TK_SELECT},     {"from", TK_FROM},
                                   {"where", TK_WHERE},       {"update", TK_UPDATE},
                                   {"set", TK_SET},           {"delete", TK_DELETE},
                                   {"and", TK_AND},           {"or", TK_OR},
                                   {"not", TK_NOT},           {"is", TK_IS},
                                   {"null", TK_NULL},         {"true", TK_TRUE},
                                   {"false", TK_FALSE},       {"integer", TK_INT_TYPE},
                                   {"int", TK_INT_TYPE},      {"float", TK_FLOAT_TYPE},
                                   {"real", TK_FLOAT_TYPE},   {"text", TK_TEXT_TYPE},
                                   {"boolean", TK_BOOL_TYPE}, {"bool", TK_BOOL_TYPE},
                                   {"primary", TK_PRIMARY},   {"key", TK_KEY},
                                   {"index", TK_INDEX},       {"on", TK_ON},
                                   {"begin", TK_BEGIN},       {"commit", TK_COMMIT},
                                   {"rollback", TK_ROLLBACK}, {"explain", TK_EXPLAIN},
                                   {"limit", TK_LIMIT}};
void cdb_lexer_init(CdbLexer *l, const char *sql) {
    l->sql = sql;
    l->length = strlen(sql);
    l->cursor = 0;
}
static bool lex_error(CdbLexer *l, CdbError *e, const char *message) {
    if (e)
        e->position = l->cursor;
    return cdb_fail(e, CDB_ERR_SYNTAX, "%s at byte %zu", message, l->cursor);
}
bool cdb_lexer_next(CdbLexer *l, CdbToken *t, CdbError *e) {
    const char *s = l->sql;
    for (;;) {
        while (l->cursor < l->length && isspace((unsigned char)s[l->cursor]))
            l->cursor++;
        size_t i = l->cursor;
        if (i + 1 < l->length && s[i] == '-' && s[i + 1] == '-') {
            l->cursor += 2;
            while (l->cursor < l->length && s[l->cursor] != '\n')
                l->cursor++;
            continue;
        }
        if (i + 1 < l->length && s[i] == '/' && s[i + 1] == '*') {
            l->cursor += 2;
            while (l->cursor + 1 < l->length && !(s[l->cursor] == '*' && s[l->cursor + 1] == '/'))
                l->cursor++;
            if (l->cursor + 1 >= l->length)
                return lex_error(l, e, "Unterminated block comment");
            l->cursor += 2;
            continue;
        }
        break;
    }
    size_t start = l->cursor;
    t->position = start;
    t->start = s + start;
    t->length = 0;
    t->type = TK_EOF;
    if (start == l->length)
        return true;
    unsigned char c = (unsigned char)s[l->cursor++];
    if ((c < 128u && isalpha(c)) || c == '_') {
        while (l->cursor < l->length) {
            unsigned char x = (unsigned char)s[l->cursor];
            if (x >= 128u || !(isalnum(x) || x == '_'))
                break;
            l->cursor++;
        }
        t->type = TK_IDENT;
        t->length = l->cursor - start;
        for (size_t k = 0; k < sizeof(keywords) / sizeof(keywords[0]); k++) {
            if (strlen(keywords[k].word) != t->length)
                continue;
            size_t j = 0;
            while (j < t->length && tolower((unsigned char)t->start[j]) == keywords[k].word[j])
                j++;
            if (j == t->length) {
                t->type = keywords[k].type;
                break;
            }
        }
        return true;
    }
    if (isdigit(c) || (c == '.' && l->cursor < l->length && isdigit((unsigned char)s[l->cursor]))) {
        bool real = c == '.';
        while (l->cursor < l->length && isdigit((unsigned char)s[l->cursor]))
            l->cursor++;
        if (!real && l->cursor < l->length && s[l->cursor] == '.') {
            real = true;
            l->cursor++;
            while (l->cursor < l->length && isdigit((unsigned char)s[l->cursor]))
                l->cursor++;
        }
        if (l->cursor < l->length && (s[l->cursor] == 'e' || s[l->cursor] == 'E')) {
            real = true;
            l->cursor++;
            if (l->cursor < l->length && (s[l->cursor] == '+' || s[l->cursor] == '-'))
                l->cursor++;
            size_t digits = l->cursor;
            while (l->cursor < l->length && isdigit((unsigned char)s[l->cursor]))
                l->cursor++;
            if (digits == l->cursor)
                return lex_error(l, e, "Missing exponent digits");
        }
        t->type = real ? TK_FLOAT : TK_INTEGER;
        t->length = l->cursor - start;
        return true;
    }
    if (c == '\'') {
        bool ended = false;
        while (l->cursor < l->length) {
            if (s[l->cursor++] == '\'') {
                if (l->cursor < l->length && s[l->cursor] == '\'') {
                    l->cursor++;
                    continue;
                }
                ended = true;
                break;
            }
        }
        if (!ended)
            return lex_error(l, e, "Unterminated string");
        t->type = TK_STRING;
        t->length = l->cursor - start;
        return true;
    }
    switch (c) {
    case '(':
        t->type = TK_LPAREN;
        break;
    case ')':
        t->type = TK_RPAREN;
        break;
    case ',':
        t->type = TK_COMMA;
        break;
    case ';':
        t->type = TK_SEMI;
        break;
    case '*':
        t->type = TK_STAR;
        break;
    case '+':
        t->type = TK_PLUS;
        break;
    case '-':
        t->type = TK_MINUS;
        break;
    case '/':
        t->type = TK_SLASH;
        break;
    case '=':
        t->type = TK_EQ;
        break;
    case '!':
        if (l->cursor < l->length && s[l->cursor] == '=') {
            l->cursor++;
            t->type = TK_NE;
            break;
        }
        return lex_error(l, e, "Expected !=");
    case '<':
        t->type = TK_LT;
        if (l->cursor < l->length && s[l->cursor] == '=') {
            l->cursor++;
            t->type = TK_LE;
        } else if (l->cursor < l->length && s[l->cursor] == '>') {
            l->cursor++;
            t->type = TK_NE;
        }
        break;
    case '>':
        t->type = TK_GT;
        if (l->cursor < l->length && s[l->cursor] == '=') {
            l->cursor++;
            t->type = TK_GE;
        }
        break;
    default:
        return lex_error(l, e, "Invalid character");
    }
    t->length = l->cursor - start;
    return true;
}
