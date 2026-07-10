#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cstdarg>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

// g_state is defined in main.cpp (same translation unit)

// ============================================================
// Web log ring buffer
// ============================================================
static std::vector<std::string> g_web_log;
static constexpr size_t kWebLogMax = 500;

static inline void web_log(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (g_web_log.size() >= kWebLogMax)
        g_web_log.erase(g_web_log.begin());
    g_web_log.push_back(buf);
}

// ============================================================
// Socket helpers
// ============================================================
#ifdef _WIN32
using sock_t = SOCKET;
static inline int sck_recv(sock_t s, char* buf, int sz) { return recv(s, buf, sz, 0); }
static inline int sck_send(sock_t s, const char* buf, int sz) { return send(s, buf, sz, 0); }
static inline void sck_close(sock_t s) { shutdown(s, SD_SEND); closesocket(s); }
#else
using sock_t = int;
static inline int sck_recv(sock_t s, char* buf, int sz) { return (int)read(s, buf, (size_t)sz); }
static inline int sck_send(sock_t s, const char* buf, int sz) { return (int)write(s, buf, (size_t)sz); }
static inline void sck_close(sock_t s) { shutdown(s, SHUT_WR); close(s); }
#endif

static inline void send_all(sock_t s, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int n = (int)(len - sent > 65536 ? 65536 : len - sent);
        int r = sck_send(s, data + sent, n);
        if (r <= 0) break;
        sent += (size_t)r;
    }
}

static inline void send_response(sock_t s, int status, const char* status_text,
                                   const char* content_type, const char* body, size_t body_len) {
    char header[512];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "\r\n",
        status, status_text, content_type, body_len);
    send_all(s, header, (size_t)hlen);
    if (body_len > 0) send_all(s, body, body_len);
    sck_close(s);
}

static inline void send_json(sock_t s, int status, const std::string& json) {
    send_response(s, status, status == 200 ? "OK" : status == 400 ? "Bad Request"
                                        : status == 404 ? "Not Found"
                                        : status == 405 ? "Method Not Allowed"
                                        : "Internal Server Error",
                  "application/json; charset=utf-8", json.c_str(), json.size());
}

// ============================================================
// HTTP request parsing
// ============================================================
struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::string content_type;
};

static inline HttpRequest parse_http_request(const char* raw, size_t len) {
    HttpRequest req;
    std::string buf(raw, len);

    // Parse request line: "GET /path HTTP/1.1"
    auto line_end = buf.find("\r\n");
    if (line_end == std::string::npos) return req;
    auto first_space = buf.find(' ');
    if (first_space == std::string::npos || first_space >= line_end) return req;
    auto second_space = buf.find(' ', first_space + 1);
    if (second_space == std::string::npos || second_space >= line_end) return req;
    req.method = buf.substr(0, first_space);
    req.path = buf.substr(first_space + 1, second_space - first_space - 1);

    // Parse headers to find Content-Length and Content-Type
    size_t headers_end = buf.find("\r\n\r\n");
    if (headers_end == std::string::npos) return req;

    auto cl_pos = buf.find("Content-Length:");
    if (cl_pos != std::string::npos && cl_pos < headers_end) {
        auto val_start = buf.find_first_of("0123456789", cl_pos + 15);
        if (val_start != std::string::npos) {
            size_t body_len = (size_t)atoll(buf.c_str() + val_start);
            size_t body_start = headers_end + 4;
            if (body_start + body_len <= len)
                req.body = buf.substr(body_start, body_len);
        }
    }

    auto ct_pos = buf.find("Content-Type:");
    if (ct_pos != std::string::npos && ct_pos < headers_end) {
        auto val_start = buf.find_first_not_of(" \t", ct_pos + 13);
        auto val_end = buf.find("\r\n", val_start);
        if (val_start != std::string::npos && val_end != std::string::npos)
            req.content_type = buf.substr(val_start, val_end - val_start);
    }

    return req;
}

// ============================================================
// Minimal JSON helpers
// ============================================================
static inline std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (auto c : s) {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (c == '\n') { out += "\\n"; }
        else if (c == '\r') { out += "\\r"; }
        else if (c == '\t') { out += "\\t"; }
        else out += c;
    }
    return out;
}

static inline std::string json_key_val(const std::string& key, const std::string& val) {
    return "\"" + json_escape(key) + "\":\"" + json_escape(val) + "\"";
}

static inline std::string json_key_num(const std::string& key, uint64_t val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%llu", (unsigned long long)val);
    return "\"" + json_escape(key) + "\":" + buf;
}

static inline std::string json_key_bool(const std::string& key, bool val) {
    return "\"" + json_escape(key) + "\":" + (val ? "true" : "false");
}

static inline std::string json_extract_string(const std::string& json, const std::string& key) {
    auto k = "\"" + key + "\":\"";
    auto pos = json.find(k);
    if (pos == std::string::npos) return "";
    pos += k.size();
    std::string out;
    for (; pos < json.size(); ++pos) {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            out += json[++pos];
        } else if (json[pos] == '"') {
            break;
        } else {
            out += json[pos];
        }
    }
    return out;
}

static inline uint64_t json_extract_uint64(const std::string& json, const std::string& key) {
    auto k = "\"" + key + "\":";
    auto pos = json.find(k);
    if (pos == std::string::npos) return 0;
    pos += k.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    return (uint64_t)atoll(json.c_str() + pos);
}

static inline std::string json_array(const std::vector<std::string>& items) {
    std::string out = "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) out += ",";
        out += "\"" + json_escape(items[i]) + "\"";
    }
    out += "]";
    return out;
}

// ============================================================
// API handlers
// ============================================================
static inline void handle_api_status(sock_t s) {
    std::string json = "{";
    // Wallet
    json += "\"wallet\":{";
    if (g_state.wallet.public_key != std::array<uint8_t, 32>{}) {
        auto addr = base64_encode(g_state.wallet.public_key.data(), 16);
        json += json_key_val("address", "MYT" + addr) + ",";
        json += json_key_num("balance", g_state.wallet.balance) + ",";
        json += json_key_num("staked", g_state.wallet.staked_balance) + ",";
        json += json_key_num("available", g_state.wallet.available());
    } else {
        json += "\"address\":null,\"balance\":0,\"staked\":0,\"available\":0";
    }
    json += "},";
    // Node
    json += "\"node\":{";
    json += json_key_bool("running", g_state.node_running) + ",";
    json += json_key_val("peer_id", g_state.node ? g_state.node->local_peer_id() : "") + ",";
    json += json_key_num("peers", g_state.node ? g_state.node->peer_count() : 0) + ",";
    json += json_key_num("mempool", g_state.node ? g_state.node->mempool_size() : 0) + ",";
    json += json_key_num("height", g_state.node ? g_state.node->chain.size() : 0);
    json += "},";
    // Tokenomics
    json += "\"tokenomics\":{";
    json += json_key_num("epoch", g_state.tokenomics.current_epoch) + ",";
    json += json_key_num("reward", mining_block_reward(g_state.tokenomics.current_epoch)) + ",";
    json += json_key_num("difficulty", mining_difficulty_bits(g_state.tokenomics.current_epoch)) + ",";
    json += json_key_num("minted", g_state.tokenomics.minted_supply) + ",";
    json += json_key_num("supply", g_state.tokenomics.total_supply);
    json += "},";
    // Profile
    json += "\"profile\":{";
    if (!g_state.my_profile.peer_id.empty()) {
        json += json_key_val("peer_id", g_state.my_profile.peer_id) + ",";
        json += json_key_val("display_name", g_state.my_profile.display_name) + ",";
        json += json_key_val("username", g_state.my_profile.username) + ",";
        json += json_key_val("bio", g_state.my_profile.bio) + ",";
        json += json_key_val("avatar_cid", g_state.my_profile.avatar_cid) + ",";
        json += json_key_val("banner_cid", g_state.my_profile.banner_cid) + ",";
        json += json_key_val("wallet", g_state.wallet.public_key != std::array<uint8_t, 32>{} ? ("MYT" + base64_encode(g_state.wallet.public_key.data(), 16)) : "");
    } else {
        json += "\"peer_id\":null";
    }
    json += "}";
    json += "}";
    send_json(s, 200, json);
}

static inline void handle_api_logs(sock_t s, const std::string& path) {
    size_t since = 0;
    auto qpos = path.find("?since=");
    if (qpos != std::string::npos)
        since = (size_t)atoll(path.c_str() + qpos + 7);

    std::string json = "{";
    json += json_key_num("total", g_web_log.size()) + ",";
    json += "\"entries\":[";
    for (size_t i = since; i < g_web_log.size(); ++i) {
        if (i > since) json += ",";
        json += "\"" + json_escape(g_web_log[i]) + "\"";
    }
    json += "]}";
    send_json(s, 200, json);
}

