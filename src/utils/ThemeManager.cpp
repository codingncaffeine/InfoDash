#include "utils/ThemeManager.hpp"
#include "utils/Config.hpp"
#include <sstream>

namespace InfoDash {

ThemeManager& ThemeManager::getInstance() {
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager() 
    : currentMode_(ThemeMode::Dark)
    , currentScheme_(ColorScheme::Default)
    , customAccentColor_("#e94560")
    , cssProvider_(nullptr) {
    loadFromConfig();
}

void ThemeManager::setThemeMode(ThemeMode mode) {
    if (currentMode_ != mode) {
        currentMode_ = mode;
        saveToConfig();
        applyTheme();
        notifyThemeChanged();
    }
}

ThemeMode ThemeManager::getThemeMode() const {
    return currentMode_;
}

void ThemeManager::setColorScheme(ColorScheme scheme) {
    if (currentScheme_ != scheme) {
        currentScheme_ = scheme;
        saveToConfig();
        applyTheme();
        notifyThemeChanged();
    }
}

ColorScheme ThemeManager::getColorScheme() const {
    return currentScheme_;
}

void ThemeManager::setCustomAccentColor(const std::string& color) {
    customAccentColor_ = color;
    if (currentScheme_ == ColorScheme::Custom) {
        saveToConfig();
        applyTheme();
        notifyThemeChanged();
    }
}

std::string ThemeManager::getCustomAccentColor() const {
    return customAccentColor_;
}

void ThemeManager::setCustomColors(const ThemeColors& colors) {
    customColors_ = colors;
    if (currentScheme_ == ColorScheme::Custom) {
        saveToConfig();
        applyTheme();
        notifyThemeChanged();
    }
}

ThemeColors ThemeManager::getCustomColors() const {
    return customColors_;
}

bool ThemeManager::isSystemDarkMode() const {
    // Check GTK's color scheme preference
    GtkSettings* settings = gtk_settings_get_default();
    if (settings) {
        gboolean preferDark = FALSE;
        g_object_get(settings, "gtk-application-prefer-dark-theme", &preferDark, NULL);
        return preferDark;
    }
    return true; // Default to dark
}

Theme ThemeManager::getCurrentTheme() const {
    Theme theme;
    theme.mode = currentMode_;
    theme.scheme = currentScheme_;
    theme.colors = getCurrentColors();
    
    // Set name based on scheme
    switch (currentScheme_) {
        case ColorScheme::Default: theme.name = "Default"; theme.id = "default"; break;
        case ColorScheme::Ocean: theme.name = "Ocean"; theme.id = "ocean"; break;
        case ColorScheme::Forest: theme.name = "Forest"; theme.id = "forest"; break;
        case ColorScheme::Sunset: theme.name = "Sunset"; theme.id = "sunset"; break;
        case ColorScheme::Midnight: theme.name = "Midnight"; theme.id = "midnight"; break;
        case ColorScheme::Nord: theme.name = "Nord"; theme.id = "nord"; break;
        case ColorScheme::Dracula: theme.name = "Dracula"; theme.id = "dracula"; break;
        case ColorScheme::Solarized: theme.name = "Solarized"; theme.id = "solarized"; break;
        case ColorScheme::Rose: theme.name = "Rosé"; theme.id = "rose"; break;
        case ColorScheme::Custom: theme.name = "Custom"; theme.id = "custom"; break;
    }
    
    return theme;
}

ThemeColors ThemeManager::getCurrentColors() const {
    bool isDark = (currentMode_ == ThemeMode::Dark) || 
                  (currentMode_ == ThemeMode::System && isSystemDarkMode());
    
    if (currentScheme_ == ColorScheme::Custom) {
        return customColors_;
    }
    
    return getColorsForScheme(currentScheme_, isDark);
}

ThemeColors ThemeManager::getColorsForScheme(ColorScheme scheme, bool dark) const {
    switch (scheme) {
        case ColorScheme::Default:
            return dark ? getDarkDefaultColors() : getLightDefaultColors();
        case ColorScheme::Ocean:
            return getOceanColors(dark);
        case ColorScheme::Forest:
            return getForestColors(dark);
        case ColorScheme::Sunset:
            return getSunsetColors(dark);
        case ColorScheme::Midnight:
            return getMidnightColors();
        case ColorScheme::Nord:
            return getNordColors(dark);
        case ColorScheme::Dracula:
            return getDraculaColors();
        case ColorScheme::Solarized:
            return getSolarizedColors(dark);
        case ColorScheme::Rose:
            return getRoseColors(dark);
        case ColorScheme::Custom:
            return customColors_;
        default:
            return getDarkDefaultColors();
    }
}

std::vector<Theme> ThemeManager::getAvailableThemes() const {
    std::vector<Theme> themes;
    
    // Add all predefined themes
    themes.push_back({"default-dark", "Default Dark", "Original InfoDash theme", ThemeMode::Dark, ColorScheme::Default, getDarkDefaultColors()});
    themes.push_back({"default-light", "Default Light", "Light version of default theme", ThemeMode::Light, ColorScheme::Default, getLightDefaultColors()});
    themes.push_back({"ocean-dark", "Ocean", "Deep blue with teal accents", ThemeMode::Dark, ColorScheme::Ocean, getOceanColors(true)});
    themes.push_back({"ocean-light", "Ocean Light", "Light ocean theme", ThemeMode::Light, ColorScheme::Ocean, getOceanColors(false)});
    themes.push_back({"forest-dark", "Forest", "Dark green with emerald accents", ThemeMode::Dark, ColorScheme::Forest, getForestColors(true)});
    themes.push_back({"forest-light", "Forest Light", "Light forest theme", ThemeMode::Light, ColorScheme::Forest, getForestColors(false)});
    themes.push_back({"sunset", "Sunset", "Warm orange and purple tones", ThemeMode::Dark, ColorScheme::Sunset, getSunsetColors(true)});
    themes.push_back({"midnight", "Midnight", "Pure dark with purple accents", ThemeMode::Dark, ColorScheme::Midnight, getMidnightColors()});
    themes.push_back({"nord-dark", "Nord", "Arctic, north-bluish color palette", ThemeMode::Dark, ColorScheme::Nord, getNordColors(true)});
    themes.push_back({"nord-light", "Nord Light", "Light Nord theme", ThemeMode::Light, ColorScheme::Nord, getNordColors(false)});
    themes.push_back({"dracula", "Dracula", "Dark theme with vibrant colors", ThemeMode::Dark, ColorScheme::Dracula, getDraculaColors()});
    themes.push_back({"solarized-dark", "Solarized Dark", "Precision colors for machines and people", ThemeMode::Dark, ColorScheme::Solarized, getSolarizedColors(true)});
    themes.push_back({"solarized-light", "Solarized Light", "Light solarized theme", ThemeMode::Light, ColorScheme::Solarized, getSolarizedColors(false)});
    themes.push_back({"rose-dark", "Rosé", "Soft pink and rose tones", ThemeMode::Dark, ColorScheme::Rose, getRoseColors(true)});
    themes.push_back({"rose-light", "Rosé Light", "Light rose theme", ThemeMode::Light, ColorScheme::Rose, getRoseColors(false)});
    
    return themes;
}

Theme ThemeManager::getThemeById(const std::string& id) const {
    auto themes = getAvailableThemes();
    for (const auto& theme : themes) {
        if (theme.id == id) return theme;
    }
    return themes[0]; // Return default if not found
}

// ==================== THEME COLOR DEFINITIONS ====================

ThemeColors ThemeManager::getDarkDefaultColors() {
    return {
        // Backgrounds
        "#1a1a2e",    // windowBg - deep navy
        "#16213e",    // cardBg - slightly lighter navy
        "#1a2744",    // cardBgHover
        "#16213e",    // sidebarBg
        "#0f3460",    // inputBg
        
        // Borders
        "#0f3460",    // borderColor
        "#e94560",    // borderAccent
        
        // Text
        "#ffffff",    // textPrimary
        "#aaaaaa",    // textSecondary
        "#666666",    // textMuted
        
        // Accent
        "#e94560",    // accent - vibrant pink/red
        "#ff6b6b",    // accentHover
        "rgba(233, 69, 96, 0.2)",  // accentSubtle
        
        // Status
        "#00ff88",    // success
        "#ff4444",    // danger
        "#ffaa00",    // warning
        "#4da6ff",    // info
        
        // Special
        "#0f3460",    // selection
        "#0f3460",    // scrollbar
        "rgba(0, 0, 0, 0.3)"  // shadow
    };
}

ThemeColors ThemeManager::getLightDefaultColors() {
    return {
        // Backgrounds
        "#f5f5f7",    // windowBg - light gray
        "#ffffff",    // cardBg - white
        "#f0f0f2",    // cardBgHover
        "#ffffff",    // sidebarBg
        "#e8e8ea",    // inputBg
        
        // Borders
        "#d0d0d5",    // borderColor
        "#e94560",    // borderAccent
        
        // Text
        "#1a1a2e",    // textPrimary
        "#555555",    // textSecondary
        "#888888",    // textMuted
        
        // Accent
        "#d63553",    // accent - slightly darker for light bg
        "#e94560",    // accentHover
        "rgba(214, 53, 83, 0.15)",  // accentSubtle
        
        // Status
        "#00b359",    // success
        "#d63031",    // danger
        "#e67e00",    // warning
        "#0984e3",    // info
        
        // Special
        "rgba(214, 53, 83, 0.1)",  // selection
        "#c0c0c5",    // scrollbar
        "rgba(0, 0, 0, 0.1)"  // shadow
    };
}

ThemeColors ThemeManager::getOceanColors(bool dark) {
    if (dark) {
        return {
            "#0a192f",    // windowBg - deep ocean
            "#112240",    // cardBg
            "#1d3557",    // cardBgHover
            "#112240",    // sidebarBg
            "#1d3557",    // inputBg
            
            "#1d3557",    // borderColor
            "#64ffda",    // borderAccent - teal
            
            "#ccd6f6",    // textPrimary
            "#8892b0",    // textSecondary
            "#495670",    // textMuted
            
            "#64ffda",    // accent - teal/cyan
            "#9effeb",    // accentHover
            "rgba(100, 255, 218, 0.15)",
            
            "#64ffda",    // success
            "#ff6b6b",    // danger
            "#ffd93d",    // warning
            "#74b9ff",    // info
            
            "#1d3557",
            "#1d3557",
            "rgba(0, 0, 0, 0.4)"
        };
    } else {
        return {
            "#e8f4f8",    // windowBg
            "#ffffff",    // cardBg
            "#d0e8f0",    // cardBgHover
            "#ffffff",    // sidebarBg
            "#d0e8f0",    // inputBg
            
            "#b0d0e0",    // borderColor
            "#0d9488",    // borderAccent
            
            "#0a192f",    // textPrimary
            "#334155",    // textSecondary
            "#64748b",    // textMuted
            
            "#0d9488",    // accent
            "#14b8a6",    // accentHover
            "rgba(13, 148, 136, 0.12)",
            
            "#059669",    // success
            "#dc2626",    // danger
            "#d97706",    // warning
            "#0284c7",    // info
            
            "rgba(13, 148, 136, 0.1)",
            "#b0c4ce",
            "rgba(0, 0, 0, 0.08)"
        };
    }
}

ThemeColors ThemeManager::getForestColors(bool dark) {
    if (dark) {
        return {
            "#1a2f1a",    // windowBg - deep forest
            "#243524",    // cardBg
            "#2d442d",    // cardBgHover
            "#1f2e1f",    // sidebarBg
            "#2d442d",    // inputBg
            
            "#3d5c3d",    // borderColor
            "#50fa7b",    // borderAccent - bright green
            
            "#e8f5e9",    // textPrimary
            "#a5d6a7",    // textSecondary
            "#6b8e6b",    // textMuted
            
            "#50fa7b",    // accent - emerald
            "#69ff94",    // accentHover
            "rgba(80, 250, 123, 0.15)",
            
            "#50fa7b",    // success
            "#ff7979",    // danger
            "#ffeaa7",    // warning
            "#74b9ff",    // info
            
            "#3d5c3d",
            "#3d5c3d",
            "rgba(0, 0, 0, 0.35)"
        };
    } else {
        return {
            "#f1f8e9",    // windowBg
            "#ffffff",    // cardBg
            "#dcedc8",    // cardBgHover
            "#ffffff",    // sidebarBg
            "#dcedc8",    // inputBg
            
            "#aed581",    // borderColor
            "#2e7d32",    // borderAccent
            
            "#1b5e20",    // textPrimary
            "#33691e",    // textSecondary
            "#689f38",    // textMuted
            
            "#2e7d32",    // accent
            "#388e3c",    // accentHover
            "rgba(46, 125, 50, 0.12)",
            
            "#2e7d32",    // success
            "#c62828",    // danger
            "#f57f17",    // warning
            "#1565c0",    // info
            
            "rgba(46, 125, 50, 0.1)",
            "#a5d6a7",
            "rgba(0, 0, 0, 0.08)"
        };
    }
}

ThemeColors ThemeManager::getSunsetColors(bool dark) {
    if (dark) {
        return {
            "#1f1135",    // windowBg - deep purple
            "#2d1b4e",    // cardBg
            "#3d2564",    // cardBgHover
            "#261544",    // sidebarBg
            "#3d2564",    // inputBg
            
            "#4a2c7a",    // borderColor
            "#ff6b35",    // borderAccent - orange
            
            "#fff0e5",    // textPrimary
            "#d4a5a5",    // textSecondary
            "#8b6b8b",    // textMuted
            
            "#ff6b35",    // accent - sunset orange
            "#ff8c5a",    // accentHover
            "rgba(255, 107, 53, 0.18)",
            
            "#00d9a0",    // success
            "#ff6b6b",    // danger
            "#feca57",    // warning
            "#54a0ff",    // info
            
            "#4a2c7a",
            "#4a2c7a",
            "rgba(0, 0, 0, 0.4)"
        };
    } else {
        return {
            "#fff5f0",    // windowBg
            "#ffffff",    // cardBg
            "#ffe4d6",    // cardBgHover
            "#ffffff",    // sidebarBg
            "#ffe4d6",    // inputBg
            
            "#ffcab0",    // borderColor
            "#e65100",    // borderAccent
            
            "#3e2723",    // textPrimary
            "#5d4037",    // textSecondary
            "#8d6e63",    // textMuted
            
            "#e65100",    // accent
            "#ff6d00",    // accentHover
            "rgba(230, 81, 0, 0.12)",
            
            "#2e7d32",    // success
            "#c62828",    // danger
            "#ef6c00",    // warning
            "#1565c0",    // info
            
            "rgba(230, 81, 0, 0.1)",
            "#ffb088",
            "rgba(0, 0, 0, 0.08)"
        };
    }
}

ThemeColors ThemeManager::getMidnightColors() {
    return {
        "#0d0d0d",    // windowBg - near black
        "#151515",    // cardBg
        "#1f1f1f",    // cardBgHover
        "#0d0d0d",    // sidebarBg
        "#1f1f1f",    // inputBg
        
        "#2a2a2a",    // borderColor
        "#bb86fc",    // borderAccent - purple
        
        "#e0e0e0",    // textPrimary
        "#9e9e9e",    // textSecondary
        "#616161",    // textMuted
        
        "#bb86fc",    // accent - soft purple
        "#d4b0ff",    // accentHover
        "rgba(187, 134, 252, 0.15)",
        
        "#03dac6",    // success - teal
        "#cf6679",    // danger
        "#ffb74d",    // warning
        "#64b5f6",    // info
        
        "#2a2a2a",
        "#2a2a2a",
        "rgba(0, 0, 0, 0.5)"
    };
}

ThemeColors ThemeManager::getNordColors(bool dark) {
    if (dark) {
        return {
            "#2e3440",    // windowBg - nord0
            "#3b4252",    // cardBg - nord1
            "#434c5e",    // cardBgHover - nord2
            "#2e3440",    // sidebarBg
            "#434c5e",    // inputBg
            
            "#4c566a",    // borderColor - nord3
            "#88c0d0",    // borderAccent - nord8
            
            "#eceff4",    // textPrimary - nord6
            "#d8dee9",    // textSecondary - nord4
            "#4c566a",    // textMuted
            
            "#88c0d0",    // accent - frost blue
            "#8fbcbb",    // accentHover - nord7
            "rgba(136, 192, 208, 0.15)",
            
            "#a3be8c",    // success - nord14
            "#bf616a",    // danger - nord11
            "#ebcb8b",    // warning - nord13
            "#81a1c1",    // info - nord9
            
            "#434c5e",
            "#4c566a",
            "rgba(0, 0, 0, 0.3)"
        };
    } else {
        return {
            "#eceff4",    // windowBg - nord6
            "#e5e9f0",    // cardBg - nord5
            "#d8dee9",    // cardBgHover - nord4
            "#eceff4",    // sidebarBg
            "#d8dee9",    // inputBg
            
            "#d8dee9",    // borderColor
            "#5e81ac",    // borderAccent - nord10
            
            "#2e3440",    // textPrimary - nord0
            "#3b4252",    // textSecondary
            "#4c566a",    // textMuted
            
            "#5e81ac",    // accent - nord10
            "#81a1c1",    // accentHover
            "rgba(94, 129, 172, 0.12)",
            
            "#a3be8c",    // success
            "#bf616a",    // danger
            "#d08770",    // warning
            "#5e81ac",    // info
            
            "rgba(94, 129, 172, 0.1)",
            "#c0c8d4",
            "rgba(0, 0, 0, 0.08)"
        };
    }
}

ThemeColors ThemeManager::getDraculaColors() {
    return {
        "#282a36",    // windowBg - background
        "#44475a",    // cardBg - current line
        "#6272a4",    // cardBgHover - comment color
        "#282a36",    // sidebarBg
        "#44475a",    // inputBg
        
        "#6272a4",    // borderColor
        "#bd93f9",    // borderAccent - purple
        
        "#f8f8f2",    // textPrimary - foreground
        "#f8f8f2",    // textSecondary
        "#6272a4",    // textMuted - comment
        
        "#bd93f9",    // accent - purple
        "#ff79c6",    // accentHover - pink
        "rgba(189, 147, 249, 0.18)",
        
        "#50fa7b",    // success - green
        "#ff5555",    // danger - red
        "#ffb86c",    // warning - orange
        "#8be9fd",    // info - cyan
        
        "#44475a",
        "#44475a",
        "rgba(0, 0, 0, 0.4)"
    };
}

ThemeColors ThemeManager::getSolarizedColors(bool dark) {
    if (dark) {
        return {
            "#002b36",    // windowBg - base03
            "#073642",    // cardBg - base02
            "#0a4351",    // cardBgHover
            "#002b36",    // sidebarBg
            "#073642",    // inputBg
            
            "#586e75",    // borderColor - base01
            "#268bd2",    // borderAccent - blue
            
            "#839496",    // textPrimary - base0
            "#657b83",    // textSecondary - base00
            "#586e75",    // textMuted - base01
            
            "#268bd2",    // accent - blue
            "#2aa198",    // accentHover - cyan
            "rgba(38, 139, 210, 0.18)",
            
            "#859900",    // success - green
            "#dc322f",    // danger - red
            "#b58900",    // warning - yellow
            "#2aa198",    // info - cyan
            
            "#073642",
            "#586e75",
            "rgba(0, 0, 0, 0.3)"
        };
    } else {
        return {
            "#fdf6e3",    // windowBg - base3
            "#eee8d5",    // cardBg - base2
            "#e4ddc8",    // cardBgHover
            "#fdf6e3",    // sidebarBg
            "#eee8d5",    // inputBg
            
            "#93a1a1",    // borderColor - base1
            "#268bd2",    // borderAccent - blue
            
            "#657b83",    // textPrimary - base00
            "#839496",    // textSecondary - base0
            "#93a1a1",    // textMuted - base1
            
            "#268bd2",    // accent - blue
            "#2aa198",    // accentHover - cyan
            "rgba(38, 139, 210, 0.12)",
            
            "#859900",    // success
            "#dc322f",    // danger
            "#b58900",    // warning
            "#2aa198",    // info
            
            "rgba(38, 139, 210, 0.1)",
            "#b8b0a0",
            "rgba(0, 0, 0, 0.08)"
        };
    }
}

ThemeColors ThemeManager::getRoseColors(bool dark) {
    if (dark) {
        return {
            "#1f1a24",    // windowBg - dark rose
            "#2a232f",    // cardBg
            "#352d3a",    // cardBgHover
            "#1f1a24",    // sidebarBg
            "#352d3a",    // inputBg
            
            "#453a4f",    // borderColor
            "#f472b6",    // borderAccent - pink
            
            "#fce7f3",    // textPrimary
            "#f9a8d4",    // textSecondary
            "#9d7a8c",    // textMuted
            
            "#f472b6",    // accent - pink
            "#fb7ec7",    // accentHover
            "rgba(244, 114, 182, 0.18)",
            
            "#4ade80",    // success
            "#fb7185",    // danger
            "#fbbf24",    // warning
            "#60a5fa",    // info
            
            "#453a4f",
            "#453a4f",
            "rgba(0, 0, 0, 0.35)"
        };
    } else {
        return {
            "#fdf2f8",    // windowBg
            "#ffffff",    // cardBg
            "#fce7f3",    // cardBgHover
            "#ffffff",    // sidebarBg
            "#fce7f3",    // inputBg
            
            "#fbcfe8",    // borderColor
            "#db2777",    // borderAccent
            
            "#4a1942",    // textPrimary
            "#831843",    // textSecondary
            "#9d174d",    // textMuted
            
            "#db2777",    // accent
            "#ec4899",    // accentHover
            "rgba(219, 39, 119, 0.12)",
            
            "#15803d",    // success
            "#be123c",    // danger
            "#ca8a04",    // warning
            "#1d4ed8",    // info
            
            "rgba(219, 39, 119, 0.1)",
            "#f0abcf",
            "rgba(0, 0, 0, 0.08)"
        };
    }
}

// ==================== CSS GENERATION ====================

std::string ThemeManager::generateCSS() const {
    ThemeColors c = getCurrentColors();
    std::ostringstream css;
    
    css << R"(
        window {
            background-color: )" << c.windowBg << R"(;
        }
        
        .main-container {
            background-color: )" << c.windowBg << R"(;
        }
        
        /* Sidebar styles */
        .sidebar {
            background-color: )" << c.sidebarBg << R"(;
            border-color: )" << c.borderColor << R"(;
            border-style: solid;
            border-width: 0 1px 0 0;
        }
        
        .sidebar-title {
            font-size: 20px;
            font-weight: bold;
            color: )" << c.textPrimary << R"(;
        }
        
        .category-list {
            background-color: transparent;
        }
        
        .category-list row {
            background-color: transparent;
            border-radius: 8px;
            margin: 2px 8px;
        }
        
        .category-list row:selected {
            background-color: )" << c.selection << R"(;
        }
        
        .category-list row:hover:not(:selected) {
            background-color: )" << c.cardBgHover << R"(;
        }
        
        .category-name {
            font-size: 14px;
            color: )" << c.textPrimary << R"(;
        }
        
        .category-badge {
            font-size: 11px;
            color: )" << c.textMuted << R"(;
            background-color: )" << c.inputBg << R"(;
            padding: 2px 8px;
            border-radius: 10px;
        }
        
        .content-header {
            font-size: 24px;
            font-weight: bold;
            color: )" << c.textPrimary << R"(;
        }
        
        /* Feedly-style article cards */
        .feedly-card {
            background-color: )" << c.cardBg << R"(;
            border-radius: 12px;
        }
        
        .feedly-card:hover {
            background-color: )" << c.cardBgHover << R"(;
        }
        
        .article-read {
            opacity: 0.7;
        }
        
        .article-read .feedly-title {
            color: )" << c.textMuted << R"(;
        }
        
        .title-read {
            color: )" << c.textMuted << R"(;
        }
        
        .unread-indicator {
            color: )" << c.accent << R"(;
            font-size: 10px;
        }
        
        .feedly-image-container {
            background-color: )" << c.inputBg << R"(;
            border-radius: 12px 12px 0 0;
            min-width: 320px;
            min-height: 180px;
        }
        
        .feedly-image {
            border-radius: 12px 12px 0 0;
            min-width: 320px;
            min-height: 180px;
        }
        
        .feedly-no-image {
            background: linear-gradient(135deg, )" << c.accent << R"( 0%, )" << c.inputBg << R"( 50%, )" << c.cardBg << R"( 100%);
            min-height: 180px;
        }
        
        .feedly-content {
            background-color: transparent;
        }
        
        .feedly-source {
            font-size: 11px;
            font-weight: 600;
            color: )" << c.accent << R"(;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        
        .feedly-separator {
            font-size: 10px;
            color: )" << c.textMuted << R"(;
        }
        
        .feedly-date {
            font-size: 11px;
            color: )" << c.textSecondary << R"(;
        }
        
        .feedly-title {
            font-size: 15px;
            font-weight: 700;
            color: )" << c.textPrimary << R"(;
            line-height: 1.3;
            margin-top: 6px;
        }
        
        .feedly-description {
            font-size: 13px;
            color: )" << c.textSecondary << R"(;
            line-height: 1.4;
            margin-top: 6px;
        }
        
        flowbox {
            background-color: transparent;
        }
        
        flowboxchild {
            background-color: transparent;
            padding: 0;
            border: none;
        }
        
        flowboxchild:focus {
            outline: none;
        }
        
        /* Dialog styles */
        .title-2 {
            font-size: 20px;
            font-weight: bold;
            color: )" << c.textPrimary << R"(;
        }
        
        .boxed-list {
            background-color: )" << c.inputBg << R"(;
            border-radius: 12px;
        }
        
        .boxed-list row {
            background-color: transparent;
            border-color: )" << c.cardBg << R"(;
            border-style: solid;
            border-width: 0 0 1px 0;
        }
        
        .boxed-list row:last-child {
            border-width: 0;
        }
        
        .heading {
            font-size: 14px;
            font-weight: 600;
            color: )" << c.textPrimary << R"(;
        }
        
        .dim-label {
            font-size: 12px;
            color: )" << c.textSecondary << R"(;
        }
        
        .destructive-action {
            color: )" << c.danger << R"(;
        }
        
        .suggested-action {
            background-color: )" << c.accent << R"(;
            color: white;
        }
        
        .suggested-action:hover {
            background-color: )" << c.accentHover << R"(;
        }
        
        /* Panel styles */
        .panel-card {
            background-color: )" << c.cardBg << R"(;
            border-radius: 12px;
            padding: 16px;
            margin: 8px;
        }
        
        .panel-title {
            font-size: 18px;
            font-weight: bold;
            color: )" << c.accent << R"(;
            margin-bottom: 12px;
        }
        
        .article-card {
            background-color: )" << c.inputBg << R"(;
            border-radius: 8px;
            padding: 12px;
            margin: 6px 0;
        }
        
        .article-card:hover {
            background-color: )" << c.cardBgHover << R"(;
        }
        
        .article-title {
            font-size: 14px;
            font-weight: bold;
            color: )" << c.textPrimary << R"(;
        }
        
        .article-source {
            font-size: 11px;
            color: )" << c.textSecondary << R"(;
        }
        
        .article-date {
            font-size: 10px;
            color: )" << c.textMuted << R"(;
        }
        
        /* Weather styles */
        .weather-card {
            background-color: )" << c.inputBg << R"(;
            border-radius: 12px;
            padding: 16px;
            margin: 8px;
        }
        
        .weather-temp {
            font-size: 48px;
            font-weight: bold;
            color: )" << c.textPrimary << R"(;
        }
        
        .weather-location {
            font-size: 16px;
            color: )" << c.accent << R"(;
        }
        
        .weather-condition {
            font-size: 14px;
            color: )" << c.textSecondary << R"(;
        }
        
        .weather-details {
            font-size: 13px;
            color: )" << c.textSecondary << R"(;
        }
        
        .weather-feels {
            font-size: 13px;
            color: )" << c.textSecondary << R"(;
        }
        
        .weather-icon {
            color: )" << c.accent << R"(;
        }
        
        .weather-alert {
            background-color: )" << c.danger << R"(;
            border-radius: 8px;
            padding: 12px;
            margin-bottom: 8px;
        }
        
        .alert-text {
            font-size: 13px;
            font-weight: bold;
            color: #ffffff;
        }
        
        .loading-label {
            font-size: 14px;
            color: )" << c.textSecondary << R"(;
        }
        
        .forecast-day {
            background-color: )" << c.inputBg << R"(;
            border-radius: 8px;
            padding: 12px;
            margin: 4px;
            min-width: 70px;
        }
        
        .forecast-day-name {
            font-size: 12px;
            font-weight: bold;
            color: )" << c.accent << R"(;
        }
        
        .forecast-temp-high {
            font-size: 16px;
            font-weight: bold;
            color: )" << c.textPrimary << R"(;
        }
        
        .forecast-temp-low {
            font-size: 14px;
            color: )" << c.textSecondary << R"(;
        }
        
        .forecast-condition {
            font-size: 10px;
            color: )" << c.textSecondary << R"(;
            margin: 4px 0;
        }
        
        .forecast-header {
            font-size: 14px;
            font-weight: bold;
            color: )" << c.textSecondary << R"(;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        
        .forecast-icon {
            color: )" << c.textSecondary << R"(;
        }
        
        .locations-label {
            font-size: 12px;
            color: )" << c.textSecondary << R"(;
            margin-right: 4px;
        }
        
        .location-tag {
            background-color: )" << c.inputBg << R"(;
            border-radius: 12px;
            padding: 4px 10px;
            margin-right: 4px;
            font-size: 11px;
            color: )" << c.textPrimary << R"(;
        }
        
        .location-remove-btn {
            min-width: 16px;
            min-height: 16px;
            padding: 0;
            margin-left: 4px;
        }
        
        /* Stock styles */
        .stock-ticker {
            background-color: )" << c.inputBg << R"(;
            padding: 8px 16px;
            border-radius: 8px;
        }
        
        .stock-symbol {
            font-size: 14px;
            font-weight: bold;
            color: )" << c.textPrimary << R"(;
        }
        
        .stock-price {
            font-size: 16px;
            color: )" << c.textPrimary << R"(;
        }
        
        .stock-up {
            color: )" << c.success << R"(;
        }
        
        .stock-down {
            color: )" << c.danger << R"(;
        }
        
        /* Buttons and controls */
        .add-button {
            background-color: )" << c.accent << R"(;
            color: white;
            border-radius: 8px;
            padding: 8px 16px;
        }
        
        .add-button:hover {
            background-color: )" << c.accentHover << R"(;
        }
        
        headerbar {
            background-color: )" << c.cardBg << R"(;
            color: )" << c.textPrimary << R"(;
        }
        
        stackswitcher button {
            background-color: )" << c.inputBg << R"(;
            color: )" << c.textPrimary << R"(;
            border-radius: 8px;
            margin: 4px;
        }
        
        stackswitcher button:checked {
            background-color: )" << c.accent << R"(;
        }
        
        entry {
            background-color: )" << c.inputBg << R"(;
            color: )" << c.textPrimary << R"(;
            border-radius: 6px;
            padding: 8px;
            border: 1px solid )" << c.borderColor << R"(;
        }
        
        entry:focus {
            border-color: )" << c.accent << R"(;
        }
        
        scrolledwindow {
            background-color: transparent;
        }
        
        button.flat {
            background-color: transparent;
            color: )" << c.textSecondary << R"(;
        }
        
        button.flat:hover {
            background-color: )" << c.accentSubtle << R"(;
        }
        
        dropdown {
            background-color: )" << c.inputBg << R"(;
            color: )" << c.textPrimary << R"(;
            border-radius: 6px;
        }
        
        dropdown button {
            background-color: )" << c.inputBg << R"(;
            color: )" << c.textPrimary << R"(;
        }
        
        dropdown popover {
            background-color: )" << c.cardBg << R"(;
        }
        
        dropdown popover listview row {
            color: )" << c.textPrimary << R"(;
        }
        
        dropdown popover listview row:selected {
            background-color: )" << c.accent << R"(;
        }
        
        /* Card styles */
        .card {
            background-color: )" << c.cardBg << R"(;
            border-radius: 12px;
            transition: opacity 0.2s;
        }
        
        .card:hover {
            background-color: )" << c.cardBgHover << R"(;
        }
        
        .card.read {
            opacity: 0.55;
        }
        
        .card.read:hover {
            opacity: 0.75;
        }
        
        .card.saved {
            box-shadow: inset 0 0 0 2px )" << c.accent << R"(;
        }
        
        /* List layout styles */
        .list-item {
            background-color: )" << c.cardBg << R"(;
            border-radius: 8px;
            transition: opacity 0.2s, background-color 0.2s;
        }
        
        .list-item:hover {
            background-color: )" << c.cardBgHover << R"(;
        }
        
        .list-item.read {
            opacity: 0.55;
        }
        
        .list-item.read:hover {
            opacity: 0.75;
        }
        
        .list-item.saved {
            box-shadow: inset 0 0 0 2px )" << c.accent << R"(;
        }
        
        .badge {
            font-size: 11px;
            font-weight: 600;
            background-color: )" << c.accent << R"(;
            color: white;
            padding: 2px 8px;
            border-radius: 10px;
            min-width: 16px;
        }
        
        .badge.small {
            font-size: 10px;
            padding: 1px 6px;
        }
        
        .accent {
            color: )" << c.accent << R"(;
        }

        /* Theme dialog specific styles */
        .theme-section-title {
            font-size: 12px;
            font-weight: 600;
            color: )" << c.textMuted << R"(;
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-bottom: 8px;
        }

        .theme-preview {
            background-color: )" << c.cardBg << R"(;
            border-radius: 8px;
            border: 2px solid transparent;
            padding: 12px;
            min-width: 140px;
        }

        .theme-preview:hover {
            border-color: )" << c.borderColor << R"(;
        }

        .theme-preview.selected {
            border-color: )" << c.accent << R"(;
        }

        .theme-preview-name {
            font-size: 13px;
            font-weight: 600;
            color: )" << c.textPrimary << R"(;
        }

        .theme-preview-desc {
            font-size: 11px;
            color: )" << c.textSecondary << R"(;
        }

        .color-swatch {
            border-radius: 50%;
            min-width: 24px;
            min-height: 24px;
            border: 2px solid )" << c.borderColor << R"(;
        }

        .color-swatch.selected {
            border-color: )" << c.textPrimary << R"(;
            border-width: 3px;
        }

        .mode-button {
            background-color: )" << c.inputBg << R"(;
            border-radius: 8px;
            padding: 12px 20px;
            border: 2px solid transparent;
        }

        .mode-button:hover {
            background-color: )" << c.cardBgHover << R"(;
        }

        .mode-button.selected {
            border-color: )" << c.accent << R"(;
            background-color: )" << c.accentSubtle << R"(;
        }

        .mode-button-label {
            font-size: 14px;
            font-weight: 500;
            color: )" << c.textPrimary << R"(;
        }

        .mode-button-icon {
            font-size: 24px;
            margin-bottom: 4px;
        }

    )";
    
    return css.str();
}

void ThemeManager::applyTheme() {
    if (cssProvider_) {
        gtk_style_context_remove_provider_for_display(
            gdk_display_get_default(),
            GTK_STYLE_PROVIDER(cssProvider_)
        );
        g_object_unref(cssProvider_);
    }
    
    cssProvider_ = gtk_css_provider_new();
    std::string css = generateCSS();
    gtk_css_provider_load_from_string(cssProvider_, css.c_str());
    
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(cssProvider_),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}

void ThemeManager::onThemeChanged(ThemeChangedCallback callback) {
    callbacks_.push_back(callback);
}

void ThemeManager::notifyThemeChanged() {
    for (auto& callback : callbacks_) {
        callback();
    }
}

void ThemeManager::saveToConfig() {
    auto& config = Config::getInstance();
    ThemePreferences prefs;
    prefs.mode = currentMode_;
    prefs.scheme = currentScheme_;
    prefs.customAccentColor = customAccentColor_;
    if (currentScheme_ == ColorScheme::Custom) {
        prefs.customWindowBg = customColors_.windowBg;
        prefs.customCardBg = customColors_.cardBg;
        prefs.customTextPrimary = customColors_.textPrimary;
        prefs.customTextSecondary = customColors_.textSecondary;
    }
    config.setThemePreferences(prefs);
    // We'll implement this after updating Config.hpp
}

void ThemeManager::loadFromConfig() {
    auto& config = Config::getInstance();
    auto prefs = config.getThemePreferences();
    currentMode_ = prefs.mode;
    currentScheme_ = prefs.scheme;
    customAccentColor_ = prefs.customAccentColor;
    if (prefs.scheme == ColorScheme::Custom && !prefs.customWindowBg.empty()) {
        customColors_ = getDarkDefaultColors();
        customColors_.windowBg = prefs.customWindowBg;
        if (!prefs.customCardBg.empty()) customColors_.cardBg = prefs.customCardBg;
        if (!prefs.customTextPrimary.empty()) customColors_.textPrimary = prefs.customTextPrimary;
        if (!prefs.customTextSecondary.empty()) customColors_.textSecondary = prefs.customTextSecondary;
    }
    // We'll implement this after updating Config.hpp
}

}

// Update saveToConfig and loadFromConfig at the end of the file
