#include <lodb/LoDB.h>

#include <memory>
#include <Arduino.h>
#include <SHA256.h>
#include <algorithm>
#include <cstring>
#include <pb_decode.h>
#include <pb_encode.h>

namespace {

constexpr char kB64url[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

void lodb_uuid_to_fn11(lodb_uuid_t u, char out[12])
{
    for (int i = 10; i >= 0; --i) {
        out[i] = kB64url[u & 63];
        u >>= 6;
    }
    out[11] = '\0';
}

bool lodb_fn11_to_uuid(const char s[11], lodb_uuid_t *out)
{
    lodb_uuid_t v = 0;
    for (int i = 0; i < 11; ++i) {
        char c = s[i];
        uint32_t x = 256;
        if (c >= 'A' && c <= 'Z')
            x = (uint32_t)(c - 'A');
        else if (c >= 'a' && c <= 'z')
            x = (uint32_t)(c - 'a' + 26);
        else if (c >= '0' && c <= '9')
            x = (uint32_t)(c - '0' + 52);
        else if (c == '-')
            x = 62;
        else if (c == '_')
            x = 63;
        if (x >= 64) return false;
        v = (v << 6) | (lodb_uuid_t)x;
    }
    *out = v;
    return true;
}

void fill_ns_token(const std::string &db, const char *table, char ns_out[7])
{
    uint64_t h = 14695981039346656037ull;  // FNV-1a 64-bit offset basis
    for (unsigned char c : db) {
        h ^= c;
        h *= 1099511628211ull;  // FNV-1a 64-bit prime
    }
    h ^= ':';
    h *= 1099511628211ull;
    for (const char *p = table; *p; ++p) {
        h ^= (unsigned char)*p;
        h *= 1099511628211ull;
    }
    uint64_t v = h & 0xfffffffffull;  // keep 36 bits => 6 base64url chars
    for (int i = 5; i >= 0; --i) {
        ns_out[i] = kB64url[v & 63];
        v >>= 6;
    }
    ns_out[6] = '\0';
}

void record_file_path(const char *table_path, lodb_uuid_t uuid, char *path, size_t cap)
{
    char fn11[12];
    lodb_uuid_to_fn11(uuid, fn11);
    snprintf(path, cap, "%s/%s.pr", table_path, fn11);
}

bool parse_record_filename(const std::string &filename, lodb_uuid_t *uuid_out)
{
    if (filename.size() != 14) return false;
    if (filename.compare(11, 3, ".pr") != 0) return false;
    return lodb_fn11_to_uuid(filename.c_str(), uuid_out);
}

bool record_filename_belongs_table(const std::string &filename)
{
    lodb_uuid_t dummy;
    return parse_record_filename(filename, &dummy);
}

}  // namespace

__attribute__((weak)) uint32_t lodb_now_ms(void)
{
    return static_cast<uint32_t>(millis());
}

void lodb_uuid_to_hex(lodb_uuid_t uuid, char hex_out[17])
{
    snprintf(hex_out, 17, LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
}

static lodb_uuid_t lodb_uuid_from_key(const char *str, uint64_t salt)
{
    char generated_str[32];
    const char *input_str = str;

    if (str == nullptr) {
        uint32_t timestamp = lodb_now_ms();
        uint32_t random_val = (uint32_t)random(0x7fffffff) ^ ((uint32_t)random(0x7fffffff) << 1);
        snprintf(generated_str, sizeof(generated_str), "%u:%u", timestamp, random_val);
        input_str = generated_str;
    }

    SHA256 sha256;
    uint8_t hash[32];

    sha256.reset();
    sha256.update(input_str, strlen(input_str));

    uint8_t salt_bytes[8];
    memcpy(salt_bytes, &salt, 8);
    sha256.update(salt_bytes, 8);

    sha256.finalize(hash, 32);

    lodb_uuid_t uuid;
    memcpy(&uuid, hash, sizeof(lodb_uuid_t));
    return uuid;
}

LoDb::LoDb(const char *db_name, const char *mount) : db_name(db_name)
{
    const char *m = mount && mount[0] ? mount : "/__ext__";
    int n = snprintf(db_path, sizeof(db_path), "%s/l", m);
    if (n <= 0 || (size_t)n >= sizeof(db_path)) {
        db_path[0] = '\0';
        LODB_LOG_ERROR("LoDB: db_path overflow");
        return;
    }

    if (!LoFS::mkdir(m)) {
        LODB_LOG_DEBUG("LoDB: mount dir may exist: %s", m);
    }

    if (!LoFS::mkdir(db_path)) {
        LODB_LOG_DEBUG("LoDB: records dir may already exist: %s", db_path);
    }

    LODB_LOG_INFO("Initialized LoDB database: %s", db_path);
}

LoDb::~LoDb() {}

lodb_uuid_t LoDb::keyToUuid(const char *key) const
{
    return lodb_uuid_from_key(key, 0);
}

LoDbError LoDb::registerTable(const char *table_name, const pb_msgdesc_t *pb_descriptor, size_t record_size)
{
    if (!table_name || !pb_descriptor || record_size == 0) {
        return LODB_ERR_INVALID;
    }

    TableMetadata metadata;
    metadata.table_name = table_name;
    metadata.pb_descriptor = pb_descriptor;
    metadata.record_size = record_size;
    fill_ns_token(db_name, table_name, metadata.ns_token);

    int n = snprintf(metadata.table_path, sizeof(metadata.table_path), "%s/%s", db_path, metadata.ns_token);
    if (n <= 0 || (size_t)n >= sizeof(metadata.table_path)) {
        LODB_LOG_ERROR("Table path overflow: %s", table_name);
        return LODB_ERR_INVALID;
    }
    if (!LoFS::mkdir(metadata.table_path)) {
        LODB_LOG_DEBUG("LoDB: table dir may already exist: %s", metadata.table_path);
    }

    tables[table_name] = metadata;
    LODB_LOG_INFO("Registered table: %s:%s token=%s at %s", db_name.c_str(), table_name,
                  metadata.ns_token, metadata.table_path);
    return LODB_OK;
}

LoDb::TableMetadata *LoDb::getTable(const char *table_name)
{
    auto it = tables.find(table_name);
    if (it == tables.end()) {
        LODB_LOG_ERROR("Table not registered: %s", table_name);
        return nullptr;
    }
    return &it->second;
}

LoDbError LoDb::insert(const char *table_name, lodb_uuid_t uuid, const void *record)
{
    if (!table_name || !record) {
        return LODB_ERR_INVALID;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        return LODB_ERR_INVALID;
    }

    char file_path[192];
    record_file_path(table->table_path, uuid, file_path, sizeof(file_path));

    if (LoFS::exists(file_path)) {
        return LODB_ERR_INVALID;
    }

    constexpr size_t kBufSize = 2048;
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[kBufSize]);
    pb_ostream_t stream = pb_ostream_from_buffer(buffer.get(), kBufSize);

    if (!pb_encode(&stream, table->pb_descriptor, record)) {
        LODB_LOG_ERROR("Failed to encode protobuf for insert");
        return LODB_ERR_ENCODE;
    }

    size_t encoded_size = stream.bytes_written;
    if (!LoFS::writeFileAtomic(file_path, buffer.get(), encoded_size)) {
        LODB_LOG_ERROR("Failed atomic write for insert: %s", file_path);
        return LODB_ERR_IO;
    }
    return LODB_OK;
}

LoDbError LoDb::insert(const char *table_name, const char *key, const void *record)
{
    if (!key) return LODB_ERR_INVALID;
    return insert(table_name, keyToUuid(key), record);
}

LoDbError LoDb::get(const char *table_name, lodb_uuid_t uuid, void *record_out, const char *debug_key)
{
    if (!table_name || !record_out) {
        return LODB_ERR_INVALID;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        return LODB_ERR_INVALID;
    }

    char fn11[12];
    lodb_uuid_to_fn11(uuid, fn11);
    char file_path[192];
    record_file_path(table->table_path, uuid, file_path, sizeof(file_path));
    if (debug_key && debug_key[0]) {
        LODB_LOG_DEBUG("get db=%s table=%s key=%s token=%s rec=%s path=%s", db_name.c_str(),
                       table->table_name.c_str(), debug_key, table->ns_token, fn11, file_path);
    } else {
        LODB_LOG_DEBUG("get db=%s table=%s token=%s rec=%s path=%s", db_name.c_str(), table->table_name.c_str(),
                       table->ns_token, fn11, file_path);
    }

    constexpr size_t kBufSize = 2048;
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[kBufSize]);
    size_t file_size = 0;

