#pragma once
#include <string>
#include <cstdint>
#include <array>

struct ColorScheme {
    std::string primary = "#6366f1";
    std::string primary_hover = "#4f46e5";
    std::string secondary = "#ec4899";
    std::string accent = "#06b6d4";
    std::string background = "#0f172a";
    std::string surface = "#1e293b";
    std::string text_primary = "#f1f5f9";
    std::string text_secondary = "#94a3b8";
    std::string border = "#334155";
    std::string error = "#ef4444";
    std::string success = "#22c55e";
};

struct FontSettings {
    std::string body = "system-ui, sans-serif";
    std::string heading = "system-ui, sans-serif";
    std::string mono = "ui-monospace, monospace";
    double size_scale = 1.0;
};

struct SpacingSettings {
    int xs = 4, sm = 8, md = 16, lg = 24, xl = 32;
};

struct BorderRadiusSettings {
    int sm = 4, md = 8, lg = 16;
};

struct AnimationSettings {
    bool enabled = true;
    int duration_ms = 200;
    std::string easing = "ease-in-out";
};

enum ThemePreset : uint8_t {
    kThemeDefault = 0,
    kThemeMidnight = 1,
    kThemeOcean = 2,
    kThemeForest = 3,
    kThemeSunset = 4,
    kThemeMinimal = 5,
    kThemeHacker = 6,
};

static inline const char* theme_preset_name(ThemePreset p) {
    switch (p) {
        case kThemeDefault: return "Default";
        case kThemeMidnight: return "Midnight";
        case kThemeOcean: return "Ocean";
        case kThemeForest: return "Forest";
        case kThemeSunset: return "Sunset";
        case kThemeMinimal: return "Minimal";
        case kThemeHacker: return "Hacker";
        default: return "Unknown";
    }
}

struct Theme {
    ThemePreset preset = kThemeDefault;
    ColorScheme colors;
    FontSettings fonts;
    SpacingSettings spacing;
    BorderRadiusSettings border_radius;
    AnimationSettings animations;
    std::string custom_css;

    static inline Theme from_preset(ThemePreset p) {
        Theme t;
        t.preset = p;
        switch (p) {
            case kThemeMidnight:
                t.colors.background = "#0f172a"; t.colors.primary = "#818cf8";
                break;
            case kThemeOcean:
                t.colors.background = "#0c4a6e"; t.colors.primary = "#22d3ee";
                break;
            case kThemeForest:
                t.colors.background = "#14532d"; t.colors.primary = "#4ade80";
                break;
            case kThemeSunset:
                t.colors.background = "#1e0a3c"; t.colors.primary = "#f472b6";
                break;
            case kThemeMinimal:
                t.colors.background = "#ffffff"; t.colors.text_primary = "#0f172a";
                t.colors.text_secondary = "#475569"; t.colors.border = "#e2e8f0";
                break;
            case kThemeHacker:
                t.colors.background = "#000000"; t.colors.primary = "#00ff00";
                t.colors.text_primary = "#00ff00"; t.colors.text_secondary = "#00aa00";
                t.colors.surface = "#0a0a0a"; t.colors.border = "#00ff00";
                break;
            default: break;
        }
        return t;
    }
};
