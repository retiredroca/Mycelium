#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include "crypto/myc_crypto.hpp"
#include "protocol/myc_protocol.hpp"

#ifdef _WIN32
#include <windows.h>
#include <fileapi.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#endif

static inline constexpr bool kEncryptionEnabled = true;
static inline constexpr PQKeyType kDefaultKeyType = kHybridX25519Kyber768;

// ============================================================
// Binary file format:
// [4 bytes] magic = 0x4D5943 ("MYC")
// [2 bytes] version = 0x0001
// [2 bytes] type: 1=post, 2=profile, 3=state, 4=wallet
// [4 bytes] payload_size (little-endian)
// [payload_size bytes] payload
// ============================================================
enum FileType : uint16_t {
    kFilePost    = 1,
    kFileProfile = 2,
    kFileState   = 3,
    kFileWallet  = 4,
    kFileBlock   = 5,
};

static inline constexpr uint32_t kFileMagic = 0x4D5943;
static inline constexpr uint16_t kFileVersion = 1;

struct FileHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint32_t payload_size;

    static FileHeader read(const uint8_t* data, size_t len) {
        FileHeader h = {};
        if (len < 12) return h;
        size_t off = 0;
        h.magic = ((uint32_t)data[off] << 16) | ((uint32_t)data[off+1] << 8) | data[off+2]; off += 4;
        h.version = (uint16_t)((data[off] << 8) | data[off+1]); off += 2;
        h.type = (uint16_t)((data[off] << 8) | data[off+1]); off += 2;
        h.payload_size = ((uint32_t)data[off] << 24) | ((uint32_t)data[off+1] << 16) |
                         ((uint32_t)data[off+2] << 8) | data[off+3];
        return h;
    }

    static void write(std::vector<uint8_t>& out, uint16_t type, const uint8_t* payload, uint32_t sz) {
        out.resize(12 + sz);
        size_t off = 0;
        out[off++] = 0x4D; out[off++] = 0x59; out[off++] = 0x43; // "MYC"
        out[off++] = 0; // padding
        out[off++] = 0; out[off++] = 1; // version 1
        out[off++] = (uint8_t)(type >> 8); out[off++] = (uint8_t)type;
        out[off++] = (uint8_t)(sz >> 24); out[off++] = (uint8_t)(sz >> 16);
        out[off++] = (uint8_t)(sz >> 8); out[off++] = (uint8_t)sz;
        if (sz > 0) memcpy(out.data() + 12, payload, sz);
    }
};

// Helper: write string to buffer (4-byte LE length + data)
static inline void buf_write_str(std::vector<uint8_t>& buf, const std::string& s) {
    uint32_t len = (uint32_t)s.size();
    buf.push_back((uint8_t)(len >> 24)); buf.push_back((uint8_t)(len >> 16));
    buf.push_back((uint8_t)(len >> 8)); buf.push_back((uint8_t)len);
    buf.insert(buf.end(), s.begin(), s.end());
}

// Helper: write u32 to buffer
static inline void buf_write_u32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back((uint8_t)(v >> 24)); buf.push_back((uint8_t)(v >> 16));
    buf.push_back((uint8_t)(v >> 8)); buf.push_back((uint8_t)v);
}

// Helper: write u64 to buffer
static inline void buf_write_u64(std::vector<uint8_t>& buf, uint64_t v) {
    buf.push_back((uint8_t)(v >> 56)); buf.push_back((uint8_t)(v >> 48));
    buf.push_back((uint8_t)(v >> 40)); buf.push_back((uint8_t)(v >> 32));
    buf.push_back((uint8_t)(v >> 24)); buf.push_back((uint8_t)(v >> 16));
    buf.push_back((uint8_t)(v >> 8)); buf.push_back((uint8_t)v);
}

// Helper: write double to buffer
static inline void buf_write_double(std::vector<uint8_t>& buf, double v) {
    uint64_t bits;
    memcpy(&bits, &v, sizeof(bits));
    buf_write_u64(buf, bits);
}

// Helper: write bytes to buffer (4-byte LE length + data)
static inline void buf_write_bytes(std::vector<uint8_t>& buf, const uint8_t* data, uint32_t len) {
    buf_write_u32(buf, len);
    buf.insert(buf.end(), data, data + len);
}