static inline void handle_api_wallet_create(sock_t s, const std::string& body) {
    auto passphrase = json_extract_string(body, "passphrase");
    if (passphrase.empty() || !validate_passphrase(passphrase.c_str())) {
        send_json(s, 400, "{\"ok\":false,\"error\":\"Invalid passphrase\"}");
        return;
    }

    std::vector<std::string> words;
    std::array<uint8_t, 64> seed;
    std::array<uint8_t, 32> priv, pub;
    if (!wallet_create_full(words, seed, priv, pub, passphrase.c_str())) {
        send_json(s, 500, "{\"ok\":false,\"error\":\"Wallet creation failed\"}");
        return;
    }

    std::string wallet_path = "./data/chain/wallet.dat";
    wallet_save(wallet_path, priv, words, passphrase.c_str());

    g_state.wallet = Wallet{};
    g_state.wallet.public_key = pub;
    g_state.private_key = priv;
    web_log("[WEB] Wallet created: MYT%s\n", base64_encode(pub.data(), 16).c_str());

    std::string json = "{\"ok\":true,";
    json += json_key_val("address", "MYT" + base64_encode(pub.data(), 16)) + ",";
    json += "\"mnemonic\":[";
    for (size_t i = 0; i < words.size(); ++i) {
        if (i > 0) json += ",";
        json += "\"" + json_escape(words[i]) + "\"";
    }
    json += "]}";
    send_json(s, 200, json);
}

static inline void handle_api_wallet_restore(sock_t s, const std::string& body) {
    auto words_str = json_extract_string(body, "words");
    auto passphrase = json_extract_string(body, "passphrase");

    // Parse space-separated words
    std::vector<std::string> words;
    size_t pos = 0;
    while (pos < words_str.size()) {
        while (pos < words_str.size() && words_str[pos] == ' ') ++pos;
        if (pos >= words_str.size()) break;
        auto start = pos;
        while (pos < words_str.size() && words_str[pos] != ' ') ++pos;
        words.push_back(words_str.substr(start, pos - start));
    }

    if (words.size() != 24) {
        send_json(s, 400, "{\"ok\":false,\"error\":\"Need exactly 24 words\"}");
        return;
    }

    std::array<uint8_t, 32> entropy;
    if (!mnemonic_restore(words, entropy)) {
        send_json(s, 400, "{\"ok\":false,\"error\":\"Invalid checksum\"}");
        return;
    }

    if (passphrase.empty()) {
        send_json(s, 400, "{\"ok\":false,\"error\":\"Passphrase required\"}");
        return;
    }

    std::array<uint8_t, 64> seed;
    mnemonic_to_seed(words, passphrase.c_str(), seed.data());
    auto hash = sha512(seed.data(), 64);
    std::array<uint8_t, 32> priv;
    memcpy(priv.data(), hash.data(), 32);
    priv[0] &= 248; priv[31] &= 127; priv[31] |= 64;
    auto pub = ed25519_pubkey(priv);

    std::string wallet_path = "./data/chain/wallet.dat";
    wallet_save(wallet_path, priv, words, passphrase.c_str());

    g_state.wallet = Wallet{};
    g_state.wallet.public_key = pub;
    g_state.private_key = priv;

    std::string json = "{\"ok\":true,";
    json += json_key_val("address", "MYT" + base64_encode(pub.data(), 16));
    json += "}";
    send_json(s, 200, json);
}

static inline void handle_api_node_start(sock_t s) {
    if (g_state.node_running) {
        send_json(s, 200, "{\"ok\":true,\"message\":\"Already running\"}");
        return;
    }

    if (g_state.node) { delete g_state.node; g_state.node = nullptr; }
    {
        P2pConfig p2p = g_state.config.to_p2p_config();
        g_state.node = new MyceliumNode(MyceliumNode::create(p2p));
        g_state.node->init_storage(p2p.storage);
        g_state.node->load_chain(g_state.tokenomics, g_state.wallet);
        g_state.node_running = true;
    }
    web_log("[WEB] Node started: %s\n", g_state.node->local_peer_id().c_str());

    std::string json = "{\"ok\":true,";
    json += json_key_val("peer_id", g_state.node->local_peer_id());
    json += "}";
    send_json(s, 200, json);
}

static inline void handle_api_node_stop(sock_t s) {
    if (!g_state.node_running) {
        send_json(s, 200, "{\"ok\":true,\"message\":\"Not running\"}");
        return;
    }
    g_state.node_running = false;
    if (g_state.node) { delete g_state.node; g_state.node = nullptr; }
    web_log("[WEB] Node stopped.\n");
    send_json(s, 200, "{\"ok\":true}");
}

static inline void handle_api_mine(sock_t s) {
    if (g_state.private_key == std::array<uint8_t, 32>{}) {
        send_json(s, 400, "{\"ok\":false,\"error\":\"No wallet\"}");
        return;
    }

    if (!g_state.node) {
        P2pConfig p2p = g_state.config.to_p2p_config();
        g_state.node = new MyceliumNode(MyceliumNode::create(p2p));
        g_state.node->init_storage(p2p.storage);
        g_state.node->load_chain(g_state.tokenomics, g_state.wallet);
        g_state.node_running = true;
    }

    uint64_t epoch = g_state.tokenomics.current_epoch;
    uint64_t reward = mining_block_reward(epoch);
    web_log("[WEB] Mining epoch %llu...\n", (unsigned long long)epoch);

    bool found = g_state.node->mine_block(
        g_state.tokenomics, g_state.wallet,
        g_state.private_key, g_state.config.mining_max_nonce);

    if (!found) {
        send_json(s, 200, "{\"ok\":false,\"error\":\"No block found\"}");
        return;
    }

    char json[256];
    snprintf(json, sizeof(json),
        "{\"ok\":true,\"block_height\":%zu,\"reward\":%llu}",
        g_state.node->chain.size() - 1, (unsigned long long)reward);
    send_json(s, 200, json);
}

static inline void handle_api_token_send(sock_t s, const std::string& body) {
    if (g_state.private_key == std::array<uint8_t, 32>{}) {
        send_json(s, 400, "{\"ok\":false,\"error\":\"No wallet\"}");
        return;
    }

    auto to_addr = json_extract_string(body, "to");
    uint64_t amount = json_extract_uint64(body, "amount");

    if (to_addr.empty() || amount == 0) {
        send_json(s, 400, "{\"ok\":false,\"error\":\"Invalid params\"}");
        return;
    }

    // Strip "MYT" prefix if present
    if (to_addr.size() > 3 && to_addr.substr(0, 3) == "MYT")
        to_addr = to_addr.substr(3);

    auto to_bytes = base64_decode(to_addr);
    if (to_bytes.size() < 32) {
        send_json(s, 400, "{\"ok\":false,\"error\":\"Invalid address\"}");
        return;
    }

    std::array<uint8_t, 32> to_pk = {};
    memcpy(to_pk.data(), to_bytes.data(), 32);

    auto err = g_state.wallet.send(amount);
    if (err != kTokenOk) {
        std::string msg = "{\"ok\":false,\"error\":\"";
        msg += token_strerror(err);
        msg += "\"}";
        send_json(s, 400, msg);
        return;
    }

    auto tx = Transaction::create(g_state.wallet.public_key, to_pk, amount);
    tx.sign(g_state.private_key);

    if (g_state.node) {
        g_state.node->add_to_mempool(tx);
        g_state.node->broadcast_transaction(tx);
    }

    web_log("[WEB] Sent %llu to %s (tx: %s)\n",
            (unsigned long long)amount, to_addr.c_str(), tx.tx_id.c_str());

    std::string json = "{\"ok\":true,";
    json += json_key_val("tx_id", tx.tx_id) + ",";
    json += json_key_num("balance", g_state.wallet.balance);
    json += "}";
    send_json(s, 200, json);
}

static inline void handle_api_mempool(sock_t s) {
    if (!g_state.node) {
        send_json(s, 200, "{\"txs\":[],\"count\":0}");
        return;
    }
    auto txs = g_state.node->get_mempool_txs();
    std::string json = "{\"count\":" + std::to_string(txs.size()) + ",\"txs\":[";
    for (size_t i = 0; i < txs.size(); ++i) {
        if (i > 0) json += ",";
        json += "{";
        json += json_key_val("id", txs[i].tx_id) + ",";
        json += json_key_val("from", base64_encode(txs[i].from.data(), 16)) + ",";
        json += json_key_val("to", base64_encode(txs[i].to.data(), 16)) + ",";
        json += json_key_num("amount", txs[i].amount) + ",";
        json += json_key_num("fee", txs[i].fee) + ",";
        json += json_key_num("timestamp", (uint64_t)txs[i].timestamp);
        json += "}";
    }
    json += "]}";
    send_json(s, 200, json);
}

static inline void handle_api_feed(sock_t s) {
    if (!g_state.node) {
        send_json(s, 200, "{\"posts\":[]}");
        return;
    }
    std::string json = "{\"posts\":[";
    bool first = true;
    for (auto& [id, post] : g_state.node->post_cache) {
        if (!first) json += ",";
        first = false;
        json += "{";
        json += json_key_val("id", post.id) + ",";
        json += json_key_val("author", post.author) + ",";
        json += json_key_val("content", post.content.text) + ",";
        json += json_key_num("timestamp", (uint64_t)post.created_at);
        json += "}";
    }
    json += "]}";
    if (first) json = "{\"posts\":[]}";
    send_json(s, 200, json);
}

