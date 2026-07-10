#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#define MYCELIUM_VERSION "0.5.0"
#include "crypto/myc_crypto.hpp"
#include "crypto/myc_bip39.hpp"
#include "protocol/myc_protocol.hpp"
#include "post/myc_post.hpp"
#include "storage/myc_storage.hpp"
#include "token/myc_token.hpp"
#include "social/myc_social_graph.hpp"
#include "profile/myc_profile.hpp"
#include "guestbook/myc_guestbook.hpp"
#include "identity/myc_identity.hpp"
#include "p2p/myc_p2p.hpp"
#include "media/myc_video.hpp"
#include "config/myc_config.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

// ============================================================
// Global state (single node) — defined before GUI include
// ============================================================
struct WalletEntry {
    std::array<uint8_t, 32> private_key = {};
    Wallet wallet;
};

struct AppState {
    Profile my_profile;
    std::vector<Profile> profiles;
    int active_profile = 0;
    SocialGraph social;
    Guestbook guestbook;
    UsernameRegistry identity;
    Wallet wallet;
    std::vector<WalletEntry> wallet_store;
    Tokenomics tokenomics;
    MyceliumNode* node = nullptr;
    bool node_running = false;
    VideoMetadata current_video;
    std::array<uint8_t, 32> private_key = {};
    MyceliumConfig config;
};

static AppState g_state;

#include "web/myc_web.hpp"
#include "gui/myc_gui.hpp"
#include "tui/myc_tui.hpp"

// ============================================================
// ANSI color helpers (YouTube red)
// ============================================================
static inline const char* ansi_red() {
#ifdef _WIN32
    static bool enabled = false;
    static bool checked = false;
    if (!checked) {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode)) {
            SetConsoleMode(h, mode | 0x0004);
            enabled = true;
        }
        checked = true;
    }
    return enabled ? "\033[38;2;255;0;0m" : "";
#else
    return "\033[38;2;255;0;0m";
#endif
}

static inline const char* ansi_bold_red() {
#ifdef _WIN32
    static bool enabled = false;
    static bool checked = false;
    if (!checked) {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode)) {
            SetConsoleMode(h, mode | 0x0004);
            enabled = true;
        }
        checked = true;
    }
    return enabled ? "\033[1;38;2;255;0;0m" : "";
#else
    return "\033[1;38;2;255;0;0m";
#endif
}

static inline const char* ansi_reset() {
#ifdef _WIN32
    static bool enabled = false;
    static bool checked = false;
    if (!checked) {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode)) {
            SetConsoleMode(h, mode | 0x0004);
            enabled = true;
        }
        checked = true;
    }
    return enabled ? "\033[0m" : "";
#else
    return "\033[0m";
#endif
}

#define R ansi_bold_red()
#define X ansi_reset()

// ============================================================
// Helper: YouTube-style red header bar
// ============================================================
static inline void print_yt_header(const char* icon, const char* title) {
    printf("\n%s============================================%s\n", R, X);
    printf("%s  %s  %s%s\n", R, icon ? icon : "", title ? title : "", X);
    printf("%s============================================%s\n", R, X);
}

static inline void print_yt_line(const char* label, const char* value) {
    printf("  %s: %s\n", label, value ? value : "");
}

static inline void print_yt_sep() {
    printf("%s--------------------------------------------%s\n", R, X);
}

// ============================================================
// Config handlers
// ============================================================
static inline void handle_config_generate(const char* path) {
    if (!path) path = "./mycelium.json";
    if (save_config_file(path, g_state.config)) {
        printf("  Default config saved to: %s\n", path);
        printf("  Edit it and use: mycelium start --config %s\n", path);
    } else {
        printf("  Error: could not write to %s\n", path);
    }
}

static inline void handle_config_show() {
    printf("%s\n", config_string(g_state.config).c_str());
}

static inline void handle_config_load(const char* path) {
    if (!path) return;
    if (load_config_file(path, g_state.config)) {
        printf("  Config loaded from: %s\n", path);
    } else {
        printf("  Warning: could not load config from %s (using defaults)\n", path);
    }
}

static inline void handle_start_from_config() {
    auto& cfg = g_state.config;
    P2pConfig p2p = cfg.to_p2p_config();
    g_state.node = new MyceliumNode(MyceliumNode::create(p2p));
    g_state.node->init_storage(p2p.storage);
    g_state.node->load_chain(g_state.tokenomics, g_state.wallet);
    g_state.node_running = true;
}

static inline void run_http_server(uint16_t port, bool is_mgmt) {
    printf("  Starting HTTP server on port %u...\n", port);
#ifdef _WIN32
    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        printf("  Error: socket() failed\n");
        return;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("  Error: bind() failed (port %u in use?)\n", port);
        closesocket(server_fd);
        return;
    }
    listen(server_fd, SOMAXCONN);
    printf("  Listening at \033[1mhttp://localhost:%u\033[0m\n", port);
    while (true) {
        SOCKET client = accept(server_fd, nullptr, nullptr);
        if (client == INVALID_SOCKET) break;
        serve_http_page_ex((uintptr_t)client, is_mgmt);
    }
    closesocket(server_fd);
#else
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("  Error: socket() failed\n");
        return;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("  Error: bind() failed (port %u in use?)\n", port);
        close(server_fd);
        return;
    }
    listen(server_fd, SOMAXCONN);
    printf("  Listening at \033[1mhttp://localhost:%u\033[0m\n", port);
    while (true) {
        int client = accept(server_fd, nullptr, nullptr);
        if (client < 0) break;
        serve_http_page_ex((uintptr_t)client, is_mgmt);
    }
    close(server_fd);
#endif
}

