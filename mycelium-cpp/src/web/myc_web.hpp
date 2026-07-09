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

// ============================================================
// Dashboard HTML
// ============================================================
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
input,textarea{padding:10px 14px;background:var(--bg3);border:1px solid var(--border);border-radius:8px;color:var(--text);font-size:14px;width:100%;outline:none;margin:4px 0}
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
<button class="btn btn-g" onclick="fetch('/api/node/start',{method:'POST'}).then(r=>r.json()).then(d=>{if(d.ok)updateStatus()})">Start Node</button>
<button class="btn btn-r" onclick="fetch('/api/node/stop',{method:'POST'}).then(r=>r.json()).then(d=>{if(d.ok)updateStatus()})">Stop Node</button>
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
  $('node-status').textContent=s.node.running?'Online':'Offline';
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
}
setInterval(updateStatus,2000);

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
static inline void serve_http_page(uintptr_t client_fd) {
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
        serve_dashboard(s);
    }
    else if (path == "/api/status") {
        handle_api_status(s);
    }
    else if (path.substr(0, 10) == "/api/logs") {
        handle_api_logs(s, path);
    }
    else if (path == "/api/wallet/create" && req.method == "POST") {
        handle_api_wallet_create(s, req.body);
    }
    else if (path == "/api/wallet/restore" && req.method == "POST") {
        handle_api_wallet_restore(s, req.body);
    }
    else if (path == "/api/node/start" && req.method == "POST") {
        handle_api_node_start(s);
    }
    else if (path == "/api/node/stop" && req.method == "POST") {
        handle_api_node_stop(s);
    }
    else if (path == "/api/mine" && req.method == "POST") {
        handle_api_mine(s);
    }
    else if (path == "/api/token/send" && req.method == "POST") {
        handle_api_token_send(s, req.body);
    }
    else if (path == "/api/mempool") {
        handle_api_mempool(s);
    }
    else {
        send_response(s, 404, "Not Found", "text/plain", "Not Found", 9);
    }
}