// Helper: read string from buffer
static inline std::string buf_read_str(const uint8_t* data, size_t& off, size_t len) {
    if (off + 4 > len) return {};
    uint32_t slen = ((uint32_t)data[off] << 24) | ((uint32_t)data[off+1] << 16) |
                    ((uint32_t)data[off+2] << 8) | data[off+3];
    off += 4;
    if (off + slen > len) return {};
    std::string s((const char*)data + off, slen);
    off += slen;
    return s;
}

// Helper: read u32 from buffer
static inline uint32_t buf_read_u32(const uint8_t* data, size_t& off, size_t len) {
    if (off + 4 > len) return 0;
    uint32_t v = ((uint32_t)data[off] << 24) | ((uint32_t)data[off+1] << 16) |
                 ((uint32_t)data[off+2] << 8) | data[off+3];
    off += 4;
    return v;
}

// Helper: read u64 from buffer
static inline uint64_t buf_read_u64(const uint8_t* data, size_t& off, size_t len) {
    if (off + 8 > len) return 0;
    uint64_t v = ((uint64_t)data[off] << 56) | ((uint64_t)data[off+1] << 48) |
                 ((uint64_t)data[off+2] << 40) | ((uint64_t)data[off+3] << 32) |
                 ((uint64_t)data[off+4] << 24) | ((uint64_t)data[off+5] << 16) |
                 ((uint64_t)data[off+6] << 8) | data[off+7];
    off += 8;
    return v;
}

// Helper: read double from buffer
static inline double buf_read_double(const uint8_t* data, size_t& off, size_t len) {
    uint64_t bits = buf_read_u64(data, off, len);
    double v;
    memcpy(&v, &bits, sizeof(v));
    return v;
}

// Helper: read bytes from buffer (returns vector)
static inline std::vector<uint8_t> buf_read_bytes(const uint8_t* data, size_t& off, size_t len) {
    uint32_t blen = buf_read_u32(data, off, len);
    if (off + blen > len) return {};
    std::vector<uint8_t> vec(data + off, data + off + blen);
    off += blen;
    return vec;
}

// ============================================================
// Storage Config
// ============================================================
struct StorageConfig {
    std::string data_dir = "./data";
    std::string posts_dir = "./data/posts";
    std::string chain_dir = "./data/chain";
    std::string profiles_dir = "./data/profiles";
    std::string logs_dir = "./data/logs";
    uint64_t default_post_ttl_seconds = 86400;

    std::string resolve(const std::string& base, const std::string& sub) const {
        // If base is absolute, return it; else join with data_dir
        if (base.empty()) return data_dir + "/" + sub;
        if (base[0] == '/' || base[0] == '\\') return base;
        if (base.size() > 2 && base[1] == ':') return base; // "X:\..."
        return data_dir + "/" + base;
    }

    void resolve_all() {
        posts_dir = resolve(posts_dir, "posts");
        chain_dir = resolve(chain_dir, "chain");
        profiles_dir = resolve(profiles_dir, "profiles");
        logs_dir = resolve(logs_dir, "logs");
    }
};