    if (!LoFS::readFileAtomic(file_path, buffer.get(), kBufSize, &file_size)) {
        return LODB_ERR_NOT_FOUND;
    }
    if (file_size == 0) {
        LODB_LOG_ERROR("Record file is empty: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        if (LoFS::exists(file_path)) {
            if (LoFS::remove(file_path)) {
                LODB_LOG_WARN("Removed empty record file: %s", file_path);
            } else {
                LODB_LOG_WARN("Failed to remove empty record file: %s", file_path);
            }
        } else {
            LODB_LOG_DEBUG("Empty-record path already absent: %s", file_path);
        }
        return LODB_ERR_IO;
    }

    pb_istream_t stream = pb_istream_from_buffer(buffer.get(), file_size);
    memset(record_out, 0, table->record_size);

    if (!pb_decode(&stream, table->pb_descriptor, record_out)) {
        LODB_LOG_ERROR("Failed to decode protobuf from " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_ERR_DECODE;
    }

    return LODB_OK;
}

LoDbError LoDb::get(const char *table_name, const char *key, void *record_out)
{
    if (!key) return LODB_ERR_INVALID;
    return get(table_name, keyToUuid(key), record_out, key);
}

LoDbError LoDb::update(const char *table_name, lodb_uuid_t uuid, const void *record)
{
    if (!table_name || !record) {
        return LODB_ERR_INVALID;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        return LODB_ERR_INVALID;
    }

    char file_path[192];
    record_file_path(table->table_path, uuid, file_path, sizeof(file_path));

    if (!LoFS::exists(file_path)) {
        return LODB_ERR_NOT_FOUND;
    }

    constexpr size_t kBufSize = 2048;
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[kBufSize]);
    pb_ostream_t stream = pb_ostream_from_buffer(buffer.get(), kBufSize);

