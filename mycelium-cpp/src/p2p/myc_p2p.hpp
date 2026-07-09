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
#include "token/myc_token.hpp"

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
    kCapVideoHosting = 3,
};

struct NodeCapInfo {
    NodeCap cap;
    uint64_t max_gb = 0;
    uint32_t bandwidth_mbps = 0;
    uint32_t max_concurrent_streams = 0;
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
    kGossipVideoChunkRequest = 5,
    kGossipVideoChunkResponse = 6,
    kGossipTx = 7,
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
    bool enable_tor = false;
    uint16_t tor_socks_port = 9050;
    uint16_t tor_control_port = 9051;
    std::string onion_service_dir;
    StorageConfig storage;
};

struct LocalNodeInfo {
    std::string peer_id;
    std::vector<std::string> listen_addresses;
    std::vector<NodeCapInfo> capabilities;
    int64_t started_at = 0;
    std::string version = MYCELIUM_VERSION;
    std::string onion_address;
};

struct MyceliumNode {
    LocalNodeInfo local_info;
    PeerTable peer_table;
    GossipManager gossip;
    std::unordered_map<std::string, Post> post_cache;
    std::unordered_map<std::string, Transaction> mempool;
    std::vector<Block> chain;
    std::array<uint8_t, 32> chain_tip_hash = {};

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
        if (config.enable_tor)
            node.local_info.onion_address = onion_address_from_pubkey(pk);
        return node;
    }

    std::string local_peer_id() const { return local_info.peer_id; }

    void add_peer(const std::string& peer_id, const PeerInfo& info) {
        peer_table.add_peer(peer_id, info);
    }

    void remove_peer(const std::string& peer_id) {
        peer_table.remove_peer(peer_id);
    }

    Post* get_post(const std::string& post_id) {
        auto it = post_cache.find(post_id);
        if (it == post_cache.end()) return nullptr;
        return &it->second;
    }

    size_t peer_count() const { return peer_table.peer_count(); }

    // ============================================================
    // Mempool management
    // ============================================================
    bool add_to_mempool(const Transaction& tx) {
        if (mempool.count(tx.tx_id)) return false;
        mempool[tx.tx_id] = tx;
        return true;
    }

    bool remove_from_mempool(const std::string& tx_id) {
        return mempool.erase(tx_id) > 0;
    }

    Transaction* get_mempool_tx(const std::string& tx_id) {
        auto it = mempool.find(tx_id);
        if (it == mempool.end()) return nullptr;
        return &it->second;
    }

    std::vector<Transaction> get_mempool_txs() const {
        std::vector<Transaction> out;
        out.reserve(mempool.size());
        for (auto& [id, tx] : mempool) {
            (void)id;
            out.push_back(tx);
        }
        return out;
    }

    size_t mempool_size() const { return mempool.size(); }

    // ============================================================
    // Block chain management
    // ============================================================
    bool save_block(const Block& block, size_t height) {
        std::vector<uint8_t> buf;
        block_serialize(block, buf);
        std::vector<uint8_t> out;
        FileHeader::write(out, kFileBlock, buf.data(), (uint32_t)buf.size());
        char path[256];
        snprintf(path, sizeof(path), "%s/block_%zu.blk", storage_cfg.chain_dir.c_str(), height);
        return write_file(path, out.data(), out.size());
    }

    bool add_block(const Block& block, Tokenomics& tokenomics, Wallet& wallet) {
        chain.push_back(block);
        chain_tip_hash = block.calc_hash();

        if (!block.txs.empty()) {
            auto& coinbase = block.txs[0];
            if (coinbase.from == wallet.public_key && coinbase.to == wallet.public_key) {
                mint_to_wallet(tokenomics, wallet, coinbase.amount);
            }
        }

        save_block(block, chain.size() - 1);
        tokenomics.apply_disinflation();

        for (size_t i = 1; i < block.txs.size(); ++i) {
            remove_from_mempool(block.txs[i].tx_id);
        }
        return true;
    }

    bool load_chain(Tokenomics& tokenomics, Wallet& wallet) {
        for (size_t h = 0; ; ++h) {
            char path[256];
            snprintf(path, sizeof(path), "%s/block_%zu.blk", storage_cfg.chain_dir.c_str(), h);
            std::vector<uint8_t> buf;
            if (!read_file(path, buf)) break;
            auto hdr = FileHeader::read(buf.data(), buf.size());
            if (hdr.magic != kFileMagic || hdr.type != kFileBlock) break;
            size_t off = 12;
            auto block = block_deserialize(buf.data(), off, buf.size());
            chain.push_back(block);
            chain_tip_hash = block.calc_hash();

            if (!block.txs.empty()) {
                auto& coinbase = block.txs[0];
                if (coinbase.from == wallet.public_key && coinbase.to == wallet.public_key) {
                    mint_to_wallet(tokenomics, wallet, coinbase.amount);
                }
            }
            tokenomics.current_epoch = block.epoch + 1;
        }
        return !chain.empty();
    }

    bool mine_block(Tokenomics& tokenomics, Wallet& wallet,
                    const std::array<uint8_t, 32>& private_key,
                    uint64_t max_nonce = kMiningMaxNoncePerAttempt) {
        uint64_t epoch = tokenomics.current_epoch;
        uint64_t reward = mining_block_reward(epoch);
        if (reward == 0) return false;

        auto coinbase = Transaction::create(wallet.public_key, wallet.public_key, reward);
        coinbase.status = kTxConfirmed;

        std::vector<Transaction> block_txs;
        block_txs.push_back(coinbase);
        for (auto& [id, tx] : mempool) {
            (void)id;
            block_txs.push_back(tx);
        }

        auto merkle = calc_merkle_root(block_txs);

        Block block;
        block.prev_hash = chain_tip_hash;
        block.merkle_root = merkle;
        block.timestamp = ProtocolMessage{}.now_sec();
        block.nonce = 0;
        block.epoch = epoch;
        block.miner_pubkey = wallet.public_key;
        block.txs = block_txs;

        uint64_t nonce = block_mining_search(block, max_nonce);
        if (nonce == UINT64_MAX) return false;
        block.nonce = nonce;

        auto block_hash = block.calc_hash();
        auto sig = ed25519_sign(block_hash.data(), 32, private_key);
        block.signature.assign(sig.begin(), sig.end());

        return add_block(block, tokenomics, wallet);
    }

    void broadcast_transaction(const Transaction& tx) {
        (void)tx;
        // Stub: In full P2P implementation, serialize tx into
        // a ProtocolMessage with kMsgTransaction and gossip it.
    }

    // Storage management
    StorageConfig storage_cfg;

    bool init_storage(const StorageConfig& cfg) {
        storage_cfg = cfg;
        if (!ensure_dirs(storage_cfg)) return false;

        // Load posts from disk
        auto files = scan_dir(storage_cfg.posts_dir, ".mycpost");
        for (auto& f : files) {
            std::vector<uint8_t> buf;
            if (!read_file(f, buf)) continue;
            Post p;
            if (post_deserialize(buf.data(), buf.size(), p))
                post_cache[p.id] = p;
        }

        return true;
    }

    void save_post_to_disk(const Post& p) {
        std::vector<uint8_t> buf;
        post_serialize(p, buf);
        std::string path = storage_cfg.posts_dir + "/" + p.id + ".mycpost";
        write_file(path, buf.data(), buf.size());
    }

    void store_post(const Post& post) {
        post_cache[post.id] = post;
        save_post_to_disk(post);
    }

    void delete_post_from_disk(const std::string& post_id) {
        std::string path = storage_cfg.posts_dir + "/" + post_id + ".mycpost";
        delete_file(path);
    }

    void prune_expired_posts() {
        int64_t now = ProtocolMessage{}.now_sec();
        std::vector<std::string> expired;
        for (auto& [id, post] : post_cache) {
            if (post.is_expired() || (post.remaining_ttl(now) <= 0 && !post.is_permanent))
                expired.push_back(id);
        }
        for (auto& id : expired) {
            delete_post_from_disk(id);
            post_cache.erase(id);
        }
    }

    // Save tokenomics + wallet state
    bool save_state(const Tokenomics& t, const Wallet& w) {
        std::vector<uint8_t> payload;
        buf_write_u64(payload, t.total_supply);
        buf_write_u64(payload, t.minted_supply);
        buf_write_u64(payload, t.staked_supply);
        buf_write_u64(payload, t.burned_supply);
        buf_write_u32(payload, t.annual_inflation_bps);
        buf_write_u64(payload, t.current_epoch);
        buf_write_bytes(payload, w.public_key.data(), 32);
        buf_write_u64(payload, w.balance);
        buf_write_u64(payload, w.staked_balance);
        buf_write_u64(payload, w.locked_balance);
        buf_write_u64(payload, w.nonce);

        std::vector<uint8_t> out;
        FileHeader::write(out, kFileState, payload.data(), (uint32_t)payload.size());
        return write_file(storage_cfg.chain_dir + "/state.dat", out.data(), out.size());
    }

    bool load_state(Tokenomics& t, Wallet& w) {
        std::vector<uint8_t> buf;
        if (!read_file(storage_cfg.chain_dir + "/state.dat", buf)) return false;
        auto hdr = FileHeader::read(buf.data(), buf.size());
        if (hdr.magic != kFileMagic || hdr.type != kFileState) return false;
        size_t off = 12;
        t.total_supply = buf_read_u64(buf.data(), off, buf.size());
        t.minted_supply = buf_read_u64(buf.data(), off, buf.size());
        t.staked_supply = buf_read_u64(buf.data(), off, buf.size());
        t.burned_supply = buf_read_u64(buf.data(), off, buf.size());
        t.annual_inflation_bps = buf_read_u32(buf.data(), off, buf.size());
        t.current_epoch = buf_read_u64(buf.data(), off, buf.size());
        auto pk = buf_read_bytes(buf.data(), off, buf.size());
        if (pk.size() >= 32) memcpy(w.public_key.data(), pk.data(), 32);
        w.balance = buf_read_u64(buf.data(), off, buf.size());
        w.staked_balance = buf_read_u64(buf.data(), off, buf.size());
        w.locked_balance = buf_read_u64(buf.data(), off, buf.size());
        w.nonce = buf_read_u64(buf.data(), off, buf.size());
        return true;
    }
};