// ============================================================
// Directory helpers
// ============================================================
static inline bool dir_exists(const char* path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

static inline bool ensure_dir(const char* path) {
    if (dir_exists(path)) return true;
#ifdef _WIN32
    return CreateDirectoryA(path, 0) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

static inline bool ensure_dirs(StorageConfig& cfg) {
    cfg.resolve_all();
    bool ok = true;
    ok = ensure_dir(cfg.data_dir.c_str()) && ok;
    ok = ensure_dir(cfg.posts_dir.c_str()) && ok;
    ok = ensure_dir(cfg.chain_dir.c_str()) && ok;
    ok = ensure_dir(cfg.profiles_dir.c_str()) && ok;
    ok = ensure_dir(cfg.logs_dir.c_str()) && ok;
    return ok;
}

// ============================================================
// .env parser
// ============================================================
static inline bool load_env_file(const char* path, StorageConfig& cfg) {
    FILE* f = fopen(path, "r");
    if (!f) return false;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        // Strip newline and trailing whitespace
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r' || line[len-1] == ' ')) line[--len] = 0;
        if (len == 0 || line[0] == '#') continue;

        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char* key = line;
        const char* val = eq + 1;

        // Trim key
        while (*key == ' ') ++key;
        char* end = (char*)key + strlen(key) - 1;
        while (end > key && *end == ' ') *end-- = 0;

        // Trim value
        while (*val == ' ') ++val;
        end = (char*)val + strlen(val) - 1;
        while (end > val && *end == ' ') *end-- = 0;

        // Remove optional quotes
        if (val[0] == '"') { ++val; end = (char*)val + strlen(val) - 1; if (*end == '"') *end = 0; }

        if (strcmp(key, "POSTS_DIR") == 0) cfg.posts_dir = val;
        else if (strcmp(key, "CHAIN_DIR") == 0) cfg.chain_dir = val;
        else if (strcmp(key, "PROFILES_DIR") == 0) cfg.profiles_dir = val;
        else if (strcmp(key, "LOGS_DIR") == 0) cfg.logs_dir = val;
        else if (strcmp(key, "DATA_DIR") == 0) cfg.data_dir = val;
        else if (strcmp(key, "DEFAULT_POST_TTL") == 0) cfg.default_post_ttl_seconds = (uint64_t)atoll(val);
    }
    fclose(f);
    return true;
}

// ============================================================
// Scan directory for files with extension
// ============================================================
static inline std::vector<std::string> scan_dir(const std::string& dir, const std::string& ext) {
    std::vector<std::string> files;
#ifdef _WIN32
    std::string pattern = dir + "\\*" + ext;
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return files;
    do {
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            files.push_back(dir + "\\" + ffd.cFileName);
    } while (FindNextFileA(hFind, &ffd));
    FindClose(hFind);
#else
    DIR* d = opendir(dir.c_str());
    if (!d) return files;
    struct dirent* entry;
    while ((entry = readdir(d))) {
        std::string name = entry->d_name;
        if (name.size() > ext.size() && name.substr(name.size() - ext.size()) == ext)
            files.push_back(dir + "/" + name);
    }
    closedir(d);
#endif
    std::sort(files.begin(), files.end());
    return files;
}

// ============================================================
// Read/write file helpers
// ============================================================
static inline bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return false; }
    out.resize((size_t)sz);
    if (sz > 0 && fread(out.data(), 1, (size_t)sz, f) != (size_t)sz) { fclose(f); return false; }
    fclose(f);
    return true;
}

static inline bool write_file(const std::string& path, const uint8_t* data, size_t len) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, len, f) == len;
    fclose(f);
    return ok;
}

static inline bool delete_file(const std::string& path) {
#ifdef _WIN32
    return DeleteFileA(path.c_str()) != 0;
#else
    return unlink(path.c_str()) == 0;
#endif
}

// ============================================================
// Post serialization (forward declarations — actual serialization
// uses Post fields; callers include myc_post.hpp)
// ============================================================

struct EncryptionMetadata {
    std::string algorithm;
    std::string key_type;
    std::vector<uint8_t> nonce;
    std::string merkle_root;
    std::string content_hash;

    static inline EncryptionMetadata create(
        const std::string& alg,
        PQKeyType kt,
        const std::vector<uint8_t>& n,
        const std::string& hash)
    {
        EncryptionMetadata m;
        m.algorithm = alg;
        m.key_type = pq_display_name(kt);
        m.nonce = n;
        m.content_hash = hash;
        return m;
    }
};

struct EncryptedStorageEntry {
    std::string entry_id;
    std::string content_cid;
    std::vector<uint8_t> encrypted_content;
    EncryptionMetadata encryption_metadata;
    uint64_t size_bytes = 0;
    int64_t created_at = 0;
    int64_t last_verified = 0;

    bool needs_verification() const {
        int64_t now = ProtocolMessage{}.now_sec();
        return (now - last_verified) > 86400;
    }
};

struct StorageEntrySummary {
    std::string entry_id;
    std::string content_cid;
    uint64_t size_bytes = 0;
    bool encrypted = false;
    int64_t created_at = 0;
};

struct StorageManifest {
    std::string manifest_id;
    std::string owner_id;
    std::vector<uint8_t> public_key;
    std::vector<StorageEntrySummary> entries;
    uint64_t total_size_bytes = 0;
    bool encryption_enabled = kEncryptionEnabled;
    int64_t created_at = 0;
    int64_t updated_at = 0;

