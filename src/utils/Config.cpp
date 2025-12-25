#include "utils/Config.hpp"
#include <json-glib/json-glib.h>
#include <sys/stat.h>
#include <fstream>
#include <algorithm>
#include <cstring>

namespace InfoDash {

Config& Config::getInstance() {
    static Config instance;
    return instance;
}

Config::Config() : weatherLocation_("auto") {
    load();
}

std::string Config::getConfigPath() const {
    const char* home = getenv("HOME");
    return std::string(home) + "/.config/infodash/config.json";
}

void Config::ensureDefaults() {
    if (categories_.empty()) {
        categories_ = {
            {"all", "All Articles", "📰", 0},
            {"saved", "Saved for Later", "⭐", 1},
            {"tech", "Technology", "💻", 2},
            {"science", "Science", "🔬", 3},
            {"news", "News", "📢", 4},
            {"gaming", "Gaming", "🎮", 5},
            {"uncategorized", "Uncategorized", "📁", 99}
        };
    }
    
    // Ensure "saved" category exists
    bool hasSaved = false;
    for (const auto& c : categories_) {
        if (c.id == "saved") { hasSaved = true; break; }
    }
    if (!hasSaved) {
        categories_.insert(categories_.begin() + 1, {"saved", "Saved for Later", "⭐", 1});
    }
    
    if (feeds_.empty()) {
        feeds_ = {
            {"https://feeds.arstechnica.com/arstechnica/index", "Ars Technica", "tech", true},
            {"https://www.reddit.com/r/linux.rss", "r/linux", "tech", true},
            {"https://news.ycombinator.com/rss", "Hacker News", "tech", true}
        };
    }
    
    if (stockSymbols_.empty()) {
        stockSymbols_ = {"AAPL", "GOOGL", "MSFT", "AMZN"};
    }
}

void Config::load() {
    std::string configPath = getConfigPath();
    
    std::string dir = configPath.substr(0, configPath.rfind('/'));
    mkdir(dir.c_str(), 0755);
    
    JsonParser* parser = json_parser_new();
    GError* error = nullptr;
    
    if (!json_parser_load_from_file(parser, configPath.c_str(), &error)) {
        if (error) g_error_free(error);
        g_object_unref(parser);
        ensureDefaults();
        save();
        return;
    }
    
    JsonNode* root = json_parser_get_root(parser);
    if (!root || !JSON_NODE_HOLDS_OBJECT(root)) {
        g_object_unref(parser);
        ensureDefaults();
        return;
    }
    
    JsonObject* obj = json_node_get_object(root);
    
    // Load categories
    categories_.clear();
    if (json_object_has_member(obj, "categories")) {
        JsonArray* cats = json_object_get_array_member(obj, "categories");
        guint len = json_array_get_length(cats);
        for (guint i = 0; i < len; i++) {
            JsonObject* cat = json_array_get_object_element(cats, i);
            Category c;
            c.id = json_object_get_string_member(cat, "id");
            c.name = json_object_get_string_member(cat, "name");
            c.icon = json_object_has_member(cat, "icon") ? json_object_get_string_member(cat, "icon") : "📁";
            c.order = json_object_has_member(cat, "order") ? json_object_get_int_member(cat, "order") : i;
            categories_.push_back(c);
        }
    }
    
    // Load feeds
    feeds_.clear();
    if (json_object_has_member(obj, "feeds")) {
        JsonArray* feedsArr = json_object_get_array_member(obj, "feeds");
        guint len = json_array_get_length(feedsArr);
        for (guint i = 0; i < len; i++) {
            JsonObject* feed = json_array_get_object_element(feedsArr, i);
            FeedInfo f;
            f.url = json_object_get_string_member(feed, "url");
            f.name = json_object_has_member(feed, "name") ? json_object_get_string_member(feed, "name") : "";
            f.category = json_object_has_member(feed, "category") ? json_object_get_string_member(feed, "category") : "uncategorized";
            f.enabled = !json_object_has_member(feed, "enabled") || json_object_get_boolean_member(feed, "enabled");
            feeds_.push_back(f);
        }
    }
    
    // Load legacy RSS feeds format
    if (feeds_.empty() && json_object_has_member(obj, "rssFeeds")) {
        JsonArray* rssArr = json_object_get_array_member(obj, "rssFeeds");
        guint len = json_array_get_length(rssArr);
        for (guint i = 0; i < len; i++) {
            FeedInfo f;
            f.url = json_array_get_string_element(rssArr, i);
            f.name = "";
            f.category = "uncategorized";
            f.enabled = true;
            feeds_.push_back(f);
        }
    }
    
    // Load read articles
    readArticles_.clear();
    if (json_object_has_member(obj, "readArticles")) {
        JsonArray* readArr = json_object_get_array_member(obj, "readArticles");
        guint len = json_array_get_length(readArr);
        for (guint i = 0; i < len; i++) {
            readArticles_.insert(json_array_get_string_element(readArr, i));
        }
    }
    
    // Load saved articles
    savedArticles_.clear();
    if (json_object_has_member(obj, "savedArticles")) {
        JsonArray* savedArr = json_object_get_array_member(obj, "savedArticles");
        guint len = json_array_get_length(savedArr);
        for (guint i = 0; i < len; i++) {
            savedArticles_.insert(json_array_get_string_element(savedArr, i));
        }
    }
    
    // Load expanded categories
    expandedCategories_.clear();
    if (json_object_has_member(obj, "expandedCategories")) {
        JsonArray* expArr = json_object_get_array_member(obj, "expandedCategories");
        guint len = json_array_get_length(expArr);
        for (guint i = 0; i < len; i++) {
            expandedCategories_.insert(json_array_get_string_element(expArr, i));
        }
    }
    
    // Load weather location
    if (json_object_has_member(obj, "weatherLocation")) {
        weatherLocation_ = json_object_get_string_member(obj, "weatherLocation");
    }
    
    // Load stock symbols
    stockSymbols_.clear();
    if (json_object_has_member(obj, "stockSymbols")) {
        JsonArray* stocksArr = json_object_get_array_member(obj, "stockSymbols");
        guint len = json_array_get_length(stocksArr);
        for (guint i = 0; i < len; i++) {
            stockSymbols_.push_back(json_array_get_string_element(stocksArr, i));
        }
    }
    
    // Load layout mode
    layoutMode_ = LayoutMode::Cards;
    if (json_object_has_member(obj, "layoutMode")) {
        const char* mode = json_object_get_string_member(obj, "layoutMode");
        if (mode && strcmp(mode, "list") == 0) {
            layoutMode_ = LayoutMode::List;
        }
    }
    
    g_object_unref(parser);
    ensureDefaults();
}

void Config::save() {
    JsonBuilder* builder = json_builder_new();
    json_builder_begin_object(builder);
    
    // Save categories
    json_builder_set_member_name(builder, "categories");
    json_builder_begin_array(builder);
    for (const auto& cat : categories_) {
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "id");
        json_builder_add_string_value(builder, cat.id.c_str());
        json_builder_set_member_name(builder, "name");
        json_builder_add_string_value(builder, cat.name.c_str());
        json_builder_set_member_name(builder, "icon");
        json_builder_add_string_value(builder, cat.icon.c_str());
        json_builder_set_member_name(builder, "order");
        json_builder_add_int_value(builder, cat.order);
        json_builder_end_object(builder);
    }
    json_builder_end_array(builder);
    