static inline void handle_api_profile_create(sock_t s, const std::string& body) {
    auto dn_pos = body.find("\"display_name\"");
    auto un_pos = body.find("\"username\"");
    std::string display_name, username;

    if (dn_pos != std::string::npos) {
        auto start = body.find('"', dn_pos + 15);
        if (start != std::string::npos) {
            auto end = body.find('"', start + 1);
            if (end != std::string::npos) display_name = body.substr(start + 1, end - start - 1);
        }
    }
    if (un_pos != std::string::npos) {
        auto start = body.find('"', un_pos + 11);
        if (start != std::string::npos) {
            auto end = body.find('"', start + 1);
            if (end != std::string::npos) username = body.substr(start + 1, end - start - 1);
        }
    }
    if (display_name.empty()) {
        send_json(s, 200, "{\"ok\":false,\"error\":\"display_name is required\"}");
        return;
    }

    // Auto-create wallet if none exists
    bool new_wallet = false;
    std::vector<std::string> words;
    if (g_state.wallet.public_key == std::array<uint8_t, 32>{}) {
        std::array<uint8_t, 64> seed;
        std::array<uint8_t, 32> priv, pub;
        const char* auto_pass = "mycelium-auto-" MYCELIUM_VERSION;
        if (wallet_create_full(words, seed, priv, pub, auto_pass)) {
            std::string wallet_path = "./data/chain/wallet.dat";
            wallet_save(wallet_path, priv, words, auto_pass);
            g_state.wallet = Wallet{};
            g_state.wallet.public_key = pub;
            g_state.private_key = priv;
            new_wallet = true;
        }
    }

    // Derive x25519 keypair from wallet private key
    KeyPair kp;
    if (g_state.wallet.public_key != std::array<uint8_t, 32>{}) {
        memcpy(kp.sk.scalar.data(), g_state.private_key.data(), 32);
        kp.sk.scalar[0] &= 248; kp.sk.scalar[31] &= 127; kp.sk.scalar[31] |= 64;
        uint8_t bp[32] = {9};
        _x25519_mul(kp.pk.point.data(), kp.sk.scalar.data(), bp);
    } else {
        kp = x25519_keygen();
    }

    auto pk = ed25519_pubkey(g_state.private_key);
    std::string peer_id = base64_encode(pk.data(), 16);
    auto builder = ProfileBuilder::create(peer_id, display_name.c_str());
    if (!username.empty()) builder.with_username(username.c_str());
    g_state.my_profile = builder.sign(kp).build();

    // Store in profile list
    g_state.profiles.push_back(g_state.my_profile);
    g_state.active_profile = (int)g_state.profiles.size() - 1;

    std::string json = "{\"ok\":true,";
    json += json_key_val("peer_id", g_state.my_profile.peer_id) + ",";
    json += json_key_val("display_name", g_state.my_profile.display_name) + ",";
    json += json_key_val("username", g_state.my_profile.username) + ",";
    json += json_key_num("profile_index", g_state.active_profile) + ",";
    json += json_key_val("wallet", "MYT" + base64_encode(g_state.wallet.public_key.data(), 16));
    if (new_wallet) {
        json += ",\"mnemonic\":[";
        for (size_t i = 0; i < words.size(); ++i) {
            if (i > 0) json += ",";
            json += "\"" + json_escape(words[i]) + "\"";
        }
        json += "]";
    }
    json += "}";
    send_json(s, 200, json);
}

static inline void handle_api_post_create(sock_t s, const std::string& body) {
    auto ct_pos = body.find("\"content\"");
    std::string content;
    if (ct_pos != std::string::npos) {
        auto start = body.find('"', ct_pos + 10);
        if (start != std::string::npos) {
            auto end = body.find('"', start + 1);
            if (end != std::string::npos) content = body.substr(start + 1, end - start - 1);
        }
    }
    if (content.empty()) {
        send_json(s, 200, "{\"ok\":false,\"error\":\"content is required\"}");
        return;
    }

    // Use wallet keypair if available
    KeyPair kp;
    if (g_state.wallet.public_key != std::array<uint8_t, 32>{}) {
        memcpy(kp.sk.scalar.data(), g_state.private_key.data(), 32);
        kp.sk.scalar[0] &= 248; kp.sk.scalar[31] &= 127; kp.sk.scalar[31] |= 64;
        uint8_t bp[32] = {9};
        _x25519_mul(kp.pk.point.data(), kp.sk.scalar.data(), bp);
    } else {
        kp = x25519_keygen();
    }

    auto author = base64_encode(kp.pk.point.data(), 16);
    auto post = Post::create(author, content.c_str(), kp);
    if (g_state.node) g_state.node->store_post(post);

    std::string json = "{\"ok\":true,";
    json += json_key_val("id", post.id) + ",";
    json += json_key_val("author", post.author) + ",";
    json += "\"ttl\":" + std::to_string(post.ttl_seconds);
    json += "}";
    send_json(s, 200, json);
}

static inline void handle_api_search(sock_t s, const std::string& path) {
    auto qpos = path.find("q=");
    std::string query;
    if (qpos != std::string::npos) {
        query = path.substr(qpos + 2);
        // URL decode plus to space
        for (size_t i = 0; i < query.size(); ++i)
            if (query[i] == '+') query[i] = ' ';
    }
    std::string json = "{\"query\":\"" + query + "\",\"results\":[";
    bool first = true;
    // Search posts
    if (g_state.node) {
        for (auto& [id, post] : g_state.node->post_cache) {
            if (query.empty()) continue;
            bool match = post.author.find(query) != std::string::npos ||
                         post.content.text.find(query) != std::string::npos;
            if (!match) continue;
            if (!first) json += ",";
            first = false;
            json += "{";
            json += json_key_val("id", post.id) + ",";
            json += json_key_val("author", post.author) + ",";
            json += json_key_val("content", post.content.text) + ",";
            json += "\"type\":\"post\"";
            json += "}";
        }
    }
    // Search profile
    if (!g_state.my_profile.peer_id.empty()) {
        bool match = query.empty() ||
                     g_state.my_profile.display_name.find(query) != std::string::npos ||
                     g_state.my_profile.username.find(query) != std::string::npos;
        if (match) {
            if (!first) json += ",";
            first = false;
            json += "{";
            json += json_key_val("id", g_state.my_profile.peer_id) + ",";
            json += json_key_val("author", g_state.my_profile.display_name) + ",";
            json += json_key_val("description", g_state.my_profile.bio) + ",";
            json += "\"type\":\"profile\"";
            json += "}";
        }
    }
    json += "]}";
    send_json(s, 200, json);
}

// ============================================================
// Social API handlers
// ============================================================
static inline void handle_api_social_follow(sock_t s, const std::string& body) {
    auto peer = json_extract_string(body, "peer");
    if (peer.empty()) {
        send_json(s, 400, "{\"ok\":false,\"error\":\"peer required\"}");
        return;
    }
    auto err = g_state.social.follow(peer);
    send_json(s, 200, "{\"ok\":true,\"status\":" + std::to_string(err) + "}");
}

static inline void handle_api_social_unfollow(sock_t s, const std::string& body) {
    auto peer = json_extract_string(body, "peer");
    if (peer.empty()) {
        send_json(s, 400, "{\"ok\":false,\"error\":\"peer required\"}");
        return;
    }
    auto err = g_state.social.unfollow(peer);
    send_json(s, 200, "{\"ok\":true,\"status\":" + std::to_string(err) + "}");
}

static inline void handle_api_social_block(sock_t s, const std::string& body) {
    auto peer = json_extract_string(body, "peer");
    if (peer.empty()) {
        send_json(s, 400, "{\"ok\":false,\"error\":\"peer required\"}");
        return;
    }
    auto err = g_state.social.block(peer);
    send_json(s, 200, "{\"ok\":true,\"status\":" + std::to_string(err) + "}");
}

static inline void handle_api_social_unblock(sock_t s, const std::string& body) {
    auto peer = json_extract_string(body, "peer");
    if (peer.empty()) {
        send_json(s, 400, "{\"ok\":false,\"error\":\"peer required\"}");
        return;
    }
    auto err = g_state.social.unblock(peer);
    send_json(s, 200, "{\"ok\":true,\"status\":" + std::to_string(err) + "}");
}

static inline void handle_api_social_relationship(sock_t s, const std::string& path) {
    auto qpos = path.find("peer=");
    std::string peer;
    if (qpos != std::string::npos) {
        peer = path.substr(qpos + 5);
    }
    if (peer.empty()) {
        send_json(s, 400, "{\"ok\":false,\"error\":\"peer required\"}");
        return;
    }
    auto rel = g_state.social.relationship(peer);
    std::string json = "{";
    json += json_key_val("peer", peer) + ",";
    json += json_key_num("relationship", rel) + ",";
    json += json_key_bool("following", g_state.social.is_following(peer)) + ",";
    json += json_key_bool("blocked", g_state.social.is_blocked(peer));
    json += "}";
    send_json(s, 200, json);
}

// ============================================================
// Profile list / switch API
// ============================================================
static inline void handle_api_profiles(sock_t s) {
    std::string json = "{\"profiles\":[";
    for (size_t i = 0; i < g_state.profiles.size(); ++i) {
        if (i > 0) json += ",";
        auto& p = g_state.profiles[i];
        json += "{";
        json += json_key_num("index", i) + ",";
        json += json_key_val("peer_id", p.peer_id) + ",";
        json += json_key_val("display_name", p.display_name) + ",";
        json += json_key_val("username", p.username) + ",";
        json += json_key_val("bio", p.bio) + ",";
        json += json_key_bool("active", (int)i == g_state.active_profile);
        json += "}";
    }
    json += "]}";
    send_json(s, 200, json);
}

