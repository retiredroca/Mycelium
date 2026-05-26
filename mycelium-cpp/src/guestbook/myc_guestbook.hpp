#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include "crypto/myc_crypto.hpp"
#include "protocol/myc_protocol.hpp"

static inline constexpr size_t kMaxEntryLength = 1000;
static inline constexpr size_t kMaxNameLength = 50;

enum EntryStatus : uint8_t { kEntryPending = 0, kEntryApproved = 1, kEntryRejected = 2 };

struct GuestbookEntry {
    std::string id;
    std::string author_key;
    std::string author_name;
    std::string content;
    int64_t timestamp = 0;
    EntryStatus status = kEntryPending;
    std::vector<uint8_t> signature;
    std::string reply_to;

    static inline GuestbookEntry create(
        const std::string& author_key,
        const std::string& author_name,
        const std::string& content)
    {
        GuestbookEntry e;
        std::array<uint8_t, 32> id_buf;
        random_bytes(id_buf.data(), 32);
        e.id = base64_encode(id_buf.data(), 16);
        e.author_key = author_key;
        e.author_name = author_name.substr(0, kMaxNameLength);
        e.content = content.substr(0, kMaxEntryLength);
        e.timestamp = ProtocolMessage{}.now_sec();
        return e;
    }
};

enum EntryErr : int {
    kEntryOk = 0,
    kEntryNameTooLong = -1,
    kEntryContentTooLong = -2,
    kEntryEmpty = -3,
    kEntryInvalidKey = -4,
    kEntryNotFound = -5,
};

static inline const char* entry_strerror(EntryErr e) {
    switch (e) {
        case kEntryOk: return "ok";
        case kEntryNameTooLong: return "name too long";
        case kEntryContentTooLong: return "content too long";
        case kEntryEmpty: return "empty content";
        case kEntryInvalidKey: return "invalid author key";
        case kEntryNotFound: return "entry not found";
        default: return "unknown";
    }
}

struct PendingQueue {
    std::deque<GuestbookEntry> entries;
    std::unordered_map<std::string, std::vector<std::string>> by_author;
    std::unordered_map<std::string, size_t> by_id;

    void push(const GuestbookEntry& e) {
        by_id[e.id] = entries.size();
        by_author[e.author_key].push_back(e.id);
        entries.push_back(e);
    }

    bool remove(const std::string& id) {
        auto it = by_id.find(id);
        if (it == by_id.end()) return false;
        by_id.erase(it);
        for (auto& [k, v] : by_author) {
            auto vit = std::find(v.begin(), v.end(), id);
            if (vit != v.end()) { v.erase(vit); break; }
        }
        for (size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].id == id) {
                entries.erase(entries.begin() + i);
                break;
            }
        }
        return true;
    }

    size_t size() const { return entries.size(); }
    bool empty() const { return entries.empty(); }
};

enum ApprovalAction : uint8_t { kActionApprove = 0, kActionReject = 1, kActionBlock = 2, kActionReport = 3 };

struct ApprovalRecord {
    std::string entry_id;
    ApprovalAction action;
    std::string decided_by;
    int64_t decided_at;
    std::string reason;
};

enum AutoApprovePolicy : uint8_t {
    kAutoNever = 0,
    kAutoAlways = 1,
    kAutoTrustedOnly = 2,
    kAutoTrustedThreshold = 3,
};

struct ApprovalManager {
    std::unordered_map<std::string, std::vector<ApprovalRecord>> history;
    std::unordered_map<std::string, std::vector<ApprovalAction>> author_actions;
    std::unordered_set<std::string> blocked_authors;
    AutoApprovePolicy policy = kAutoNever;
    int trusted_threshold = 0;

    void record_action(const std::string& entry_id, ApprovalAction action,
                       const std::string& decided_by, const std::string& reason = "") {
        ApprovalRecord rec;
        rec.entry_id = entry_id;
        rec.action = action;
        rec.decided_by = decided_by;
        rec.decided_at = ProtocolMessage{}.now_sec();
        rec.reason = reason;
        history[entry_id].push_back(rec);
        author_actions[decided_by].push_back(action);
    }

    bool is_blocked(const std::string& author_key) const {
        return blocked_authors.count(author_key);
    }

    void block_author(const std::string& author_key) {
        blocked_authors.insert(author_key);
    }

    size_t action_count(const std::string& author_key, ApprovalAction action) const {
        auto it = author_actions.find(author_key);
        if (it == author_actions.end()) return 0;
        size_t n = 0;
        for (auto a : it->second)
            if (a == action) ++n;
        return n;
    }
};

enum GuestbookErr : int {
    kGbOk = 0,
    kGbEntryNotFound = -1,
    kGbInvalidSignature = -2,
    kGbContentTooLong = -3,
    kGbClosed = -4,
    kGbAuthorBlocked = -5,
};

static inline const char* gb_strerror(GuestbookErr e) {
    switch (e) {
        case kGbOk: return "ok";
        case kGbEntryNotFound: return "entry not found";
        case kGbInvalidSignature: return "invalid signature";
        case kGbContentTooLong: return "content too long";
        case kGbClosed: return "guestbook closed";
        case kGbAuthorBlocked: return "author blocked";
        default: return "unknown";
    }
}

struct Guestbook {
    std::string owner_peer_id;
    std::unordered_map<std::string, GuestbookEntry> entries;
    PendingQueue pending;
    ApprovalManager approval;

    GuestbookErr sign(const GuestbookEntry& entry) {
        if (approval.is_blocked(entry.author_key)) return kGbAuthorBlocked;
        if (entry.content.size() > kMaxEntryLength) return kGbContentTooLong;
        pending.push(entry);
        return kGbOk;
    }

    GuestbookErr approve(const std::string& entry_id, const std::string& admin) {
        if (!pending.by_id.count(entry_id)) return kGbEntryNotFound;
        GuestbookEntry e;
        for (auto& pe : pending.entries) {
            if (pe.id == entry_id) { e = pe; break; }
        }
        e.status = kEntryApproved;
        entries[e.id] = e;
        pending.remove(entry_id);
        approval.record_action(entry_id, kActionApprove, admin);
        return kGbOk;
    }

    GuestbookErr reject(const std::string& entry_id, const std::string& admin) {
        if (!pending.by_id.count(entry_id)) return kGbEntryNotFound;
        for (auto& pe : pending.entries) {
            if (pe.id == entry_id) {
                pe.status = kEntryRejected;
                break;
            }
        }
        approval.record_action(entry_id, kActionReject, admin);
        pending.remove(entry_id);
        return kGbOk;
    }
};
