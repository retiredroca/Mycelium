#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include "crypto/myc_crypto.hpp"
#include "storage/myc_storage.hpp"

static inline constexpr uint64_t kTotalSupply = 1'000'000'000;
static inline constexpr uint8_t kDecimals = 6;
static inline constexpr uint64_t kMinStakeAmount = 100;
static inline constexpr uint64_t kPermanentStakeMin = 100;

static inline constexpr uint64_t kMiningBaseReward = 50;        // per block
static inline constexpr uint64_t kMiningHalvingEpochs = 210'000; // reward halves every N epochs
static inline constexpr uint32_t kMiningDiffStart = 16;           // leading zero bits at epoch 0
static inline constexpr uint32_t kMiningDiffInterval = 10'000;    // +1 bit every N epochs
static inline constexpr uint32_t kMiningDiffCap = 28;
static inline constexpr uint64_t kMiningMaxNoncePerAttempt = 2'000'000;

struct Tokenomics {
    uint64_t total_supply = kTotalSupply;
    uint64_t minted_supply = 0;
    uint64_t staked_supply = 0;
    uint64_t burned_supply = 0;
    uint32_t annual_inflation_bps = 800;
    uint64_t current_epoch = 0;

    uint64_t calc_circulating() const {
        return minted_supply - staked_supply - burned_supply;
    }

    uint64_t calc_epoch_reward(uint64_t validator_stake, uint64_t total_stake) const {
        if (total_stake == 0) return 0;
        uint64_t annual_reward = (uint64_t)((uint64_t)total_supply * (uint64_t)annual_inflation_bps / 10000);
        uint64_t daily = annual_reward / 365;
        return daily * validator_stake / total_stake;
    }

    void apply_disinflation() {
        ++current_epoch;
        if (annual_inflation_bps > 150) {
            annual_inflation_bps = (uint32_t)(annual_inflation_bps * 85 / 100);
            if (annual_inflation_bps < 150) annual_inflation_bps = 150;
        }
    }

    void burn(uint64_t amount) { burned_supply += amount; }
};

enum TokenErr : int {
    kTokenOk = 0,
    kTokenInsufficientBalance = -1,
    kTokenInsufficientStaked = -2,
    kTokenInsufficientStake = -3,
    kTokenInvalidSignature = -4,
    kTokenTransferFailed = -5,
    kTokenStakeLocked = -6,
};

static inline const char* token_strerror(TokenErr e) {
    switch (e) {
        case kTokenOk: return "ok";
        case kTokenInsufficientBalance: return "insufficient balance";
        case kTokenInsufficientStaked: return "insufficient staked balance";
        case kTokenInsufficientStake: return "amount below minimum stake";
        case kTokenInvalidSignature: return "invalid signature";
        case kTokenTransferFailed: return "transfer failed";
        case kTokenStakeLocked: return "stake is locked";
        default: return "unknown";
    }
}

struct Wallet {
    std::array<uint8_t, 32> public_key = {};
    uint64_t balance = 0;
    uint64_t staked_balance = 0;
    uint64_t locked_balance = 0;
    uint64_t nonce = 0;

    uint64_t available() const { return balance > locked_balance ? balance - locked_balance : 0; }

    TokenErr stake(uint64_t amount) {
        if (amount < kMinStakeAmount) return kTokenInsufficientStake;
        if (available() < amount) return kTokenInsufficientBalance;
        balance -= amount;
        staked_balance += amount;
        return kTokenOk;
    }

    TokenErr unstake(uint64_t amount) {
        if (staked_balance < amount) return kTokenInsufficientStaked;
        staked_balance -= amount;
        balance += amount;
        return kTokenOk;
    }

    void receive(uint64_t amount) { balance += amount; }

    TokenErr send(uint64_t amount) {
        if (available() < amount) return kTokenInsufficientBalance;
        balance -= amount;
        return kTokenOk;
    }
};

static inline bool mint_to_wallet(Tokenomics& t, Wallet& w, uint64_t amount) {
    if (t.minted_supply + amount > t.total_supply) return false;
    t.minted_supply += amount;
    w.receive(amount);
    return true;
}