static inline void handle_api_profile_switch(sock_t s, const std::string& body) {
    int idx = (int)json_extract_uint64(body, "index");
    if (idx < 0 || idx >= (int)g_state.profiles.size()) {
        send_json(s, 400, "{\"ok\":false,\"error\":\"Invalid profile index\"}");
        return;
    }
    g_state.active_profile = idx;
    g_state.my_profile = g_state.profiles[idx];
    std::string json = "{\"ok\":true,";
    json += json_key_val("peer_id", g_state.my_profile.peer_id) + ",";
    json += json_key_val("display_name", g_state.my_profile.display_name);
    json += "}";
    send_json(s, 200, json);
}

// ============================================================
// Wallet management API (mgmt)
// ============================================================
static inline void handle_api_wallets(sock_t s) {
    std::string json = "{\"wallets\":[";
    bool has_wallet = g_state.wallet.public_key != std::array<uint8_t, 32>{};
    if (has_wallet) {
        auto addr = base64_encode(g_state.wallet.public_key.data(), 16);
        json += "{";
        json += json_key_val("address", "MYT" + addr) + ",";
        json += json_key_num("balance", g_state.wallet.balance) + ",";
        json += json_key_num("available", g_state.wallet.available()) + ",";
        json += json_key_num("staked", g_state.wallet.staked_balance);
        json += "}";
    }
    json += "],";
    json += json_key_val("active_wallet", has_wallet ? "MYT" + base64_encode(g_state.wallet.public_key.data(), 16) : "");
    json += "}";
    send_json(s, 200, json);
}

static inline void handle_api_wallet_link(sock_t s, const std::string& body) {
    if (g_state.my_profile.peer_id.empty()) {
        send_json(s, 400, "{\"ok\":false,\"error\":\"No active profile\"}");
        return;
    }
    if (g_state.wallet.public_key == std::array<uint8_t, 32>{}) {
        send_json(s, 400, "{\"ok\":false,\"error\":\"No wallet to link\"}");
        return;
    }
    // Link = profile is already associated with the global wallet
    std::string json = "{\"ok\":true,";
    json += json_key_val("profile", g_state.my_profile.peer_id) + ",";
    auto addr = base64_encode(g_state.wallet.public_key.data(), 16);
    json += json_key_val("wallet", "MYT" + addr);
    json += "}";
    send_json(s, 200, json);
}

static inline void handle_api_wallet_unlink(sock_t s, const std::string& body) {
    if (g_state.my_profile.peer_id.empty()) {
        send_json(s, 400, "{\"ok\":false,\"error\":\"No active profile\"}");
        return;
    }
    // Unlink: nothing to do for now since wallet is global
    std::string json = "{\"ok\":true,";
    json += json_key_val("profile", g_state.my_profile.peer_id);
    json += "}";
    send_json(s, 200, json);
}

// ============================================================
// Dashboard HTML
// ============================================================
// Search history (in-memory)
// ============================================================
static std::vector<std::string> g_search_history;