    static inline StorageManifest create(const std::string& owner,
                                          const std::vector<uint8_t>& pk) {
        StorageManifest m;
        std::array<uint8_t, 32> id_buf;
        random_bytes(id_buf.data(), 32);
        m.manifest_id = base64_encode(id_buf.data(), 16);
        m.owner_id = owner;
        m.public_key = pk;
        m.created_at = ProtocolMessage{}.now_sec();
        m.updated_at = m.created_at;
        return m;
    }

    void add_entry(const StorageEntrySummary& summary, uint64_t size) {
        total_size_bytes += size;
        updated_at = ProtocolMessage{}.now_sec();
        entries.push_back(summary);
    }

    bool remove_entry(const std::string& entry_id) {
        for (size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].entry_id == entry_id) {
                total_size_bytes -= entries[i].size_bytes;
                updated_at = ProtocolMessage{}.now_sec();
                entries.erase(entries.begin() + i);
                return true;
            }
        }
        return false;
    }
};

struct MerkleProof {
    std::string root;

    bool verify(const uint8_t* data, size_t len) const {
        auto h = sha256(data, len);
        auto cid = base64_encode(h.data(), 16);
        return root.size() >= 8 && cid.substr(0, 8) == root.substr(0, 8);
    }
};

struct EncryptedDataHandoff {
    std::string handoff_id;
    std::string content_cid;
    std::vector<uint8_t> encrypted_blob;
    EncryptionMetadata encryption_metadata;
    MerkleProof merkle_proof;
    uint8_t replication_factor = 3;
    std::vector<std::string> verified_hosts;
    int64_t created_at = 0;

    static inline EncryptedDataHandoff create(
        const std::string& cid,
        std::vector<uint8_t> blob,
        const EncryptionMetadata& meta,
        const std::string& merkle_root,
        uint8_t replicas)
    {
        EncryptedDataHandoff h;
        std::array<uint8_t, 32> id_buf;
        random_bytes(id_buf.data(), 32);
        h.handoff_id = base64_encode(id_buf.data(), 16);
        h.content_cid = cid;
        h.encrypted_blob = std::move(blob);
        h.encryption_metadata = meta;
        h.merkle_proof.root = merkle_root;
        h.replication_factor = replicas;
        h.created_at = ProtocolMessage{}.now_sec();
        return h;
    }

    void add_verified_host(const std::string& node_id) {
        for (auto& h : verified_hosts)
            if (h == node_id) return;
        verified_hosts.push_back(node_id);
    }

    bool is_fully_replicated() const {
        return verified_hosts.size() >= (size_t)replication_factor;
    }
};

struct StreamingSlot {
    std::string slot_id;
    std::string video_cid;
    std::string host_peer_id;
    std::string consumer_peer_id;
    uint32_t reserved_mbps = 0;
    uint64_t duration_secs = 0;
    int64_t started_at = 0;
    int64_t expires_at = 0;
    bool active = false;

    static inline StreamingSlot create(
        const std::string& vcid,
        const std::string& host,
        const std::string& consumer,
        uint32_t mbps,
        uint64_t secs)
    {
        StreamingSlot s;
        std::array<uint8_t, 32> id_buf;
        random_bytes(id_buf.data(), 32);
        s.slot_id = base64_encode(id_buf.data(), 16);
        s.video_cid = vcid;
        s.host_peer_id = host;
        s.consumer_peer_id = consumer;
        s.reserved_mbps = mbps;
        s.duration_secs = secs;
        s.started_at = ProtocolMessage{}.now_sec();
        s.expires_at = s.started_at + (int64_t)secs;
        s.active = true;
        return s;
    }

    bool is_expired() const {
        return !active || ProtocolMessage{}.now_sec() >= expires_at;
    }
};

// Compute CID-style identifier
static inline std::string compute_cid(const uint8_t* data, size_t len) {
    auto h = sha256(data, len);
    return "Qm" + base64_encode(h.data(), 16).substr(0, 44);
}

static inline std::string compute_hash_str(const uint8_t* data, size_t len) {
    auto h = sha256(data, len);
    return base64_encode(h.data(), 16);
}
