#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include "crypto/myc_crypto.hpp"
#include "protocol/myc_protocol.hpp"
#include "myc_profile_theme.hpp"
#include "myc_profile_layout.hpp"
#include "myc_profile_validation.hpp"

struct SocialLink {
    std::string id;
    std::string title;
    std::string url;
    std::string icon;
    bool is_private = false;
};

struct Profile {
    std::string id;
    std::string peer_id;
    std::string username;
    std::string display_name;
    std::string bio;
    std::string avatar_cid;
    std::string banner_cid;
    Layout layout;
    Theme theme;
    std::vector<SocialLink> links;
    int64_t created_at = 0;
    int64_t updated_at = 0;
    std::vector<uint8_t> signature;

    std::string canonical_form() const {
        return id + ":" + peer_id + ":" + username + ":" + display_name + ":"
            + std::to_string(created_at);
    }

    bool verify_signature() const {
        if (signature.size() != 64) return false;
        std::array<uint8_t, 64> sig;
        memcpy(sig.data(), signature.data(), 64);
        auto pk = sha256((const uint8_t*)peer_id.data(), peer_id.size());
        auto canon = canonical_form();
        return ed25519_verify((const uint8_t*)canon.data(), canon.size(), sig, pk);
    }
};

struct ProfileBuilder {
    Profile profile;

    static inline ProfileBuilder create(const std::string& peer_id, const std::string& display_name) {
        ProfileBuilder b;
        std::array<uint8_t, 32> id_buf;
        random_bytes(id_buf.data(), 32);
        b.profile.id = base64_encode(id_buf.data(), 16);
        b.profile.peer_id = peer_id;
        b.profile.display_name = display_name;
        b.profile.created_at = ProtocolMessage{}.now_sec();
        b.profile.updated_at = b.profile.created_at;
        b.profile.layout = Layout::default_layout();
        b.profile.theme = Theme::from_preset(kThemeDefault);
        return b;
    }

    ProfileBuilder& with_username(const std::string& u) {
        if (validate_username(u) == kProfileValOk) profile.username = u;
        return *this;
    }

    ProfileBuilder& with_bio(const std::string& b) {
        if (validate_bio(b) == kProfileValOk) profile.bio = b;
        return *this;
    }

    ProfileBuilder& with_avatar(const std::string& cid) {
        profile.avatar_cid = cid;
        return *this;
    }

    ProfileBuilder& with_banner(const std::string& cid) {
        profile.banner_cid = cid;
        return *this;
    }

    ProfileBuilder& with_link(const std::string& title, const std::string& url) {
        if (validate_link_title(title) != kProfileValOk) return *this;
        if (validate_url(url) != kProfileValOk) return *this;
        if (profile.links.size() >= 20) return *this;
        std::array<uint8_t, 32> id_buf;
        random_bytes(id_buf.data(), 32);
        SocialLink link;
        link.id = base64_encode(id_buf.data(), 8);
        link.title = title;
        link.url = url;
        profile.links.push_back(link);
        return *this;
    }

    ProfileBuilder& with_theme(ThemePreset p) {
        profile.theme = Theme::from_preset(p);
        return *this;
    }

    ProfileBuilder& add_link(const std::string& title, const std::string& url) {
        if (validate_link_title(title) != kProfileValOk) return *this;
        if (validate_url(url) != kProfileValOk) return *this;
        if (profile.links.size() >= 20) return *this;
        std::array<uint8_t, 32> id_buf;
        random_bytes(id_buf.data(), 32);
        SocialLink link;
        link.id = base64_encode(id_buf.data(), 8);
        link.title = title;
        link.url = url;
        profile.links.push_back(link);
        return *this;
    }

    ProfileBuilder& sign(const KeyPair& keypair) {
        auto canon = profile.canonical_form();
        auto sig = ed25519_sign((const uint8_t*)canon.data(), canon.size(), keypair.sk.scalar);
        profile.signature.assign(sig.begin(), sig.end());
        profile.updated_at = ProtocolMessage{}.now_sec();
        return *this;
    }

    Profile build() { return profile; }
};

static inline std::string compute_profile_cid(const Profile& p) {
    auto canon = p.canonical_form();
    auto h = sha256((const uint8_t*)canon.data(), canon.size());
    return "Qm" + base64_encode(h.data(), 16).substr(0, 44);
}