static inline void serve_client_dashboard(sock_t s) {
    size_t peer_count = g_state.node ? g_state.node->peer_count() : 0;
    size_t height = g_state.node ? g_state.node->chain.size() : 0;
    uint64_t epoch = g_state.tokenomics.current_epoch;
    uint64_t reward = mining_block_reward(epoch);
    bool has_profile = !g_state.my_profile.peer_id.empty();

    char html[32768];
    int len = snprintf(html, sizeof(html), R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>MyTube</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
:root{--red:#FF0000;--bg:#0f0f0f;--bg2:#1a1a1a;--bg3:#222;--text:#fff;--text2:#aaa;--border:#333}
body{font-family:'Segoe UI',Arial,sans-serif;background:var(--bg);color:var(--text);min-height:100vh}
.nav{position:fixed;top:0;left:0;right:0;height:56px;background:var(--bg2);border-bottom:1px solid var(--border);display:flex;align-items:center;padding:0 16px;z-index:100;gap:12px}
.nav .logo{font-size:20px;font-weight:700;color:var(--text);display:flex;align-items:center;gap:6px;cursor:pointer;white-space:nowrap}
.nav .logo .r{color:var(--red)}
.nav .logo svg{width:28px;height:28px}
.search-wrap{flex:1;max-width:640px;margin:0 auto;display:flex}
.search-wrap input{flex:1;padding:8px 14px;background:var(--bg);border:1px solid var(--border);border-right:none;border-radius:24px 0 0 24px;color:var(--text);font-size:14px;outline:none}
.search-wrap input:focus{border-color:var(--red)}
.search-wrap button{padding:8px 20px;background:var(--bg3);border:1px solid var(--border);border-radius:0 24px 24px 0;color:var(--text2);font-size:14px;cursor:pointer;display:flex;align-items:center}
.nav-right{display:flex;align-items:center;gap:8px;position:relative}
.nav-right .profile-btn{width:32px;height:32px;border-radius:50%%;background:var(--red);border:none;color:#fff;font-size:14px;cursor:pointer;display:flex;align-items:center;justify-content:center}
.nav-right .profile-dropdown{display:none;position:absolute;top:44px;right:0;background:var(--bg2);border:1px solid var(--border);border-radius:12px;min-width:220px;z-index:300;overflow:hidden}
.nav-right .profile-dropdown.show{display:block}
.nav-right .profile-dropdown .pd-item{padding:12px 16px;cursor:pointer;font-size:14px;color:var(--text);border-bottom:1px solid var(--border)}
.nav-right .profile-dropdown .pd-item:hover{background:var(--bg3)}
.nav-right .profile-dropdown .pd-item .pd-small{font-size:12px;color:var(--text2)}
.nav-right .profile-dropdown .pd-item.pd-active{background:var(--bg3);border-left:3px solid var(--red)}
.content{display:flex;margin-top:56px;max-width:1400px;margin-left:auto;margin-right:auto;padding:16px 24px;gap:24px}
.sidebar{width:260px;flex-shrink:0}
.sidebar .section{margin-bottom:24px}
.sidebar .section h3{font-size:14px;color:var(--text2);margin-bottom:8px;padding-left:12px}
.sidebar .tag{display:block;padding:8px 12px;color:var(--text2);font-size:14px;cursor:pointer;border-radius:8px;text-decoration:none}
.sidebar .tag:hover{background:var(--bg3);color:var(--text)}
.main-col{flex:1;min-width:0}
.video-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:16px}
.video-card{background:var(--bg2);border-radius:12px;overflow:hidden;cursor:pointer;transition:transform .1s;border:1px solid var(--border)}
.video-card:hover{transform:scale(1.02)}
.thumb{width:100%%;aspect-ratio:16/9;display:flex;align-items:center;justify-content:center;font-size:48px;position:relative}
.thumb .dur{position:absolute;bottom:6px;right:6px;background:rgba(0,0,0,.8);padding:2px 6px;border-radius:4px;font-size:12px;color:#fff}
.video-info{padding:12px}
.video-info .title{font-size:14px;font-weight:600;margin-bottom:4px;display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden}
.video-info .meta{font-size:13px;color:var(--text2)}
.video-info .actions{display:flex;gap:6px;margin-top:8px}
.video-info .actions button{font-size:12px;padding:4px 10px;border:none;border-radius:4px;cursor:pointer;font-weight:600}
.section-title{font-size:20px;font-weight:700;margin-bottom:16px;display:flex;align-items:center;gap:8px}
.section-title .badge{font-size:12px;padding:2px 8px;border-radius:4px}
.modal-overlay{display:none;position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,.7);z-index:200;align-items:center;justify-content:center}
.modal-box{background:var(--bg2);border:1px solid var(--border);border-radius:16px;padding:24px;max-width:520px;width:90%%;position:relative}
.modal-box h2{font-size:18px;margin-bottom:16px}
.modal-box input,.modal-box textarea{width:100%%;padding:10px 14px;background:var(--bg3);border:1px solid var(--border);border-radius:8px;color:var(--text);font-size:14px;outline:none;margin:6px 0}
.modal-box input:focus{border-color:var(--red)}
.modal-box .btn-row{display:flex;gap:8px;margin-top:12px;justify-content:flex-end;flex-wrap:wrap}
.btn{padding:10px 20px;border:none;border-radius:8px;font-size:14px;cursor:pointer;font-weight:600}
.btn-r{background:var(--red);color:#fff}
.btn-g{background:#2e7d32;color:#fff}
.btn-b{background:#1565c0;color:#fff}
.btn-o{background:#e65100;color:#fff}
.btn-gy{background:var(--bg3);color:var(--text)}
.btn-sm{font-size:12px;padding:6px 12px}
.search-results{margin-top:16px}
.search-result-item{display:flex;gap:12px;padding:12px 0;border-bottom:1px solid var(--border)}
.search-result-item .sr-thumb{width:160px;height:90px;border-radius:8px;display:flex;align-items:center;justify-content:center;font-size:24px;flex-shrink:0;background:var(--bg3)}
.search-result-item .sr-info{flex:1}
.search-result-item .sr-info .sr-title{font-size:16px;font-weight:600}
.search-result-item .sr-info .sr-meta{font-size:13px;color:var(--text2);margin-top:4px}
.empty-state{padding:32px;text-align:center;color:var(--text2)}
</style>
</head>
<body onload="clickAwayProfileDropdown()">

<nav class="nav">
<div class="logo" onclick="showHome()"><svg viewBox="0 0 24 24" fill="var(--red)"><path d="M10 8l6 4-6 4V8z"/><path d="M3 3h18v18H3V3zm2 2v14h14V5H5z"/></svg><span class="r">My</span>Tube</div>
<div class="search-wrap"><input id="search-q" placeholder="Search" onkeydown="if(event.key==='Enter')doSearch()"><button onclick="doSearch()"><svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor"><path d="M15.5 14h-.79l-.28-.27A6.471 6.471 0 0016 9.5 6.5 6.5 0 109.5 16c1.61 0 3.09-.59 4.23-1.57l.27.28v.79l5 4.99L20.49 19l-4.99-5zm-6 0C7.01 14 5 11.99 5 9.5S7.01 5 9.5 5 14 7.01 14 9.5 11.99 14 9.5 14z"/></svg></button></div>
<div class="nav-right" id="nav-right">
<div id="nav-profile"></div>
<div class="profile-dropdown" id="profile-dropdown"></div>
</div>
</nav>

<div class="content">
<div class="sidebar">
<div class="section"><h3>Browse</h3><a class="tag" onclick="showHome()">🏠 Home</a><a class="tag" onclick="doTrending()">🔥 Trending</a></div>
<div class="section" id="history-section" style="display:none"><h3>Search History</h3><div id="history-list"></div></div>
</div>

<div class="main-col">
<div id="home-view">
<div class="section-title">Recommended <span class="badge" style="background:var(--bg3);color:var(--text2);font-weight:400">based on engagement</span></div>
<div class="video-grid" id="video-grid"></div>
</div>

<div id="search-view" style="display:none">
<div class="section-title">Search Results</div>
<div id="results-list"></div>
</div>
</div>
</div>

<!-- Modals -->
<div class="modal-overlay" id="modal-overlay" onclick="if(event.target==this)hideModal()">
<div class="modal-box" id="modal-box" onclick="event.stopPropagation()"></div>
</div>

<script>
let gProfile=null, gAllPosts=[], gAllProfiles=[], gSearchHistory=JSON.parse(localStorage.getItem('yt_history')||'[]');
function $(id){return document.getElementById(id)}
function hide(e){e.style.display='none'}
function show(e){e.style.display='block'}
function hideModal(){hide($('modal-overlay'))}
function showModal(h){$('modal-box').innerHTML=h;show($('modal-overlay'))}

const COLORS=['#e74c3c','#3498db','#2ecc71','#f39c12','#9b59b6','#1abc9c','#e67e22','#34495e'];

async function updateStatus(){
  try{
    let s=await(await fetch('/api/status')).json();
    gProfile=s.profile;
    let pl=await(await fetch('/api/profiles')).json();
    gAllProfiles=pl.profiles||[];
    console.log('updateStatus: profile=',gProfile,'profiles=',gAllProfiles.length);
    if(gProfile&&gProfile.peer_id){
      let c=(gProfile.display_name[0]||'?').toUpperCase();
      $('nav-profile').innerHTML='<button class="profile-btn" id="pf-btn" onclick="event.stopPropagation();toggleProfileDropdown()">'+c+'</button>';
    }else{
      $('nav-profile').innerHTML='<button class="profile-btn" id="pf-btn" onclick="event.stopPropagation();showAuth()">+</button>';
    }
    renderProfileDropdown();
  }catch(e){console.log('updateStatus error:',e)}
}
setInterval(updateStatus,5000);

function clickAwayProfileDropdown(){
  document.addEventListener('click',function(){let d=$('profile-dropdown');if(d)d.classList.remove('show')});
}

function toggleProfileDropdown(){
  let d=$('profile-dropdown');
  if(d.classList.contains('show')){d.classList.remove('show');return}
  renderProfileDropdown();
  show(d);
}

function renderProfileDropdown(){
  let d=$('profile-dropdown');
  if(!d)return;
  let html='';
  // Profile list
  if(gAllProfiles.length){
    for(let i=0;i<gAllProfiles.length;i++){
      let p=gAllProfiles[i];
      let activeCls=p.active?' pd-active':'';
      html+='<div class="pd-item'+activeCls+'" onclick="switchProfile('+i+')"><strong>'+(p.display_name||'?')+'</strong><div class="pd-small">@'+(p.username||'no username')+'</div></div>';
    }
    html+='<div class="pd-item" onclick="showAuth()" style="color:var(--red)">+ Add Profile</div>';
  }else{
    html+='<div class="pd-item" onclick="showAuth()">+ Create Profile</div>';
  }
  html+='<div class="pd-item" onclick="showProfileMenu()">⚙ Settings</div>';
  d.innerHTML=html;
}

function showHome(){
  hide($('search-view'));show($('home-view'));
  loadFeed();
}

function showAuth(){
  hide($('profile-dropdown'));
  showModal('<h2>Create Your Channel</h2><p style="color:var(--text2);margin-bottom:12px">A wallet will be created automatically to receive rewards.</p><input id="pf-dn" placeholder="Display name" autofocus><input id="pf-un" placeholder="Username (optional)"><div class="btn-row"><button class="btn btn-gy" onclick="hideModal()">Cancel</button><button class="btn btn-r" onclick="createProfile()">Create</button></div>');
  setTimeout(()=>{let e=$('pf-dn');if(e)e.focus()},100);
}

async function createProfile(){
  let dn=$('pf-dn').value.trim();
  if(!dn){alert('Display name is required');return}
  let un=$('pf-un').value.trim();
  let r=await(await fetch('/api/profile/create',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({display_name:dn,username:un})})).json();
  if(r.ok){
    hideModal();updateStatus();
    if(r.wallet&&r.mnemonic){showWalletInfo(r.wallet,r.mnemonic)}
    else if(r.wallet){showWalletInfo(r.wallet)}
  }
  else alert(r.error||'Failed');
}

function showWalletInfo(addr,mnemonic){
  let html='<h2>Wallet Created</h2><p>Address: <strong style="color:var(--red)">'+addr+'</strong></p><p style="color:var(--text2);margin-top:8px">This wallet is linked to your profile and will receive mining rewards.</p>';
  if(mnemonic&&mnemonic.length){
    html+='<h3 style="margin:16px 0 8px;font-size:14px;color:var(--red)">Back up your mnemonic (24 words):</h3><div style="background:var(--bg3);border-radius:8px;padding:12px;font-family:monospace;font-size:13px;line-height:1.6">';
    for(let i=0;i<mnemonic.length;i++){html+=(i<9?'&nbsp;':'')+(i+1)+'. '+mnemonic[i]+'<br>'}
    html+='</div><p style="color:var(--red);font-size:12px;margin-top:8px">Store these safely! They are the only way to restore your wallet.</p>';
  }
  html+='<div class="btn-row"><button class="btn btn-r" onclick="hideModal()">OK</button></div>';
  showModal(html);
}

async function switchProfile(idx){
  hide($('profile-dropdown'));
  let r=await(await fetch('/api/profile/switch',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index:idx})})).json();
  if(r.ok){updateStatus();loadFeed()}
  else alert(r.error||'Failed');
}

function showProfileMenu(){
  if(!gProfile||!gProfile.peer_id)return;
  let html='<h2>'+gProfile.display_name+'</h2>'+(gProfile.username?'<p style="color:var(--text2)">@'+gProfile.username+'</p>':'')+'<p style="color:var(--text2);margin-top:8px">Peer: '+gProfile.peer_id+'</p>';
  if(gProfile.wallet)html+='<p style="margin-top:8px;font-size:13px">Wallet: <span style="color:var(--red)">'+gProfile.wallet+'</span></p>';
  html+='<div class="btn-row"><button class="btn btn-g" onclick="showNewPost()">New Post</button><button class="btn btn-gy" onclick="hideModal()">Close</button></div>';
  showModal(html);
}

function showNewPost(){
  hideModal();
  showModal('<h2>New Post</h2><textarea id="post-content" rows="4" placeholder="Share your thoughts..." style="resize:none"></textarea><div class="btn-row"><button class="btn btn-gy" onclick="hideModal()">Cancel</button><button class="btn btn-r" onclick="createPost()">Post</button></div>');
  setTimeout(()=>{let e=$('post-content');if(e)e.focus()},100);
}

async function createPost(){
  let ct=$('post-content').value.trim();
  if(!ct){alert('Content is required');return}
  let r=await(await fetch('/api/post/create',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({content:ct})})).json();
  if(r.ok){hideModal();loadFeed()}
  else alert(r.error||'Failed');
}

async function followProfile(peer){
  let r=await(await fetch('/api/social/follow',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({peer:peer})})).json();
  if(r.ok)loadFeed();
  else alert('Follow failed');
}

async function unfollowProfile(peer){
  let r=await(await fetch('/api/social/unfollow',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({peer:peer})})).json();
  if(r.ok)loadFeed();
  else alert('Unfollow failed');
}

async function blockProfile(peer){
  if(!confirm('Block this author?'))return;
  let r=await(await fetch('/api/social/block',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({peer:peer})})).json();
  if(r.ok)loadFeed();
  else alert('Block failed');
}

async function unblockProfile(peer){
  let r=await(await fetch('/api/social/unblock',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({peer:peer})})).json();
  if(r.ok)loadFeed();
  else alert('Unblock failed');
}

