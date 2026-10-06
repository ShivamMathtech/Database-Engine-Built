#include "cdb/repl.h"
#include <string.h>
#include <unistd.h>
int main(int argc, char **argv) {
    if (argc == 2 && (!strcmp(argv[1], "--version"))) {
        puts("CDB v" CDB_VERSION);
        return 0;
    }
    if (argc < 2 || argc > 4 || (argc == 3) || (argc == 4 && strcmp(argv[2], "--file")) ||
        !strcmp(argv[1], "--help")) {
        fprintf(argc < 2 ? stderr : stdout,
                "Usage: %s DATABASE [--file SCRIPT.sql]\n       %s --version\n", argv[0], argv[0]);
        return argc == 2 && !strcmp(argv[1], "--help") ? 0 : 2;
    }
    FILE *input = stdin;
    if (argc == 4) {
        input = fopen(argv[3], "rb");
        if (!input) {
            perror("Cannot open SQL script");
            return 1;
        }
    }
    Cdb *db = NULL;
    CdbError error = {0};
    if (!cdb_open(argv[1], &db, &error)) {
        fprintf(stderr, "CDB %s: %s\n", cdb_code_name(error.code), error.message);
        if (input != stdin)
            fclose(input);
        return 1;
    }
    bool interactive = input == stdin && isatty(STDIN_FILENO) != 0;
    if (interactive)
        printf("CDB v%s\nDatabase: %s\nType .help for commands.\n\n", CDB_VERSION, argv[1]);
    int status = cdb_repl(db, input, stdout, interactive);
    cdb_close(db);
    if (input != stdin)
        fclose(input);
    return status;
}
