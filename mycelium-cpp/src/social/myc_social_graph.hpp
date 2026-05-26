#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "crypto/myc_crypto.hpp"
#include "protocol/myc_protocol.hpp"

struct FollowState {
    std::string peer_id;
    bool approved = false;
    int64_t followed_at = 0;
    int64_t approved_at = 0;
};

struct FollowRequest {
    std::string id;
    std::string from_peer_id;
    std::string to_peer_id;
    int64_t created_at = 0;
    std::string message;
};

enum Relationship : uint8_t {
    kRelNone = 0,
    kRelFollowing = 1,
    kRelFollower = 2,
    kRelMutual = 3,
    kRelBlocked = 4,
};

enum FollowErr : int {
    kFollowOk = 0,
    kFollowSelf = -1,
    kFollowAlready = -2,
    kFollowNotFollowing = -3,
    kFollowFollowerNotFound = -4,
    kFollowUserBlocked = -5,
    kFollowAlreadyBlocked = -6,
    kFollowNotBlocked = -7,
    kFollowRequestNotFound = -8,
};

static inline const char* follow_strerror(FollowErr e) {
    switch (e) {
        case kFollowOk: return "ok";
        case kFollowSelf: return "cannot follow yourself";
        case kFollowAlready: return "already following";
        case kFollowNotFollowing: return "not following";
        case kFollowFollowerNotFound: return "follower not found";
        case kFollowUserBlocked: return "user is blocked";
        case kFollowAlreadyBlocked: return "already blocked";
        case kFollowNotBlocked: return "not blocked";
        case kFollowRequestNotFound: return "request not found";
        default: return "unknown";
    }
}

struct SocialGraph {
    std::string peer_id;
    std::unordered_map<std::string, FollowState> followers;
    std::unordered_set<std::string> following;
    std::unordered_set<std::string> blocked;
    std::unordered_map<std::string, FollowRequest> pending_requests;
    int64_t created_at = 0;
    int64_t updated_at = 0;

    static inline SocialGraph create(const uint8_t* peer_key, size_t key_len) {
        SocialGraph g;
        g.peer_id = base64_encode(peer_key, key_len);
        g.created_at = ProtocolMessage{}.now_sec();
        g.updated_at = g.created_at;
        return g;
    }

    bool is_self(const std::string& key) const { return peer_id == key; }

    FollowErr follow(const std::string& target) {
        if (is_self(target)) return kFollowSelf;
        if (blocked.count(target)) return kFollowUserBlocked;
        if (following.count(target)) return kFollowAlready;
        following.insert(target);
        updated_at = ProtocolMessage{}.now_sec();
        return kFollowOk;
    }

    FollowErr unfollow(const std::string& target) {
        if (!following.count(target)) return kFollowNotFollowing;
        following.erase(target);
        updated_at = ProtocolMessage{}.now_sec();
        return kFollowOk;
    }

    FollowErr send_follow_request(const std::string& target) {
        if (is_self(target)) return kFollowSelf;
        if (blocked.count(target)) return kFollowUserBlocked;
        if (following.count(target)) return kFollowAlready;
        FollowRequest req;
        std::array<uint8_t, 32> id_buf;
        random_bytes(id_buf.data(), 32);
        req.id = base64_encode(id_buf.data(), 16);
        req.from_peer_id = peer_id;
        req.to_peer_id = target;
        req.created_at = ProtocolMessage{}.now_sec();
        pending_requests[req.id] = req;
        updated_at = req.created_at;
        return kFollowOk;
    }

    FollowErr add_follower(const std::string& follower_id, bool approved) {
        if (is_self(follower_id)) return kFollowSelf;
        FollowState st;
        st.peer_id = follower_id;
        st.approved = approved;
        st.followed_at = ProtocolMessage{}.now_sec();
        if (approved) st.approved_at = st.followed_at;
        followers[follower_id] = st;
        updated_at = st.followed_at;
        return kFollowOk;
    }

    FollowErr approve_follower(const std::string& follower_id) {
        auto it = followers.find(follower_id);
        if (it == followers.end()) return kFollowFollowerNotFound;
        it->second.approved = true;
        it->second.approved_at = ProtocolMessage{}.now_sec();
        updated_at = it->second.approved_at;
        return kFollowOk;
    }

    FollowErr reject_follower(const std::string& follower_id) {
        if (!followers.count(follower_id)) return kFollowFollowerNotFound;
        followers.erase(follower_id);
        updated_at = ProtocolMessage{}.now_sec();
        return kFollowOk;
    }

    FollowErr remove_follower(const std::string& follower_id) {
        if (!followers.count(follower_id)) return kFollowFollowerNotFound;
        followers.erase(follower_id);
        updated_at = ProtocolMessage{}.now_sec();
        return kFollowOk;
    }

    FollowErr block(const std::string& target) {
        if (is_self(target)) return kFollowSelf;
        if (blocked.count(target)) return kFollowAlreadyBlocked;
        following.erase(target);
        followers.erase(target);
        blocked.insert(target);
        updated_at = ProtocolMessage{}.now_sec();
        return kFollowOk;
    }

    FollowErr unblock(const std::string& target) {
        if (!blocked.count(target)) return kFollowNotBlocked;
        blocked.erase(target);
        updated_at = ProtocolMessage{}.now_sec();
        return kFollowOk;
    }

    bool is_following(const std::string& target) const { return following.count(target); }
    bool is_follower(const std::string& peer) const { return followers.count(peer); }
    bool is_mutual(const std::string& peer) const { return is_following(peer) && is_follower(peer); }
    bool is_blocked(const std::string& target) const { return blocked.count(target); }

    size_t followers_count() const {
        size_t n = 0;
        for (auto& [_, st] : followers)
            if (st.approved) ++n;
        return n;
    }

    size_t following_count() const { return following.size(); }

    Relationship relationship(const std::string& peer) const {
        if (is_blocked(peer)) return kRelBlocked;
        if (is_mutual(peer)) return kRelMutual;
        if (is_following(peer)) return kRelFollowing;
        if (is_follower(peer)) return kRelFollower;
        return kRelNone;
    }
};
