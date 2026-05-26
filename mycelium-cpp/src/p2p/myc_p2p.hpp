#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "crypto/myc_crypto.hpp"
#include "protocol/myc_protocol.hpp"
#include "post/myc_post.hpp"
#include "storage/myc_storage.hpp"

static inline constexpr const char* kProtocolName = "/mycelium-node/1.0.0";
static inline constexpr const char* kGossipTopicPosts = "posts";
static inline constexpr const char* kGossipTopicAnnouncements = "announcements";

// ============================================================
// Node Capabilities
// ============================================================
enum NodeCap : uint8_t {
    kCapFull = 0,
    kCapStorage = 1,
    kCapRelay = 2,
};

struct NodeCapInfo {
    NodeCap cap;
    uint64_t max_gb = 0;
    uint32_t bandwidth_mbps = 0;
};

// ============================================================
// Peer Info / Peer Table
// ============================================================
struct PeerInfo {
    std::string peer_id;
    std::vector<std::string> addresses;
    int64_t connected_at = 0;
    int64_t last_seen = 0;
    int64_t trust_score = 0;
    std::vector<NodeCapInfo> capabilities;
    uint64_t bytes_transferred = 0;
    uint64_t messages_received = 0;
    uint64_t messages_sent = 0;
    bool is_persistent = false;
};

struct TrustScore {
    int64_t base_score = 100;
    int64_t uptime_weight = 50;
    int64_t behavior_weight = 30;
    int64_t stake_weight = 20;
};

struct PeerTable {
    std::string local_peer_id;
    std::unordered_map<std::string, PeerInfo> peers;
    size_t max_peers = 100;

    void add_peer(const std::string& peer_id, const PeerInfo& info) {
        peers[peer_id] = info;
    }

    bool remove_peer(const std::string& peer_id) {
        return peers.erase(peer_id) > 0;
    }

    PeerInfo* get_peer(const std::string& peer_id) {
        auto it = peers.find(peer_id);
        if (it == peers.end()) return nullptr;
        return &it->second;
    }

    size_t peer_count() const { return peers.size(); }

    std::vector<std::pair<std::string, PeerInfo>> get_all_peers() const {
        std::vector<std::pair<std::string, PeerInfo>> out;
        for (auto& [id, info] : peers)
            out.push_back({id, info});
        return out;
    }

    void touch_peer(const std::string& peer_id) {
        auto it = peers.find(peer_id);
        if (it != peers.end())
            it->second.last_seen = ProtocolMessage{}.now_sec();
    }
};

// ============================================================
// Discovery Config
// ============================================================
struct BootstrapNode {
    std::string peer_id;
    std::string address;
};

struct DiscoveryConfig {
    uint64_t bootstrap_interval_secs = 30;
    uint64_t query_timeout_secs = 10;
    uint64_t refresh_interval_secs = 300;
    int parallel_queries = 3;
    int replication_factor = 20;
    bool advertise_enabled = true;
    std::vector<BootstrapNode> bootstrap_nodes;
};

static inline std::vector<BootstrapNode> default_bootstrap_nodes() {
    return {};
}

// ============================================================
// Gossip
// ============================================================
enum GossipPayloadType : uint8_t {
    kGossipPost = 0,
    kGossipPostAnnouncement = 1,
    kGossipPeerAnnounce = 2,
    kGossipSyncRequest = 3,
    kGossipSyncResponse = 4,
};

struct GossipMessage {
    std::string id;
    uint8_t msg_type = 0;
    std::string sender;
    int64_t timestamp = 0;
    std::vector<uint8_t> payload;
    uint32_t ttl = 60;
    std::vector<uint8_t> signature;
};

struct GossipConfig {
    size_t max_payload_size = 1 << 20;
    uint32_t default_ttl = 60;
    int fanout_count = 6;
    int prune_interval_secs = 120;
};

struct GossipManager {
    std::unordered_map<std::string, int64_t> seen_messages;
    GossipConfig config;

    bool seen(const std::string& id) const {
        return seen_messages.count(id);
    }

    void mark_seen(const std::string& id) {
        seen_messages[id] = ProtocolMessage{}.now_sec();
    }

    void prune() {
        int64_t now = ProtocolMessage{}.now_sec();
        for (auto it = seen_messages.begin(); it != seen_messages.end();) {
            if (now - it->second > config.prune_interval_secs)
                it = seen_messages.erase(it);
            else
                ++it;
        }
    }
};

