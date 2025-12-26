#pragma once
#include <string>
#include <vector>
#include <functional>
#include <gtk/gtk.h>

namespace InfoDash {

// Theme mode: Dark, Light, or System (follows OS preference)
enum class ThemeMode {
    Dark,
    Light,
    System
};

// Predefined color schemes
enum class ColorScheme {
    Default,      // Original InfoDash theme (dark blue/red)
    Ocean,        // Deep blue with teal accents
    Forest,       // Dark green with emerald accents
    Sunset,       // Warm orange/purple gradient feel
    Midnight,     // Pure dark with purple accents
    Nord,         // Nord theme colors
    Dracula,      // Dracula theme colors
    Solarized,    // Solarized dark/light
    Rose,         // Soft pink/rose theme
    Custom        // User-defined colors
};

// Color definitions for a complete theme
struct ThemeColors {
    // Background colors
    std::string windowBg;          // Main window background
    std::string cardBg;            // Card/panel background
    std::string cardBgHover;       // Card hover state
    std::string sidebarBg;         // Sidebar background
    std::string inputBg;           // Input/entry background
    
    // Border colors
    std::string borderColor;       // General borders
    std::string borderAccent;      // Accent borders
    
    // Text colors
    std::string textPrimary;       // Main text
    std::string textSecondary;     // Muted/secondary text
    std::string textMuted;         // Very muted text
    
    // Accent colors
    std::string accent;            // Primary accent color
    std::string accentHover;       // Accent hover state
    std::string accentSubtle;      // Subtle accent for badges, etc.
    
    // Status colors
    std::string success;           // Positive/success (stocks up)
    std::string danger;            // Negative/danger (stocks down)
    std::string warning;           // Warning color
    std::string info;              // Info color
    
    // Special
    std::string selection;         // Selected items
    std::string scrollbar;         // Scrollbar color
    std::string shadow;            // Shadow color for depth
};

// Complete theme definition
struct Theme {
    std::string id;
    std::string name;
    std::string description;
    ThemeMode mode;
    ColorScheme scheme;
    ThemeColors colors;
};

class ThemeManager {
public:
    static ThemeManager& getInstance();
    
    // Theme management
    void setThemeMode(ThemeMode mode);
    ThemeMode getThemeMode() const;
    
    void setColorScheme(ColorScheme scheme);
    ColorScheme getColorScheme() const;
    
    void setCustomAccentColor(const std::string& color);
    std::string getCustomAccentColor() const;
    
    void setCustomColors(const ThemeColors& colors);
    ThemeColors getCustomColors() const;
    
    // Get current effective theme
    Theme getCurrentTheme() const;
    ThemeColors getCurrentColors() const;
    
    // Get available themes
    std::vector<Theme> getAvailableThemes() const;
    Theme getThemeById(const std::string& id) const;
    
    // CSS generation
    std::string generateCSS() const;
    void applyTheme();
    
    // Callbacks for theme changes
    using ThemeChangedCallback = std::function<void()>;
    void onThemeChanged(ThemeChangedCallback callback);
    
    // Predefined themes
    static ThemeColors getDarkDefaultColors();
    static ThemeColors getLightDefaultColors();
    static ThemeColors getOceanColors(bool dark = true);
    static ThemeColors getForestColors(bool dark = true);
    static ThemeColors getSunsetColors(bool dark = true);
    static ThemeColors getMidnightColors();
    static ThemeColors getNordColors(bool dark = true);
    static ThemeColors getDraculaColors();
    static ThemeColors getSolarizedColors(bool dark = true);
    static ThemeColors getRoseColors(bool dark = true);
    
    // Persistence
    void saveToConfig();
    void loadFromConfig();

private:
    ThemeManager();
    ~ThemeManager() = default;
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;
    
    ThemeMode currentMode_;
    ColorScheme currentScheme_;
    std::string customAccentColor_;
    ThemeColors customColors_;
    GtkCssProvider* cssProvider_;
    std::vector<ThemeChangedCallback> callbacks_;
    
    void notifyThemeChanged();
    bool isSystemDarkMode() const;
    ThemeColors getColorsForScheme(ColorScheme scheme, bool dark) const;
};

}