// ============================================================
// Command handlers
// ============================================================
static inline void handle_start(const char* listen_addr, const char* bootstrap,
                                 int enable_tor, uint16_t tor_socks, uint16_t tor_ctrl,
                                 uint16_t http_port) {
    // Apply CLI overrides on top of config (only when explicitly set)
    if (listen_addr) g_state.config.listen_addr = listen_addr;
    if (bootstrap) {
        g_state.config.bootstrap_nodes.clear();
        g_state.config.bootstrap_nodes.push_back(bootstrap);
    }
    if (enable_tor >= 0) g_state.config.enable_tor = (enable_tor > 0);
    if (tor_socks > 0) g_state.config.tor_socks_port = tor_socks;
    if (tor_ctrl > 0) g_state.config.tor_control_port = tor_ctrl;
    if (http_port > 0) {
        g_state.config.enable_http = true;
        g_state.config.http_port = http_port;
    }

    handle_start_from_config();

    print_yt_header("\xF0\x9F\x94\x8C", "MYTUBE NODE STARTED");
    print_yt_line("Peer ID", g_state.node->local_peer_id().c_str());
    print_yt_line("Listening", listen_addr ? listen_addr : "default");
    char peers[32];
    snprintf(peers, sizeof(peers), "%zu", g_state.node->peer_count());
    print_yt_line("Peers", peers);
    if (enable_tor) {
        print_yt_line("Tor", "Enabled");
        if (!g_state.node->local_info.onion_address.empty()) {
            char socks[16];
            snprintf(socks, sizeof(socks), "%u", tor_socks);
            print_yt_line("SOCKS Port", socks);
            print_yt_line("Onion Address", g_state.node->local_info.onion_address.c_str());
        }
    } else {
        print_yt_line("Tor", "Disabled");
    }
    if (g_state.config.enable_http && g_state.config.http_port > 0) {
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%u", g_state.config.http_port);
        print_yt_line("Web UI", port_str);
        if (!g_state.node->local_info.onion_address.empty()) {
            std::string onion_url = "http://" + g_state.node->local_info.onion_address + ".onion";
            print_yt_line("Tor URL", onion_url.c_str());
        }
    }
    print_yt_sep();

    if (g_state.config.enable_http && g_state.config.http_port > 0) {
        printf("\n");
        run_http_server(g_state.config.http_port, false);
    }
}

static inline void handle_profile_create(const char* display_name, const char* username) {
    std::array<uint8_t, 32> peer_seed;
    random_bytes(peer_seed.data(), 32);
    auto pk = ed25519_pubkey(peer_seed);
    std::string peer_id = base64_encode(pk.data(), 16);

    auto builder = ProfileBuilder::create(peer_id, display_name);
    if (username) builder.with_username(username);

    KeyPair kp = x25519_keygen();
    g_state.my_profile = builder.sign(kp).build();

    print_yt_header("\xF0\x9F\x93\xBA", "CHANNEL CREATED");
    print_yt_line("Channel", g_state.my_profile.display_name.c_str());
    if (!g_state.my_profile.username.empty()) {
        std::string at = "@" + g_state.my_profile.username;
        print_yt_line("Username", at.c_str());
    }
    auto cid = compute_profile_cid(g_state.my_profile);
    print_yt_line("Channel CID", cid.c_str());
    print_yt_sep();
}

static inline void handle_profile_show(const char* user) {
    if (g_state.my_profile.peer_id.empty()) {
        printf("No channel. Use 'profile create' first.\n");
        return;
    }
    print_yt_header("\xF0\x9F\x93\xBA", "CHANNEL PAGE");
    print_yt_line("Channel", g_state.my_profile.display_name.c_str());
    if (!g_state.my_profile.username.empty()) {
        std::string at = "@" + g_state.my_profile.username;
        print_yt_line("Username", at.c_str());
    }
    if (!g_state.my_profile.bio.empty())
        print_yt_line("About", g_state.my_profile.bio.c_str());
    char links[32];
    snprintf(links, sizeof(links), "%zu links", g_state.my_profile.links.size());
    print_yt_line("Links", links);
    print_yt_line("Theme", theme_preset_name(g_state.my_profile.theme.preset));
    auto cid = compute_profile_cid(g_state.my_profile);
    print_yt_line("Channel CID", cid.c_str());
    print_yt_sep();
}

static inline void handle_profile_update(const char* bio, const char* display_name) {
    if (bio) { g_state.my_profile.bio = bio; printf("  Bio updated.\n"); }
    if (display_name) { g_state.my_profile.display_name = display_name; printf("  Display name updated.\n"); }
}

static inline void handle_profile_avatar(const char* cid) {
    g_state.my_profile.avatar_cid = cid;
    printf("  Avatar set to: %s\n", cid);
}

static inline void handle_profile_banner(const char* cid) {
    g_state.my_profile.banner_cid = cid;
    printf("  Banner set to: %s\n", cid);
}

static inline void handle_profile_link(const char* title, const char* url) {
    SocialLink link;
    std::array<uint8_t, 32> id_buf;
    random_bytes(id_buf.data(), 32);
    link.id = base64_encode(id_buf.data(), 8);
    link.title = title;
    link.url = url;
    g_state.my_profile.links.push_back(link);
    printf("  Link added: %s -> %s\n", title, url);
}

static inline void handle_profile_theme(const char* preset_name, const char* primary) {
    ThemePreset p = kThemeDefault;
    if (preset_name) {
        std::string s = preset_name;
        for (auto& c : s) c = tolower(c);
        if (s == "midnight") p = kThemeMidnight;
        else if (s == "ocean") p = kThemeOcean;
        else if (s == "forest") p = kThemeForest;
        else if (s == "sunset") p = kThemeSunset;
        else if (s == "minimal") p = kThemeMinimal;
        else if (s == "hacker") p = kThemeHacker;
    }
    g_state.my_profile.theme = Theme::from_preset(p);
    if (primary) g_state.my_profile.theme.colors.primary = primary;
    printf("  Theme set to: %s\n", theme_preset_name(p));
}

