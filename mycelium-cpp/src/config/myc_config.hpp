#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include "p2p/myc_p2p.hpp"

// ============================================================
// MyceliumConfig — unified configuration for all entry points
// ============================================================
struct MyceliumConfig {
    // Network
    std::string listen_addr = "/ip4/0.0.0.0/tcp/0";
    std::vector<std::string> bootstrap_nodes;
    bool enable_mdns = true;
    bool enable_relay = true;

    // Tor
    bool enable_tor = false;
    uint16_t tor_socks_port = 9050;
    uint16_t tor_control_port = 9051;
    std::string onion_service_dir;

    // HTTP / Web UI
    bool enable_http = false;
    uint16_t http_port = 8080;

    // Storage
    std::string data_dir = "./data";
    std::string posts_dir;
    std::string chain_dir;
    std::string profiles_dir;
    std::string logs_dir;
    uint64_t default_post_ttl_seconds = 86400;

    // Mining
    uint64_t mining_max_nonce = 1000000;

    // Keypair seed (optional — if empty, random)
    std::string keypair_seed_base64;

    // ============================================================
    // Populate P2pConfig from this config
    // ============================================================
    P2pConfig to_p2p_config() const {
        P2pConfig p2p;
        p2p.keypair_seed_base64 = keypair_seed_base64;
        p2p.listen_addresses.push_back(listen_addr);
        p2p.capabilities.push_back({kCapFull});
        p2p.bootstrap_nodes = bootstrap_nodes;
        p2p.enable_mdns = enable_mdns;
        p2p.enable_relay = enable_relay;
        p2p.enable_tor = enable_tor;
        p2p.tor_socks_port = tor_socks_port;
        p2p.tor_control_port = tor_control_port;
        p2p.onion_service_dir = onion_service_dir;
        p2p.storage.data_dir = data_dir;
        p2p.storage.posts_dir = posts_dir.empty() ? data_dir + "/posts" : posts_dir;
        p2p.storage.chain_dir = chain_dir.empty() ? data_dir + "/chain" : chain_dir;
        p2p.storage.profiles_dir = profiles_dir.empty() ? data_dir + "/profiles" : profiles_dir;
        p2p.storage.logs_dir = logs_dir.empty() ? data_dir + "/logs" : logs_dir;
        return p2p;
    }
};

// ============================================================
// Minimal JSON parser (config-file subset)
// ============================================================
namespace config_json {

static inline void skip_ws(const char*& p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
}

static inline std::string parse_string(const char*& p) {
    if (*p != '"') return "";
    ++p; // skip opening quote
    std::string s;
    while (*p && *p != '"') {
        if (*p == '\\' && *(p+1)) {
            ++p;
            switch (*p) {
                case '"': s += '"'; break;
                case '\\': s += '\\'; break;
                case 'n': s += '\n'; break;
                case 'r': s += '\r'; break;
                case 't': s += '\t'; break;
                default: s += *p; break;
            }
        } else {
            s += *p;
        }
        ++p;
    }
    if (*p == '"') ++p; // skip closing quote
    return s;
}

static inline uint64_t parse_number(const char*& p) {
    char* end = nullptr;
    uint64_t v = strtoull(p, &end, 10);
    if (end) p = end;
    return v;
}

static inline bool parse_bool(const char*& p) {
    if (strncmp(p, "true", 4) == 0) { p += 4; return true; }
    if (strncmp(p, "false", 5) == 0) { p += 5; return false; }
    return false;
}

static inline std::vector<std::string> parse_string_array(const char*& p) {
    std::vector<std::string> arr;
    if (*p != '[') return arr;
    ++p; // skip '['
    skip_ws(p);
    if (*p == ']') { ++p; return arr; }
    while (*p) {
        skip_ws(p);
        if (*p == ']') { ++p; break; }
        if (*p == ',') { ++p; continue; }
        arr.push_back(parse_string(p));
        skip_ws(p);
        if (*p == ',') ++p;
    }
    return arr;
}

} // namespace config_json