// ============================================================
// Storage Handoff types
// ============================================================
enum SLATier : uint8_t { kSLABasic = 0, kSLAStandard = 1, kSLAPremium = 2, kSLAEnterprise = 3 };
enum AgreementStatus : uint8_t { kAgreementPending = 0, kAgreementActive = 1, kAgreementSuspended = 2, kAgreementTerminated = 3, kAgreementExpired = 4 };
enum HandshakePhase : uint8_t { kPhaseDiscovery = 0, kPhaseOffer = 1, kPhaseAgreement = 2, kPhaseDataTransfer = 3, kPhaseComplete = 4, kPhaseFailed = 5 };

static inline constexpr uint8_t kMinReplicationFactor = 3;
static inline constexpr int64_t kMaxHandshakeDurationSecs = 300;

struct StorageOffer {
    std::string offer_id;
    std::string host_peer_id;
    uint64_t available_gb = 0;
    uint32_t bandwidth_mbps = 0;
    uint64_t price_per_gb_month = 0;
    double uptime_score = 0.0;
    SLATier sla_tier = kSLABasic;
    int64_t valid_until = 0;
    std::vector<uint8_t> signature;
};

struct StorageAgreement {
    std::string agreement_id;
    std::string host_peer_id;
    std::string client_peer_id;
    uint64_t storage_gb = 0;
    uint64_t monthly_price = 0;
    uint64_t stake_amount = 0;
    SLATier sla_tier = kSLABasic;
    int64_t started_at = 0;
    int64_t expires_at = 0;
    AgreementStatus status = kAgreementPending;
    std::string on_chain_ref;
};

struct DataHandoff {
    std::string handoff_id;
    std::string agreement_id;
    std::vector<uint8_t> encrypted_key;
    std::string content_cid;
    uint64_t content_size_bytes = 0;
    std::string merkle_root;
    uint8_t replication_factor = kMinReplicationFactor;
    int64_t created_at = 0;
    std::vector<std::string> verified_hosts;
};

struct HandshakeState {
    HandshakePhase phase = kPhaseDiscovery;
    StorageOffer offer;
    StorageAgreement agreement;
    DataHandoff handoff;
    int64_t created_at = 0;
};

// ============================================================
// P2P Config & Node
// ============================================================
struct P2pConfig {
    std::string keypair_seed_base64;
    std::vector<std::string> listen_addresses;
    std::vector<NodeCapInfo> capabilities;
    DiscoveryConfig discovery;
    std::vector<std::string> bootstrap_nodes;
    bool enable_mdns = true;
    bool enable_relay = true;
};

struct LocalNodeInfo {
    std::string peer_id;
    std::vector<std::string> listen_addresses;
    std::vector<NodeCapInfo> capabilities;
    int64_t started_at = 0;
    std::string version = "0.1.0";
};

struct MyceliumNode {
    LocalNodeInfo local_info;
    PeerTable peer_table;
    GossipManager gossip;
    std::unordered_map<std::string, Post> post_cache;

    static inline MyceliumNode create(const P2pConfig& config) {
        MyceliumNode node;
        std::array<uint8_t, 32> seed;
        if (!config.keypair_seed_base64.empty()) {
            // In real impl: decode base64 seed
            memset(seed.data(), 0, 32);
        } else {
            random_bytes(seed.data(), 32);
        }
        auto pk = ed25519_pubkey(seed);
        node.local_info.peer_id = base64_encode(pk.data(), 16);
        for (auto& addr : config.listen_addresses)
            node.local_info.listen_addresses.push_back(addr);
        node.local_info.capabilities = config.capabilities;
        node.local_info.started_at = ProtocolMessage{}.now_sec();
        node.peer_table.local_peer_id = node.local_info.peer_id;
        return node;
    }

    std::string local_peer_id() const { return local_info.peer_id; }

    void add_peer(const std::string& peer_id, const PeerInfo& info) {
        peer_table.add_peer(peer_id, info);
    }

    void remove_peer(const std::string& peer_id) {
        peer_table.remove_peer(peer_id);
    }

    void store_post(const Post& post) {
        post_cache[post.id] = post;
    }

    Post* get_post(const std::string& post_id) {
        auto it = post_cache.find(post_id);
        if (it == post_cache.end()) return nullptr;
        return &it->second;
    }

    size_t peer_count() const { return peer_table.peer_count(); }
};
