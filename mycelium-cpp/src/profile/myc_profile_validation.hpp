#pragma once
#include <string>
#include <cstdint>
#include <cctype>
#include <algorithm>
#include "crypto/myc_crypto.hpp"

enum ProfileValErr : int {
    kProfileValOk = 0,
    kProfileValEmpty = -1,
    kProfileValTooShort = -2,
    kProfileValTooLong = -3,
    kProfileValInvalidChars = -4,
    kProfileValNumericStart = -5,
    kProfileValReserved = -6,
    kProfileValInvalidUrl = -7,
    kProfileValPiiDetected = -8,
};

static inline const char* profile_val_strerror(ProfileValErr e) {
    switch (e) {
        case kProfileValOk: return "ok";
        case kProfileValEmpty: return "empty value";
        case kProfileValTooShort: return "too short";
        case kProfileValTooLong: return "too long";
        case kProfileValInvalidChars: return "invalid characters";
        case kProfileValNumericStart: return "cannot start with number";
        case kProfileValReserved: return "reserved name";
        case kProfileValInvalidUrl: return "invalid URL";
        case kProfileValPiiDetected: return "prohibited content";
        default: return "unknown";
    }
}

static inline ProfileValErr validate_username(const std::string& username) {
    auto s = username;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    if (s.empty()) return kProfileValEmpty;
    if (s.size() < 3 || s.size() > 30) return kProfileValInvalidChars;
    for (char c : s)
        if (!isalnum(c) && c != '_' && c != '-') return kProfileValInvalidChars;
    if (isdigit(s[0])) return kProfileValNumericStart;

    static const char* reserved[] = {
        "admin", "root", "system", "moderator", "mod", "support",
        "help", "api", "www", "mail", "ftp", "null", "undefined",
        "true", "false", "mycelium", "official", "bot", "test",
    };
    for (auto r : reserved)
        if (s == r) return kProfileValReserved;
    return kProfileValOk;
}

static inline ProfileValErr validate_url(const std::string& url) {
    if (url.empty()) return kProfileValEmpty;
    if (url.size() > 2048) return kProfileValTooLong;
    if (url.find("://") == std::string::npos) return kProfileValInvalidUrl;
    return kProfileValOk;
}

static inline ProfileValErr validate_bio(const std::string& bio) {
    if (bio.size() > 500) return kProfileValTooLong;
    return kProfileValOk;
}

static inline ProfileValErr validate_display_name(const std::string& name) {
    if (name.empty()) return kProfileValEmpty;
    if (name.size() > 50) return kProfileValTooLong;
    return kProfileValOk;
}

static inline ProfileValErr validate_link_title(const std::string& title) {
    if (title.empty()) return kProfileValEmpty;
    if (title.size() > 100) return kProfileValTooLong;
    return kProfileValOk;
}
