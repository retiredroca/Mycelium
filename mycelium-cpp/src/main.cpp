#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>
#include <memory>
#include "crypto/myc_crypto.hpp"
#include "protocol/myc_protocol.hpp"
#include "post/myc_post.hpp"
#include "storage/myc_storage.hpp"
#include "token/myc_token.hpp"
#include "social/myc_social_graph.hpp"
#include "profile/myc_profile.hpp"
#include "guestbook/myc_guestbook.hpp"
#include "identity/myc_identity.hpp"
#include "p2p/myc_p2p.hpp"

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32")
#endif

// ============================================================
// Helper: print box
// ============================================================
static inline void print_box_header(const char* title) {
    printf("\n\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\n");
    printf("\xBA  %s\n", title);
    printf("\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\n");
}

// ============================================================
// Global state (single node)
// ============================================================
struct AppState {
    Profile my_profile;
    SocialGraph social;
    Guestbook guestbook;
    UsernameRegistry identity;
    Wallet wallet;
    Tokenomics tokenomics;
    MyceliumNode* node = nullptr;
    bool node_running = false;
};

static AppState g_state;

// ============================================================
// Command handlers
// ============================================================
static inline void handle_start(const char* listen, const char* bootstrap) {
    P2pConfig cfg;
    cfg.listen_addresses.push_back(listen ? listen : "/ip4/0.0.0.0/tcp/0");
    if (bootstrap) cfg.bootstrap_nodes.push_back(bootstrap);
    cfg.capabilities.push_back({kCapFull});

    g_state.node = new MyceliumNode(MyceliumNode::create(cfg));
    g_state.node_running = true;

    print_box_header("NODE STARTED");
    printf("\xBA  Peer ID: %s\n", g_state.node->local_peer_id().c_str());
    printf("\xBA  Listening: %s\n", listen ? listen : "default");
    printf("\xBA  Peers: %zu\n", g_state.node->peer_count());
    printf("\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\n");
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

    print_box_header("PROFILE CREATED");
    printf("\xBA  Display Name: %s\n", g_state.my_profile.display_name.c_str());
    if (!g_state.my_profile.username.empty())
        printf("\xBA  Username: @%s\n", g_state.my_profile.username.c_str());
    auto cid = compute_profile_cid(g_state.my_profile);
    printf("\xBA  CID: %s\n", cid.c_str());
    printf("\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\n");
}

static inline void handle_profile_show(const char* user) {
    if (g_state.my_profile.peer_id.empty()) {
        printf("No profile. Use 'profile create' first.\n");
        return;
    }
    print_box_header("PROFILE VIEW");
    printf("\xBA  Peer ID: %s\n", g_state.my_profile.peer_id.c_str());
    printf("\xBA  Username: @%s\n", g_state.my_profile.username.c_str());
    printf("\xBA  Display Name: %s\n", g_state.my_profile.display_name.c_str());
    if (!g_state.my_profile.bio.empty())
        printf("\xBA  Bio: %s\n", g_state.my_profile.bio.c_str());
    printf("\xBA  Links: %zu\n", g_state.my_profile.links.size());
    printf("\xBA  Theme: %s\n", theme_preset_name(g_state.my_profile.theme.preset));
    printf("\xBA  Sections: %zu\n", g_state.my_profile.layout.sections.size());
    auto cid = compute_profile_cid(g_state.my_profile);
    printf("\xBA  CID: %s\n", cid.c_str());
    printf("\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\n");
}

static inline void handle_profile_update(const char* bio, const char* display_name) {
    if (bio) { g_state.my_profile.bio = bio; printf("Bio updated.\n"); }
    if (display_name) { g_state.my_profile.display_name = display_name; printf("Display name updated.\n"); }
}

static inline void handle_profile_avatar(const char* cid) {
    g_state.my_profile.avatar_cid = cid;
    printf("Avatar set to: %s\n", cid);
}

static inline void handle_profile_banner(const char* cid) {
    g_state.my_profile.banner_cid = cid;
    printf("Banner set to: %s\n", cid);
}

static inline void handle_profile_link(const char* title, const char* url) {
    SocialLink link;
    std::array<uint8_t, 32> id_buf;
    random_bytes(id_buf.data(), 32);
    link.id = base64_encode(id_buf.data(), 8);
    link.title = title;
    link.url = url;
    g_state.my_profile.links.push_back(link);
    printf("Link added: %s -> %s\n", title, url);
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
    printf("Theme set to: %s\n", theme_preset_name(p));
}

