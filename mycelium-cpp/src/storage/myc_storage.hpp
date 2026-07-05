#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "crypto/myc_crypto.hpp"
#include "protocol/myc_protocol.hpp"

static inline constexpr bool kEncryptionEnabled = true;
static inline constexpr PQKeyType kDefaultKeyType = kHybridX25519Kyber768;

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