    // Save feeds
    json_builder_set_member_name(builder, "feeds");
    json_builder_begin_array(builder);
    for (const auto& feed : feeds_) {
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "url");
        json_builder_add_string_value(builder, feed.url.c_str());
        json_builder_set_member_name(builder, "name");
        json_builder_add_string_value(builder, feed.name.c_str());
        json_builder_set_member_name(builder, "category");
        json_builder_add_string_value(builder, feed.category.c_str());
        json_builder_set_member_name(builder, "enabled");
        json_builder_add_boolean_value(builder, feed.enabled);
        json_builder_end_object(builder);
    }
    json_builder_end_array(builder);
    
    // Save read articles (limit to last 1000)
    json_builder_set_member_name(builder, "readArticles");
    json_builder_begin_array(builder);
    int count = 0;
    for (const auto& id : readArticles_) {
        if (count++ >= 1000) break;
        json_builder_add_string_value(builder, id.c_str());
    }
    json_builder_end_array(builder);
    
    // Save saved articles
    json_builder_set_member_name(builder, "savedArticles");
    json_builder_begin_array(builder);
    for (const auto& id : savedArticles_) {
        json_builder_add_string_value(builder, id.c_str());
    }
    json_builder_end_array(builder);
    
    // Save expanded categories
    json_builder_set_member_name(builder, "expandedCategories");
    json_builder_begin_array(builder);
    for (const auto& id : expandedCategories_) {
        json_builder_add_string_value(builder, id.c_str());
    }
    json_builder_end_array(builder);
    
    // Save weather location
    json_builder_set_member_name(builder, "weatherLocation");
    json_builder_add_string_value(builder, weatherLocation_.c_str());
    
    // Save stock symbols
    json_builder_set_member_name(builder, "stockSymbols");
    json_builder_begin_array(builder);
    for (const auto& sym : stockSymbols_) {
        json_builder_add_string_value(builder, sym.c_str());
    }
    json_builder_end_array(builder);
    
    // Save layout mode
    json_builder_set_member_name(builder, "layoutMode");
    json_builder_add_string_value(builder, layoutMode_ == LayoutMode::List ? "list" : "cards");
    
    json_builder_end_object(builder);
    
    JsonGenerator* gen = json_generator_new();
    json_generator_set_pretty(gen, TRUE);
    JsonNode* rootNode = json_builder_get_root(builder);
    json_generator_set_root(gen, rootNode);
    
    GError* error = nullptr;
    json_generator_to_file(gen, getConfigPath().c_str(), &error);
    if (error) g_error_free(error);
    
    json_node_unref(rootNode);
    g_object_unref(gen);
    g_object_unref(builder);
}

