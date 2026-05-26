#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "myc_profile_validation.hpp"

enum WidgetType : uint8_t {
    kWWidgetBio = 0,
    kWidgetLinks = 1,
    kWidgetGuestbook = 2,
    kWidgetRecentPosts = 3,
    kWidgetFollowers = 4,
    kWidgetFollowing = 5,
    kWidgetCustomHTML = 6,
    kWidgetMedia = 7,
    kWidgetStats = 8,
};

static inline const char* widget_type_name(WidgetType w) {
    switch (w) {
        case kWWidgetBio: return "Bio";
        case kWidgetLinks: return "Links";
        case kWidgetGuestbook: return "Guestbook";
        case kWidgetRecentPosts: return "Recent Posts";
        case kWidgetFollowers: return "Followers";
        case kWidgetFollowing: return "Following";
        case kWidgetCustomHTML: return "Custom HTML";
        case kWidgetMedia: return "Media";
        case kWidgetStats: return "Stats";
        default: return "Unknown";
    }
}

enum Visibility : uint8_t { kVisibilityPublic = 0, kVisibilityFollowers = 1, kVisibilityPrivate = 2 };

struct Widget {
    WidgetType widget_type;
    Visibility visibility = kVisibilityPublic;
    std::unordered_map<std::string, std::string> config;
};

struct Section {
    std::string id;
    WidgetType widget_type;
    Visibility visibility = kVisibilityPublic;
    std::vector<std::string> widget_order;
    std::string title;
};

struct Layout {
    std::vector<Section> sections;
    int version = 1;

    static inline Layout default_layout() {
        Layout l;
        l.sections.push_back({"bio", kWWidgetBio, kVisibilityPublic, {}, "About"});
        l.sections.push_back({"links", kWidgetLinks, kVisibilityPublic, {}, "Links"});
        l.sections.push_back({"posts", kWidgetRecentPosts, kVisibilityPublic, {}, "Posts"});
        l.sections.push_back({"guestbook", kWidgetGuestbook, kVisibilityFollowers, {}, "Guestbook"});
        return l;
    }
};

enum LayoutErr : int {
    kLayoutOk = 0,
    kLayoutTooManySections = -1,
    kLayoutSectionExists = -2,
    kLayoutSectionNotFound = -3,
    kLayoutInvalidPosition = -4,
    kLayoutEmpty = -5,
};

static inline const char* layout_strerror(LayoutErr e) {
    switch (e) {
        case kLayoutOk: return "ok";
        case kLayoutTooManySections: return "too many sections";
        case kLayoutSectionExists: return "section exists";
        case kLayoutSectionNotFound: return "section not found";
        case kLayoutInvalidPosition: return "invalid position";
        case kLayoutEmpty: return "empty layout";
        default: return "unknown";
    }
}

static inline LayoutErr layout_add_section(Layout& layout, const std::string& id,
                                            WidgetType wt, Visibility vis,
                                            const std::string& title) {
    if (layout.sections.size() >= 20) return kLayoutTooManySections;
    for (auto& s : layout.sections)
        if (s.id == id) return kLayoutSectionExists;
    layout.sections.push_back({id, wt, vis, {}, title});
    return kLayoutOk;
}

static inline LayoutErr layout_remove_section(Layout& layout, const std::string& id) {
    for (size_t i = 0; i < layout.sections.size(); ++i) {
        if (layout.sections[i].id == id) {
            layout.sections.erase(layout.sections.begin() + i);
            return kLayoutOk;
        }
    }
    return kLayoutSectionNotFound;
}
