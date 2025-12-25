#pragma once
#include <gtk/gtk.h>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include "services/RSSService.hpp"
#include "utils/Config.hpp"

namespace InfoDash {

struct DiscoveredFeed {
    std::string url;
    std::string title;
    std::string type;
};

class RSSPanel {
public:
    RSSPanel();
    ~RSSPanel();
    GtkWidget* getWidget() { return mainPaned_; }
    void refresh();
    
    // Public for callbacks
    void showFeedManagementDialog();
    void showAddFeedDialog();
    void showEditFeedDialog(const std::string& feedUrl, GtkWidget* parentDialog);
    void showAddCategoryDialog();
    void showFeedDiscoveryDialog(const std::vector<DiscoveredFeed>& feeds, 
                                 const std::string& customName, 
                                 const std::string& categoryId);
    std::vector<DiscoveredFeed> discoverFeeds(const std::string& url);
    void updateSidebar();
    void loadFeedsForCategory(const std::string& category);
    
    // Article actions
    void markArticleRead(const std::string& articleUrl);
    void toggleArticleSaved(const std::string& articleUrl);
    
    // Get items for saved articles
    const std::vector<RSSItem>& getAllItems() const { return allItems_; }

private:
    void setupUI();
    void setupSidebar();
    void setupContentArea();
    void loadFeeds();
    void addArticleCard(const RSSItem& item);
    void addArticleListItem(const RSSItem& item);
    void selectCategory(const std::string& categoryId);
    void selectFeed(const std::string& feedName);
    void updateLayoutToggle();
    bool isRSSFeed(const std::string& content);
    void showArticleContextMenu(GtkWidget* widget, const RSSItem& item, double x, double y);
    
    static void onAddFeedClicked(GtkButton* button, gpointer userData);
    static void onManageFeedsClicked(GtkButton* button, gpointer userData);
    static void onCategorySelected(GtkListBox* listBox, GtkListBoxRow* row, gpointer userData);
    static void onMarkAllReadClicked(GtkButton* button, gpointer userData);
    static void onAddCategoryClicked(GtkButton* button, gpointer userData);

    GtkWidget* mainPaned_;
    GtkWidget* categoryList_;
    GtkWidget* articlesContainer_;
    GtkWidget* articlesScrolled_;
    GtkWidget* categoryTitle_;
    GtkWidget* layoutToggleBtn_;
    
    std::string currentCategory_;
    std::string currentFeed_;  // Empty means show all feeds in category
    std::vector<RSSItem> allItems_;
};

}
