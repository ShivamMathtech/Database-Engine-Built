#include "cdb/pager.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
bool cdb_io_read(int fd, void *buffer, size_t n, uint64_t offset, CdbError *e) {
    uint8_t *p = buffer;
    while (n) {
        ssize_t got = pread(fd, p, n, (off_t)offset);
        if (got < 0 && errno == EINTR)
            continue;
        if (got < 0)
            return cdb_fail(e, CDB_ERR_IO, "Read failed: %s", strerror(errno));
        if (!got)
            return cdb_fail(e, CDB_ERR_CORRUPT, "Unexpected end of file at byte %llu",
                            (unsigned long long)offset);
        p += (size_t)got;
        n -= (size_t)got;
        offset += (uint64_t)got;
    }
    return true;
}
bool cdb_io_write(int fd, const void *buffer, size_t n, uint64_t offset, CdbError *e) {
    const uint8_t *p = buffer;
    while (n) {
        ssize_t put = pwrite(fd, p, n, (off_t)offset);
        if (put < 0 && errno == EINTR)
            continue;
        if (put <= 0)
            return cdb_fail(e, CDB_ERR_IO, "Write failed: %s", strerror(errno));
        p += (size_t)put;
        n -= (size_t)put;
        offset += (uint64_t)put;
    }
    return true;
}
bool cdb_io_sync(int fd, CdbError *e) {
    int rc;
    do {
        rc = fsync(fd);
    } while (rc < 0 && errno == EINTR);
    return rc == 0 || cdb_fail(e, CDB_ERR_IO, "fsync failed: %s", strerror(errno));
}
bool cdb_io_size(int fd, uint64_t *n, CdbError *e) {
    struct stat st;
    if (fstat(fd, &st) < 0)
        return cdb_fail(e, CDB_ERR_IO, "fstat failed: %s", strerror(errno));
    if (!S_ISREG(st.st_mode) || st.st_size < 0)
        return cdb_fail(e, CDB_ERR_IO, "Database and WAL must be regular files");
    *n = (uint64_t)st.st_size;
    return true;
}
void cdb_pager_close(CdbPager *p) {
    if (p->wal_fd >= 0)
        (void)close(p->wal_fd);
    if (p->fd >= 0)
        (void)close(p->fd);
    free(p->path);
    p->path = NULL;
    p->fd = -1;
    p->wal_fd = -1;
}
bool cdb_pager_open(CdbPager *p, const char *path, CdbError *e) {
    memset(p, 0, sizeof(*p));
    p->fd = -1;
    p->wal_fd = -1;
    int flags = O_RDWR | O_CREAT | O_CLOEXEC;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    p->fd = open(path, flags, 0600);
    if (p->fd < 0)
        return cdb_fail(e, CDB_ERR_IO, "Cannot open '%s': %s", path, strerror(errno));
    if (flock(p->fd, LOCK_EX | LOCK_NB) < 0) {
        cdb_fail(e, CDB_ERR_BUSY, "Database is locked by another connection");
        goto fail;
    }
    p->path = realpath(path, NULL);
    if (!p->path) {
        cdb_fail(e, CDB_ERR_IO, "Cannot resolve database path");
        goto fail;
    }
    size_t n = strlen(p->path);
    char *wal = malloc(n + 5);
    if (!wal) {
        cdb_fail(e, CDB_ERR_MEMORY, "Cannot allocate WAL path");
        goto fail;
    }
    memcpy(wal, p->path, n);
    memcpy(wal + n, ".wal", 5);
    p->wal_fd = open(wal, flags, 0600);
    free(wal);
    if (p->wal_fd < 0) {
        cdb_fail(e, CDB_ERR_IO, "Cannot open WAL: %s", strerror(errno));
        goto fail;
    }
    uint64_t size;
    if (!cdb_io_size(p->fd, &size, e) || !cdb_io_size(p->wal_fd, &size, e))
        goto fail;
    struct stat data_stat, wal_stat;
    if (fstat(p->fd, &data_stat) < 0 || fstat(p->wal_fd, &wal_stat) < 0) {
        cdb_fail(e, CDB_ERR_IO, "Cannot inspect file identities");
        goto fail;
    }
    /* A hard-link alias could pair one database inode with different WAL paths,
     * or even make truncating the WAL truncate the database itself. */
    if (data_stat.st_nlink != 1 || wal_stat.st_nlink != 1 ||
        (data_stat.st_dev == wal_stat.st_dev && data_stat.st_ino == wal_stat.st_ino)) {
        cdb_fail(e, CDB_ERR_IO, "Database and WAL must be distinct files without hard links");
        goto fail;
    }
    /* Persist both directory entries before any commit can rely on the WAL. */ char *parent =
        strdup(p->path);
    if (!parent) {
        cdb_fail(e, CDB_ERR_MEMORY, "Cannot allocate parent path");
        goto fail;
    }
    char *slash = strrchr(parent, '/');
    if (slash == parent)
        slash[1] = '\0';
    else if (slash)
        *slash = '\0';
    int dir = open(parent, O_RDONLY | O_CLOEXEC);
    free(parent);
    if (dir < 0) {
        cdb_fail(e, CDB_ERR_IO, "Cannot open parent directory");
        goto fail;
    }
    bool ok = cdb_io_sync(dir, e);
    (void)close(dir);
    if (!ok)
        goto fail;
    return true;
fail:
    cdb_pager_close(p);
    return false;
}
bool cdb_pager_read(CdbPager *p, uint32_t id, uint8_t *out, CdbError *e) {
    if (id >= CDB_MAX_PAGES)
        return cdb_fail(e, CDB_ERR_CORRUPT, "Page id out of range");
    CDB_TRY(cdb_io_read(p->fd, out, CDB_PAGE_SIZE, (uint64_t)id * CDB_PAGE_SIZE, e));
    p->reads++;
    return cdb_checksum(out) || cdb_fail(e, CDB_ERR_CORRUPT, "Checksum mismatch on page %u", id);
}
bool cdb_pager_write(CdbPager *p, uint32_t id, const uint8_t *data, CdbError *e) {
    if (id >= CDB_MAX_PAGES)
        return cdb_fail(e, CDB_ERR_LIMIT, "Page limit exceeded");
    CDB_TRY(cdb_io_write(p->fd, data, CDB_PAGE_SIZE, (uint64_t)id * CDB_PAGE_SIZE, e));
    p->writes++;
    return true;
}
