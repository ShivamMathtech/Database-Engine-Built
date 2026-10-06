#ifndef CDB_COMMON_H
#define CDB_COMMON_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#define CDB_VERSION "0.1.0"
#define CDB_PAGE_SIZE 4096u
#define CDB_PAGE_END 4092u
#define CDB_PAGE_HEADER 24u
#define CDB_MAX_RECORD (CDB_PAGE_END - CDB_PAGE_HEADER - 4u)
#define CDB_NAME_MAX 31u
#define CDB_MAX_COLUMNS 16u
#define CDB_MAX_TABLES 32u
#define CDB_MAX_PAGES 1048576u
#define CDB_MAX_TX_PAGES 65536u
#define CDB_CACHE_FRAMES 64u
#define CDB_MAX_SQL 1048576u
#define CDB_MAX_EXPR_DEPTH 64u
#define CDB_MAX_RESULT_ROWS 100000u
#define CDB_NO_PAGE UINT32_MAX
#define CDB_TRY(x)                                                                                 \
    do {                                                                                           \
        if (!(x))                                                                                  \
            return false;                                                                          \
    } while (0)
uint16_t cdb_get16(const uint8_t *p);
uint32_t cdb_get32(const uint8_t *p);
uint64_t cdb_get64(const uint8_t *p);
void cdb_put16(uint8_t *p, uint16_t v);
void cdb_put32(uint8_t *p, uint32_t v);
void cdb_put64(uint8_t *p, uint64_t v);
uint32_t cdb_hash(uint32_t seed, const void *data, size_t n);
void cdb_seal(uint8_t *page);
bool cdb_checksum(const uint8_t *page);
bool cdb_identifier(const char *s);
void cdb_lower(char *s);
#endif