static inline void handle_post_create(const char* content) {
    auto kp = x25519_keygen();
    auto post = Post::create(
        base64_encode(kp.pk.point.data(), 16),
        content ? content : "",
        kp);
    printf("  Post created: %s\n", post.id.c_str());
    printf("  Author: %s\n", post.author.c_str());
    char ttl[32];
    snprintf(ttl, sizeof(ttl), "%llu seconds", (unsigned long long)post.ttl_seconds);
    print_yt_line("TTL", ttl);

    if (g_state.node) g_state.node->store_post(post);
}

static inline void handle_feed(int limit) {
    (void)limit;
    print_yt_header("\xF0\x9F\x93\xB1", "RECOMMENDED VIDEOS");
    printf("  No videos yet (connect to network to sync)\n");
    print_yt_sep();
}

static inline bool read_passphrase(const char* prompt, char* buf, size_t bufsz) {
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, (int)bufsz, stdin)) return false;
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    return len > 0;
}

static inline void handle_wallet_create() {
    char passphrase[128], confirm[128];

    // Passphrase input
    if (!read_passphrase("  Enter passphrase (min 8 chars, 1 digit, 1 special): ", passphrase, sizeof(passphrase))) {
        printf("  Canceled.\n");
        return;
    }
    if (!validate_passphrase(passphrase)) {
        printf("  Passphrase must be >=8 chars with at least 1 digit and 1 special character.\n");
        return;
    }
    if (!read_passphrase("  Confirm passphrase: ", confirm, sizeof(confirm))) {
        printf("  Canceled.\n");
        return;
    }
    if (strcmp(passphrase, confirm) != 0) {
        printf("  Passphrases do not match.\n");
        return;
    }

    // Create wallet
    std::vector<std::string> mnemonic_words;
    std::array<uint8_t, 64> seed;
    std::array<uint8_t, 32> private_key, public_key;
    if (!wallet_create_full(mnemonic_words, seed, private_key, public_key, passphrase)) {
        printf("  Failed to create wallet.\n");
        return;
    }

    // Save to disk
    std::string wallet_path = "./data/chain/wallet.dat";
    if (!wallet_save(wallet_path, private_key, mnemonic_words, passphrase)) {
        printf("  Warning: could not save encrypted wallet.dat\n");
    }

    // Set up global wallet state
    g_state.wallet = Wallet{};
    g_state.wallet.public_key = public_key;
    g_state.private_key = private_key;

    auto addr = base64_encode(public_key.data(), 16);
    printf("  Wallet created: MYT%s\n", addr.c_str());
    printf("\n  \xE2\x9A\xA0\xEF\xB8\x8F  BACKUP YOUR MNEMONIC (24 words):\n\n");
    for (size_t i = 0; i < mnemonic_words.size(); ++i) {
        printf("    %2zu. %s\n", i + 1, mnemonic_words[i].c_str());
    }
    printf("\n  \xE2\x9A\xA0\xEF\xB8\x8F  Store these words safely. "
           "They are needed to restore your wallet.\n\n");
    memset(passphrase, 0, sizeof(passphrase));
    memset(confirm, 0, sizeof(confirm));
}

static inline void handle_wallet_restore(int argc, char** argv, int cmd_idx) {
    std::vector<std::string> words;
    for (int i = cmd_idx + 2; i < argc; ++i) {
        if (argv[i][0] == '-') break;
        words.push_back(argv[i]);
    }
    if (words.size() != 24) {
        printf("  Provide exactly 24 words.\n");
        return;
    }

    // Validate mnemonic
    std::array<uint8_t, 32> entropy;
    if (!mnemonic_restore(words, entropy)) {
        printf("  Invalid mnemonic — checksum mismatch or unknown words.\n");
        return;
    }

    char passphrase[128];
    if (!read_passphrase("  Enter passphrase: ", passphrase, sizeof(passphrase))) {
        printf("  Canceled.\n");
        return;
    }

    // Derive seed + keypair
    std::array<uint8_t, 64> seed;
    mnemonic_to_seed(words, passphrase, seed.data());

    auto hash = sha512(seed.data(), 64);
    std::array<uint8_t, 32> private_key;
    memcpy(private_key.data(), hash.data(), 32);
    private_key[0] &= 248; private_key[31] &= 127; private_key[31] |= 64;

    std::array<uint8_t, 32> public_key = ed25519_pubkey(private_key);

    // Save to disk
    std::string wallet_path = "./data/chain/wallet.dat";
    if (!wallet_save(wallet_path, private_key, words, passphrase)) {
        printf("  Warning: could not save encrypted wallet.dat\n");
    }

    g_state.wallet = Wallet{};
    g_state.wallet.public_key = public_key;
    g_state.private_key = private_key;

    auto addr = base64_encode(public_key.data(), 16);
    printf("  Wallet restored: MYT%s\n", addr.c_str());
    memset(passphrase, 0, sizeof(passphrase));
}

static inline void handle_mine() {
    if (g_state.private_key == std::array<uint8_t, 32>{}) {
        printf("  No wallet. Use 'wallet create' or 'wallet restore' first.\n");
        return;
    }

    if (!g_state.node) {
        printf("  No node running. Use 'start' first.\n");
        // Create a throwaway node for mining
        handle_start_from_config();
    }

    uint64_t epoch = g_state.tokenomics.current_epoch;
    uint64_t reward_amount = mining_block_reward(epoch);
    uint32_t diff = mining_difficulty_bits(epoch);
    size_t mp_sz = g_state.node->mempool_size();

    printf("  Mining block at epoch %llu... (difficulty: %u bits, reward: %llu MYTUBE, mempool: %zu txs)\n",
           (unsigned long long)epoch, diff, (unsigned long long)reward_amount, mp_sz);

    bool found = g_state.node->mine_block(
        g_state.tokenomics, g_state.wallet,
        g_state.private_key, g_state.config.mining_max_nonce);

    if (!found) {
        printf("  No block found in %llu attempts.\n", (unsigned long long)g_state.config.mining_max_nonce);
        return;
    }

    char bal[32];
    snprintf(bal, sizeof(bal), "%llu MYTUBE", (unsigned long long)g_state.wallet.balance);
    printf("  \xF0\x9F\x9B\x8F Block mined! Height: %zu, Reward: %llu MYTUBE, Balance: %s\n",
           g_state.node->chain.size() - 1, (unsigned long long)reward_amount, bal);
}