    if (!pb_encode(&stream, table->pb_descriptor, record)) {
        LODB_LOG_ERROR("Failed to encode updated record: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_ERR_ENCODE;
    }

    size_t encoded_size = stream.bytes_written;
    if (!LoFS::writeFileAtomic(file_path, buffer.get(), encoded_size)) {
        LODB_LOG_ERROR("Failed atomic write for update: %s", file_path);
        return LODB_ERR_IO;
    }

    LODB_LOG_INFO("Updated record: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
    return LODB_OK;
}

LoDbError LoDb::update(const char *table_name, const char *key, const void *record)
{
    if (!key) return LODB_ERR_INVALID;
    return update(table_name, keyToUuid(key), record);
}

LoDbError LoDb::deleteRecord(const char *table_name, lodb_uuid_t uuid)
{
    if (!table_name) {
        return LODB_ERR_INVALID;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        return LODB_ERR_INVALID;
    }

    char file_path[192];
    record_file_path(table->table_path, uuid, file_path, sizeof(file_path));

    if (LoFS::remove(file_path)) {
        LODB_LOG_DEBUG("Deleted record: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_OK;
    }
    LODB_LOG_WARN("Failed to delete record (may not exist): " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
    return LODB_ERR_NOT_FOUND;
}

LoDbError LoDb::deleteRecord(const char *table_name, const char *key)
{
    if (!key) return LODB_ERR_INVALID;
    return deleteRecord(table_name, keyToUuid(key));
}

std::vector<void *> LoDb::select(const char *table_name, LoDbFilter filter, LoDbComparator comparator, size_t limit)
{
    std::vector<void *> results;

    if (!table_name) {
        LODB_LOG_ERROR("Invalid table_name");
        return results;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        LODB_LOG_ERROR("Table not found: %s", table_name);
        return results;
    }

    File dir = LoFS::open(table->table_path, FILE_O_READ);
    if (!dir) {
        LODB_LOG_DEBUG("Table directory not found: %s", table->table_path);
        return results;
    }

    if (!dir.isDirectory()) {
        LODB_LOG_ERROR("Table path is not a directory: %s", table->table_path);
        dir.close();
        return results;
    }

    while (true) {
        File file = dir.openNextFile();
        if (!file) {
            break;
        }

        if (file.isDirectory()) {
            file.close();
            continue;
        }

        std::string pathStr = file.name();
        file.close();

        size_t lastSlash = pathStr.rfind('/');
        std::string filename = (lastSlash != std::string::npos) ? pathStr.substr(lastSlash + 1) : pathStr;

        lodb_uuid_t uuid;
        if (!parse_record_filename(filename, &uuid)) {
            LODB_LOG_DEBUG("Skipped non-record file: %s", filename.c_str());
            continue;
        }

        uint8_t *record_buffer = new uint8_t[table->record_size];
        if (!record_buffer) {
            LODB_LOG_ERROR("Failed to allocate record buffer");
            continue;
        }

        memset(record_buffer, 0, table->record_size);
        LoDbError err = get(table_name, uuid, record_buffer);

        if (err != LODB_OK) {
            LODB_LOG_WARN("Failed to read record " LODB_UUID_FMT " during select", LODB_UUID_ARGS(uuid));
            delete[] record_buffer;
            continue;
        }

        if (filter && !filter(record_buffer)) {
            LODB_LOG_DEBUG("Record " LODB_UUID_FMT " filtered out", LODB_UUID_ARGS(uuid));
            delete[] record_buffer;
            continue;
        }

        results.push_back(record_buffer);
        LODB_LOG_DEBUG("Added record " LODB_UUID_FMT " to results", LODB_UUID_ARGS(uuid));
    }

    dir.close();

    LODB_LOG_INFO("Select from %s: %u records after filtering", table_name, (unsigned)results.size());

    if (comparator && !results.empty()) {
        std::sort(results.begin(), results.end(),
                  [comparator](const void *a, const void *b) { return comparator(a, b) < 0; });
        LODB_LOG_DEBUG("Sorted %u records", (unsigned)results.size());
    }

    if (limit > 0 && results.size() > limit) {
        for (size_t i = limit; i < results.size(); i++) {
            delete[] (uint8_t *)results[i];
        }
        results.resize(limit);
        LODB_LOG_DEBUG("Limited results to %u records", (unsigned)limit);
    }

    LODB_LOG_INFO("Select from %s complete: %u records returned", table_name, (unsigned)results.size());
    return results;
}

void LoDb::freeRecords(std::vector<void *> &records)
{
    for (auto *recordPtr : records) {
        delete[] (uint8_t *)recordPtr;
    }
    records.clear();
}

int LoDb::count(const char *table_name, LoDbFilter filter)
{
    if (!table_name) {
        LODB_LOG_ERROR("Invalid table_name");
        return -1;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        LODB_LOG_ERROR("Table not found: %s", table_name);
        return -1;
    }

    int cnt = 0;

    if (!filter) {
        File dir = LoFS::open(table->table_path, FILE_O_READ);
        if (!dir) {
            LODB_LOG_DEBUG("Table directory not found: %s", table->table_path);
            return 0;
        }

        if (!dir.isDirectory()) {
            LODB_LOG_ERROR("Table path is not a directory: %s", table->table_path);
            dir.close();
            return -1;
        }

        while (true) {
            File file = dir.openNextFile();
            if (!file) {
                break;
            }

            if (file.isDirectory()) {
                file.close();
                continue;
            }

            std::string pathStr = file.name();
            file.close();

            size_t lastSlash = pathStr.rfind('/');
            std::string filename = (lastSlash != std::string::npos) ? pathStr.substr(lastSlash + 1) : pathStr;

            if (record_filename_belongs_table(filename)) {
                cnt++;
            }
        }

        dir.close();
        LODB_LOG_DEBUG("Counted %d records in %s (no filter)", cnt, table_name);
        return cnt;
    }

    auto results = select(table_name, filter, LoDbComparator(), 0);
    cnt = (int)results.size();
    freeRecords(results);

    LODB_LOG_DEBUG("Counted %d records in %s (with filter)", cnt, table_name);
    return cnt;
}

LoDbError LoDb::truncate(const char *table_name)
{
    if (!table_name) {
        LODB_LOG_ERROR("Invalid table_name");
        return LODB_ERR_INVALID;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        LODB_LOG_ERROR("Table not registered: %s", table_name);
        return LODB_ERR_INVALID;
    }

    File dir = LoFS::open(table->table_path, FILE_O_READ);
    if (!dir) {
        LODB_LOG_DEBUG("Table directory not found: %s (already empty)", table->table_path);
        return LODB_OK;
    }

    if (!dir.isDirectory()) {
        LODB_LOG_ERROR("Table path is not a directory: %s", table->table_path);
        dir.close();
        return LODB_ERR_INVALID;
    }

    int deletedCount = 0;
    while (true) {
        File file = dir.openNextFile();
        if (!file) {
            break;
        }

        if (file.isDirectory()) {
            file.close();
            continue;
        }

        std::string pathStr = file.name();
        file.close();

        size_t lastSlash = pathStr.rfind('/');
        std::string filename = (lastSlash != std::string::npos) ? pathStr.substr(lastSlash + 1) : pathStr;

        if (!record_filename_belongs_table(filename)) {
            continue;
        }

        char file_path[192];
        snprintf(file_path, sizeof(file_path), "%s/%s", table->table_path, filename.c_str());

        if (LoFS::remove(file_path)) {
            deletedCount++;
        } else {
            LODB_LOG_WARN("Failed to delete file during truncate: %s", file_path);
        }
    }

    dir.close();

    LODB_LOG_INFO("Truncated table %s: deleted %d records", table_name, deletedCount);
    return LODB_OK;
}

LoDbError LoDb::drop(const char *table_name)
{
    if (!table_name) {
        LODB_LOG_ERROR("Invalid table_name");
        return LODB_ERR_INVALID;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        LODB_LOG_ERROR("Table not registered: %s", table_name);
        return LODB_ERR_INVALID;
    }

    LoDbError err = truncate(table_name);
    if (err != LODB_OK) {
        LODB_LOG_WARN("Failed to truncate table before drop: %s", table_name);
    }

    tables.erase(table_name);

    LODB_LOG_INFO("Dropped table: %s", table_name);
    return LODB_OK;
}

#if defined(LODB_TEST)
#include "../losettings/losettings.pb.h"

bool lodb_run_selftest(const char *mount)
{
    static bool ran = false;
    if (ran) return true;
    ran = true;

    bool ok = true;
    auto expect = [&](bool cond, const char *msg) {
        if (!cond) {
            LODB_LOG_ERROR("LoDB selftest FAIL: %s", msg);
            ok = false;
        }
    };

    LoDb db("lodb_selftest", mount);
    expect(db.registerTable("kv", &LoSettingsKv_msg, sizeof(LoSettingsKv)) == LODB_OK, "register table");

    LoSettingsKv rec = LoSettingsKv_init_zero;
    strncpy(rec.key, "alpha", sizeof(rec.key) - 1);
    rec.kind = 5;  // string
    const char *v1 = "one";
    rec.data.size = (pb_size_t)strlen(v1);
    memcpy(rec.data.bytes, v1, rec.data.size);
    expect(db.insert("kv", "alpha", &rec) == LODB_OK, "insert alpha");

    LoSettingsKv out = LoSettingsKv_init_zero;
    expect(db.get("kv", "alpha", &out) == LODB_OK, "get alpha after insert");
    expect(out.kind == 5 && out.data.size == 3 && memcmp(out.data.bytes, "one", 3) == 0, "alpha value one");

    const char *v2 = "two";
    rec.data.size = (pb_size_t)strlen(v2);
    memcpy(rec.data.bytes, v2, rec.data.size);
    expect(db.update("kv", "alpha", &rec) == LODB_OK, "update alpha");
    memset(&out, 0, sizeof(out));
    expect(db.get("kv", "alpha", &out) == LODB_OK, "get alpha after update");
    expect(out.data.size == 3 && memcmp(out.data.bytes, "two", 3) == 0, "alpha value two");

    LoSettingsKv brec = LoSettingsKv_init_zero;
    strncpy(brec.key, "blob", sizeof(brec.key) - 1);
    brec.kind = 6;  // bytes
    brec.data.size = 32;
    for (size_t i = 0; i < brec.data.size; i++) brec.data.bytes[i] = (uint8_t)(i ^ 0x5a);
    expect(db.insert("kv", "blob", &brec) == LODB_OK, "insert blob");
    memset(&out, 0, sizeof(out));
    expect(db.get("kv", "blob", &out) == LODB_OK, "get blob");
    expect(out.kind == 6 && out.data.size == 32, "blob metadata");
    expect(memcmp(out.data.bytes, brec.data.bytes, 32) == 0, "blob payload");

    expect(db.deleteRecord("kv", "alpha") == LODB_OK, "delete alpha");
    memset(&out, 0, sizeof(out));
    expect(db.get("kv", "alpha", &out) == LODB_ERR_NOT_FOUND, "alpha missing after delete");

    (void)db.truncate("kv");
    LODB_LOG_INFO("LoDB selftest %s", ok ? "PASS" : "FAIL");
    return ok;
}
#endif