static inline void handle_post_create(const char* content) {
    auto kp = x25519_keygen();
    auto post = Post::create(
        base64_encode(kp.pk.point.data(), 16),
        content ? content : "",
        kp);
    printf("Post created: %s\n", post.id.c_str());
    printf("  Author: %s\n", post.author.c_str());
    printf("  TTL: %llu seconds\n", (unsigned long long)post.ttl_seconds);

    if (g_state.node) g_state.node->store_post(post);
}

static inline void handle_feed(int limit) {
    (void)limit;
    print_box_header("RECENT POSTS");
    printf("\xBA  No posts (connect to network to sync)\n");
    printf("\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\n");
}

static inline void handle_wallet_create() {
    std::array<uint8_t, 32> pk;
    random_bytes(pk.data(), 32);
    g_state.wallet = Wallet{};
    memcpy(g_state.wallet.public_key.data(), pk.data(), 32);
    g_state.wallet.receive(1'000'000);
    auto addr = base64_encode(pk.data(), 16);
    printf("Wallet created: SOVEX%s\n", addr.c_str());
}

static inline void handle_wallet_balance() {
    print_box_header("WALLET BALANCE");
    printf("\xBA  Balance:  %llu MYCELIUM\n", (unsigned long long)g_state.wallet.balance);
    printf("\xBA  Staked:   %llu MYCELIUM\n", (unsigned long long)g_state.wallet.staked_balance);
    printf("\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\n");
}

static inline void handle_wallet_stake(uint64_t amount) {
    auto err = g_state.wallet.stake(amount);
    if (err == kTokenOk)
        printf("Staked %llu tokens.\n", (unsigned long long)amount);
    else
        printf("Error: %s\n", token_strerror(err));
}

static inline void handle_wallet_unstake(uint64_t amount) {
    auto err = g_state.wallet.unstake(amount);
    if (err == kTokenOk)
        printf("Unstaked %llu tokens.\n", (unsigned long long)amount);
    else
        printf("Error: %s\n", token_strerror(err));
}

static inline void handle_wallet_send(const char* to, uint64_t amount) {
    auto err = g_state.wallet.send(amount);
    if (err == kTokenOk)
        printf("Sent %llu to %s.\n", (unsigned long long)amount, to ? to : "unknown");
    else
        printf("Error: %s\n", token_strerror(err));
}

static inline void handle_social_follow(const char* user) {
    if (!user) return;
    auto err = g_state.social.follow(user);
    if (err == kFollowOk)
        printf("Following %s.\n", user);
    else
        printf("Error: %s\n", follow_strerror(err));
}

static inline void handle_social_unfollow(const char* user) {
    if (!user) return;
    auto err = g_state.social.unfollow(user);
    if (err == kFollowOk)
        printf("Unfollowed %s.\n", user);
    else
        printf("Error: %s\n", follow_strerror(err));
}

static inline void handle_social_block(const char* user) {
    if (!user) return;
    auto err = g_state.social.block(user);
    if (err == kFollowOk)
        printf("Blocked %s.\n", user);
    else
        printf("Error: %s\n", follow_strerror(err));
}

static inline void handle_social_unblock(const char* user) {
    if (!user) return;
    auto err = g_state.social.unblock(user);
    if (err == kFollowOk)
        printf("Unblocked %s.\n", user);
    else
        printf("Error: %s\n", follow_strerror(err));
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
        printf("Registered @%s.\n", username);
    else
        printf("Error: %s\n", identity_strerror(err));
}