function makeVideoCard(p,idx){
  let color=COLORS[idx%%COLORS.length];
  let d=p.timestamp?new Date(p.timestamp):new Date();
  let timeStr=d.toLocaleDateString();
  let preview=(p.content||'').substring(0,60)+(p.content&&p.content.length>60?'...':'');
  return '<div class="video-card"><div class="thumb" style="background:'+color+'" onclick="alert(\''+p.id+'\')"><span>🎬</span><span class="dur">0:'+(Math.floor(Math.random()*5)+1)+'0</span></div><div class="video-info"><div class="title" onclick="alert(\''+p.id+'\')">'+(p.content||'Untitled').substring(0,80)+'</div><div class="meta">'+(p.author||'Anonymous')+' &bull; '+timeStr+' &bull; '+(Math.floor(Math.random()*9000)+1000)+' views</div><div class="actions">'+(p.author&&gProfile&&p.author!==gProfile.peer_id?'<button class="btn btn-sm btn-b" onclick="event.stopPropagation();followProfile(\''+p.author+'\')">Follow</button><button class="btn btn-sm btn-gy" onclick="event.stopPropagation();unfollowProfile(\''+p.author+'\')">Unfollow</button><button class="btn btn-sm btn-r" onclick="event.stopPropagation();blockProfile(\''+p.author+'\')">Block</button>':'')+'</div></div></div>';
}

function makeResultItem(p,idx){
  let color=COLORS[idx%%COLORS.length];
  let preview=(p.content||p.description||'').substring(0,120)+(p.content&&p.content.length>120?'...':'');
  return '<div class="search-result-item"><div class="sr-thumb" style="background:'+color+'">🎬</div><div class="sr-info"><div class="sr-title">'+(p.content||p.author||'Untitled').substring(0,80)+'</div><div class="sr-meta">'+(p.author||p.type||'')+'</div><div class="sr-meta">'+preview+'</div></div></div>';
}

async function loadFeed(){
  try{
    let r=await(await fetch('/api/posts/feed')).json();
    gAllPosts=r.posts||[];
    if(gAllPosts.length){
      let html='';
      let sorted=[...gAllPosts].sort((a,b)=>((b.views||0)-(a.views||0)));
      for(let i=0;i<sorted.length;i++)html+=makeVideoCard(sorted[i],i);
      $('video-grid').innerHTML=html;
    }else{
      $('video-grid').innerHTML='<div class="empty-state"><p style="font-size:48px;margin-bottom:12px">📺</p><p>No content yet</p><p style="font-size:13px;margin-top:4px">Be the first to post!</p></div>';
    }
  }catch(e){$('video-grid').innerHTML='<div class="empty-state"><p>Could not load feed</p></div>'}
}

async function doSearch(){
  let q=$('search-q').value.trim();
  if(!q)return;
  if(!gSearchHistory.includes(q)){gSearchHistory.unshift(q);if(gSearchHistory.length>10)gSearchHistory.pop();localStorage.setItem('yt_history',JSON.stringify(gSearchHistory))}
  renderHistory();
  hide($('home-view'));show($('search-view'));
  $('results-list').innerHTML='<p style="color:var(--text2);padding:16px">Searching...</p>';
  try{
    let r=await(await fetch('/api/search?q='+encodeURIComponent(q))).json();
    if(r.results&&r.results.length){
      let html='';
      for(let i=0;i<r.results.length;i++)html+=makeResultItem(r.results[i],i);
      $('results-list').innerHTML=html;
    }else $('results-list').innerHTML='<div class="empty-state"><p>No results for "'+q+'"</p></div>';
  }catch(e){$('results-list').innerHTML='<div class="empty-state"><p>Search failed</p></div>'}
}

function doTrending(){
  $('search-q').value='trending';doSearch();
}

function renderHistory(){
  let el=$('history-list');
  if(!gSearchHistory.length){hide($('history-section'));return}
  show($('history-section'));
  el.innerHTML=gSearchHistory.map(q=>'<a class="tag" onclick="document.getElementById(\'search-q\').value=\''+q+'\';doSearch()">'+q+'</a>').join('');
}

updateStatus();
loadFeed();
renderHistory();
</script>
</body>
</html>)HTML",
        peer_count,
        height,
        (unsigned long long)epoch,
        (unsigned long long)reward);
    if (len > 0 && (size_t)len < sizeof(html))
        send_response(s, 200, "OK", "text/html; charset=utf-8", html, (size_t)len);
    else
        send_response(s, 500, "Internal Server Error", "text/plain", "HTML too large", 15);
}