static inline uint64_t mining_block_reward(uint64_t epoch) {
    uint64_t halvings = epoch / kMiningHalvingEpochs;
    uint64_t reward = kMiningBaseReward;
    for (uint64_t i = 0; i < halvings; ++i) {
        reward /= 2;
        if (reward == 0) return 1;
    }
    return reward;
}

static inline uint32_t mining_difficulty_bits(uint64_t epoch) {
    uint32_t bits = kMiningDiffStart + (uint32_t)(epoch / kMiningDiffInterval);
    return bits > kMiningDiffCap ? kMiningDiffCap : bits;
}

static inline uint64_t mining_search(const std::array<uint8_t, 32>& pubkey,
                                      uint64_t epoch, uint64_t max_nonce) {
    uint32_t target_bits = mining_difficulty_bits(epoch);
    uint64_t reward = mining_block_reward(epoch);
    if (reward == 0) return UINT64_MAX;

    for (uint64_t n = 0; n < max_nonce; ++n) {
        Sha256Ctx ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, pubkey.data(), 32);
        sha256_update(&ctx, (const uint8_t*)&n, sizeof(n));
        sha256_update(&ctx, (const uint8_t*)&epoch, sizeof(epoch));
        std::array<uint8_t, 32> hash;
        sha256_final(&ctx, hash.data());

        uint32_t leading = 0;
        for (int i = 0; i < 32; ++i) {
            if (hash[i] == 0) { leading += 8; continue; }
            uint8_t b = hash[i];
            while ((b & 0x80) == 0) { ++leading; b <<= 1; }
            break;
        }
        if (leading >= target_bits) return n;
    }
    return UINT64_MAX;
}

struct StakePosition {
    std::string validator_id;
    uint64_t amount = 0;
    uint64_t start_epoch = 0;
    uint64_t unbond_start_epoch = 0;
    bool unbonding = false;
    uint64_t rewards_accrued = 0;

    void initiate_unbond(uint64_t current_epoch) {
        if (!unbonding) {
            unbonding = true;
            unbond_start_epoch = current_epoch;
        }
    }

    bool can_withdraw(uint64_t current_epoch) const {
        return unbonding && (current_epoch - unbond_start_epoch >= 7);
    }
};

struct RewardPool {
    uint64_t relay_rewards = 0;
    uint64_t hosting_rewards = 0;
    uint64_t creation_rewards = 0;
    uint64_t engagement_rewards = 0;
    uint64_t total_distributed = 0;

    void init(uint64_t initial_supply) {
        relay_rewards = initial_supply * 35 / 100;
        hosting_rewards = initial_supply * 40 / 100;
        creation_rewards = initial_supply * 15 / 100;
        engagement_rewards = initial_supply * 10 / 100;
    }

    uint64_t claim_relay(uint64_t bytes_relayed, uint64_t total_bytes) {
        if (total_bytes == 0) return 0;
        uint64_t reward = relay_rewards * bytes_relayed / total_bytes;
        relay_rewards -= reward;
        total_distributed += reward;
        return reward;
    }

    uint64_t claim_hosting(uint64_t storage_gb, uint64_t total_gb) {
        if (total_gb == 0) return 0;
        uint64_t reward = hosting_rewards * storage_gb / total_gb;
        hosting_rewards -= reward;
        total_distributed += reward;
        return reward;
    }

    uint64_t claim_video_hosting(uint64_t storage_gb, uint64_t mbps_served,
                                  uint64_t total_gb, uint64_t total_mbps) {
        if (total_gb == 0 || total_mbps == 0) return 0;
        uint64_t storage_share = hosting_rewards * storage_gb / total_gb;
        uint64_t bw_share = hosting_rewards * mbps_served / total_mbps;
        uint64_t reward = storage_share + bw_share;
        reward = reward > hosting_rewards ? hosting_rewards : reward;
        hosting_rewards -= reward;
        total_distributed += reward;
        return reward;
    }

    uint64_t claim_creation(double engagement_score, bool is_permanent) {
        uint64_t reward = 0;
        if (engagement_score >= 100.0) reward += 10;
        if (engagement_score >= 1000.0) reward += 50;
        if (is_permanent) reward += 200;
        creation_rewards = creation_rewards > reward ? creation_rewards - reward : 0;
        total_distributed += reward;
        return reward;
    }
};

