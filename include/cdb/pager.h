#ifndef CDB_PAGER_H
#define CDB_PAGER_H
#include "error.h"
typedef struct {
    int fd;
    int wal_fd;
    char *path;
    uint64_t reads, writes;
} CdbPager;
bool cdb_pager_open(CdbPager *p, const char *path, CdbError *e);
void cdb_pager_close(CdbPager *p);
bool cdb_pager_read(CdbPager *p, uint32_t id, uint8_t *out, CdbError *e);
bool cdb_pager_write(CdbPager *p, uint32_t id, const uint8_t *data, CdbError *e);
bool cdb_io_read(int fd, void *p, size_t n, uint64_t offset, CdbError *e);
bool cdb_io_write(int fd, const void *p, size_t n, uint64_t offset, CdbError *e);
bool cdb_io_sync(int fd, CdbError *e);
bool cdb_io_size(int fd, uint64_t *n, CdbError *e);
#endif
