#include "cdb/cdb.h"
#include <stdio.h>
int main(void) {
    Cdb *db = NULL;
    CdbError error = {0};
    CdbResult result = {0};
    if (!cdb_open("embedded-demo.cdb", &db, &error)) {
        fprintf(stderr, "%s\n", error.message);
        return 1;
    }
    /* The schema intentionally persists between example runs. */
    if (!cdb_execute(db, "CREATE TABLE demo (id INTEGER PRIMARY KEY, name TEXT)", &result,
                     &error) &&
        error.code != CDB_ERR_EXISTS)
        goto fail;
    cdb_result_free(&result);
    const char *statements[] = {"BEGIN", "INSERT INTO demo VALUES (1, 'Shivam')",
                                "SELECT * FROM demo", "ROLLBACK"};
    for (size_t i = 0; i < sizeof(statements) / sizeof(statements[0]); i++) {
        if (!cdb_execute(db, statements[i], &result, &error))
            goto fail;
        if (result.row_count) {
            printf("Hello, %s!\n", result.rows[0].values[1].as.text.data);
        }
        cdb_result_free(&result);
    }
    cdb_close(db);
    return 0;
fail:
    fprintf(stderr, "%s\n", error.message);
    cdb_result_free(&result);
    cdb_close(db);
    return 1;
}