// Feed management
std::vector<FeedInfo> Config::getFeeds() const {
    return feeds_;
}

void Config::addFeed(const FeedInfo& feed) {
    for (const auto& f : feeds_) {
        if (f.url == feed.url) return;
    }
    feeds_.push_back(feed);
    save();
}

void Config::updateFeed(const std::string& url, const FeedInfo& feed) {
    for (auto& f : feeds_) {
        if (f.url == url) {
            f = feed;
            save();
            return;
        }
    }
}

void Config::removeFeed(const std::string& url) {
    feeds_.erase(std::remove_if(feeds_.begin(), feeds_.end(),
        [&url](const FeedInfo& f) { return f.url == url; }), feeds_.end());
    save();
}

std::vector<FeedInfo> Config::getFeedsByCategory(const std::string& category) const {
    if (category == "all") return feeds_;
    
    std::vector<FeedInfo> result;
    for (const auto& f : feeds_) {
        if (f.category == category && f.enabled) {
            result.push_back(f);
        }
    }
    return result;
}

// Legacy support
std::vector<std::string> Config::getRSSFeeds() const {
    std::vector<std::string> urls;
    for (const auto& f : feeds_) {
        if (f.enabled) urls.push_back(f.url);
    }
    return urls;
}

void Config::addRSSFeed(const std::string& url) {
    FeedInfo feed;
    feed.url = url;
    feed.name = "";
    feed.category = "uncategorized";
    feed.enabled = true;
    addFeed(feed);
}

void Config::removeRSSFeed(const std::string& url) {
    removeFeed(url);
}

// Category management
std::vector<Category> Config::getCategories() const {
    auto cats = categories_;
    std::sort(cats.begin(), cats.end(), [](const Category& a, const Category& b) {
        return a.order < b.order;
    });
    return cats;
}

void Config::addCategory(const Category& category) {
    for (const auto& c : categories_) {
        if (c.id == category.id) return;
    }
    categories_.push_back(category);
    save();
}

void Config::updateCategory(const std::string& id, const Category& category) {
    for (auto& c : categories_) {
        if (c.id == id) {
            c = category;
            save();
            return;
        }
    }
}

void Config::removeCategory(const std::string& id) {
    if (id == "all" || id == "uncategorized" || id == "saved") return;
    
    for (auto& f : feeds_) {
        if (f.category == id) {
            f.category = "uncategorized";
        }
    }
    
    categories_.erase(std::remove_if(categories_.begin(), categories_.end(),
        [&id](const Category& c) { return c.id == id; }), categories_.end());
    save();
}

// Read status
bool Config::isArticleRead(const std::string& articleId) const {
    return readArticles_.find(articleId) != readArticles_.end();
}

void Config::markArticleRead(const std::string& articleId) {
    readArticles_.insert(articleId);
    save();
}

void Config::markArticleUnread(const std::string& articleId) {
    readArticles_.erase(articleId);
    save();
}

void Config::markAllRead(const std::string& feedUrl) {
    save();
}

std::set<std::string> Config::getReadArticles() const {
    return readArticles_;
}

// Save for later
bool Config::isArticleSaved(const std::string& articleId) const {
    return savedArticles_.find(articleId) != savedArticles_.end();
}

void Config::saveArticle(const std::string& articleId) {
    savedArticles_.insert(articleId);
    save();
}

void Config::unsaveArticle(const std::string& articleId) {
    savedArticles_.erase(articleId);
    save();
}

std::set<std::string> Config::getSavedArticles() const {
    return savedArticles_;
}

// Category expansion state
bool Config::isCategoryExpanded(const std::string& categoryId) const {
    return expandedCategories_.find(categoryId) != expandedCategories_.end();
}

void Config::setCategoryExpanded(const std::string& categoryId, bool expanded) {
    if (expanded) {
        expandedCategories_.insert(categoryId);
    } else {
        expandedCategories_.erase(categoryId);
    }
    save();
}

// Weather & Stocks
std::string Config::getWeatherLocation() const {
    return weatherLocation_;
}

void Config::setWeatherLocation(const std::string& location) {
    weatherLocation_ = location;
    save();
}

std::vector<std::string> Config::getStockSymbols() const {
    return stockSymbols_;
}

void Config::addStockSymbol(const std::string& symbol) {
    for (const auto& s : stockSymbols_) {
        if (s == symbol) return;
    }
    stockSymbols_.push_back(symbol);
    save();
}

void Config::removeStockSymbol(const std::string& symbol) {
    stockSymbols_.erase(std::remove(stockSymbols_.begin(), stockSymbols_.end(), symbol), stockSymbols_.end());
    save();
}

// Layout preference
LayoutMode Config::getLayoutMode() const {
    return layoutMode_;
}

void Config::setLayoutMode(LayoutMode mode) {
    layoutMode_ = mode;
    save();
}

}