static inline void serve_dashboard(sock_t s) {
    // Build wallet address string
    char wallet_addr[64] = "None";
    if (g_state.wallet.public_key != std::array<uint8_t, 32>{}) {
        auto addr = base64_encode(g_state.wallet.public_key.data(), 16);
        snprintf(wallet_addr, sizeof(wallet_addr), "MYT%s", addr.c_str());
    }

    // Build peer ID string
    const char* peer_id = g_state.node ? g_state.node->local_peer_id().c_str() : "---";
    size_t peer_count = g_state.node ? g_state.node->peer_count() : 0;
    size_t mempool_count = g_state.node ? g_state.node->mempool_size() : 0;
    size_t height = g_state.node ? g_state.node->chain.size() : 0;

    uint64_t epoch = g_state.tokenomics.current_epoch;
    uint64_t reward = mining_block_reward(epoch);
    uint32_t diff = mining_difficulty_bits(epoch);

    char html[16384];
    int len = snprintf(html, sizeof(html), R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>MyCelium Dashboard</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
:root{--red:#FF0000;--bg:#0f0f0f;--bg2:#1a1a1a;--bg3:#222;--text:#fff;--text2:#aaa;--border:#333}
body{font-family:'Segoe UI',Arial,sans-serif;background:var(--bg);color:var(--text);min-height:100vh}
.nav{position:fixed;top:0;left:0;right:0;height:56px;background:var(--bg2);border-bottom:1px solid var(--border);display:flex;align-items:center;padding:0 24px;z-index:100}
.nav .logo{font-size:20px;font-weight:700;color:var(--text)}
.nav .logo .r{color:var(--red)}
.nav .tabs{display:flex;gap:0;margin-left:40px}
.nav .tabs a{padding:16px 20px;color:var(--text2);text-decoration:none;font-size:14px;border-bottom:2px solid transparent;cursor:pointer}
.nav .tabs a:hover{color:var(--text)}
.nav .tabs a.on{color:var(--text);border-bottom-color:var(--red)}
.main{margin-top:56px;padding:24px;max-width:1100px;margin-left:auto;margin-right:auto}
.card{background:var(--bg2);border-radius:12px;padding:20px;margin-bottom:16px;border:1px solid var(--border)}
.card h2{font-size:16px;margin-bottom:12px;color:var(--text2);text-transform:uppercase;letter-spacing:.5px}
.row{display:flex;gap:16px;flex-wrap:wrap}
.col{flex:1;min-width:200px}
.stat{padding:12px;background:var(--bg3);border-radius:8px;text-align:center}
.stat .v{font-size:24px;font-weight:700;color:var(--red)}
.stat .l{font-size:12px;color:var(--text2);margin-top:4px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:12px}
.btn{padding:10px 20px;border:none;border-radius:8px;font-size:14px;cursor:pointer;transition:opacity .15s;display:inline-block;margin:4px}
.btn:hover{opacity:.8}
.btn-r{background:var(--red);color:#fff}
.btn-g{background:#2e7d32;color:#fff}
.btn-b{background:#1565c0;color:#fff}
.btn-gy{background:var(--bg3);color:var(--text)}
input,textarea{padding:10px 14px;background:var(--bg3);border:1px solid var(--border);border-radius:8px;color:var(--text);font-size:14px;width:100%%;outline:none;margin:4px 0}
input:focus{border-color:var(--red)}
label{font-size:13px;color:var(--text2);display:block;margin-top:8px}
#log{background:var(--bg3);border-radius:8px;padding:12px;font-family:monospace;font-size:13px;height:300px;overflow-y:auto;white-space:pre-wrap;word-break:break-all;line-height:1.5}
#log .ts{color:var(--text2);margin-right:8px}
#wallet-section,#no-wallet{display:none}
.footer{text-align:center;padding:24px;color:var(--text2);font-size:13px}
</style>
</head>
<body>
<nav class="nav">
<div class="logo"><span class="r">My</span>Celium</div>
<div class="tabs"><a class="on" onclick="showTab('mytube')">MyTube</a><a onclick="showTab('node')">Node</a></div>
</nav>
<div class="main">

<div id="tab-mytube">
<div class="card">
<div class="row">
<div class="col">
<div class="stat"><div class="v" id="v-balance">%llu</div><div class="l">MYTUBE Balance</div></div>
</div>
<div class="col">
<div class="stat"><div class="v" id="v-staked">%llu</div><div class="l">Staked</div></div>
</div>
<div class="col">
<div class="stat"><div class="v" id="v-epoch">%llu</div><div class="l">Epoch</div></div>
</div>
<div class="col">
<div class="stat"><div class="v" id="v-reward">%llu</div><div class="l">Block Reward</div></div>
</div>
</div>
</div>

<div class="card">
<h2>MYTUBE Tokenomics</h2>
<div class="grid">
<div class="stat"><div class="v">%llu</div><div class="l">Total Supply</div></div>
<div class="stat"><div class="v">%llu</div><div class="l">Minted</div></div>
<div class="stat"><div class="v">%u</div><div class="l">Difficulty (bits)</div></div>
<div class="stat"><div class="v" id="v-peers">%zu</div><div class="l">Peers</div></div>
<div class="stat"><div class="v" id="v-height">%zu</div><div class="l">Chain Height</div></div>
<div class="stat"><div class="v" id="v-mempool">%zu</div><div class="l">Mempool Txs</div></div>
</div>
</div>

<div class="card">
<h2>Wallet</h2>
<div id="no-wallet" style="display:%s">
<p style="color:var(--text2);margin-bottom:12px">No wallet found. Create or restore one.</p>
<button class="btn btn-r" onclick="showCreateWallet()">Create Wallet</button>
<button class="btn btn-b" onclick="showRestoreWallet()">Restore Wallet</button>
</div>
<div id="wallet-section" style="display:%s">
<p><strong>Address:</strong> <span id="w-addr">%s</span></p>
<p><strong>Balance:</strong> <span id="w-bal">%llu</span> MYTUBE</p>
<p><strong>Available:</strong> <span id="w-avail">%llu</span> MYTUBE</p>
<p><strong>Staked:</strong> <span id="w-staked">%llu</span> MYTUBE</p>
<div style="margin-top:12px">
<button class="btn btn-r" onclick="showMineDialog()">Mine Block</button>
<button class="btn btn-g" onclick="showSendDialog()">Send Tokens</button>
</div>
</div>
</div>

<div class="card">
<h2>Profiles</h2>
<div id="profiles-section">
<p style="color:var(--text2);margin-bottom:8px" id="profile-status">No profiles yet.</p>
<div id="profile-list"></div>
<div style="margin-top:8px">
<button class="btn btn-r btn-sm" onclick="showCreateProfile()">+ Create Profile</button>
<button class="btn btn-b btn-sm" onclick="showWalletLink()">Link Wallet</button>
<button class="btn btn-gy btn-sm" onclick="doWalletUnlink()">Unlink Wallet</button>
</div>
</div>
</div>

<div id="dialog-overlay" style="display:none;position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,.7);z-index:200" onclick="hideDialog(event)">
<div id="dialog-box" style="background:var(--bg2);border:1px solid var(--border);border-radius:12px;padding:24px;max-width:500px;margin:80px auto;position:relative" onclick="event.stopPropagation()">
<div id="dialog-content"></div>
</div>
</div>
</div>

<div id="tab-node" style="display:none">
<div class="card">
<h2>Node Control</h2>
<p style="margin-bottom:12px"><strong>Status:</strong> <span id="node-status">%s</span></p>
<p><strong>Peer ID:</strong> <span id="node-peer">%s</span></p>
<div style="margin-top:12px">
<button id="btn-start-node" class="btn btn-g" onclick="fetch('/api/node/start',{method:'POST'}).then(r=>r.json()).then(d=>{if(d.ok)updateStatus()})">Start Node</button>
<button id="btn-stop-node" class="btn btn-r" onclick="fetch('/api/node/stop',{method:'POST'}).then(r=>r.json()).then(d=>{if(d.ok)updateStatus()})">Stop Node</button>
</div>
</div>

<div class="card">
<h2>Live Log</h2>
<div id="log"></div>
</div>
</div>

<div class="footer">MyCelium &mdash; MIT License &mdash; Built with C++17</div>
</div>

<script>
let logSince=0;
function $(id){return document.getElementById(id)}
function hide(e){e.style.display='none'}
function show(e){e.style.display='block'}
function showTab(t){document.querySelectorAll('.tabs a').forEach(a=>a.classList.remove('on'));event.target.classList.add('on');hide($('tab-mytube'));hide($('tab-node'));show($('tab-'+t))}
async function fetchJSON(u){let r=await fetch(u);return r.json()}
async function postJSON(u,d){let r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)});return r.json()}

async function updateStatus(){
  let s=await fetchJSON('/api/status');
  // Node
  let r=s.node.running;
  $('node-status').textContent=r?'Online':'Offline';
  $('btn-start-node').style.display=r?'none':'inline-block';
  $('btn-stop-node').style.display=r?'inline-block':'none';
  $('node-peer').textContent=s.node.peer_id||'---';
  $('v-peers').textContent=s.node.peers;
  $('v-height').textContent=s.node.height;
  $('v-mempool').textContent=s.node.mempool;
  // Wallet
  if(s.wallet.address){
    hide($('no-wallet'));show($('wallet-section'));
    $('w-addr').textContent=s.wallet.address;
    $('w-bal').textContent=s.wallet.balance;
    $('w-avail').textContent=s.wallet.available;
    $('w-staked').textContent=s.wallet.staked;
    $('v-balance').textContent=s.wallet.balance;
    $('v-staked').textContent=s.wallet.staked;
  }else{
    show($('no-wallet'));hide($('wallet-section'));
  }
  // Tokenomics
  $('v-epoch').textContent=s.tokenomics.epoch;
  $('v-reward').textContent=s.tokenomics.reward;
  // Profiles
  updateProfiles();
}
setInterval(updateStatus,2000);

async function updateProfiles(){
  try{
    let pl=await fetchJSON('/api/profiles');
    let el=$('profile-list');
    let st=$('profile-status');
    if(pl.profiles&&pl.profiles.length){
      st.textContent=pl.profiles.length+' profile(s):';
      let html='';
      for(let i=0;i<pl.profiles.length;i++){
        let p=pl.profiles[i];
        html+='<div style="padding:8px;background:var(--bg3);border-radius:8px;margin:4px 0;display:flex;align-items:center;justify-content:space-between">';
        html+='<div><strong>'+(p.display_name||'?')+'</strong> @'+(p.username||'none')+'</div>';
        html+='<div>'+(p.active?'<span style="color:var(--red)">Active</span>':'<button class="btn btn-sm btn-gy" onclick="switchProfileM('+i+')">Switch</button>')+'</div>';
        html+='</div>';
      }
      el.innerHTML=html;
    }else{
      st.textContent='No profiles yet.';
      el.innerHTML='';
    }
  }catch(e){}
}

async function switchProfileM(idx){
  let r=await postJSON('/api/profile/switch',{index:idx});
  if(r.ok)updateProfiles();
  else alert(r.error||'Switch failed');
}

function showCreateProfile(){
  $('dialog-content').innerHTML='<h2 style="margin-bottom:16px">Create Profile</h2><label>Display Name</label><input id="mpf-dn"><label>Username (optional)</label><input id="mpf-un"><div style="margin-top:16px;display:flex;gap:8px"><button class="btn btn-r" onclick="doCreateProfile()">Create</button><button class="btn btn-gy" onclick="hide($(\'dialog-overlay\'))">Cancel</button></div>';
  show($('dialog-overlay'));
}

async function doCreateProfile(){
  let dn=$('mpf-dn').value.trim();
  if(!dn){alert('Display name required');return}
  let r=await postJSON('/api/profile/create',{display_name:dn,username:$('mpf-un').value.trim()});
  if(r.ok){
    hide($('dialog-overlay'));
    if(r.mnemonic){
      let msg='<h2 style="margin-bottom:16px">Wallet Created</h2><p>Address: <strong>'+r.wallet+'</strong></p><h3 style="margin:16px 0 8px;color:var(--red)">Back up your mnemonic:</h3><div style="background:var(--bg3);border-radius:8px;padding:12px;font-family:monospace;font-size:14px;line-height:1.6">';
      for(let i=0;i<r.mnemonic.length;i++){msg+=(i<9?' ':'')+(i+1)+'. '+r.mnemonic[i]+'<br>'}
      msg+='</div><p style="color:var(--red);margin-top:12px">Store these words safely!</p><button class="btn btn-r" onclick="hide($(\'dialog-overlay\'))">I\'ve saved these words</button>';
      $('dialog-content').innerHTML=msg;
    }
    updateProfiles();
  }
  else alert(r.error||'Failed');
}

function showWalletLink(){
  $('dialog-content').innerHTML='<h2 style="margin-bottom:16px">Link Wallet to Profile</h2><p style="color:var(--text2);margin-bottom:12px">Links the current wallet to the active profile. The profile will be able to sign posts and receive rewards.</p><div style="display:flex;gap:8px"><button class="btn btn-b" onclick="doWalletLink()">Link Wallet</button><button class="btn btn-gy" onclick="hide($(\'dialog-overlay\'))">Cancel</button></div>';
  show($('dialog-overlay'));
}

async function doWalletLink(){
  let r=await postJSON('/api/wallet/link',{});
  if(r.ok){
    $('dialog-content').innerHTML='<h2 style="margin-bottom:16px">Wallet Linked</h2><p>Profile <strong>'+r.profile+'</strong> linked to wallet <strong>'+r.wallet+'</strong></p><button class="btn btn-r" onclick="hide($(\'dialog-overlay\'))">OK</button>';
  }else alert(r.error||'No wallet or profile');
}

async function doWalletUnlink(){
  if(!confirm('Unlink wallet from current profile?'))return;
  let r=await postJSON('/api/wallet/unlink',{});
  if(r.ok)updateProfiles();
  else alert(r.error||'No profile');
}

async function updateLog(){
  let l=await fetchJSON('/api/logs?since='+logSince);
  if(l.entries&&l.entries.length){
    let el=$('log');
    for(let e of l.entries){let d=document.createElement('div');d.textContent=e;el.appendChild(d)}
    el.scrollTop=el.scrollHeight;
    logSince=l.total;
  }
}
setInterval(updateLog,1000);

function showCreateWallet(){
  $('dialog-content').innerHTML='<h2 style="margin-bottom:16px">Create Wallet</h2><label>Passphrase (>=8 chars, 1 digit, 1 special)</label><input type="password" id="pw1"><label>Confirm</label><input type="password" id="pw2"><div style="margin-top:16px;display:flex;gap:8px"><button class="btn btn-r" onclick="doCreateWallet()">Create</button><button class="btn btn-gy" onclick="hide($(\'dialog-overlay\'))">Cancel</button></div>';
  show($('dialog-overlay'));
}
async function doCreateWallet(){
  let pw1=$('pw1').value,pw2=$('pw2').value;
  if(pw1!==pw2){alert('Passphrases do not match');return}
  let r=await postJSON('/api/wallet/create',{passphrase:pw1});
  if(r.ok){
    let msg='<h2 style="margin-bottom:16px">Wallet Created</h2><p>Address: <strong>'+r.address+'</strong></p><h3 style="margin:16px 0 8px">Mnemonic (24 words):</h3><div style="background:var(--bg3);border-radius:8px;padding:12px;font-family:monospace;font-size:14px;line-height:1.6">';
    for(let i=0;i<r.mnemonic.length;i++){msg+=(i<9?' ':'')+(i+1)+'. '+r.mnemonic[i]+'<br>'}
    msg+='</div><p style="color:var(--red);margin-top:12px">Store these words safely!</p><button class="btn btn-r" onclick="hide($(\'dialog-overlay\'))">I\'ve saved these words</button>';
    $('dialog-content').innerHTML=msg;
  }else alert(r.error||'Failed');
}
function showRestoreWallet(){
  $('dialog-content').innerHTML='<h2 style="margin-bottom:16px">Restore Wallet</h2><label>24 mnemonic words (space-separated)</label><textarea id="rwords" rows="3" placeholder="word1 word2 ... word24" style="resize:none"></textarea><label>Passphrase</label><input type="password" id="rpw"><div style="margin-top:16px;display:flex;gap:8px"><button class="btn btn-b" onclick="doRestoreWallet()">Restore</button><button class="btn btn-gy" onclick="hide($(\'dialog-overlay\'))">Cancel</button></div>';
  show($('dialog-overlay'));
}
async function doRestoreWallet(){
  let r=await postJSON('/api/wallet/restore',{words:$('rwords').value,passphrase:$('rpw').value});
  if(r.ok){
    $('dialog-content').innerHTML='<h2 style="margin-bottom:16px">Wallet Restored</h2><p>Address: <strong>'+r.address+'</strong></p><button class="btn btn-r" onclick="hide($(\'dialog-overlay\'))">OK</button>';
  }else alert(r.error||'Failed');
}
function showMineDialog(){
  $('dialog-content').innerHTML='<h2 style="margin-bottom:16px">Mining Block</h2><p style="color:var(--text2)">Searching for a valid block... (this may take a moment)</p><div id="mine-result"></div>';
  show($('dialog-overlay'));
  fetch('/api/mine',{method:'POST'}).then(r=>r.json()).then(d=>{
    if(d.ok){$('mine-result').innerHTML='<p style="color:#4caf50;margin-top:12px">Block mined! Height: '+d.block_height+', Reward: '+d.reward+' MYTUBE</p>'}
    else{$('mine-result').innerHTML='<p style="color:var(--red);margin-top:12px">'+d.error+'</p>'}
    setTimeout(()=>hide($('dialog-overlay')),2000);
  });
}
function showSendDialog(){
  $('dialog-content').innerHTML='<h2 style="margin-bottom:16px">Send Tokens</h2><label>Destination Address (MYT...)</label><input id="s-to" placeholder="MYT..."><label>Amount (MYTUBE)</label><input type="number" id="s-amt"><div style="margin-top:16px;display:flex;gap:8px"><button class="btn btn-g" onclick="doSend()">Send</button><button class="btn btn-gy" onclick="hide($(\'dialog-overlay\'))">Cancel</button></div>';
  show($('dialog-overlay'));
}
async function doSend(){
  let r=await postJSON('/api/token/send',{to:$('s-to').value,amount:parseInt($('s-amt').value)});
  if(r.ok){$('dialog-content').innerHTML='<h2 style="margin-bottom:16px">Sent!</h2><p>TX: '+r.tx_id+'</p><p>Balance: '+r.balance+' MYTUBE</p><button class="btn btn-r" onclick="hide($(\'dialog-overlay\'))">OK</button>'}
  else alert(r.error||'Failed');
}
function hideDialog(e){if(e.target.id==='dialog-overlay')hide($('dialog-overlay'))}

updateStatus();
updateLog();
</script>
</body>
</html>)HTML",
        (unsigned long long)g_state.wallet.balance,
        (unsigned long long)g_state.wallet.staked_balance,
        (unsigned long long)epoch,
        (unsigned long long)reward,
        (unsigned long long)g_state.tokenomics.total_supply,
        (unsigned long long)g_state.tokenomics.minted_supply,
        diff,
        peer_count,
        height,
        mempool_count,
        g_state.wallet.public_key == std::array<uint8_t, 32>{} ? "block" : "none",
        g_state.wallet.public_key == std::array<uint8_t, 32>{} ? "none" : "block",
        wallet_addr,
        (unsigned long long)g_state.wallet.balance,
        (unsigned long long)g_state.wallet.available(),
        (unsigned long long)g_state.wallet.staked_balance,
        g_state.node_running ? "Online" : "Offline",
        peer_id);
    if (len > 0 && (size_t)len < sizeof(html))
        send_response(s, 200, "OK", "text/html; charset=utf-8", html, (size_t)len);
    else
        send_response(s, 500, "Internal Server Error", "text/plain", "HTML too large", 15);
}