enum TxStatus : uint8_t { kTxPending = 0, kTxConfirmed = 1, kTxFinalized = 2, kTxFailed = 3 };

struct Transaction {
    std::string tx_id;
    std::array<uint8_t, 32> from = {};
    std::array<uint8_t, 32> to = {};
    uint64_t amount = 0;
    uint64_t fee = 5000;
    int64_t timestamp = 0;
    std::vector<uint8_t> signature;
    TxStatus status = kTxPending;

    static inline Transaction create(
        const std::array<uint8_t, 32>& from,
        const std::array<uint8_t, 32>& to,
        uint64_t amount)
    {
        Transaction tx;
        auto now = ProtocolMessage{}.now_sec();
        Sha256Ctx ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, (const uint8_t*)"tx", 2);
        sha256_update(&ctx, from.data(), 32);
        sha256_update(&ctx, to.data(), 32);
        sha256_update(&ctx, (const uint8_t*)&amount, sizeof(amount));
        sha256_update(&ctx, (const uint8_t*)&now, sizeof(now));
        std::array<uint8_t, 32> id_hash;
        sha256_final(&ctx, id_hash.data());
        tx.tx_id = base64_encode(id_hash.data(), 16);
        tx.from = from;
        tx.to = to;
        tx.amount = amount;
        tx.timestamp = now;
        return tx;
    }

    void sign(const std::array<uint8_t, 32>& private_key) {
        std::string msg;
        msg.resize(32 + 32 + 8 + 8);
        memcpy(&msg[0], from.data(), 32);
        memcpy(&msg[32], to.data(), 32);
        memcpy(&msg[64], &amount, sizeof(amount));
        memcpy(&msg[72], &timestamp, sizeof(timestamp));
        auto sig = ed25519_sign((const uint8_t*)msg.data(), msg.size(),
            *reinterpret_cast<const std::array<uint8_t, 32>*>(&private_key));
        signature.assign(sig.begin(), sig.end());
    }

    std::array<uint8_t, 32> tx_hash() const {
        Sha256Ctx ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, (const uint8_t*)tx_id.data(), tx_id.size());
        sha256_update(&ctx, from.data(), 32);
        sha256_update(&ctx, to.data(), 32);
        sha256_update(&ctx, (const uint8_t*)&amount, sizeof(amount));
        sha256_update(&ctx, (const uint8_t*)&fee, sizeof(fee));
        sha256_update(&ctx, (const uint8_t*)&timestamp, sizeof(timestamp));
        std::array<uint8_t, 32> h;
        sha256_final(&ctx, h.data());
        return h;
    }
};

static inline void tx_serialize(const Transaction& tx, std::vector<uint8_t>& out) {
    buf_write_str(out, tx.tx_id);
    out.insert(out.end(), tx.from.begin(), tx.from.end());
    out.insert(out.end(), tx.to.begin(), tx.to.end());
    buf_write_u64(out, tx.amount);
    buf_write_u64(out, tx.fee);
    buf_write_u64(out, (uint64_t)tx.timestamp);
    buf_write_bytes(out, tx.signature.data(), (uint32_t)tx.signature.size());
    out.push_back((uint8_t)tx.status);
}

static inline Transaction tx_deserialize(const uint8_t* data, size_t& off, size_t len) {
    Transaction tx;
    tx.tx_id = buf_read_str(data, off, len);
    if (off + 64 <= len) {
        memcpy(tx.from.data(), data + off, 32); off += 32;
        memcpy(tx.to.data(), data + off, 32); off += 32;
    }
    tx.amount = buf_read_u64(data, off, len);
    tx.fee = buf_read_u64(data, off, len);
    tx.timestamp = (int64_t)buf_read_u64(data, off, len);
    tx.signature = buf_read_bytes(data, off, len);
    if (off < len) tx.status = (TxStatus)data[off++];
    return tx;
}