static inline void handle_wallet_balance() {
    print_yt_header("\xF0\x9F\x92\xB0", "WALLET EARNINGS");
    char bal[32], staked[32];
    snprintf(bal, sizeof(bal), "%llu MYTUBE", (unsigned long long)g_state.wallet.balance);
    snprintf(staked, sizeof(staked), "%llu MYTUBE", (unsigned long long)g_state.wallet.staked_balance);
    print_yt_line("Balance", bal);
    print_yt_line("Staked", staked);
    print_yt_sep();
}

static inline void handle_wallet_stake(uint64_t amount) {
    auto err = g_state.wallet.stake(amount);
    if (err == kTokenOk) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Staked %llu MYTUBE tokens.", (unsigned long long)amount);
        printf("  %s\n", buf);
    } else
        printf("  Error: %s\n", token_strerror(err));
}

static inline void handle_wallet_unstake(uint64_t amount) {
    auto err = g_state.wallet.unstake(amount);
    if (err == kTokenOk) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Unstaked %llu MYTUBE tokens.", (unsigned long long)amount);
        printf("  %s\n", buf);
    } else
        printf("  Error: %s\n", token_strerror(err));
}

static inline void handle_wallet_send(const char* to, uint64_t amount) {
    if (g_state.private_key == std::array<uint8_t, 32>{}) {
        printf("  No wallet. Use 'wallet create' or 'wallet restore' first.\n");
        return;
    }
    if (!to || strlen(to) == 0) {
        printf("  Error: --to ADDR is required.\n");
        return;
    }

    // Decode destination address (base64 -> 32 bytes)
    auto to_bytes = base64_decode(to);
    std::array<uint8_t, 32> to_pk = {};
    if (to_bytes.size() < 32) {
        printf("  Invalid destination address (expected base64, got '%s').\n", to);
        return;
    }
    memcpy(to_pk.data(), to_bytes.data(), 32);

    // Deduct balance locally
    auto err = g_state.wallet.send(amount);
    if (err != kTokenOk) {
        printf("  Error: %s\n", token_strerror(err));
        return;
    }

    // Create and sign transaction
    auto tx = Transaction::create(g_state.wallet.public_key, to_pk, amount);
    tx.sign(g_state.private_key);
    tx.status = kTxPending;

    // Add to mempool
    if (g_state.node) {
        g_state.node->add_to_mempool(tx);
        g_state.node->broadcast_transaction(tx);
    }

    char bal[32];
    snprintf(bal, sizeof(bal), "%llu MYTUBE", (unsigned long long)g_state.wallet.balance);
    printf("  Sent %llu MYTUBE to %s. Tx: %s, Balance: %s\n",
           (unsigned long long)amount, to, tx.tx_id.c_str(), bal);
}

static inline void handle_social_follow(const char* user) {
    if (!user) return;
    auto err = g_state.social.follow(user);
    if (err == kFollowOk)
        printf("  Subscribed to %s.\n", user);
    else
        printf("  Error: %s\n", follow_strerror(err));
}

static inline void handle_social_unfollow(const char* user) {
    if (!user) return;
    auto err = g_state.social.unfollow(user);
    if (err == kFollowOk)
        printf("  Unsubscribed from %s.\n", user);
    else
        printf("  Error: %s\n", follow_strerror(err));
}

static inline void handle_social_block(const char* user) {
    if (!user) return;
    auto err = g_state.social.block(user);
    if (err == kFollowOk)
        printf("  Blocked %s.\n", user);
    else
        printf("  Error: %s\n", follow_strerror(err));
}

static inline void handle_social_unblock(const char* user) {
    if (!user) return;
    auto err = g_state.social.unblock(user);
    if (err == kFollowOk)
        printf("  Unblocked %s.\n", user);
    else
        printf("  Error: %s\n", follow_strerror(err));
}

static inline void handle_identity_register(const char* username) {
    if (!username) return;
    std::array<uint8_t, 32> peer_id;
    random_bytes(peer_id.data(), 32);
    std::array<uint8_t, 32> priv;
    random_bytes(priv.data(), 32);
    auto err = g_state.identity.register_username(
        peer_id.data(), peer_id.size(), username, priv);
    if (err == kIdentityOk)
        printf("  Registered @%s.\n", username);
    else
        printf("  Error: %s\n", identity_strerror(err));
}

static inline void handle_identity_lookup(const char* username) {
    if (!username) return;
    auto* rec = g_state.identity.lookup(username);
    if (rec) {
        printf("  @%s -> %s\n", username, base64_encode(rec->peer_id.data(), rec->peer_id.size()).c_str());
    } else {
        printf("  @%s not found.\n", username);
    }
}

static inline void handle_guestbook_sign(const char* user, const char* message, const char* name) {
    std::array<uint8_t, 32> key;
    random_bytes(key.data(), 32);
    auto entry = GuestbookEntry::create(
        base64_encode(key.data(), 16),
        name ? name : "Anonymous",
        message ? message : "");
    auto err = g_state.guestbook.sign(entry);
    if (err == kGbOk)
        printf("  Signed %s's guestbook.\n", user ? user : "?");
    else
        printf("  Error: %s\n", gb_strerror(err));
}