// ============================================================
// Router
// ============================================================
static inline void serve_http_page_ex(uintptr_t client_fd, bool is_mgmt) {
    sock_t s;
#ifdef _WIN32
    s = (SOCKET)client_fd;
#else
    s = (int)client_fd;
#endif

    char buf[8192];
    int n = sck_recv(s, buf, (int)sizeof(buf) - 1);
    if (n <= 0) { sck_close(s); return; }
    buf[n] = 0;

    auto req = parse_http_request(buf, (size_t)n);

    // CORS preflight
    if (req.method == "OPTIONS") {
        const char* h = "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "Connection: close\r\n\r\n";
        send_all(s, h, strlen(h));
        sck_close(s);
        return;
    }

    // Routing
    auto path = req.path;

    if (path == "/" || path == "/mytube" || path == "/index.html") {
        if (is_mgmt) serve_dashboard(s);
        else serve_client_dashboard(s);
    }
    else if (path == "/api/status") {
        handle_api_status(s);
    }
    else if (path.substr(0, 9) == "/api/logs") {
        if (!is_mgmt) { send_response(s, 404, "Not Found", "text/plain", "Not Found", 9); return; }
        handle_api_logs(s, path);
    }
    else if (path == "/api/mempool") {
        if (!is_mgmt) { send_response(s, 404, "Not Found", "text/plain", "Not Found", 9); return; }
        handle_api_mempool(s);
    }
    else if (path == "/api/posts/feed") {
        handle_api_feed(s);
    }
    else if (path.substr(0, 11) == "/api/search") {
        handle_api_search(s, path);
    }
    else if (path == "/api/profile/create" && req.method == "POST") {
        handle_api_profile_create(s, req.body);
    }
    else if (path == "/api/profiles") {
        handle_api_profiles(s);
    }
    else if (path == "/api/profile/switch" && req.method == "POST") {
        handle_api_profile_switch(s, req.body);
    }
    else if (path == "/api/social/follow" && req.method == "POST") {
        handle_api_social_follow(s, req.body);
    }
    else if (path == "/api/social/unfollow" && req.method == "POST") {
        handle_api_social_unfollow(s, req.body);
    }
    else if (path == "/api/social/block" && req.method == "POST") {
        handle_api_social_block(s, req.body);
    }
    else if (path == "/api/social/unblock" && req.method == "POST") {
        handle_api_social_unblock(s, req.body);
    }
    else if (path == "/api/ping") {
        send_json(s, 200, "{\"pong\":true,\"time\":" + std::to_string(ProtocolMessage{}.now_sec()) + "}");
    }
    else if (path.substr(0, 20) == "/api/social/relationship") {
        handle_api_social_relationship(s, path);
    }
    else if (path == "/api/post/create" && req.method == "POST") {
        handle_api_post_create(s, req.body);
    }
    else if (is_mgmt && path == "/api/wallets") {
        handle_api_wallets(s);
    }
    else if (is_mgmt && path == "/api/wallet/link" && req.method == "POST") {
        handle_api_wallet_link(s, req.body);
    }
    else if (is_mgmt && path == "/api/wallet/unlink" && req.method == "POST") {
        handle_api_wallet_unlink(s, req.body);
    }
    else if (is_mgmt && path == "/api/wallet/create" && req.method == "POST") {
        handle_api_wallet_create(s, req.body);
    }
    else if (is_mgmt && path == "/api/wallet/restore" && req.method == "POST") {
        handle_api_wallet_restore(s, req.body);
    }
    else if (is_mgmt && path == "/api/node/start" && req.method == "POST") {
        handle_api_node_start(s);
    }
    else if (is_mgmt && path == "/api/node/stop" && req.method == "POST") {
        handle_api_node_stop(s);
    }
    else if (is_mgmt && path == "/api/mine" && req.method == "POST") {
        handle_api_mine(s);
    }
    else if (is_mgmt && path == "/api/token/send" && req.method == "POST") {
        handle_api_token_send(s, req.body);
    }
    else {
        send_response(s, 404, "Not Found", "text/plain", "Not Found", 9);
    }
}

static inline void serve_http_page(uintptr_t client_fd) {
    serve_http_page_ex(client_fd, false);
}
