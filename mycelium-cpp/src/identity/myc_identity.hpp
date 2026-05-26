#pragma once
#include <cstdint>
#include <cctype>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include "crypto/myc_crypto.hpp"
#include "protocol/myc_protocol.hpp"
#include "profile/myc_profile_validation.hpp"

static inline constexpr const char* kUsernameNamespace = "/mycelium/usernames/";

struct IdentityRecord {
    std::vector<uint8_t> peer_id;
    std::string username;
    std::string profile_cid;
    int64_t registered_at = 0;
    int64_t updated_at = 0;
    std::vector<uint8_t> signature;
    uint32_t version = 1;

    static inline IdentityRecord create(
        const uint8_t* peer_id_data, size_t peer_id_len,
        const std::string& username)
    {
        IdentityRecord r;
        r.peer_id.assign(peer_id_data, peer_id_data + peer_id_len);
        auto u = username;
        std::transform(u.begin(), u.end(), u.begin(), ::tolower);
        r.username = u;
        r.registered_at = ProtocolMessage{}.now_sec();
        r.updated_at = r.registered_at;
        return r;
    }

    IdentityRecord with_profile_cid(const std::string& cid) const {
        IdentityRecord r = *this;
        r.profile_cid = cid;
        r.updated_at = ProtocolMessage{}.now_sec();
        ++r.version;
        return r;
    }

    std::string signature_message() const {
        return username + ":" + base64_encode(peer_id.data(), peer_id.size()) + ":"
            + std::to_string(registered_at) + ":" + std::to_string(version);
    }

    void sign(const std::array<uint8_t, 32>& private_key) {
        auto msg = signature_message();
        auto sig = ed25519_sign((const uint8_t*)msg.data(), msg.size(), private_key);
        signature.assign(sig.begin(), sig.end());
    }

    bool verify_signature() const {
        if (signature.empty()) return false;
        auto msg = signature_message();
        if (signature.size() != 64) return false;
        std::array<uint8_t, 64> sig;
        memcpy(sig.data(), signature.data(), 64);
        // Reconstruct public key from peer_id hash
        auto pk = sha256(peer_id.data(), peer_id.size());
        return ed25519_verify((const uint8_t*)msg.data(), msg.size(), sig, pk);
    }

    std::string dht_key() const {
        return std::string(kUsernameNamespace) + username;
    }
};

enum IdentityErr : int {
    kIdentityOk = 0,
    kIdentityTaken = -1,
    kIdentityNotFound = -2,
    kIdentityInvalid = -3,
    kIdentityInvalidSignature = -4,
    kIdentityUnauthorized = -5,
    kIdentityReserved = -6,
};

static inline const char* identity_strerror(IdentityErr e) {
    switch (e) {
        case kIdentityOk: return "ok";
        case kIdentityTaken: return "username taken";
        case kIdentityNotFound: return "username not found";
        case kIdentityInvalid: return "invalid username";
        case kIdentityInvalidSignature: return "invalid signature";
        case kIdentityUnauthorized: return "unauthorized";
        case kIdentityReserved: return "username reserved";
        default: return "unknown";
    }
}

struct UsernameRegistry {
    std::unordered_map<std::string, IdentityRecord> records;
    std::unordered_map<std::string, std::string> expired_usernames;

    IdentityErr register_username(
        const uint8_t* peer_id, size_t peer_id_len,
        const std::string& username,
        const std::array<uint8_t, 32>& private_key)
    {
        auto u = username;
        std::transform(u.begin(), u.end(), u.begin(), ::tolower);
        auto val = validate_username(u);
        if (val != kProfileValOk) return kIdentityInvalid;
        if (records.count(u)) return kIdentityTaken;

        auto record = IdentityRecord::create(peer_id, peer_id_len, u);
        record.sign(private_key);
        records[u] = record;
        return kIdentityOk;
    }

    IdentityRecord* lookup(const std::string& username) {
        auto u = username;
        std::transform(u.begin(), u.end(), u.begin(), ::tolower);
        auto it = records.find(u);
        if (it == records.end()) return nullptr;
        return &it->second;
    }

    IdentityErr update_profile_cid(
        const std::string& username,
        const std::string& cid,
        const std::array<uint8_t, 32>& private_key)
    {
        auto u = username;
        std::transform(u.begin(), u.end(), u.begin(), ::tolower);
        auto it = records.find(u);
        if (it == records.end()) return kIdentityNotFound;

        auto updated = it->second.with_profile_cid(cid);
        updated.sign(private_key);
        if (!updated.verify_signature()) return kIdentityInvalidSignature;
        records[u] = updated;
        return kIdentityOk;
    }

    IdentityErr unregister(
        const std::string& username,
        const std::array<uint8_t, 32>& private_key)
    {
        auto u = username;
        std::transform(u.begin(), u.end(), u.begin(), ::tolower);
        auto it = records.find(u);
        if (it == records.end()) return kIdentityNotFound;

        auto msg = it->second.signature_message();
        auto sig = ed25519_sign((const uint8_t*)msg.data(), msg.size(), private_key);
        std::array<uint8_t, 64> sig_arr;
        memcpy(sig_arr.data(), sig.data(), 64);
        auto pk = sha256(it->second.peer_id.data(), it->second.peer_id.size());
        if (!ed25519_verify((const uint8_t*)msg.data(), msg.size(), sig_arr, pk))
            return kIdentityInvalidSignature;

        expired_usernames[u] = it->second.registered_at;
        records.erase(it);
        return kIdentityOk;
    }

    std::vector<uint8_t> resolve(const std::string& username) const {
        auto u = username;
        std::string ul = u;
        std::transform(ul.begin(), ul.end(), ul.begin(), ::tolower);
        auto it = records.find(ul);
        if (it == records.end()) return {};
        return it->second.peer_id;
    }
};