static inline void handle_guestbook_show(const char* user) {
    print_yt_header("\xF0\x9F\x93\x9D", "COMMENTS");
    printf("  Comments for %s:\n", user ? user : "?");
    for (auto& [id, entry] : g_state.guestbook.entries) {
        (void)id;
        printf("    %s: %s\n", entry.author_name.c_str(), entry.content.c_str());
    }
    print_yt_sep();
}

static inline void handle_network_peers() {
    print_yt_header("\xF0\x9F\x94\x97", "CONNECTED PEERS");
    if (g_state.node_running && g_state.node) {
        for (auto& [id, info] : g_state.node->peer_table.get_all_peers()) {
            printf("  %s (last seen: %lld)\n", id.c_str(), (long long)info.last_seen);
        }
    } else {
        printf("  No peers (start node first)\n");
    }
    print_yt_sep();
}

static inline void handle_status() {
    print_yt_header("\xF0\x9F\x8E\xAC", "MYTUBE STATUS");
    print_yt_line("Version", MYCELIUM_VERSION);
    print_yt_line("Network", g_state.node_running ? "Online" : "Offline");
    if (g_state.node_running && g_state.node) {
        char peers[32];
        snprintf(peers, sizeof(peers), "%zu", g_state.node->peer_count());
        print_yt_line("Peers", peers);
        if (!g_state.node->local_info.onion_address.empty()) {
            print_yt_line("Tor", "Enabled");
            print_yt_line("Onion", g_state.node->local_info.onion_address.c_str());
        } else {
            print_yt_line("Tor", "Disabled");
        }
    }
    char supply[64];
    snprintf(supply, sizeof(supply), "%llu MYTUBE", (unsigned long long)g_state.tokenomics.total_supply);
    print_yt_line("Supply", supply);
    char minted[64];
    snprintf(minted, sizeof(minted), "%llu MYTUBE", (unsigned long long)g_state.tokenomics.minted_supply);
    print_yt_line("Minted", minted);
    char reward[32];
    snprintf(reward, sizeof(reward), "%llu MYTUBE",
             (unsigned long long)mining_block_reward(g_state.tokenomics.current_epoch));
    print_yt_line("Block Reward", reward);
    char diff[16];
    snprintf(diff, sizeof(diff), "%u bits",
             mining_difficulty_bits(g_state.tokenomics.current_epoch));
    print_yt_line("Difficulty", diff);
    char epoch[32];
    snprintf(epoch, sizeof(epoch), "%llu", (unsigned long long)g_state.tokenomics.current_epoch);
    print_yt_line("Epoch", epoch);
    print_yt_line("Channel", g_state.my_profile.peer_id.empty() ? "None" : "Created");
    char videos[32];
    snprintf(videos, sizeof(videos), "%s uploaded",
             g_state.current_video.video_id.empty() ? "0" : "1");
    print_yt_line("Videos", videos);
    print_yt_sep();
}

// ============================================================
// Video handlers
// ============================================================
static inline void handle_video_upload(
    const char* video_id,
    uint64_t duration_ms,
    uint32_t width,
    uint32_t height,
    uint32_t codec_val,
    uint64_t bitrate_bps,
    uint32_t chunk_count)
{
    if (!video_id) { printf("  Error: --video-id is required\n"); return; }
    VideoCodec codec = (VideoCodec)(codec_val > 3 ? 0 : codec_val);
    g_state.current_video = VideoMetadata::create(
        video_id, duration_ms, width, height, codec, bitrate_bps);

    for (uint32_t i = 0; i < chunk_count; ++i) {
        std::array<uint8_t, 32> chunk_hash;
        random_bytes(chunk_hash.data(), 32);
        std::array<uint8_t, 32> cid_buf;
        random_bytes(cid_buf.data(), 16);
        std::string chunk_cid = "QmVc" + base64_encode(cid_buf.data(), 16).substr(0, 44);
        g_state.current_video.add_chunk(chunk_cid, i,
            (uint64_t)i * kDefaultChunkSizeBytes,
            kDefaultChunkSizeBytes,
            std::vector<uint8_t>(chunk_hash.begin(), chunk_hash.end()));
    }

    auto manifest_cid = compute_video_cid(g_state.current_video);

    print_yt_header("\xF0\x9F\x93\xB9", "VIDEO UPLOADED");
    print_yt_line("Video ID", video_id);
    print_yt_line("Manifest CID", manifest_cid.c_str());
    print_yt_line("Codec", video_codec_name(g_state.current_video.codec));
    char res[32];
    snprintf(res, sizeof(res), "%ux%u", g_state.current_video.width, g_state.current_video.height);
    print_yt_line("Resolution", res);
    char dur[32];
    snprintf(dur, sizeof(dur), "%llu ms", (unsigned long long)g_state.current_video.duration_ms);
    print_yt_line("Duration", dur);
    char br[32];
    snprintf(br, sizeof(br), "%llu bps", (unsigned long long)g_state.current_video.bitrate_bps);
    print_yt_line("Bitrate", br);
    char chunks[32];
    snprintf(chunks, sizeof(chunks), "%zu chunks", g_state.current_video.chunks.size());
    print_yt_line("Chunks", chunks);
    char sz[32];
    snprintf(sz, sizeof(sz), "%llu bytes", (unsigned long long)g_state.current_video.total_size_bytes());
    print_yt_line("Total Size", sz);
    print_yt_sep();
}

