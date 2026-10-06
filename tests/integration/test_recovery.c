#include "test.h"
#include <fcntl.h>
#include <sys/wait.h>
static bool crash_commit(Fixture *f, const char *point) {
    cdb_close(f->db);
    f->db = NULL;
    fflush(NULL);
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        Cdb *db = NULL;
        CdbError e = {0};
        if (!cdb_open(f->path, &db, &e))
            _exit(90);
        db->log_level = -1;
        if (!sql_ok(db, "BEGIN") || !sql_ok(db, "INSERT INTO t VALUES (2,'committed after crash')"))
            _exit(91);
        if (setenv("CDB_CRASH_POINT", point, 1))
            _exit(92);
        if (!sql_ok(db, "COMMIT"))
            _exit(93);
        cdb_close(db);
        _exit(94);
    }
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 86);
    return true;
}
bool test_recovery(void) {
    const char *points[] = {"wal_header", "wal_records", "wal_sync", "mid_apply", "db_sync"};
    for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); i++) {
        Fixture f;
        CHECK(fixture_open(&f));
        CHECK(sql_ok(f.db, "CREATE TABLE t (id INTEGER PRIMARY KEY, note TEXT)"));
        CHECK(sql_ok(f.db, "INSERT INTO t VALUES (1,'original')"));
        CHECK(crash_commit(&f, points[i]));
        CHECK(fixture_reopen(&f));
        CHECK(sql_rows(f.db, "SELECT * FROM t", i < 2 ? 1 : 2));
        uint64_t size;
        CdbError e = {0};
        OK(cdb_io_size(f.db->pager.wal_fd, &size, &e));
        CHECK(size == 0);
        CHECK(fixture_reopen(&f));
        CHECK(sql_rows(f.db, "SELECT * FROM t", i < 2 ? 1 : 2));
        fixture_close(&f);
    }
    return true;
}
bool test_corruption(void) {
    Fixture f;
    CHECK(fixture_open(&f));
    CHECK(sql_ok(f.db, "CREATE TABLE t (id INTEGER PRIMARY KEY, note TEXT)"));
    CHECK(sql_ok(f.db, "INSERT INTO t VALUES (1,'original')"));
    CdbError e = {0};
    uint8_t page[CDB_PAGE_SIZE];
    OK(cdb_pager_read(&f.db->pager, 2, page, &e));
    page[100] ^= 1;
    OK(cdb_io_write(f.db->pager.fd, page, sizeof(page), 2u * CDB_PAGE_SIZE, &e));
    cdb_close(f.db);
    f.db = NULL;
    CHECK(!cdb_open(f.path, &f.db, &e));
    CHECK(e.code == CDB_ERR_CORRUPT);
    fixture_close(&f);
    CHECK(fixture_open(&f));
    CHECK(sql_ok(f.db, "CREATE TABLE t (id INTEGER PRIMARY KEY, note TEXT)"));
    CHECK(crash_commit(&f, "wal_sync"));
    char wal[140];
    snprintf(wal, sizeof(wal), "%s.wal", f.path);
    int fd = open(wal, O_RDWR);
    CHECK(fd >= 0);
    uint8_t byte;
    OK(cdb_io_read(fd, &byte, 1, 100, &e));
    byte ^= 1;
    OK(cdb_io_write(fd, &byte, 1, 100, &e));
    CHECK(close(fd) == 0);
    CHECK(!cdb_open(f.path, &f.db, &e));
    CHECK(e.code == CDB_ERR_CORRUPT);
    fixture_close(&f);
    /* Check format version rejection even when a corrupted header is resealed. */ CHECK(
        fixture_open(&f));
    OK(cdb_pager_read(&f.db->pager, 0, page, &e));
    cdb_put32(page + 4, 99);
    cdb_seal(page);
    OK(cdb_io_write(f.db->pager.fd, page, sizeof(page), 0, &e));
    cdb_close(f.db);
    f.db = NULL;
    CHECK(!cdb_open(f.path, &f.db, &e));
    CHECK(e.code == CDB_ERR_CORRUPT);
    fixture_close(&f);
    /* Do not let an attacker turn WAL truncation into database truncation. */
    CHECK(fixture_open(&f));
    cdb_close(f.db);
    f.db = NULL;
    snprintf(wal, sizeof(wal), "%s.wal", f.path);
    CHECK(unlink(wal) == 0);
    CHECK(link(f.path, wal) == 0);
    CHECK(!cdb_open(f.path, &f.db, &e));
    CHECK(e.code == CDB_ERR_IO);
    int data_fd = open(f.path, O_RDONLY);
    CHECK(data_fd >= 0);
    uint64_t remaining;
    OK(cdb_io_size(data_fd, &remaining, &e));
    CHECK(remaining == 2u * CDB_PAGE_SIZE);
    CHECK(close(data_fd) == 0);
    fixture_close(&f);
    return true;
}