// ============================================================
// Config file I/O
// ============================================================
static inline std::string config_string(const MyceliumConfig& c) {
    std::string j;
    j += "{\n";
    j += "  \"listen_addr\": \"" + c.listen_addr + "\",\n";
    j += "  \"bootstrap_nodes\": [";
    for (size_t i = 0; i < c.bootstrap_nodes.size(); ++i) {
        if (i > 0) j += ", ";
        j += "\"" + c.bootstrap_nodes[i] + "\"";
    }
    j += "],\n";
    j += "  \"enable_mdns\": " + std::string(c.enable_mdns ? "true" : "false") + ",\n";
    j += "  \"enable_relay\": " + std::string(c.enable_relay ? "true" : "false") + ",\n";
    j += "  \"enable_tor\": " + std::string(c.enable_tor ? "true" : "false") + ",\n";
    j += "  \"tor_socks_port\": " + std::to_string(c.tor_socks_port) + ",\n";
    j += "  \"tor_control_port\": " + std::to_string(c.tor_control_port) + ",\n";
    j += "  \"onion_service_dir\": \"" + c.onion_service_dir + "\",\n";
    j += "  \"enable_http\": " + std::string(c.enable_http ? "true" : "false") + ",\n";
    j += "  \"http_port\": " + std::to_string(c.http_port) + ",\n";
    j += "  \"data_dir\": \"" + c.data_dir + "\",\n";
    j += "  \"posts_dir\": \"" + c.posts_dir + "\",\n";
    j += "  \"chain_dir\": \"" + c.chain_dir + "\",\n";
    j += "  \"profiles_dir\": \"" + c.profiles_dir + "\",\n";
    j += "  \"logs_dir\": \"" + c.logs_dir + "\",\n";
    j += "  \"default_post_ttl_seconds\": " + std::to_string(c.default_post_ttl_seconds) + ",\n";
    j += "  \"mining_max_nonce\": " + std::to_string(c.mining_max_nonce) + ",\n";
    j += "  \"keypair_seed_base64\": \"" + c.keypair_seed_base64 + "\"\n";
    j += "}\n";
    return j;
}

static inline bool save_config_file(const std::string& path, const MyceliumConfig& c) {
    std::string json = config_string(c);
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    fwrite(json.data(), 1, json.size(), f);
    fclose(f);
    return true;
}

static inline bool load_config_file(const std::string& path, MyceliumConfig& c) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return false; }
    fseek(f, 0, SEEK_SET);
    std::string buf((size_t)sz, '\0');
    [[maybe_unused]] auto nread = fread(&buf[0], 1, (size_t)sz, f);
    fclose(f);

    const char* p = buf.c_str();
    config_json::skip_ws(p);
    if (*p != '{') return false;
    ++p;

    while (*p && *p != '}') {
        config_json::skip_ws(p);
        if (*p == '}') break;
        if (*p == ',') { ++p; config_json::skip_ws(p); continue; }

        std::string key = config_json::parse_string(p);
        config_json::skip_ws(p);
        if (*p != ':') return false;
        ++p;
        config_json::skip_ws(p);

        if (key == "listen_addr") {
            c.listen_addr = config_json::parse_string(p);
        } else if (key == "bootstrap_nodes") {
            c.bootstrap_nodes = config_json::parse_string_array(p);
        } else if (key == "enable_mdns") {
            c.enable_mdns = config_json::parse_bool(p);
        } else if (key == "enable_relay") {
            c.enable_relay = config_json::parse_bool(p);
        } else if (key == "enable_tor") {
            c.enable_tor = config_json::parse_bool(p);
        } else if (key == "tor_socks_port") {
            c.tor_socks_port = (uint16_t)config_json::parse_number(p);
        } else if (key == "tor_control_port") {
            c.tor_control_port = (uint16_t)config_json::parse_number(p);
        } else if (key == "onion_service_dir") {
            c.onion_service_dir = config_json::parse_string(p);
        } else if (key == "enable_http") {
            c.enable_http = config_json::parse_bool(p);
        } else if (key == "http_port") {
            c.http_port = (uint16_t)config_json::parse_number(p);
        } else if (key == "data_dir") {
            c.data_dir = config_json::parse_string(p);
        } else if (key == "posts_dir") {
            c.posts_dir = config_json::parse_string(p);
        } else if (key == "chain_dir") {
            c.chain_dir = config_json::parse_string(p);
        } else if (key == "profiles_dir") {
            c.profiles_dir = config_json::parse_string(p);
        } else if (key == "logs_dir") {
            c.logs_dir = config_json::parse_string(p);
        } else if (key == "default_post_ttl_seconds") {
            c.default_post_ttl_seconds = config_json::parse_number(p);
        } else if (key == "mining_max_nonce") {
            c.mining_max_nonce = config_json::parse_number(p);
        } else if (key == "keypair_seed_base64") {
            c.keypair_seed_base64 = config_json::parse_string(p);
        } else {
            // Unknown key — skip value
            if (*p == '"') config_json::parse_string(p);
            else if (*p == '[') config_json::parse_string_array(p);
            else if (*p == 't' || *p == 'f') config_json::parse_bool(p);
            else config_json::parse_number(p);
        }
        config_json::skip_ws(p);
        if (*p == ',') ++p;
    }
    return true;
}
