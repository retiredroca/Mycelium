#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include "crypto/myc_crypto.hpp"
#include "protocol/myc_protocol.hpp"

static inline constexpr double kHypedThreshold = 1000.0;

struct PostContent {
    std::string text;
    std::vector<std::string> media_cids;
    std::vector<std::string> mentions;
};

enum StorageType : uint8_t { kStorageLocal = 0, kStorageDistributed = 1, kStoragePermanent = 2 };

struct ContentReference {
    StorageType storage_type;
    std::string cid;
    bool encrypted = false;
    std::string encryption_key_cid;
};

struct PostSignature {
    std::string algorithm;
    std::vector<uint8_t> signature_bytes;
    std::vector<uint8_t> public_key;
};

struct EncryptionInfo {
    bool enabled = false;
    std::string algorithm;
    std::string key_type;
    int64_t encrypted_at = 0;
};

struct Post {
    std::string id;
    std::string author;
    std::vector<uint8_t> author_public_key;
    PostContent content;
    ContentReference content_reference;
    int64_t created_at = 0;
    uint64_t ttl_seconds = 86400;
    double engagement_score = 0.0;
    uint64_t view_count = 0;
    uint64_t share_count = 0;
    uint64_t comment_count = 0;
    bool is_permanent = false;
    uint64_t staked_tokens = 0;
    PostSignature signature;
    EncryptionInfo encryption;

    static inline Post create(
        const std::string& author,
        const std::string& text,
        const KeyPair& keypair)
    {
        std::array<uint8_t, 32> id_buf;
        random_bytes(id_buf.data(), 32);
        Post p;
        p.id = base64_encode(id_buf.data(), 16);
        p.author = author;
        p.author_public_key.assign(keypair.pk.point.begin(), keypair.pk.point.end());
        p.content.text = text;
        p.created_at = ProtocolMessage{}.now_sec();

        auto canon = p.canonical_form();
        auto sig = ed25519_sign(
            (const uint8_t*)canon.data(), canon.size(), keypair.sk.scalar);
        p.signature.algorithm = "Ed25519";
        p.signature.signature_bytes.assign(sig.begin(), sig.end());
        p.signature.public_key.assign(keypair.pk.point.begin(), keypair.pk.point.end());
        return p;
    }

    std::string canonical_form() const {
        return id + ":" + author + ":" + std::to_string(created_at) + ":" + content.text;
    }

    bool verify_signature() const {
        if (signature.signature_bytes.size() != 64) return false;
        if (signature.public_key.size() != 32) return false;
        std::array<uint8_t, 64> sig;
        std::array<uint8_t, 32> pk;
        memcpy(sig.data(), signature.signature_bytes.data(), 64);
        memcpy(pk.data(), signature.public_key.data(), 32);
        auto canon = canonical_form();
        return ed25519_verify(
            (const uint8_t*)canon.data(), canon.size(), sig, pk);
    }

    void record_view() {
        ++view_count;
        engagement_score += 1.0;
        ttl_seconds += 7200;
    }

    void record_share(double author_reputation) {
        ++share_count;
        engagement_score += 10.0;
        double base_ext = 14400.0 * (1.0 + author_reputation / 100.0);
        ttl_seconds += (uint64_t)base_ext;
    }

    void record_comment(uint32_t thread_depth) {
        ++comment_count;
        engagement_score += 5.0;
        double bonus = 1.0 + (double)thread_depth * 0.1;
        ttl_seconds += (uint64_t)(21600.0 * bonus);
    }

    bool is_expired() const { return !is_permanent && ttl_seconds == 0; }
    bool is_hyped() const { return engagement_score >= kHypedThreshold; }

    int64_t remaining_ttl(int64_t current_time) const {
        int64_t elapsed = current_time - created_at;
        int64_t rem = (int64_t)ttl_seconds - elapsed;
        return rem > 0 ? rem : 0;
    }

    std::string content_hash() const {
        auto canon = canonical_form();
        auto h = sha256((const uint8_t*)canon.data(), canon.size());
        return base64_encode(h.data(), 16);
    }
};

enum PostErr : int {
    kPostOk = 0,
    kPostAlreadyPermanent = -1,
    kPostInsufficientStake = -2,
    kPostExpired = -3,
    kPostEncryptionFailed = -4,
    kPostDecryptionFailed = -5,
};

static inline const char* post_strerror(PostErr e) {
    switch (e) {
        case kPostOk: return "ok";
        case kPostAlreadyPermanent: return "already permanent";
        case kPostInsufficientStake: return "insufficient stake";
        case kPostExpired: return "post expired";
        case kPostEncryptionFailed: return "encryption failed";
        case kPostDecryptionFailed: return "decryption failed";
        default: return "unknown";
    }
}

static inline uint64_t calculate_ttl_extension(const Post& post) {
    (void)post;
    return 7200;
}
