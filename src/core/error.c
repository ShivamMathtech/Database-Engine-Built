#include "cdb/error.h"
#include <stdarg.h>
#include <string.h>
void cdb_error_clear(CdbError *e) {
    if (e)
        memset(e, 0, sizeof(*e));
}
bool cdb_fail(CdbError *e, CdbCode code, const char *fmt, ...) {
    if (e) {
        va_list args;
        e->code = code;
        va_start(args, fmt);
        (void)vsnprintf(e->message, sizeof(e->message), fmt, args);
        va_end(args);
    }
    return false;
}
const char *cdb_code_name(CdbCode code) {
    static const char *const names[] = {
        "OK",     "IO",      "MEMORY", "SYNTAX",      "TYPE", "CONSTRAINT",  "NOT_FOUND",
        "EXISTS", "CORRUPT", "LIMIT",  "TRANSACTION", "BUSY", "UNSUPPORTED", "INTERNAL"};
    return (unsigned)code < sizeof(names) / sizeof(names[0]) ? names[code] : "UNKNOWN";
}