static inline std::array<uint8_t, 32> calc_merkle_root(const std::vector<Transaction>& txs) {
    if (txs.empty()) return {};
    std::vector<std::array<uint8_t, 32>> hashes;
    hashes.reserve(txs.size());
    for (auto& tx : txs) {
        auto h = tx.tx_hash();
        hashes.push_back(h);
    }
    while (hashes.size() > 1) {
        std::vector<std::array<uint8_t, 32>> next;
        next.reserve((hashes.size() + 1) / 2);
        for (size_t i = 0; i < hashes.size(); i += 2) {
            Sha256Ctx ctx;
            sha256_init(&ctx);
            sha256_update(&ctx, hashes[i].data(), 32);
            if (i + 1 < hashes.size())
                sha256_update(&ctx, hashes[i+1].data(), 32);
            else
                sha256_update(&ctx, hashes[i].data(), 32);
            std::array<uint8_t, 32> h;
            sha256_final(&ctx, h.data());
            next.push_back(h);
        }
        hashes = next;
    }
    return hashes[0];
}

struct Block {
    std::array<uint8_t, 32> prev_hash = {};
    std::array<uint8_t, 32> merkle_root = {};
    int64_t timestamp = 0;
    uint64_t nonce = 0;
    uint64_t epoch = 0;
    std::array<uint8_t, 32> miner_pubkey = {};
    std::vector<Transaction> txs;
    std::vector<uint8_t> signature;

    std::array<uint8_t, 32> calc_hash() const {
        Sha256Ctx ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, prev_hash.data(), 32);
        sha256_update(&ctx, merkle_root.data(), 32);
        sha256_update(&ctx, (const uint8_t*)&timestamp, sizeof(timestamp));
        sha256_update(&ctx, (const uint8_t*)&nonce, sizeof(nonce));
        sha256_update(&ctx, (const uint8_t*)&epoch, sizeof(epoch));
        sha256_update(&ctx, miner_pubkey.data(), 32);
        std::array<uint8_t, 32> h;
        sha256_final(&ctx, h.data());
        return h;
    }

    std::string block_id() const {
        auto h = calc_hash();
        return base64_encode(h.data(), 16);
    }
};

static inline void block_serialize(const Block& block, std::vector<uint8_t>& out) {
    out.insert(out.end(), block.prev_hash.begin(), block.prev_hash.end());
    out.insert(out.end(), block.merkle_root.begin(), block.merkle_root.end());
    buf_write_u64(out, (uint64_t)block.timestamp);
    buf_write_u64(out, block.nonce);
    buf_write_u64(out, block.epoch);
    out.insert(out.end(), block.miner_pubkey.begin(), block.miner_pubkey.end());
    buf_write_bytes(out, block.signature.data(), (uint32_t)block.signature.size());
    uint32_t num_txs = (uint32_t)block.txs.size();
    buf_write_u32(out, num_txs);
    for (auto& tx : block.txs) {
        tx_serialize(tx, out);
    }
}

static inline Block block_deserialize(const uint8_t* data, size_t& off, size_t len) {
    Block block;
    if (off + 32 <= len) { memcpy(block.prev_hash.data(), data + off, 32); off += 32; }
    if (off + 32 <= len) { memcpy(block.merkle_root.data(), data + off, 32); off += 32; }
    block.timestamp = (int64_t)buf_read_u64(data, off, len);
    block.nonce = buf_read_u64(data, off, len);
    block.epoch = buf_read_u64(data, off, len);
    if (off + 32 <= len) { memcpy(block.miner_pubkey.data(), data + off, 32); off += 32; }
    block.signature = buf_read_bytes(data, off, len);
    uint32_t num_txs = buf_read_u32(data, off, len);
    for (uint32_t i = 0; i < num_txs; ++i)
        block.txs.push_back(tx_deserialize(data, off, len));
    return block;
}

static inline uint64_t block_mining_search(Block& block, uint64_t max_nonce) {
    uint32_t target_bits = mining_difficulty_bits(block.epoch);
    for (uint64_t n = 0; n < max_nonce; ++n) {
        block.nonce = n;
        auto hash = block.calc_hash();
        uint32_t leading = 0;
        for (int i = 0; i < 32; ++i) {
            if (hash[i] == 0) { leading += 8; continue; }
            uint8_t b = hash[i];
            while ((b & 0x80) == 0) { ++leading; b <<= 1; }
            break;
        }
        if (leading >= target_bits) return n;
    }
    return UINT64_MAX;
}
