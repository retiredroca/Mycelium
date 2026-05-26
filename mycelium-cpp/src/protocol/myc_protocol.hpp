#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include "crypto/myc_crypto.hpp"

enum MsgType : uint8_t {
    kMsgHandshake = 0,
    kMsgPostCreate = 1,
    kMsgPostUpdate = 2,
    kMsgPostDelete = 3,
    kMsgPostView = 4,
    kMsgPostShare = 5,
    kMsgPostComment = 6,
    kMsgPeerAnnounce = 7,
    kMsgPeerDiscover = 8,
    kMsgStorageOffer = 9,
    kMsgStorageRequest = 10,
    kMsgStorageConfirm = 11,
    kMsgHeartbeat = 12,
    kMsgSyncRequest = 13,
    kMsgSyncResponse = 14,
    kMsgError = 15,
};

struct HandshakePayload {
    uint8_t protocol_version;
    std::string capabilities;     // comma-separated
    std::vector<std::string> listen_addresses;
    std::string user_agent;
};

struct ErrorPayload {
    int32_t code;
    std::string message;
};

struct ProtocolMessage {
    uint8_t version = 1;
    uint8_t msg_type = 0;
    uint64_t sender = 0;          // truncated peer id prefix
    int64_t timestamp = 0;
    std::vector<uint8_t> payload; // serialized inner payload
    std::array<uint8_t, 64> signature = {};

    int64_t now_sec() const {
#ifdef _WIN32
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
        return (int64_t)((t - 116444736000000000ULL) / 10000000ULL);
#else
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        return (int64_t)ts.tv_sec;
#endif
    }
};

static inline std::vector<uint8_t> serialize_handshake(const HandshakePayload& h) {
    std::vector<uint8_t> out;
    out.push_back(h.protocol_version);
    auto caps = (uint16_t)h.capabilities.size();
    out.push_back((uint8_t)(caps >> 8)); out.push_back((uint8_t)caps);
    out.insert(out.end(), h.capabilities.begin(), h.capabilities.end());
    auto num_addrs = (uint16_t)h.listen_addresses.size();
    out.push_back((uint8_t)(num_addrs >> 8)); out.push_back((uint8_t)num_addrs);
    for (auto& addr : h.listen_addresses) {
        auto alen = (uint16_t)addr.size();
        out.push_back((uint8_t)(alen >> 8)); out.push_back((uint8_t)alen);
        out.insert(out.end(), addr.begin(), addr.end());
    }
    auto ua = (uint16_t)h.user_agent.size();
    out.push_back((uint8_t)(ua >> 8)); out.push_back((uint8_t)ua);
    out.insert(out.end(), h.user_agent.begin(), h.user_agent.end());
    return out;
}

static inline HandshakePayload deserialize_handshake(const uint8_t* data, size_t len) {
    HandshakePayload h = {};
    if (len < 1) return h;
    size_t off = 0;
    h.protocol_version = data[off++];
    auto read16 = [&]() -> uint16_t {
        if (off + 2 > len) return 0;
        uint16_t v = ((uint16_t)data[off] << 8) | data[off+1];
        off += 2; return v;
    };
    auto read_str = [&](size_t n) -> std::string {
        if (off + n > len) return {};
        auto s = std::string((const char*)data + off, n);
        off += n; return s;
    };
    uint16_t caps_len = read16();
    if (caps_len > 0) h.capabilities = read_str(caps_len);
    uint16_t num_addrs = read16();
    for (uint16_t i = 0; i < num_addrs; ++i) {
        uint16_t alen = read16();
        if (alen > 0) h.listen_addresses.push_back(read_str(alen));
    }
    uint16_t ua_len = read16();
    if (ua_len > 0) h.user_agent = read_str(ua_len);
    return h;
}