static inline void handle_video_manifest() {
    if (g_state.current_video.video_id.empty()) {
        printf("  No video uploaded yet. Use 'video upload' first.\n");
        return;
    }
    auto manifest_cid = compute_video_cid(g_state.current_video);

    print_yt_header("\xF0\x9F\x93\x84", "VIDEO MANIFEST");
    print_yt_line("Manifest CID", manifest_cid.c_str());
    print_yt_line("Video ID", g_state.current_video.video_id.c_str());
    print_yt_line("Codec", video_codec_name(g_state.current_video.codec));
    char res[32];
    snprintf(res, sizeof(res), "%ux%u", g_state.current_video.width, g_state.current_video.height);
    print_yt_line("Resolution", res);
    char br[32];
    snprintf(br, sizeof(br), "%llu bps", (unsigned long long)g_state.current_video.bitrate_bps);
    print_yt_line("Bitrate", br);
    char dur[32];
    snprintf(dur, sizeof(dur), "%llu ms", (unsigned long long)g_state.current_video.duration_ms);
    print_yt_line("Duration", dur);
    char cksz[32];
    snprintf(cksz, sizeof(cksz), "%u bytes", g_state.current_video.chunk_size_bytes);
    print_yt_line("Chunk Size", cksz);
    printf("  Chunks:\n");
    for (auto& c : g_state.current_video.chunks) {
        printf("    [%u] offset=%llu len=%llu cid=%s\n",
               c.index,
               (unsigned long long)c.byte_offset,
               (unsigned long long)c.byte_length,
               c.chunk_cid.c_str());
    }
    print_yt_sep();
}

// ============================================================
// Config handlers
// ============================================================
// ============================================================
// Simple arg parser
// ============================================================
static inline void print_usage(const char* prog) {
    printf("\n%s============================================%s\n", R, X);
    printf("%s  \xF0\x9F\x8E\xAC  MYTUBE PROTOCOL v%s%s\n", R, MYCELIUM_VERSION, X);
    printf("%s============================================%s\n", R, X);
    printf("  A YouTube-like P2P video network\n\n");
    printf("  Usage: %s <command> [args]\n\n", prog);
    printf("  Commands:\n");
    printf("    start [--config FILE] [--listen ADDR] [--bootstrap ADDR] [--tor] [--tor-socks-port PORT] [--tor-control-port PORT] [--http-port PORT]\n");
    printf("    wallet create\n    wallet restore <24 words>\n    wallet balance\n    wallet stake --amount N\n    wallet unstake --amount N\n    wallet send --to ADDR --amount N\n");
    printf("    profile create --display-name NAME [--username USER]\n");
    printf("    profile show [--user USER]\n");
    printf("    profile update [--bio TEXT] [--display-name NAME]\n");
    printf("    profile avatar --cid CID\n");
    printf("    profile banner --cid CID\n");
    printf("    profile link --title T --url U\n");
    printf("    profile theme [--preset P] [--primary COLOR]\n");
    printf("    post --content TEXT\n");
    printf("    feed [--limit N]\n");
    printf("    mine\n");
    printf("    gui [--config FILE]\n");
    printf("    tui [--config FILE] [--http-port PORT]\n");
    printf("    webonly [--config FILE] [--http-port PORT] [--mgmt-port PORT]\n");
    printf("    config generate [--path FILE]\n");
    printf("    config show\n");
    printf("    social follow --user U\n");
    printf("    social unfollow --user U\n");
    printf("    social block --user U\n");
    printf("    social unblock --user U\n");
    printf("    identity register --username U\n");
    printf("    identity lookup --username U\n");
    printf("    guestbook sign --user U --message M [--name N]\n");
    printf("    guestbook show --user U\n");
    printf("    video upload --video-id ID --duration DUR --width W --height H [--codec C] [--bitrate B] [--chunks N]\n");
    printf("    video manifest\n");
    printf("    network peers\n");
    printf("    status\n");
    printf("    help\n\n");
}

