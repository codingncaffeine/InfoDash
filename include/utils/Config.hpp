#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>

namespace InfoDash {

enum class LayoutMode {
    Cards,
    List
};

enum class TempUnit {
    Celsius,
    Fahrenheit
};

// Forward declarations - actual enums in ThemeManager.hpp
enum class ThemeMode;
enum class ColorScheme;

struct FeedInfo {
    std::string url;
    std::string name;
    std::string category;
    bool enabled = true;
};

struct Category {
    std::string id;
    std::string name;
    std::string icon;
    int order = 0;
};

// Theme preference storage
struct ThemePreferences {
    ThemeMode mode;
    ColorScheme scheme;
    std::string customAccentColor = "#e94560";
    // For custom theme colors
    std::string customWindowBg;
    std::string customCardBg;
    std::string customTextPrimary;
    std::string customTextSecondary;
    
    ThemePreferences();
};

class Config {
public:
    static Config& getInstance();
    
    // Feed management
    std::vector<FeedInfo> getFeeds() const;
    void addFeed(const FeedInfo& feed);
    void updateFeed(const std::string& url, const FeedInfo& feed);
    void removeFeed(const std::string& url);
    std::vector<FeedInfo> getFeedsByCategory(const std::string& category) const;
    
    // Legacy support
    std::vector<std::string> getRSSFeeds() const;
    void addRSSFeed(const std::string& url);
    void removeRSSFeed(const std::string& url);
    
    // Category management
    std::vector<Category> getCategories() const;
    void addCategory(const Category& category);
    void updateCategory(const std::string& id, const Category& category);
    void removeCategory(const std::string& id);
    
    // Read status
    bool isArticleRead(const std::string& articleId) const;
    void markArticleRead(const std::string& articleId);
    void markArticleUnread(const std::string& articleId);
    void markAllRead(const std::string& feedUrl = "");
    std::set<std::string> getReadArticles() const;
    
    // Save for later
    bool isArticleSaved(const std::string& articleId) const;
    void saveArticle(const std::string& articleId);
    void unsaveArticle(const std::string& articleId);
    std::set<std::string> getSavedArticles() const;
    
    // Weather locations (multiple)
    std::vector<std::string> getWeatherLocations() const;
    void addWeatherLocation(const std::string& location);
    void removeWeatherLocation(const std::string& location);
    void setWeatherLocation(const std::string& location); // Legacy - adds if not exists
    std::string getWeatherLocation() const; // Legacy - returns first
    
    // Temperature unit
    TempUnit getTempUnit() const;
    void setTempUnit(TempUnit unit);
    
    // Stocks
    std::vector<std::string> getStockSymbols() const;
    void addStockSymbol(const std::string& symbol);
    void removeStockSymbol(const std::string& symbol);
    
    // Category expansion state
    bool isCategoryExpanded(const std::string& categoryId) const;
    void setCategoryExpanded(const std::string& categoryId, bool expanded);
    
    // Layout preference
    LayoutMode getLayoutMode() const;
    void setLayoutMode(LayoutMode mode);
    
    // Theme preferences
    ThemePreferences getThemePreferences() const;
    void setThemePreferences(const ThemePreferences& prefs);
    ThemeMode getThemeMode() const;
    void setThemeMode(ThemeMode mode);
    ColorScheme getColorScheme() const;
    void setColorScheme(ColorScheme scheme);
    std::string getCustomAccentColor() const;
    void setCustomAccentColor(const std::string& color);
    
    void save();
    void load();

private:
    Config();
    ~Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    std::string getConfigPath() const;
    void ensureDefaults();
    
    std::vector<FeedInfo> feeds_;
    std::vector<Category> categories_;
    std::set<std::string> readArticles_;
    std::set<std::string> savedArticles_;
    std::set<std::string> expandedCategories_;
    std::vector<std::string> weatherLocations_;
    std::vector<std::string> stockSymbols_;
    LayoutMode layoutMode_ = LayoutMode::Cards;
    TempUnit tempUnit_ = TempUnit::Fahrenheit;
    ThemePreferences themePrefs_;
};

}
