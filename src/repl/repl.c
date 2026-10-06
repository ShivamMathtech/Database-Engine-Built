#include "cdb/repl.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#define DISPLAY_WIDTH 72u
static void display(const CdbValue *v, char *out) {
    if (v->type == CDB_NULL)
        strcpy(out, "NULL");
    else if (v->type == CDB_INTEGER)
        snprintf(out, DISPLAY_WIDTH + 1, "%" PRId64, v->as.integer);
    else if (v->type == CDB_FLOAT)
        snprintf(out, DISPLAY_WIDTH + 1, "%.17g", v->as.real);
    else if (v->type == CDB_BOOLEAN)
        strcpy(out, v->as.boolean ? "TRUE" : "FALSE");
    else {
        size_t at = 0, i = 0;
        for (; i < v->as.text.length && at < DISPLAY_WIDTH - 4; i++) {
            unsigned char c = (unsigned char)v->as.text.data[i];
            if (c < 32u || c == 127u) {
                out[at++] = '\\';
                out[at++] = c == '\n' ? 'n' : c == '\r' ? 'r' : c == '\t' ? 't' : '?';
            } else
                out[at++] = (char)c;
        }
        if (i < v->as.text.length) {
            memcpy(out + at, "...", 3);
            at += 3;
        }
        out[at] = '\0';
    }
}
static void border(FILE *out, const size_t *width, uint16_t count) {
    fputc('+', out);
    for (uint16_t i = 0; i < count; i++) {
        for (size_t j = 0; j < width[i] + 2; j++)
            fputc('-', out);
        fputc('+', out);
    }
    fputc('\n', out);
}
void cdb_result_print(FILE *out, const CdbResult *r) {
    if (!r->column_count) {
        fprintf(out, "%s\n", r->message);
        return;
    }
    size_t widths[CDB_MAX_COLUMNS] = {0};
    char text[DISPLAY_WIDTH + 1];
    for (uint16_t j = 0; j < r->column_count; j++)
        widths[j] = strlen(r->columns[j]);
    for (size_t i = 0; i < r->row_count; i++)
        for (uint16_t j = 0; j < r->column_count; j++) {
            display(&r->rows[i].values[j], text);
            size_t n = strlen(text);
            if (n > widths[j])
                widths[j] = n;
        }
    border(out, widths, r->column_count);
    fputc('|', out);
    for (uint16_t j = 0; j < r->column_count; j++)
        fprintf(out, " %-*s |", (int)widths[j], r->columns[j]);
    fputc('\n', out);
    border(out, widths, r->column_count);
    for (size_t i = 0; i < r->row_count; i++) {
        fputc('|', out);
        for (uint16_t j = 0; j < r->column_count; j++) {
            display(&r->rows[i].values[j], text);
            fprintf(out, " %-*s |", (int)widths[j], text);
        }
        fputc('\n', out);
    }
    border(out, widths, r->column_count);
    fprintf(out, "%zu rows returned.\n", r->row_count);
}
static bool append(char **text, size_t *n, size_t *cap, int c) {
    if (*n >= CDB_MAX_SQL)
        return false;
    if (*n + 1 >= *cap) {
        size_t size = *cap ? *cap * 2 : 4096;
        if (size > CDB_MAX_SQL + 1u)
            size = CDB_MAX_SQL + 1u;
        char *p = realloc(*text, size);
        if (!p)
            return false;
        *text = p;
        *cap = size;
    }
    (*text)[(*n)++] = (char)c;
    (*text)[*n] = '\0';
    return true;
}
static bool execute(Cdb *db, const char *text, FILE *out) {
    CdbResult result = {0};
    CdbError error = {0};
    bool ok = cdb_execute(db, text, &result, &error);
    if (ok)
        cdb_result_print(out, &result);
    else
        fprintf(stderr, "CDB %s: %s\n", cdb_code_name(error.code), error.message);
    cdb_result_free(&result);
    return ok;
}
int cdb_repl(Cdb *db, FILE *input, FILE *out, bool interactive) {
    char *sql = NULL;
    size_t n = 0, cap = 0;
    bool quote = false, line_comment = false, block_comment = false, has_sql = false,
         start_line = true, meta = false;
    int status = 0;
    for (;;) {
        if (start_line && interactive) {
            fputs(has_sql ? " ...> " : "cdb> ", out);
            fflush(out);
        }
        start_line = false;
        int c = fgetc(input);
        if (c == EOF)
            break;
        if (!n && c == '.')
            meta = true;
        if (!append(&sql, &n, &cap, c)) {
            fputs("CDB LIMIT: input exceeds 1 MiB or memory is exhausted\n", stderr);
            status = 1;
            goto done;
        }
        if (meta) {
            if (c == '\n') {
                while (n && isspace((unsigned char)sql[n - 1]))
                    sql[--n] = '\0';
                if (strcmp(sql, ".quit") == 0 || strcmp(sql, ".exit") == 0)
                    goto done;
                CdbError error = {0};
                if (!cdb_meta(db, sql, out, &error)) {
                    fprintf(stderr, "CDB %s: %s\n", cdb_code_name(error.code), error.message);
                    if (!interactive) {
                        status = 1;
                        goto done;
                    }
                }
                n = 0;
                meta = false;
                start_line = true;
            }
            continue;
        }
        if (line_comment) {
            if (c == '\n')
                line_comment = false;
        } else if (block_comment) {
            if (c == '*') {
                int next = fgetc(input);
                if (next == '/') {
                    if (!append(&sql, &n, &cap, next)) {
                        status = 1;
                        goto done;
                    }
                    block_comment = false;
                } else if (next != EOF)
                    (void)ungetc(next, input);
            }
        } else if (quote) {
            if (c == '\'') {
                int next = fgetc(input);
                if (next == '\'') {
                    if (!append(&sql, &n, &cap, next)) {
                        status = 1;
                        goto done;
                    }
                } else {
                    quote = false;
                    if (next != EOF)
                        (void)ungetc(next, input);
                }
            }
        } else {
            if (c == '-' || c == '/') {
                int next = fgetc(input);
                bool comment = (c == '-' && next == '-') || (c == '/' && next == '*');
                if (comment) {
                    if (!append(&sql, &n, &cap, next)) {
                        status = 1;
                        goto done;
                    }
                    line_comment = c == '-';
                    block_comment = c == '/';
                } else {
                    if (next != EOF)
                        (void)ungetc(next, input);
                    has_sql = true;
                }
            } else if (c == '\'') {
                quote = true;
                has_sql = true;
            } else if (c == ';') {
                if (has_sql && !execute(db, sql, out) && !interactive) {
                    status = 1;
                    goto done;
                }
                n = 0;
                has_sql = false;
            } else if (!isspace((unsigned char)c))
                has_sql = true;
        }
        if (c == '\n') {
            start_line = true;
            if (!has_sql && !block_comment)
                n = 0;
        }
    }
    if (ferror(input)) {
        fputs("CDB IO: input read failed\n", stderr);
        status = 1;
    } else if (meta) {
        while (n && isspace((unsigned char)sql[n - 1]))
            sql[--n] = '\0';
        if (strcmp(sql, ".quit") && strcmp(sql, ".exit")) {
            CdbError e = {0};
            if (!cdb_meta(db, sql, out, &e)) {
                fprintf(stderr, "CDB: %s\n", e.message);
                status = 1;
            }
        }
    } else if (quote || block_comment) {
        fputs("CDB SYNTAX: unexpected EOF inside string/comment\n", stderr);
        status = 1;
    } else if (has_sql && !execute(db, sql, out))
        status = 1;
done:
    free(sql);
    return status;
}