int main(int argc, char** argv) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    g_state.tokenomics.minted_supply = 0;
    g_state.tokenomics.staked_supply = 0;
    g_state.tokenomics.burned_supply = 0;
    g_state.tokenomics.current_epoch = 0;
    g_state.tokenomics.annual_inflation_bps = 800;
    g_state.tokenomics.total_supply = kTotalSupply;

    // Parse --config early (before command dispatch)
    const char* config_path = nullptr;
    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--config") == 0 || strcmp(argv[i], "-c") == 0) && i + 1 < argc) {
            config_path = argv[++i];
            break;
        }
    }
    if (config_path) handle_config_load(config_path);

    if (argc < 2) { print_usage(argv[0]); return 1; }

    // Find the first non-flag argument (skip --config, -c, etc.)
    std::string cmd;
    int cmd_idx = 0;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if ((strcmp(argv[i], "--config") == 0 || strcmp(argv[i], "-c") == 0) && i + 1 < argc) ++i;
            continue;
        }
        cmd = argv[i];
        cmd_idx = i;
        break;
    }
    if (cmd.empty()) { print_usage(argv[0]); return 1; }

    if (cmd == "help" || cmd == "--help") { print_usage(argv[0]); return 0; }

    if (cmd == "gui") {
#ifdef _WIN32
        ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif
        return gui_run();
    }

    if (cmd == "tui") {
        uint16_t http_port = 0;
        for (int i = cmd_idx + 1; i < argc; ++i) {
            if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) { if (!config_path) handle_config_load(argv[++i]); else ++i; }
            else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) { if (!config_path) handle_config_load(argv[++i]); else ++i; }
            else if (strcmp(argv[i], "--http-port") == 0 && i + 1 < argc) http_port = (uint16_t)atoi(argv[++i]);
        }
        if (http_port == 0 && g_state.config.enable_http)
            http_port = g_state.config.http_port;
        return tui_run(http_port);
    }

    if (cmd == "webonly") {
        uint16_t http_port = 0;
        uint16_t mgmt_port = 0;
        for (int i = cmd_idx + 1; i < argc; ++i) {
            if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) { if (!config_path) handle_config_load(argv[++i]); else ++i; }
            else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) { if (!config_path) handle_config_load(argv[++i]); else ++i; }
            else if (strcmp(argv[i], "--http-port") == 0 && i + 1 < argc) http_port = (uint16_t)atoi(argv[++i]);
            else if (strcmp(argv[i], "--mgmt-port") == 0 && i + 1 < argc) mgmt_port = (uint16_t)atoi(argv[++i]);
        }
        if (http_port == 0) {
            if (g_state.config.enable_http) http_port = g_state.config.http_port;
            else http_port = 8080;
        }
        if (mgmt_port == 0) mgmt_port = 8081;

        handle_start_from_config();
        print_yt_header("\xF0\x9F\x94\x8C", "MYTUBE WEB ONLY");
        print_yt_line("Peer ID", g_state.node->local_peer_id().c_str());
        char peers[32];
        snprintf(peers, sizeof(peers), "%zu", g_state.node->peer_count());
        print_yt_line("Peers", peers);
        print_yt_sep();

        printf("\n");
        if (mgmt_port > 0) {
            printf("  Management port: \033[1mhttp://localhost:%u\033[0m\n", mgmt_port);
            printf("    (wallet, mining, node control)\n\n");
        }
        if (http_port > 0) {
            printf("  Client port: \033[1mhttp://localhost:%u\033[0m\n", http_port);
            printf("    (dashboard, content browsing)\n\n");
        }
        printf("  Press Ctrl+C to stop.\n\n");

        std::thread mgmt_thread;
        if (mgmt_port > 0)
            mgmt_thread = std::thread(run_http_server, mgmt_port, true);
        if (http_port > 0)
            run_http_server(http_port, false);
        if (mgmt_thread.joinable())
            mgmt_thread.join();
        return 0;
    }

    if (cmd == "config") {
        if (cmd_idx + 1 >= argc) { print_usage(argv[0]); return 1; }
        std::string action = argv[cmd_idx + 1];
        if (action == "generate") {
            const char* path = nullptr;
            for (int i = cmd_idx + 2; i < argc - 1; ++i)
                if (strcmp(argv[i], "--path") == 0) { path = argv[++i]; break; }
            handle_config_generate(path);
        } else if (action == "show") {
            handle_config_show();
        } else {
            print_usage(argv[0]);
        }
        return 0;
    }

    if (cmd == "start") {
        const char* listen = nullptr;
        const char* bootstrap = nullptr;
        int enable_tor = -1; // -1 = not set, use config default
        uint16_t tor_socks = 0;
        uint16_t tor_ctrl = 0;
        uint16_t http_port = 0;
        for (int i = cmd_idx + 1; i < argc; ++i) {
            if (strcmp(argv[i], "--listen") == 0 && i + 1 < argc) listen = argv[++i];
            else if (strcmp(argv[i], "--bootstrap") == 0 && i + 1 < argc) bootstrap = argv[++i];
            else if (strcmp(argv[i], "--tor") == 0) enable_tor = 1;
            else if (strcmp(argv[i], "--tor-socks-port") == 0 && i + 1 < argc) tor_socks = (uint16_t)atoi(argv[++i]);
            else if (strcmp(argv[i], "--tor-control-port") == 0 && i + 1 < argc) tor_ctrl = (uint16_t)atoi(argv[++i]);
            else if (strcmp(argv[i], "--http-port") == 0 && i + 1 < argc) http_port = (uint16_t)atoi(argv[++i]);
        }
        handle_start(listen, bootstrap, enable_tor, tor_socks, tor_ctrl, http_port);
    }
    else if (cmd == "profile") {
        if (cmd_idx + 1 >= argc) { print_usage(argv[0]); return 1; }
        std::string action = argv[cmd_idx + 1];
        if (action == "create") {
            const char* dn = nullptr, *un = nullptr;
            for (int i = cmd_idx + 2; i < argc - 1; ++i) {
                if (strcmp(argv[i], "--display-name") == 0) dn = argv[++i];
                if (strcmp(argv[i], "--username") == 0) un = argv[++i];
            }
            handle_profile_create(dn, un);
        }
        else if (action == "show") {
            const char* user = nullptr;
            for (int i = cmd_idx + 2; i < argc - 1; ++i)
                if (strcmp(argv[i], "--user") == 0) user = argv[++i];
            handle_profile_show(user);
        }
        else if (action == "update") {
            const char* bio = nullptr, *dn = nullptr;
            for (int i = cmd_idx + 2; i < argc - 1; ++i) {
                if (strcmp(argv[i], "--bio") == 0) bio = argv[++i];
                if (strcmp(argv[i], "--display-name") == 0) dn = argv[++i];
            }
            handle_profile_update(bio, dn);
        }
        else if (action == "avatar") {
            for (int i = cmd_idx + 2; i < argc - 1; ++i)
                if (strcmp(argv[i], "--cid") == 0) { handle_profile_avatar(argv[++i]); break; }
        }
        else if (action == "banner") {
            for (int i = cmd_idx + 2; i < argc - 1; ++i)
                if (strcmp(argv[i], "--cid") == 0) { handle_profile_banner(argv[++i]); break; }
        }
        else if (action == "link") {
            const char* t = nullptr, *u = nullptr;
            for (int i = cmd_idx + 2; i < argc - 1; ++i) {
                if (strcmp(argv[i], "--title") == 0) t = argv[++i];
                if (strcmp(argv[i], "--url") == 0) u = argv[++i];
            }
            handle_profile_link(t, u);
        }
        else if (action == "theme") {
            const char* preset = nullptr, *primary = nullptr;
            for (int i = cmd_idx + 2; i < argc - 1; ++i) {
                if (strcmp(argv[i], "--preset") == 0) preset = argv[++i];
                if (strcmp(argv[i], "--primary") == 0) primary = argv[++i];
            }
            handle_profile_theme(preset, primary);
        }
        else print_usage(argv[0]);
    }
    else if (cmd == "post") {
        const char* content = nullptr;
        for (int i = cmd_idx + 1; i < argc - 1; ++i)
            if (strcmp(argv[i], "--content") == 0) content = argv[++i];
        handle_post_create(content);
    }
    else if (cmd == "feed") {
        int limit = 50;
        for (int i = cmd_idx + 1; i < argc - 1; ++i)
            if (strcmp(argv[i], "--limit") == 0) limit = atoi(argv[++i]);
        handle_feed(limit);
    }
    else if (cmd == "wallet") {
        if (cmd_idx + 1 >= argc) { print_usage(argv[0]); return 1; }
        std::string action = argv[cmd_idx + 1];
        if (action == "create") handle_wallet_create();
        else if (action == "restore") handle_wallet_restore(argc, argv, cmd_idx);
        else if (action == "balance") handle_wallet_balance();
        else if (action == "stake") {
            for (int i = cmd_idx + 2; i < argc - 1; ++i)
                if (strcmp(argv[i], "--amount") == 0) { handle_wallet_stake((uint64_t)atoll(argv[++i])); break; }
        }
        else if (action == "unstake") {
            for (int i = cmd_idx + 2; i < argc - 1; ++i)
                if (strcmp(argv[i], "--amount") == 0) { handle_wallet_unstake((uint64_t)atoll(argv[++i])); break; }
        }
        else if (action == "send") {
            const char* to = nullptr; uint64_t amt = 0;
            for (int i = cmd_idx + 2; i < argc - 1; ++i) {
                if (strcmp(argv[i], "--to") == 0) to = argv[++i];
                if (strcmp(argv[i], "--amount") == 0) amt = (uint64_t)atoll(argv[++i]);
            }
            handle_wallet_send(to, amt);
        }
        else print_usage(argv[0]);
    }
    else if (cmd == "mine") {
        handle_mine();
    }
    else if (cmd == "social") {
        if (cmd_idx + 1 >= argc) { print_usage(argv[0]); return 1; }
        std::string action = argv[cmd_idx + 1];
        const char* user = nullptr;
        for (int i = cmd_idx + 2; i < argc - 1; ++i)
            if (strcmp(argv[i], "--user") == 0) { user = argv[++i]; break; }
        if (action == "follow") handle_social_follow(user);
        else if (action == "unfollow") handle_social_unfollow(user);
        else if (action == "block") handle_social_block(user);
        else if (action == "unblock") handle_social_unblock(user);
        else print_usage(argv[0]);
    }
    else if (cmd == "identity") {
        if (cmd_idx + 1 >= argc) { print_usage(argv[0]); return 1; }
        std::string action = argv[cmd_idx + 1];
        const char* username = nullptr;
        for (int i = cmd_idx + 2; i < argc - 1; ++i)
            if (strcmp(argv[i], "--username") == 0) { username = argv[++i]; break; }
        if (action == "register") handle_identity_register(username);
        else if (action == "lookup") handle_identity_lookup(username);
        else print_usage(argv[0]);
    }
    else if (cmd == "guestbook") {
        if (cmd_idx + 1 >= argc) { print_usage(argv[0]); return 1; }
        std::string action = argv[cmd_idx + 1];
        if (action == "sign") {
            const char* user = nullptr, *msg = nullptr, *name = nullptr;
            for (int i = cmd_idx + 2; i < argc - 1; ++i) {
                if (strcmp(argv[i], "--user") == 0) user = argv[++i];
                if (strcmp(argv[i], "--message") == 0) msg = argv[++i];
                if (strcmp(argv[i], "--name") == 0) name = argv[++i];
            }
            handle_guestbook_sign(user, msg, name);
        }
        else if (action == "show") {
            const char* user = nullptr;
            for (int i = cmd_idx + 2; i < argc - 1; ++i)
                if (strcmp(argv[i], "--user") == 0) { user = argv[++i]; break; }
            handle_guestbook_show(user);
        }
        else print_usage(argv[0]);
    }
    else if (cmd == "video") {
        if (cmd_idx + 1 >= argc) { print_usage(argv[0]); return 1; }
        std::string action = argv[cmd_idx + 1];
        if (action == "upload") {
            const char* video_id = nullptr;
            uint64_t duration = 0;
            uint32_t width = 0, height = 0, codec = 0, chunks = 1;
            uint64_t bitrate = 0;
            for (int i = cmd_idx + 2; i < argc - 1; ++i) {
                if (strcmp(argv[i], "--video-id") == 0 && i + 1 < argc) video_id = argv[++i];
                else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) duration = (uint64_t)atoll(argv[++i]);
                else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) width = (uint32_t)atoi(argv[++i]);
                else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) height = (uint32_t)atoi(argv[++i]);
                else if (strcmp(argv[i], "--codec") == 0 && i + 1 < argc) codec = (uint32_t)atoi(argv[++i]);
                else if (strcmp(argv[i], "--bitrate") == 0 && i + 1 < argc) bitrate = (uint64_t)atoll(argv[++i]);
                else if (strcmp(argv[i], "--chunks") == 0 && i + 1 < argc) chunks = (uint32_t)atoi(argv[++i]);
            }
            handle_video_upload(video_id, duration, width, height, codec, bitrate, chunks);
        }
        else if (action == "manifest") handle_video_manifest();
        else print_usage(argv[0]);
    }
    else if (cmd == "network") {
        if (cmd_idx + 1 >= argc) { print_usage(argv[0]); return 1; }
        if (strcmp(argv[cmd_idx + 1], "peers") == 0) handle_network_peers();
        else print_usage(argv[0]);
    }
    else if (cmd == "status") {
        handle_status();
    }
    else {
        print_usage(argv[0]);
    }

    if (g_state.node) { delete g_state.node; g_state.node = nullptr; }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