static inline void handle_identity_lookup(const char* username) {
    if (!username) return;
    auto* rec = g_state.identity.lookup(username);
    if (rec) {
        printf("@%s -> %s\n", username, base64_encode(rec->peer_id.data(), rec->peer_id.size()).c_str());
    } else {
        printf("@%s not found.\n", username);
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
        printf("Signed %s's guestbook.\n", user ? user : "?");
    else
        printf("Error: %s\n", gb_strerror(err));
}

static inline void handle_guestbook_show(const char* user) {
    print_box_header("GUESTBOOK");
    printf("\xBA  Entries for %s:\n", user ? user : "?");
    for (auto& [id, entry] : g_state.guestbook.entries) {
        (void)id;
        printf("\xBA    %s: %s\n", entry.author_name.c_str(), entry.content.c_str());
    }
    printf("\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\n");
}

static inline void handle_network_peers() {
    print_box_header("CONNECTED PEERS");
    if (g_state.node_running && g_state.node) {
        for (auto& [id, info] : g_state.node->peer_table.get_all_peers()) {
            printf("\xBA  %s (last seen: %lld)\n", id.c_str(), (long long)info.last_seen);
        }
    } else {
        printf("\xBA  No peers (start node first)\n");
    }
    printf("\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\n");
}

static inline void handle_status() {
    print_box_header("MYCELIUM PROTOCOL STATUS");
    printf("\xBA  Version: 0.1.0\n");
    printf("\xBA  Network: %s\n", g_state.node_running ? "Online" : "Offline");
    if (g_state.node_running && g_state.node)
        printf("\xBA  Peers: %zu\n", g_state.node->peer_count());
    printf("\xBA  Supply: %llu MYCELIUM\n", (unsigned long long)g_state.tokenomics.total_supply);
    printf("\xBA  Epoch: %llu\n", (unsigned long long)g_state.tokenomics.current_epoch);
    printf("\xBA  Profile: %s\n", g_state.my_profile.peer_id.empty() ? "None" : "Created");
    printf("\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\n");
}

// ============================================================
// Simple arg parser
// ============================================================
static inline void print_usage(const char* prog) {
    printf("Mycelium Protocol v0.1.0\n");
    printf("Usage: %s <command> [args]\n\n", prog);
    printf("Commands:\n");
    printf("  start [--listen ADDR] [--bootstrap ADDR]\n");
    printf("  profile create --display-name NAME [--username USER]\n");
    printf("  profile show [--user USER]\n");
    printf("  profile update [--bio TEXT] [--display-name NAME]\n");
    printf("  profile avatar --cid CID\n");
    printf("  profile banner --cid CID\n");
    printf("  profile link --title T --url U\n");
    printf("  profile theme [--preset P] [--primary COLOR]\n");
    printf("  post --content TEXT\n");
    printf("  feed [--limit N]\n");
    printf("  wallet create\n");
    printf("  wallet balance\n");
    printf("  wallet stake --amount N\n");
    printf("  wallet unstake --amount N\n");
    printf("  wallet send --to ADDR --amount N\n");
    printf("  social follow --user U\n");
    printf("  social unfollow --user U\n");
    printf("  social block --user U\n");
    printf("  social unblock --user U\n");
    printf("  identity register --username U\n");
    printf("  identity lookup --username U\n");
    printf("  guestbook sign --user U --message M [--name N]\n");
    printf("  guestbook show --user U\n");
    printf("  network peers\n");
    printf("  status\n");
    printf("  help\n");
}

int main(int argc, char** argv) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    if (argc < 2) { print_usage(argv[0]); return 1; }

    std::string cmd = argv[1];

    if (cmd == "help" || cmd == "--help") { print_usage(argv[0]); return 0; }

    if (cmd == "start") {
        const char* listen = nullptr;
        const char* bootstrap = nullptr;
        for (int i = 2; i < argc - 1; ++i) {
            if (strcmp(argv[i], "--listen") == 0) listen = argv[++i];
            if (strcmp(argv[i], "--bootstrap") == 0) bootstrap = argv[++i];
        }
        handle_start(listen, bootstrap);
    }
    else if (cmd == "profile" && argc > 2) {
        std::string action = argv[2];
        if (action == "create") {
            const char* dn = nullptr, *un = nullptr;
            for (int i = 3; i < argc - 1; ++i) {
                if (strcmp(argv[i], "--display-name") == 0) dn = argv[++i];
                if (strcmp(argv[i], "--username") == 0) un = argv[++i];
            }
            handle_profile_create(dn, un);
        }
        else if (action == "show") {
            const char* user = nullptr;
            for (int i = 3; i < argc - 1; ++i)
                if (strcmp(argv[i], "--user") == 0) user = argv[++i];
            handle_profile_show(user);
        }
        else if (action == "update") {
            const char* bio = nullptr, *dn = nullptr;
            for (int i = 3; i < argc - 1; ++i) {
                if (strcmp(argv[i], "--bio") == 0) bio = argv[++i];
                if (strcmp(argv[i], "--display-name") == 0) dn = argv[++i];
            }
            handle_profile_update(bio, dn);
        }
        else if (action == "avatar") {
            for (int i = 3; i < argc - 1; ++i)
                if (strcmp(argv[i], "--cid") == 0) { handle_profile_avatar(argv[++i]); break; }
        }
        else if (action == "banner") {
            for (int i = 3; i < argc - 1; ++i)
                if (strcmp(argv[i], "--cid") == 0) { handle_profile_banner(argv[++i]); break; }
        }
        else if (action == "link") {
            const char* t = nullptr, *u = nullptr;
            for (int i = 3; i < argc - 1; ++i) {
                if (strcmp(argv[i], "--title") == 0) t = argv[++i];
                if (strcmp(argv[i], "--url") == 0) u = argv[++i];
            }
            handle_profile_link(t, u);
        }
        else if (action == "theme") {
            const char* preset = nullptr, *primary = nullptr;
            for (int i = 3; i < argc - 1; ++i) {
                if (strcmp(argv[i], "--preset") == 0) preset = argv[++i];
                if (strcmp(argv[i], "--primary") == 0) primary = argv[++i];
            }
            handle_profile_theme(preset, primary);
        }
        else print_usage(argv[0]);
    }
    else if (cmd == "post") {
        const char* content = nullptr;
        for (int i = 2; i < argc - 1; ++i)
            if (strcmp(argv[i], "--content") == 0) content = argv[++i];
        handle_post_create(content);
    }
    else if (cmd == "feed") {
        int limit = 50;
        for (int i = 2; i < argc - 1; ++i)
            if (strcmp(argv[i], "--limit") == 0) limit = atoi(argv[++i]);
        handle_feed(limit);
    }
    else if (cmd == "wallet" && argc > 2) {
        std::string action = argv[2];
        if (action == "create") handle_wallet_create();
        else if (action == "balance") handle_wallet_balance();
        else if (action == "stake") {
            for (int i = 3; i < argc - 1; ++i)
                if (strcmp(argv[i], "--amount") == 0) { handle_wallet_stake((uint64_t)atoll(argv[++i])); break; }
        }
        else if (action == "unstake") {
            for (int i = 3; i < argc - 1; ++i)
                if (strcmp(argv[i], "--amount") == 0) { handle_wallet_unstake((uint64_t)atoll(argv[++i])); break; }
        }
        else if (action == "send") {
            const char* to = nullptr; uint64_t amt = 0;
            for (int i = 3; i < argc - 1; ++i) {
                if (strcmp(argv[i], "--to") == 0) to = argv[++i];
                if (strcmp(argv[i], "--amount") == 0) amt = (uint64_t)atoll(argv[++i]);
            }
            handle_wallet_send(to, amt);
        }
        else print_usage(argv[0]);
    }
    else if (cmd == "social" && argc > 2) {
        std::string action = argv[2];
        const char* user = nullptr;
        for (int i = 3; i < argc - 1; ++i)
            if (strcmp(argv[i], "--user") == 0) { user = argv[++i]; break; }
        if (action == "follow") handle_social_follow(user);
        else if (action == "unfollow") handle_social_unfollow(user);
        else if (action == "block") handle_social_block(user);
        else if (action == "unblock") handle_social_unblock(user);
        else print_usage(argv[0]);
    }
    else if (cmd == "identity" && argc > 2) {
        std::string action = argv[2];
        const char* username = nullptr;
        for (int i = 3; i < argc - 1; ++i)
            if (strcmp(argv[i], "--username") == 0) { username = argv[++i]; break; }
        if (action == "register") handle_identity_register(username);
        else if (action == "lookup") handle_identity_lookup(username);
        else print_usage(argv[0]);
    }
    else if (cmd == "guestbook" && argc > 2) {
        std::string action = argv[2];
        if (action == "sign") {
            const char* user = nullptr, *msg = nullptr, *name = nullptr;
            for (int i = 3; i < argc - 1; ++i) {
                if (strcmp(argv[i], "--user") == 0) user = argv[++i];
                if (strcmp(argv[i], "--message") == 0) msg = argv[++i];
                if (strcmp(argv[i], "--name") == 0) name = argv[++i];
            }
            handle_guestbook_sign(user, msg, name);
        }
        else if (action == "show") {
            const char* user = nullptr;
            for (int i = 3; i < argc - 1; ++i)
                if (strcmp(argv[i], "--user") == 0) { user = argv[++i]; break; }
            handle_guestbook_show(user);
        }
        else print_usage(argv[0]);
    }
    else if (cmd == "network" && argc > 2) {
        if (strcmp(argv[2], "peers") == 0) handle_network_peers();
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
