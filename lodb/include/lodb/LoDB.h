#pragma once

#include <lofs/LoFS.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <pb.h>
#include <string>
#include <vector>

/*
 * LoDB exposes `pb_msgdesc_t*` in its public API. Callers who supply descriptors
 * must compile with the same nanopb descriptor-width setting as lo-star's internals
 * (by default: 16-bit field descriptors; i.e. `PB_FIELD_32BIT` unset). Any mismatch
 * between the caller's descriptor layout and lodb's reader is a classic ODR hazard.
 */

/**
 * LoDB - Synchronous Protobuf Database
 *
 * Logical paths are `<mount>/l/<6-b64url>/<file>.pr` (e.g. mount `/__ext__`, `/__int__`). LoFS strips the
 * virtual mount prefix before SPIFFS/LittleFS; only that on-device path must respect the backend
 * name limit (e.g. SPIFFS 31 chars + NUL). The table directory is a fixed-width 6-char base64url
 * token from FNV-1a(db,table), and record filenames are `{11-base64url}.pr`; mount prefixes are
 * unchanged.
 */

#ifndef LODB_VERSION
#define LODB_VERSION "1.5.0"
#endif

#if __has_include(<lolog/LoLog.h>)
#include <lolog/LoLog.h>
#ifndef LODB_LOG_DEBUG
#define LODB_LOG_DEBUG(...) ::lolog::LoLog::debug("lodb", __VA_ARGS__)
#endif
#ifndef LODB_LOG_INFO
#define LODB_LOG_INFO(...) ::lolog::LoLog::info("lodb", __VA_ARGS__)
#endif
#ifndef LODB_LOG_WARN
#define LODB_LOG_WARN(...) ::lolog::LoLog::warn("lodb", __VA_ARGS__)
#endif
#ifndef LODB_LOG_ERROR
#define LODB_LOG_ERROR(...) ::lolog::LoLog::error("lodb", __VA_ARGS__)
#endif
#else
#ifndef LODB_LOG_DEBUG
#define LODB_LOG_DEBUG(...) ((void)0)
#endif
#ifndef LODB_LOG_INFO
#define LODB_LOG_INFO(...) ((void)0)
#endif
#ifndef LODB_LOG_WARN
#define LODB_LOG_WARN(...) ((void)0)
#endif
#ifndef LODB_LOG_ERROR
#define LODB_LOG_ERROR(...) ((void)0)
#endif
#endif

typedef uint64_t lodb_uuid_t;

#define LODB_UUID_FMT "%08x%08x"
#define LODB_UUID_ARGS(uuid) (uint32_t)((uuid) >> 32), (uint32_t)((uuid)&0xFFFFFFFF)

typedef enum {
    LODB_OK = 0,
    LODB_ERR_NOT_FOUND,
    LODB_ERR_IO,
    LODB_ERR_DECODE,
    LODB_ERR_ENCODE,
    LODB_ERR_INVALID
} LoDbError;

typedef std::function<bool(const void *)> LoDbFilter;
typedef std::function<int(const void *, const void *)> LoDbComparator;

void lodb_uuid_to_hex(lodb_uuid_t uuid, char hex_out[17]);

/** Weak by default (`millis()`); override with a strong definition for wall time. */
uint32_t lodb_now_ms(void);

#if defined(LODB_TEST)
bool lodb_run_selftest(const char *mount = "/__ext__");
#endif

class LoDb
{
  public:
    /** @param mount VFS-only prefix (e.g. `/__ext__`); stripped by LoFS before the real FS path. */
    LoDb(const char *db_name, const char *mount = "/__ext__");
    ~LoDb();

    LoDbError registerTable(const char *table_name, const pb_msgdesc_t *pb_descriptor, size_t record_size);
    LoDbError insert(const char *table_name, lodb_uuid_t uuid, const void *record);
    LoDbError insert(const char *table_name, const char *key, const void *record);
    LoDbError get(const char *table_name, lodb_uuid_t uuid, void *record_out, const char *debug_key = nullptr);
    LoDbError get(const char *table_name, const char *key, void *record_out);
    LoDbError update(const char *table_name, lodb_uuid_t uuid, const void *record);
    LoDbError update(const char *table_name, const char *key, const void *record);
    LoDbError deleteRecord(const char *table_name, lodb_uuid_t uuid);
    LoDbError deleteRecord(const char *table_name, const char *key);
    std::vector<void *> select(const char *table_name, LoDbFilter filter = LoDbFilter(),
                               LoDbComparator comparator = LoDbComparator(), size_t limit = 0);
    static void freeRecords(std::vector<void *> &records);
    int count(const char *table_name, LoDbFilter filter = LoDbFilter());
    LoDbError truncate(const char *table_name);
    LoDbError drop(const char *table_name);

  private:
    struct TableMetadata {
        std::string table_name;
        const pb_msgdesc_t *pb_descriptor;
        size_t record_size;
        char table_path[160];
        char ns_token[7]; /* 6 base64url + NUL from FNV-1a(db_name + ':' + table_name) */
    };

    std::string db_name;
    char db_path[128];
    std::map<std::string, TableMetadata> tables;

    lodb_uuid_t keyToUuid(const char *key) const;
    TableMetadata *getTable(const char *table_name);
};
