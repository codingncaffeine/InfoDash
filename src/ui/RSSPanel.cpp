#include "ui/RSSPanel.hpp"
#include "utils/Config.hpp"
#include <thread>
#include <algorithm>
#include <mutex>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo.h>

namespace InfoDash {

// Image cache for article cards
struct ImageData { GdkPixbuf* pixbuf; int width; int height; };
static std::map<std::string, ImageData> imageCache;

// Favicon cache for feeds
static std::map<std::string, GdkPixbuf*> faviconCache;

// Helper to get favicon URL from feed URL using Google's favicon service
static std::string getFaviconUrl(const std::string& feedUrl) {
    // Extract domain from feed URL
    size_t start = feedUrl.find("://");
    if (start == std::string::npos) return "";
    start += 3;
    size_t end = feedUrl.find('/', start);
    std::string domain = feedUrl.substr(start, end != std::string::npos ? end - start : std::string::npos);
    // Use Google's reliable favicon service
    return "https://www.google.com/s2/favicons?sz=32&domain=" + domain;
}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

static void onDrawImage(GtkDrawingArea* area, cairo_t* cr, int w, int h, gpointer) {
    const char* url = static_cast<const char*>(g_object_get_data(G_OBJECT(area), "image-url"));
    if (!url) return;
    auto it = imageCache.find(url);
    if (it != imageCache.end() && it->second.pixbuf) {
        double scale = std::min((double)w / it->second.width, (double)h / it->second.height);
        int nw = it->second.width * scale, nh = it->second.height * scale;
        int x = (w - nw) / 2, y = (h - nh) / 2;
        cairo_save(cr);
        cairo_translate(cr, x, y);
        cairo_scale(cr, scale, scale);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gdk_cairo_set_source_pixbuf(cr, it->second.pixbuf, 0, 0);
G_GNUC_END_IGNORE_DEPRECATIONS
        cairo_paint(cr);
        cairo_restore(cr);
    } else {
        cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
        cairo_rectangle(cr, 0, 0, w, h);
        cairo_fill(cr);
    }
}

static void onCardClicked(GtkGestureClick*, gint, gdouble, gdouble, gpointer data) {
    const char* url = static_cast<const char*>(data);
    if (url) {
        std::string cmd = "xdg-open '" + std::string(url) + "'";
        system(cmd.c_str());
    }
}

static gboolean onToggleState(GtkSwitch* sw, gboolean state, gpointer) {
    std::string* url = static_cast<std::string*>(g_object_get_data(G_OBJECT(sw), "feed-url"));
    if (url) {
        auto feeds = Config::getInstance().getFeeds();
        for (auto& f : feeds) {
            if (f.url == *url) {
                f.enabled = state;
                Config::getInstance().updateFeed(*url, f);
                break;
            }
        }
    }
    return FALSE;
}

RSSPanel::RSSPanel() : mainPaned_(nullptr), categoryList_(nullptr), 
                       articlesContainer_(nullptr), articlesScrolled_(nullptr),
                       categoryTitle_(nullptr), layoutToggleBtn_(nullptr),
                       currentCategory_("all"), currentFeed_("") {
    setupUI();
    loadFeeds();
}

RSSPanel::~RSSPanel() {
    for (auto& pair : imageCache) {
        if (pair.second.pixbuf) g_object_unref(pair.second.pixbuf);
    }
    imageCache.clear();
}

void RSSPanel::setupUI() {
    mainPaned_ = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_wide_handle(GTK_PANED(mainPaned_), TRUE);
    setupSidebar();
    setupContentArea();
    gtk_paned_set_position(GTK_PANED(mainPaned_), 250);
    updateSidebar();
}

void RSSPanel::setupSidebar() {
    GtkWidget* sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(sidebar, "sidebar");
    gtk_widget_set_size_request(sidebar, 180, -1);
    
    GtkWidget* headerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(headerBox, 16);
    gtk_widget_set_margin_end(headerBox, 16);
    gtk_widget_set_margin_top(headerBox, 16);
    gtk_widget_set_margin_bottom(headerBox, 8);
    
    GtkWidget* title = gtk_label_new("Feeds");
    gtk_widget_add_css_class(title, "title-2");
    gtk_widget_set_hexpand(title, TRUE);
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_box_append(GTK_BOX(headerBox), title);
    
    GtkWidget* addBtn = gtk_button_new_from_icon_name("list-add-symbolic");
    gtk_widget_add_css_class(addBtn, "flat");
    gtk_widget_add_css_class(addBtn, "circular");
    g_signal_connect(addBtn, "clicked", G_CALLBACK(onAddFeedClicked), this);
    gtk_box_append(GTK_BOX(headerBox), addBtn);
    gtk_box_append(GTK_BOX(sidebar), headerBox);
    
    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    
    categoryList_ = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(categoryList_), GTK_SELECTION_NONE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), categoryList_);
    gtk_box_append(GTK_BOX(sidebar), scrolled);
    
    GtkWidget* btnBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(btnBox, 12);
    gtk_widget_set_margin_end(btnBox, 12);
    gtk_widget_set_margin_top(btnBox, 8);
    gtk_widget_set_margin_bottom(btnBox, 12);
    
    GtkWidget* addCatBtn = gtk_button_new_with_label("Add Category");
    gtk_widget_add_css_class(addCatBtn, "flat");
    g_signal_connect(addCatBtn, "clicked", G_CALLBACK(onAddCategoryClicked), this);
    gtk_box_append(GTK_BOX(btnBox), addCatBtn);
    
    GtkWidget* manageBtn = gtk_button_new_with_label("Manage Feeds");
    gtk_widget_add_css_class(manageBtn, "flat");
    g_signal_connect(manageBtn, "clicked", G_CALLBACK(onManageFeedsClicked), this);
    gtk_box_append(GTK_BOX(btnBox), manageBtn);
    gtk_box_append(GTK_BOX(sidebar), btnBox);
    
    gtk_paned_set_start_child(GTK_PANED(mainPaned_), sidebar);
    gtk_paned_set_shrink_start_child(GTK_PANED(mainPaned_), FALSE);
    gtk_paned_set_resize_start_child(GTK_PANED(mainPaned_), FALSE);
}

void RSSPanel::setupContentArea() {
    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(content, TRUE);
    
    GtkWidget* toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(toolbar, "toolbar");
    gtk_widget_set_margin_start(toolbar, 16);
    gtk_widget_set_margin_end(toolbar, 16);
    gtk_widget_set_margin_top(toolbar, 12);
    gtk_widget_set_margin_bottom(toolbar, 12);
    
    categoryTitle_ = gtk_label_new("All Feeds");
    gtk_widget_add_css_class(categoryTitle_, "title-2");
    gtk_widget_set_hexpand(categoryTitle_, TRUE);
    gtk_label_set_xalign(GTK_LABEL(categoryTitle_), 0);
    gtk_box_append(GTK_BOX(toolbar), categoryTitle_);
    
    GtkWidget* markReadBtn = gtk_button_new_with_label("Mark All Read");
    gtk_widget_add_css_class(markReadBtn, "flat");
    g_signal_connect(markReadBtn, "clicked", G_CALLBACK(onMarkAllReadClicked), this);
    gtk_box_append(GTK_BOX(toolbar), markReadBtn);
    
    // Layout toggle button
    bool isListMode = Config::getInstance().getLayoutMode() == LayoutMode::List;
    layoutToggleBtn_ = gtk_button_new_from_icon_name(
        isListMode ? "view-grid-symbolic" : "view-list-symbolic");
    gtk_widget_add_css_class(layoutToggleBtn_, "flat");
    gtk_widget_add_css_class(layoutToggleBtn_, "circular");
    gtk_widget_set_tooltip_text(layoutToggleBtn_, isListMode ? "Switch to Cards" : "Switch to List");
    g_object_set_data(G_OBJECT(layoutToggleBtn_), "panel", this);
    g_signal_connect(layoutToggleBtn_, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
        RSSPanel* panel = static_cast<RSSPanel*>(data);
        auto& config = Config::getInstance();
        bool isList = config.getLayoutMode() == LayoutMode::List;
        config.setLayoutMode(isList ? LayoutMode::Cards : LayoutMode::List);
        panel->updateLayoutToggle();
        panel->loadFeedsForCategory(panel->currentCategory_);
    }), this);
    gtk_box_append(GTK_BOX(toolbar), layoutToggleBtn_);
    
    GtkWidget* refreshBtn = gtk_button_new_from_icon_name("view-refresh-symbolic");
    gtk_widget_add_css_class(refreshBtn, "flat");
    gtk_widget_add_css_class(refreshBtn, "circular");
    g_object_set_data(G_OBJECT(refreshBtn), "panel", this);
    g_signal_connect(refreshBtn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
        static_cast<RSSPanel*>(data)->refresh();
    }), this);
    gtk_box_append(GTK_BOX(toolbar), refreshBtn);
    gtk_box_append(GTK_BOX(content), toolbar);
    
    GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(content), sep);
    
    articlesScrolled_ = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(articlesScrolled_), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(articlesScrolled_, TRUE);
    
    articlesContainer_ = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(articlesContainer_), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(articlesContainer_), TRUE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(articlesContainer_), 1);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(articlesContainer_), 4);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(articlesContainer_), 16);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(articlesContainer_), 16);
    gtk_widget_set_margin_start(articlesContainer_, 16);
    gtk_widget_set_margin_end(articlesContainer_, 16);
    gtk_widget_set_margin_top(articlesContainer_, 16);
    gtk_widget_set_margin_bottom(articlesContainer_, 16);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(articlesScrolled_), articlesContainer_);
    gtk_box_append(GTK_BOX(content), articlesScrolled_);
    
    gtk_paned_set_end_child(GTK_PANED(mainPaned_), content);
    gtk_paned_set_shrink_end_child(GTK_PANED(mainPaned_), FALSE);
    gtk_paned_set_resize_end_child(GTK_PANED(mainPaned_), TRUE);
}

void RSSPanel::updateLayoutToggle() {
    bool isListMode = Config::getInstance().getLayoutMode() == LayoutMode::List;
    gtk_button_set_icon_name(GTK_BUTTON(layoutToggleBtn_), 
        isListMode ? "view-grid-symbolic" : "view-list-symbolic");
    gtk_widget_set_tooltip_text(layoutToggleBtn_, 
        isListMode ? "Switch to Cards" : "Switch to List");
}

// Expandable sidebar with feeds
void RSSPanel::updateSidebar() {
    GtkWidget* child;
    while ((child = gtk_widget_get_first_child(categoryList_))) 
        gtk_list_box_remove(GTK_LIST_BOX(categoryList_), child);
    
    auto& config = Config::getInstance();
    auto feeds = config.getFeeds();
    auto savedArticles = config.getSavedArticles();
    
    // Helper to count unread for a feed
    auto countUnreadForFeed = [&](const std::string& feedName) -> int {
        int count = 0;
        for (const auto& item : allItems_) {
            if (item.source == feedName && !config.isArticleRead(item.link)) count++;
        }
        return count;
    };
    
    // Helper to count saved articles that actually exist in current feeds
    auto countDisplayableSavedArticles = [&]() -> int {
        int count = 0;
        for (const auto& item : allItems_) {
            if (savedArticles.count(item.link)) count++;
        }
        return count;
    };
    
    // Helper to count unread for category
    auto countUnreadForCategory = [&](const std::string& catId) -> int {
        if (catId == "all") {
            int total = 0;
            for (const auto& item : allItems_) 
                if (!config.isArticleRead(item.link)) total++;
            return total;
        }
        if (catId == "saved") {
            return countDisplayableSavedArticles();
        }
        int count = 0;
        for (const auto& f : feeds) {
            if (f.category == catId && f.enabled) {
                count += countUnreadForFeed(f.name);
            }
        }
        return count;
    };
    
    // Helper to add category row with optional expansion
    auto addCategoryRow = [&](const Category& cat, bool canExpand) {
        GtkWidget* outerBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        
        // Main category row
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_start(row, 8);
        gtk_widget_set_margin_end(row, 12);
        gtk_widget_set_margin_top(row, 8);
        gtk_widget_set_margin_bottom(row, 8);
        
        // Expander arrow (only for categories with feeds)
        if (canExpand) {
            bool isExpanded = config.isCategoryExpanded(cat.id);
            GtkWidget* expander = gtk_button_new_from_icon_name(
                isExpanded ? "pan-down-symbolic" : "pan-end-symbolic");
            gtk_widget_add_css_class(expander, "flat");
            gtk_widget_add_css_class(expander, "circular");
            gtk_widget_add_css_class(expander, "dim-label");
            gtk_widget_set_size_request(expander, 24, 24);
            
            char* catIdCopy = g_strdup(cat.id.c_str());
            g_object_set_data_full(G_OBJECT(expander), "cat-id", catIdCopy, g_free);
            g_object_set_data(G_OBJECT(expander), "panel", this);
            g_signal_connect(expander, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer) {
                const char* catId = static_cast<const char*>(g_object_get_data(G_OBJECT(btn), "cat-id"));
                RSSPanel* panel = static_cast<RSSPanel*>(g_object_get_data(G_OBJECT(btn), "panel"));
                if (catId && panel) {
                    auto& cfg = Config::getInstance();
                    bool nowExpanded = !cfg.isCategoryExpanded(catId);
                    cfg.setCategoryExpanded(catId, nowExpanded);
                    panel->updateSidebar();
                }
            }), nullptr);
            gtk_box_append(GTK_BOX(row), expander);
        } else {
            GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_widget_set_size_request(spacer, 24, 24);
            gtk_box_append(GTK_BOX(row), spacer);
        }
        
        // Category icon - map to proper GTK icons based on category ID
        std::string iconName = "folder-symbolic";
        if (cat.id == "all") iconName = "view-list-symbolic";
        else if (cat.id == "saved") iconName = "starred-symbolic";
        else if (cat.id == "tech") iconName = "computer-symbolic";
        else if (cat.id == "science") iconName = "applications-science-symbolic";
        else if (cat.id == "news") iconName = "newspaper-symbolic";
        else if (cat.id == "gaming") iconName = "input-gaming-symbolic";
        else if (cat.id == "uncategorized") iconName = "folder-symbolic";
        
        GtkWidget* iconW = gtk_image_new_from_icon_name(iconName.c_str());
        gtk_box_append(GTK_BOX(row), iconW);
        
        // Category name (clickable)
        GtkWidget* labelBtn = gtk_button_new_with_label(cat.name.c_str());
        gtk_widget_add_css_class(labelBtn, "flat");
        gtk_widget_set_hexpand(labelBtn, TRUE);
        gtk_button_set_has_frame(GTK_BUTTON(labelBtn), FALSE);
        GtkWidget* labelChild = gtk_button_get_child(GTK_BUTTON(labelBtn));
        if (GTK_IS_LABEL(labelChild)) {
            gtk_label_set_xalign(GTK_LABEL(labelChild), 0);
            if (currentCategory_ == cat.id) {
                gtk_widget_add_css_class(labelChild, "accent");
            }
        }
        
        char* catIdForBtn = g_strdup(cat.id.c_str());
        g_object_set_data_full(G_OBJECT(labelBtn), "cat-id", catIdForBtn, g_free);
        g_object_set_data(G_OBJECT(labelBtn), "panel", this);
        g_signal_connect(labelBtn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer) {
            const char* catId = static_cast<const char*>(g_object_get_data(G_OBJECT(btn), "cat-id"));
            RSSPanel* panel = static_cast<RSSPanel*>(g_object_get_data(G_OBJECT(btn), "panel"));
            if (catId && panel) {
                panel->selectCategory(catId);
                panel->updateSidebar();
            }
        }), nullptr);
        gtk_box_append(GTK_BOX(row), labelBtn);
        
        // Unread count badge
        int count = countUnreadForCategory(cat.id);
        if (count > 0) {
            GtkWidget* badge = gtk_label_new(std::to_string(count).c_str());
            gtk_widget_add_css_class(badge, "badge");
            gtk_box_append(GTK_BOX(row), badge);
        }
        
        gtk_box_append(GTK_BOX(outerBox), row);
        
        // If expanded, show feeds under this category
        if (canExpand && config.isCategoryExpanded(cat.id)) {
            for (const auto& f : feeds) {
                if (f.category == cat.id && f.enabled) {
                    GtkWidget* feedRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
                    gtk_widget_set_margin_start(feedRow, 48);
                    gtk_widget_set_margin_end(feedRow, 12);
                    gtk_widget_set_margin_top(feedRow, 4);
                    gtk_widget_set_margin_bottom(feedRow, 4);
                    
                    // Favicon for the feed
                    std::string faviconUrl = getFaviconUrl(f.url);
                    GtkWidget* faviconImg = gtk_image_new_from_icon_name("application-rss+xml-symbolic");
                    gtk_image_set_pixel_size(GTK_IMAGE(faviconImg), 16);
                    gtk_widget_add_css_class(faviconImg, "dim-label");
                    gtk_box_append(GTK_BOX(feedRow), faviconImg);
                    
                    // Load favicon asynchronously if not cached
                    if (!faviconUrl.empty() && faviconCache.find(faviconUrl) == faviconCache.end()) {
                        faviconCache[faviconUrl] = nullptr;
                        std::string url = faviconUrl;
                        GtkWidget* imgWidget = faviconImg;
                        g_object_ref(imgWidget);
                        std::thread([url, imgWidget]() {
                            CURL* curl = curl_easy_init();
                            if (curl) {
                                std::string data;
                                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
                                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
                                curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36");
                                if (curl_easy_perform(curl) == CURLE_OK && !data.empty()) {
                                    GInputStream* stream = g_memory_input_stream_new_from_data(
                                        g_memdup2(data.data(), data.size()), data.size(), g_free);
                                    GdkPixbuf* pb = gdk_pixbuf_new_from_stream_at_scale(stream, 16, 16, TRUE, nullptr, nullptr);
                                    g_object_unref(stream);
                                    if (pb) {
                                        faviconCache[url] = pb;
                                        // Update the widget on the main thread
                                        auto* updateData = new std::pair<GtkWidget*, GdkPixbuf*>(imgWidget, pb);
                                        g_idle_add([](gpointer data) -> gboolean {
                                            auto* p = static_cast<std::pair<GtkWidget*, GdkPixbuf*>*>(data);
                                            if (GTK_IS_IMAGE(p->first)) {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                                                GdkTexture* texture = gdk_texture_new_for_pixbuf(p->second);
G_GNUC_END_IGNORE_DEPRECATIONS
                                                gtk_image_set_from_paintable(GTK_IMAGE(p->first), GDK_PAINTABLE(texture));
                                                g_object_unref(texture);
                                                gtk_widget_remove_css_class(p->first, "dim-label");
                                            }
                                            g_object_unref(p->first);
                                            delete p;
                                            return G_SOURCE_REMOVE;
                                        }, updateData);
                                    } else {
                                        g_object_unref(imgWidget);
                                    }
                                } else {
                                    g_object_unref(imgWidget);
                                }
                                curl_easy_cleanup(curl);
                            } else {
                                g_object_unref(imgWidget);
                            }
                        }).detach();
                    } else if (faviconCache.count(faviconUrl) && faviconCache[faviconUrl]) {
                        // Use cached favicon
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                        GdkTexture* texture = gdk_texture_new_for_pixbuf(faviconCache[faviconUrl]);
G_GNUC_END_IGNORE_DEPRECATIONS
                        gtk_image_set_from_paintable(GTK_IMAGE(faviconImg), GDK_PAINTABLE(texture));
                        g_object_unref(texture);
                        gtk_widget_remove_css_class(faviconImg, "dim-label");
                    }
                    
                    // Feed name as clickable button
                    GtkWidget* feedBtn = gtk_button_new_with_label(f.name.c_str());
                    gtk_widget_add_css_class(feedBtn, "flat");
                    gtk_widget_set_hexpand(feedBtn, TRUE);
                    gtk_button_set_has_frame(GTK_BUTTON(feedBtn), FALSE);
                    GtkWidget* feedBtnChild = gtk_button_get_child(GTK_BUTTON(feedBtn));
                    if (GTK_IS_LABEL(feedBtnChild)) {
                        gtk_label_set_xalign(GTK_LABEL(feedBtnChild), 0);
                        gtk_label_set_ellipsize(GTK_LABEL(feedBtnChild), PANGO_ELLIPSIZE_END);
                        // Highlight if this feed is currently selected
                        if (currentFeed_ == f.name) {
                            gtk_widget_add_css_class(feedBtnChild, "accent");
                        }
                    }
                    
                    // Store feed name and panel pointer for click handler
                    char* feedNameCopy = g_strdup(f.name.c_str());
                    g_object_set_data_full(G_OBJECT(feedBtn), "feed-name", feedNameCopy, g_free);
                    g_object_set_data(G_OBJECT(feedBtn), "panel", this);
                    g_signal_connect(feedBtn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer) {
                        const char* feedName = static_cast<const char*>(g_object_get_data(G_OBJECT(btn), "feed-name"));
                        RSSPanel* panel = static_cast<RSSPanel*>(g_object_get_data(G_OBJECT(btn), "panel"));
                        if (feedName && panel) {
                            panel->selectFeed(feedName);
                        }
                    }), nullptr);
                    gtk_box_append(GTK_BOX(feedRow), feedBtn);
                    
                    int feedCount = countUnreadForFeed(f.name);
                    if (feedCount > 0) {
                        GtkWidget* feedBadge = gtk_label_new(std::to_string(feedCount).c_str());
                        gtk_widget_add_css_class(feedBadge, "badge");
                        gtk_widget_add_css_class(feedBadge, "small");
                        gtk_box_append(GTK_BOX(feedRow), feedBadge);
                    }
                    
                    gtk_box_append(GTK_BOX(outerBox), feedRow);
                }
            }
        }
        
        GtkWidget* listRow = gtk_list_box_row_new();
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(listRow), outerBox);
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(listRow), FALSE);
        gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(listRow), FALSE);
        gtk_list_box_append(GTK_LIST_BOX(categoryList_), listRow);
    };
    
    // Add "All Feeds" (no expand)
    addCategoryRow({"all", "All Feeds", "view-list-symbolic", 0}, false);
    
    // Add "Saved for Later" (no expand)
    addCategoryRow({"saved", "Saved for Later", "starred-symbolic", 1}, false);
    
    // Add regular categories with expansion
    for (const auto& cat : config.getCategories()) {
        if (cat.id == "all" || cat.id == "saved") continue;
        
        // Check if category has feeds
        bool hasFeeds = false;
        for (const auto& f : feeds) {
            if (f.category == cat.id && f.enabled) { hasFeeds = true; break; }
        }
        
        addCategoryRow(cat, hasFeeds);
    }
}

void RSSPanel::selectCategory(const std::string& categoryId) {
    currentCategory_ = categoryId;
    currentFeed_ = "";  // Clear feed filter when selecting a category
    auto categories = Config::getInstance().getCategories();
    std::string title = "All Feeds";
    if (categoryId == "saved") {
        title = "Saved for Later";
    } else if (categoryId != "all") {
        for (const auto& c : categories) 
            if (c.id == categoryId) { title = c.name; break; }
    }
    gtk_label_set_text(GTK_LABEL(categoryTitle_), title.c_str());
    loadFeedsForCategory(categoryId);
}

void RSSPanel::selectFeed(const std::string& feedName) {
    currentFeed_ = feedName;
    
    // Find which category this feed belongs to and switch to it
    // This fixes the issue where clicking a feed while in "saved" category
    // would only show saved articles from that feed
    auto feeds = Config::getInstance().getFeeds();
    for (const auto& f : feeds) {
        if (f.name == feedName) {
            currentCategory_ = f.category;
            break;
        }
    }
    
    gtk_label_set_text(GTK_LABEL(categoryTitle_), feedName.c_str());
    updateSidebar();  // Update to show selected feed highlighted
    loadFeedsForCategory(currentCategory_);  // Reload with feed filter
}

void RSSPanel::loadFeeds() {
    allItems_.clear();
    auto feeds = Config::getInstance().getFeeds();
    
    int enabledCount = 0;
    for (const auto& f : feeds) if (f.enabled) enabledCount++;
    
    if (enabledCount == 0) {
        updateSidebar();
        loadFeedsForCategory(currentCategory_);
        return;
    }
    
    auto remaining = std::make_shared<int>(enabledCount);
    auto mtx = std::make_shared<std::mutex>();
    
    RSSService service;
    for (const auto& f : feeds) {
        if (!f.enabled) continue;
        service.fetchFeed(f.url, [this, fname = f.name, remaining, mtx](std::vector<RSSItem> items) {
            {
                std::lock_guard<std::mutex> lock(*mtx);
                for (auto& item : items) {
                    item.source = fname;
                    allItems_.push_back(item);
                }
                (*remaining)--;
            }
            
            if (*remaining == 0) {
                g_idle_add(+[](gpointer data) -> gboolean {
                    auto* panel = static_cast<RSSPanel*>(data);
                    panel->updateSidebar();
                    panel->loadFeedsForCategory(panel->currentCategory_);
                    return G_SOURCE_REMOVE;
                }, this);
            }
        });
    }
}

void RSSPanel::loadFeedsForCategory(const std::string& categoryId) {
    auto& config = Config::getInstance();
    bool isListMode = config.getLayoutMode() == LayoutMode::List;
    
    // Recreate the container based on layout mode
    if (articlesContainer_) {
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(articlesScrolled_), nullptr);
    }
    
    if (isListMode) {
        // List layout uses GtkListBox
        articlesContainer_ = gtk_list_box_new();
        gtk_list_box_set_selection_mode(GTK_LIST_BOX(articlesContainer_), GTK_SELECTION_NONE);
        gtk_widget_add_css_class(articlesContainer_, "boxed-list");
        gtk_widget_set_margin_start(articlesContainer_, 16);
        gtk_widget_set_margin_end(articlesContainer_, 16);
        gtk_widget_set_margin_top(articlesContainer_, 16);
        gtk_widget_set_margin_bottom(articlesContainer_, 16);
    } else {
        // Cards layout uses GtkFlowBox
        articlesContainer_ = gtk_flow_box_new();
        gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(articlesContainer_), GTK_SELECTION_NONE);
        gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(articlesContainer_), TRUE);
        gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(articlesContainer_), 1);
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(articlesContainer_), 4);
        gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(articlesContainer_), 16);
        gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(articlesContainer_), 16);
        gtk_widget_set_margin_start(articlesContainer_, 16);
        gtk_widget_set_margin_end(articlesContainer_, 16);
        gtk_widget_set_margin_top(articlesContainer_, 16);
        gtk_widget_set_margin_bottom(articlesContainer_, 16);
    }
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(articlesScrolled_), articlesContainer_);
    
    auto feeds = config.getFeeds();
    
    // Helper lambda to add article based on layout
    auto addArticle = [this, isListMode](const RSSItem& item) {
        if (isListMode) {
            addArticleListItem(item);
        } else {
            addArticleCard(item);
        }
    };
    
    // If a specific feed is selected, show ALL articles from that feed
    // regardless of what category we're viewing (fixes "Saved for Later" then click feed issue)
    if (!currentFeed_.empty()) {
        for (const auto& item : allItems_) {
            if (item.source == currentFeed_) {
                addArticle(item);
            }
        }
        return;
    }
    
    // Handle "Saved for Later" category
    if (categoryId == "saved") {
        auto savedArticles = config.getSavedArticles();
        for (const auto& item : allItems_) {
            if (savedArticles.count(item.link)) {
                addArticle(item);
            }
        }
        return;
    }
    
    // Show articles for the selected category
    for (const auto& item : allItems_) {
        bool show = false;
        if (categoryId == "all") {
            show = true;
        } else {
            for (const auto& f : feeds) {
                if (f.name == item.source && f.category == categoryId && f.enabled) {
                    show = true;
                    break;
                }
            }
        }
        if (show) addArticle(item);
    }
}

void RSSPanel::refresh() { loadFeeds(); }

void RSSPanel::markArticleRead(const std::string& articleUrl) {
    Config::getInstance().markArticleRead(articleUrl);
    updateSidebar();
    loadFeedsForCategory(currentCategory_);
}

void RSSPanel::toggleArticleSaved(const std::string& articleUrl) {
    auto& config = Config::getInstance();
    if (config.isArticleSaved(articleUrl)) {
        config.unsaveArticle(articleUrl);
    } else {
        config.saveArticle(articleUrl);
    }
    updateSidebar();
    if (currentCategory_ == "saved") {
        loadFeedsForCategory(currentCategory_);
    }
}

// Context menu data structure
struct ContextMenuData {
    RSSPanel* panel;
    std::string articleUrl;
    GtkWidget* popover;
    GtkWidget* card;
};

void RSSPanel::showArticleContextMenu(GtkWidget* widget, const RSSItem& item, double x, double y) {
    GtkWidget* popover = gtk_popover_new();
    gtk_widget_set_parent(popover, widget);
    
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(box, 4);
    gtk_widget_set_margin_end(box, 4);
    gtk_widget_set_margin_top(box, 4);
    gtk_widget_set_margin_bottom(box, 4);
    
    auto& config = Config::getInstance();
    bool isRead = config.isArticleRead(item.link);
    bool isSaved = config.isArticleSaved(item.link);
    
    // Mark as Read/Unread button
    GtkWidget* readBtn = gtk_button_new_with_label(isRead ? "Mark as Unread" : "Mark as Read");
    gtk_widget_add_css_class(readBtn, "flat");
    gtk_box_append(GTK_BOX(box), readBtn);
    
    auto* readData = new ContextMenuData{this, item.link, popover, widget};
    g_object_set_data_full(G_OBJECT(readBtn), "data", readData, 
        [](gpointer d) { delete static_cast<ContextMenuData*>(d); });
    g_signal_connect(readBtn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer) {
        auto* data = static_cast<ContextMenuData*>(g_object_get_data(G_OBJECT(btn), "data"));
        if (!data) return;
        auto& cfg = Config::getInstance();
        if (cfg.isArticleRead(data->articleUrl)) {
            cfg.markArticleUnread(data->articleUrl);
        } else {
            cfg.markArticleRead(data->articleUrl);
        }
        gtk_popover_popdown(GTK_POPOVER(data->popover));
        // Update the card appearance
        if (cfg.isArticleRead(data->articleUrl)) {
            gtk_widget_add_css_class(data->card, "read");
        } else {
            gtk_widget_remove_css_class(data->card, "read");
        }
        data->panel->updateSidebar();
    }), nullptr);
    
    // Save for Later button
    GtkWidget* saveBtn = gtk_button_new_with_label(isSaved ? "Remove from Saved" : "Save for Later");
    gtk_widget_add_css_class(saveBtn, "flat");
    gtk_box_append(GTK_BOX(box), saveBtn);
    
    auto* saveData = new ContextMenuData{this, item.link, popover, widget};
    g_object_set_data_full(G_OBJECT(saveBtn), "data", saveData,
        [](gpointer d) { delete static_cast<ContextMenuData*>(d); });
    g_signal_connect(saveBtn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer) {
        auto* data = static_cast<ContextMenuData*>(g_object_get_data(G_OBJECT(btn), "data"));
        if (!data) return;
        data->panel->toggleArticleSaved(data->articleUrl);
        gtk_popover_popdown(GTK_POPOVER(data->popover));
    }), nullptr);
    
    // Open in Browser button
    GtkWidget* openBtn = gtk_button_new_with_label("Open in Browser");
    gtk_widget_add_css_class(openBtn, "flat");
    gtk_box_append(GTK_BOX(box), openBtn);
    
    char* urlCopy = g_strdup(item.link.c_str());
    g_object_set_data_full(G_OBJECT(openBtn), "url", urlCopy, g_free);
    g_object_set_data(G_OBJECT(openBtn), "popover", popover);
    g_signal_connect(openBtn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer) {
        const char* url = static_cast<const char*>(g_object_get_data(G_OBJECT(btn), "url"));
        GtkWidget* pop = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(btn), "popover"));
        if (url) {
            std::string cmd = "xdg-open '" + std::string(url) + "'";
            system(cmd.c_str());
        }
        gtk_popover_popdown(GTK_POPOVER(pop));
    }), nullptr);
    
    gtk_popover_set_child(GTK_POPOVER(popover), box);
    
    GdkRectangle rect = {(int)x, (int)y, 1, 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    gtk_popover_popup(GTK_POPOVER(popover));
}

void RSSPanel::addArticleCard(const RSSItem& item) {
    bool isRead = Config::getInstance().isArticleRead(item.link);
    bool isSaved = Config::getInstance().isArticleSaved(item.link);
    
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(card, "card");
    gtk_widget_set_size_request(card, 280, 320);
    if (isRead) gtk_widget_add_css_class(card, "read");
    if (isSaved) gtk_widget_add_css_class(card, "saved");
    
    // Right-click gesture for context menu
    GtkGesture* rightClick = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(rightClick), GDK_BUTTON_SECONDARY);
    
    // Store item data for context menu
    auto* itemData = new RSSItem(item);
    g_object_set_data_full(G_OBJECT(card), "item-data", itemData,
        [](gpointer d) { delete static_cast<RSSItem*>(d); });
    g_object_set_data(G_OBJECT(card), "panel", this);
    
    g_signal_connect(rightClick, "pressed", G_CALLBACK(+[](GtkGestureClick* gesture, gint, gdouble x, gdouble y, gpointer userData) {
        GtkWidget* card = static_cast<GtkWidget*>(userData);
        RSSItem* item = static_cast<RSSItem*>(g_object_get_data(G_OBJECT(card), "item-data"));
        RSSPanel* panel = static_cast<RSSPanel*>(g_object_get_data(G_OBJECT(card), "panel"));
        if (item && panel) {
            panel->showArticleContextMenu(card, *item, x, y);
        }
    }), card);
    gtk_widget_add_controller(card, GTK_EVENT_CONTROLLER(rightClick));
    
    if (!item.imageUrl.empty()) {
        GtkWidget* imageArea = gtk_drawing_area_new();
        gtk_widget_set_size_request(imageArea, 280, 160);
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(imageArea), onDrawImage, nullptr, nullptr);
        char* urlCopy = g_strdup(item.imageUrl.c_str());
        g_object_set_data_full(G_OBJECT(imageArea), "image-url", urlCopy, g_free);
        gtk_box_append(GTK_BOX(card), imageArea);
        
        if (imageCache.find(item.imageUrl) == imageCache.end()) {
            imageCache[item.imageUrl] = {nullptr, 0, 0};
            std::string imgUrl = item.imageUrl;
            std::thread([imgUrl]() {
                CURL* curl = curl_easy_init();
                if (curl) {
                    std::string data;
                    curl_easy_setopt(curl, CURLOPT_URL, imgUrl.c_str());
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
                    if (curl_easy_perform(curl) == CURLE_OK && !data.empty()) {
                        GInputStream* stream = g_memory_input_stream_new_from_data(
                            g_memdup2(data.data(), data.size()), data.size(), g_free);
                        GdkPixbuf* pb = gdk_pixbuf_new_from_stream(stream, nullptr, nullptr);
                        g_object_unref(stream);
                        if (pb) {
                            std::string url = imgUrl;
                            g_idle_add([](gpointer data) -> gboolean {
                                auto* p = static_cast<std::pair<std::string, GdkPixbuf*>*>(data);
                                imageCache[p->first] = {p->second, gdk_pixbuf_get_width(p->second),
                                                        gdk_pixbuf_get_height(p->second)};
                                delete p;
                                return FALSE;
                            }, new std::pair<std::string, GdkPixbuf*>(url, pb));
                        }
                    }
                    curl_easy_cleanup(curl);
                }
            }).detach();
        }
    } else {
        GtkWidget* placeholder = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_size_request(placeholder, 280, 100);
        gtk_widget_set_valign(placeholder, GTK_ALIGN_CENTER);
        GtkWidget* icon = gtk_image_new_from_icon_name("application-rss+xml-symbolic");
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 48);
        gtk_widget_add_css_class(icon, "dim-label");
        gtk_box_append(GTK_BOX(placeholder), icon);
        gtk_box_append(GTK_BOX(card), placeholder);
    }
    
    GtkWidget* contentBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(contentBox, 12);
    gtk_widget_set_margin_end(contentBox, 12);
    gtk_widget_set_margin_top(contentBox, 12);
    gtk_widget_set_margin_bottom(contentBox, 12);
    gtk_widget_set_vexpand(contentBox, TRUE);
    
    // Title with saved indicator
    GtkWidget* titleBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    if (isSaved) {
        GtkWidget* star = gtk_image_new_from_icon_name("starred-symbolic");
        gtk_widget_add_css_class(star, "accent");
        gtk_box_append(GTK_BOX(titleBox), star);
    }
    
    GtkWidget* titleLabel = gtk_label_new(item.title.c_str());
    gtk_widget_add_css_class(titleLabel, "heading");
    gtk_label_set_wrap(GTK_LABEL(titleLabel), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(titleLabel), 35);
    gtk_label_set_lines(GTK_LABEL(titleLabel), 2);
    gtk_label_set_ellipsize(GTK_LABEL(titleLabel), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign(GTK_LABEL(titleLabel), 0);
    gtk_widget_set_hexpand(titleLabel, TRUE);
    gtk_box_append(GTK_BOX(titleBox), titleLabel);
    gtk_box_append(GTK_BOX(contentBox), titleBox);
    
    if (!item.description.empty()) {
        std::string desc = item.description.substr(0, 150);
        size_t lt = desc.find('<');
        while (lt != std::string::npos) {
            size_t gt = desc.find('>', lt);
            if (gt != std::string::npos) desc.erase(lt, gt - lt + 1);
            else break;
            lt = desc.find('<');
        }
        if (!desc.empty()) {
            GtkWidget* descLabel = gtk_label_new(desc.c_str());
            gtk_widget_add_css_class(descLabel, "dim-label");
            gtk_label_set_wrap(GTK_LABEL(descLabel), TRUE);
            gtk_label_set_max_width_chars(GTK_LABEL(descLabel), 40);
            gtk_label_set_lines(GTK_LABEL(descLabel), 3);
            gtk_label_set_ellipsize(GTK_LABEL(descLabel), PANGO_ELLIPSIZE_END);
            gtk_label_set_xalign(GTK_LABEL(descLabel), 0);
            gtk_box_append(GTK_BOX(contentBox), descLabel);
        }
    }
    
    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(contentBox), spacer);
    
    GtkWidget* metaBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* sourceLabel = gtk_label_new(item.source.c_str());
    gtk_widget_add_css_class(sourceLabel, "dim-label");
    gtk_widget_add_css_class(sourceLabel, "caption");
    gtk_label_set_ellipsize(GTK_LABEL(sourceLabel), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(sourceLabel, TRUE);
    gtk_label_set_xalign(GTK_LABEL(sourceLabel), 0);
    gtk_box_append(GTK_BOX(metaBox), sourceLabel);
    
    if (!item.pubDate.empty()) {
        std::string date = item.pubDate;
        if (date.length() > 16) date = date.substr(0, 16);
        GtkWidget* dateLabel = gtk_label_new(date.c_str());
        gtk_widget_add_css_class(dateLabel, "dim-label");
        gtk_widget_add_css_class(dateLabel, "caption");
        gtk_box_append(GTK_BOX(metaBox), dateLabel);
    }
    gtk_box_append(GTK_BOX(contentBox), metaBox);
    gtk_box_append(GTK_BOX(card), contentBox);
    
    // Left click gesture for opening article
    GtkGesture* leftClick = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(leftClick), GDK_BUTTON_PRIMARY);
    char* linkCopy = g_strdup(item.link.c_str());
    g_object_set_data_full(G_OBJECT(leftClick), "link", linkCopy, g_free);
    g_object_set_data(G_OBJECT(leftClick), "card", card);
    g_object_set_data(G_OBJECT(leftClick), "panel", this);
    g_signal_connect(leftClick, "pressed", G_CALLBACK(+[](GtkGestureClick*, gint, gdouble, gdouble, gpointer userData) {
        GtkGesture* gesture = static_cast<GtkGesture*>(userData);
        const char* link = static_cast<const char*>(g_object_get_data(G_OBJECT(gesture), "link"));
        GtkWidget* card = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(gesture), "card"));
        RSSPanel* panel = static_cast<RSSPanel*>(g_object_get_data(G_OBJECT(gesture), "panel"));
        if (link) {
            // Mark as read and update appearance
            Config::getInstance().markArticleRead(link);
            gtk_widget_add_css_class(card, "read");
            if (panel) panel->updateSidebar();
            
            std::string cmd = "xdg-open '" + std::string(link) + "'";
            system(cmd.c_str());
        }
    }), leftClick);
    gtk_widget_add_controller(card, GTK_EVENT_CONTROLLER(leftClick));
    
    gtk_flow_box_append(GTK_FLOW_BOX(articlesContainer_), card);
}

// List layout item - horizontal layout like Feedly
void RSSPanel::addArticleListItem(const RSSItem& item) {
    bool isRead = Config::getInstance().isArticleRead(item.link);
    bool isSaved = Config::getInstance().isArticleSaved(item.link);
    
    // Main row container
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(row, "list-item");
    gtk_widget_set_margin_start(row, 12);
    gtk_widget_set_margin_end(row, 12);
    gtk_widget_set_margin_top(row, 8);
    gtk_widget_set_margin_bottom(row, 8);
    if (isRead) gtk_widget_add_css_class(row, "read");
    if (isSaved) gtk_widget_add_css_class(row, "saved");
    
    // Right-click gesture for context menu
    GtkGesture* rightClick = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(rightClick), GDK_BUTTON_SECONDARY);
    
    auto* itemData = new RSSItem(item);
    g_object_set_data_full(G_OBJECT(row), "item-data", itemData,
        [](gpointer d) { delete static_cast<RSSItem*>(d); });
    g_object_set_data(G_OBJECT(row), "panel", this);
    
    g_signal_connect(rightClick, "pressed", G_CALLBACK(+[](GtkGestureClick*, gint, gdouble x, gdouble y, gpointer userData) {
        GtkWidget* row = static_cast<GtkWidget*>(userData);
        RSSItem* item = static_cast<RSSItem*>(g_object_get_data(G_OBJECT(row), "item-data"));
        RSSPanel* panel = static_cast<RSSPanel*>(g_object_get_data(G_OBJECT(row), "panel"));
        if (item && panel) {
            panel->showArticleContextMenu(row, *item, x, y);
        }
    }), row);
    gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(rightClick));
    
    // Thumbnail image on the left (small, fixed size)
    if (!item.imageUrl.empty()) {
        GtkWidget* imageArea = gtk_drawing_area_new();
        gtk_widget_set_size_request(imageArea, 100, 70);
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(imageArea), onDrawImage, nullptr, nullptr);
        char* urlCopy = g_strdup(item.imageUrl.c_str());
        g_object_set_data_full(G_OBJECT(imageArea), "image-url", urlCopy, g_free);
        gtk_box_append(GTK_BOX(row), imageArea);
        
        // Load image if not cached
        if (imageCache.find(item.imageUrl) == imageCache.end()) {
            imageCache[item.imageUrl] = {nullptr, 0, 0};
            std::string imgUrl = item.imageUrl;
            std::thread([imgUrl]() {
                CURL* curl = curl_easy_init();
                if (curl) {
                    std::string data;
                    curl_easy_setopt(curl, CURLOPT_URL, imgUrl.c_str());
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
                    if (curl_easy_perform(curl) == CURLE_OK && !data.empty()) {
                        GInputStream* stream = g_memory_input_stream_new_from_data(
                            g_memdup2(data.data(), data.size()), data.size(), g_free);
                        GdkPixbuf* pb = gdk_pixbuf_new_from_stream(stream, nullptr, nullptr);
                        g_object_unref(stream);
                        if (pb) {
                            std::string url = imgUrl;
                            g_idle_add([](gpointer data) -> gboolean {
                                auto* p = static_cast<std::pair<std::string, GdkPixbuf*>*>(data);
                                imageCache[p->first] = {p->second, gdk_pixbuf_get_width(p->second),
                                                        gdk_pixbuf_get_height(p->second)};
                                delete p;
                                return FALSE;
                            }, new std::pair<std::string, GdkPixbuf*>(url, pb));
                        }
                    }
                    curl_easy_cleanup(curl);
                }
            }).detach();
        }
    } else {
        // Placeholder when no image
        GtkWidget* placeholder = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_size_request(placeholder, 100, 70);
        GtkWidget* icon = gtk_image_new_from_icon_name("application-rss+xml-symbolic");
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 32);
        gtk_widget_add_css_class(icon, "dim-label");
        gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
        gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
        gtk_widget_set_vexpand(icon, TRUE);
        gtk_box_append(GTK_BOX(placeholder), icon);
        gtk_box_append(GTK_BOX(row), placeholder);
    }
    
    // Content area on the right
    GtkWidget* contentBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(contentBox, TRUE);
    gtk_widget_set_valign(contentBox, GTK_ALIGN_CENTER);
    
    // Title row with saved indicator
    GtkWidget* titleBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    if (isSaved) {
        GtkWidget* star = gtk_image_new_from_icon_name("starred-symbolic");
        gtk_widget_add_css_class(star, "accent");
        gtk_box_append(GTK_BOX(titleBox), star);
    }
    
    GtkWidget* titleLabel = gtk_label_new(item.title.c_str());
    gtk_widget_add_css_class(titleLabel, "heading");
    gtk_label_set_wrap(GTK_LABEL(titleLabel), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(titleLabel), 80);
    gtk_label_set_lines(GTK_LABEL(titleLabel), 2);
    gtk_label_set_ellipsize(GTK_LABEL(titleLabel), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign(GTK_LABEL(titleLabel), 0);
    gtk_widget_set_hexpand(titleLabel, TRUE);
    gtk_box_append(GTK_BOX(titleBox), titleLabel);
    gtk_box_append(GTK_BOX(contentBox), titleBox);
    
    // Source and date row
    GtkWidget* metaBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    
    GtkWidget* sourceLabel = gtk_label_new(item.source.c_str());
    gtk_widget_add_css_class(sourceLabel, "dim-label");
    gtk_widget_add_css_class(sourceLabel, "caption");
    gtk_box_append(GTK_BOX(metaBox), sourceLabel);
    
    if (!item.pubDate.empty()) {
        GtkWidget* dot = gtk_label_new("•");
        gtk_widget_add_css_class(dot, "dim-label");
        gtk_box_append(GTK_BOX(metaBox), dot);
        
        std::string date = item.pubDate;
        if (date.length() > 16) date = date.substr(0, 16);
        GtkWidget* dateLabel = gtk_label_new(date.c_str());
        gtk_widget_add_css_class(dateLabel, "dim-label");
        gtk_widget_add_css_class(dateLabel, "caption");
        gtk_box_append(GTK_BOX(metaBox), dateLabel);
    }
    gtk_box_append(GTK_BOX(contentBox), metaBox);
    
    // Description snippet
    if (!item.description.empty()) {
        std::string desc = item.description.substr(0, 200);
        size_t lt = desc.find('<');
        while (lt != std::string::npos) {
            size_t gt = desc.find('>', lt);
            if (gt != std::string::npos) desc.erase(lt, gt - lt + 1);
            else break;
            lt = desc.find('<');
        }
        // Clean up extra whitespace
        size_t pos = 0;
        while ((pos = desc.find("  ")) != std::string::npos) {
            desc.erase(pos, 1);
        }
        if (!desc.empty()) {
            GtkWidget* descLabel = gtk_label_new(desc.c_str());
            gtk_widget_add_css_class(descLabel, "dim-label");
            gtk_label_set_wrap(GTK_LABEL(descLabel), TRUE);
            gtk_label_set_max_width_chars(GTK_LABEL(descLabel), 100);
            gtk_label_set_lines(GTK_LABEL(descLabel), 2);
            gtk_label_set_ellipsize(GTK_LABEL(descLabel), PANGO_ELLIPSIZE_END);
            gtk_label_set_xalign(GTK_LABEL(descLabel), 0);
            gtk_box_append(GTK_BOX(contentBox), descLabel);
        }
    }
    
    gtk_box_append(GTK_BOX(row), contentBox);
    
    // Left click gesture for opening article
    GtkGesture* leftClick = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(leftClick), GDK_BUTTON_PRIMARY);
    char* linkCopy = g_strdup(item.link.c_str());
    g_object_set_data_full(G_OBJECT(leftClick), "link", linkCopy, g_free);
    g_object_set_data(G_OBJECT(leftClick), "row", row);
    g_object_set_data(G_OBJECT(leftClick), "panel", this);
    g_signal_connect(leftClick, "pressed", G_CALLBACK(+[](GtkGestureClick*, gint, gdouble, gdouble, gpointer userData) {
        GtkGesture* gesture = static_cast<GtkGesture*>(userData);
        const char* link = static_cast<const char*>(g_object_get_data(G_OBJECT(gesture), "link"));
        GtkWidget* row = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(gesture), "row"));
        RSSPanel* panel = static_cast<RSSPanel*>(g_object_get_data(G_OBJECT(gesture), "panel"));
        if (link) {
            Config::getInstance().markArticleRead(link);
            gtk_widget_add_css_class(row, "read");
            if (panel) panel->updateSidebar();
            
            std::string cmd = "xdg-open '" + std::string(link) + "'";
            system(cmd.c_str());
        }
    }), leftClick);
    gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(leftClick));
    
    // Wrap in list box row
    GtkWidget* listRow = gtk_list_box_row_new();
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(listRow), row);
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(listRow), FALSE);
    gtk_list_box_append(GTK_LIST_BOX(articlesContainer_), listRow);
}

// Dialog structures
struct ManageEditData { RSSPanel* panel; std::string url; GtkWidget* parentDlg; };
struct ManageDelData { RSSPanel* panel; std::string url; GtkWidget* dialog; };

static void onManageEditBtn(GtkButton* btn, gpointer) {
    auto* d = static_cast<ManageEditData*>(g_object_get_data(G_OBJECT(btn), "data"));
    if (d) d->panel->showEditFeedDialog(d->url, d->parentDlg);
}

static void onManageDelBtn(GtkButton* btn, gpointer) {
    auto* d = static_cast<ManageDelData*>(g_object_get_data(G_OBJECT(btn), "data"));
    if (!d) return;
    Config::getInstance().removeFeed(d->url);
    gtk_window_close(GTK_WINDOW(d->dialog));
    d->panel->showFeedManagementDialog();
    d->panel->refresh();
}

static void onManageAddBtn(GtkButton* btn, gpointer) {
    RSSPanel* panel = static_cast<RSSPanel*>(g_object_get_data(G_OBJECT(btn), "panel"));
    GtkWidget* dialog = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(btn), "dialog"));
    if (panel) {
        gtk_window_close(GTK_WINDOW(dialog));
        panel->showAddFeedDialog();
    }
}

void RSSPanel::showFeedManagementDialog() {
    GtkWidget* dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Manage Feeds");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 450);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(gtk_widget_get_root(mainPaned_)));
    
    GtkWidget* mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(header, 16);
    gtk_widget_set_margin_end(header, 16);
    gtk_widget_set_margin_top(header, 16);
    gtk_widget_set_margin_bottom(header, 8);
    GtkWidget* titleLbl = gtk_label_new("Manage Feeds");
    gtk_widget_add_css_class(titleLbl, "title-2");
    gtk_widget_set_hexpand(titleLbl, TRUE);
    gtk_label_set_xalign(GTK_LABEL(titleLbl), 0);
    gtk_box_append(GTK_BOX(header), titleLbl);
    gtk_box_append(GTK_BOX(mainBox), header);
    
    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    
    GtkWidget* listBox = gtk_list_box_new();
    gtk_widget_set_margin_start(listBox, 16);
    gtk_widget_set_margin_end(listBox, 16);
    gtk_widget_set_margin_top(listBox, 8);
    gtk_widget_set_margin_bottom(listBox, 8);
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listBox), GTK_SELECTION_NONE);
    gtk_widget_add_css_class(listBox, "boxed-list");
    
    auto feeds = Config::getInstance().getFeeds();
    for (const auto& f : feeds) {
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_margin_start(row, 12);
        gtk_widget_set_margin_end(row, 12);
        gtk_widget_set_margin_top(row, 8);
        gtk_widget_set_margin_bottom(row, 8);
        
        GtkWidget* icon = gtk_image_new_from_icon_name("application-rss+xml-symbolic");
        gtk_box_append(GTK_BOX(row), icon);
        
        GtkWidget* infoBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_hexpand(infoBox, TRUE);
        GtkWidget* nameLabel = gtk_label_new(f.name.empty() ? f.url.c_str() : f.name.c_str());
        gtk_label_set_xalign(GTK_LABEL(nameLabel), 0);
        gtk_label_set_ellipsize(GTK_LABEL(nameLabel), PANGO_ELLIPSIZE_END);
        gtk_box_append(GTK_BOX(infoBox), nameLabel);
        GtkWidget* urlLabel = gtk_label_new(f.url.c_str());
        gtk_widget_add_css_class(urlLabel, "dim-label");
        gtk_widget_add_css_class(urlLabel, "caption");
        gtk_label_set_xalign(GTK_LABEL(urlLabel), 0);
        gtk_label_set_ellipsize(GTK_LABEL(urlLabel), PANGO_ELLIPSIZE_END);
        gtk_box_append(GTK_BOX(infoBox), urlLabel);
        gtk_box_append(GTK_BOX(row), infoBox);
        
        GtkWidget* sw = gtk_switch_new();
        gtk_switch_set_active(GTK_SWITCH(sw), f.enabled);
        gtk_widget_set_valign(sw, GTK_ALIGN_CENTER);
        std::string* urlPtr = new std::string(f.url);
        g_object_set_data_full(G_OBJECT(sw), "feed-url", urlPtr, [](gpointer d) { delete static_cast<std::string*>(d); });
        g_signal_connect(sw, "state-set", G_CALLBACK(onToggleState), nullptr);
        gtk_box_append(GTK_BOX(row), sw);
        
        GtkWidget* editBtn = gtk_button_new_from_icon_name("document-edit-symbolic");
        gtk_widget_add_css_class(editBtn, "flat");
        auto* editData = new ManageEditData{this, f.url, dialog};
        g_object_set_data_full(G_OBJECT(editBtn), "data", editData, [](gpointer d) { delete static_cast<ManageEditData*>(d); });
        g_signal_connect(editBtn, "clicked", G_CALLBACK(onManageEditBtn), nullptr);
        gtk_box_append(GTK_BOX(row), editBtn);
        
        GtkWidget* delBtn = gtk_button_new_from_icon_name("user-trash-symbolic");
        gtk_widget_add_css_class(delBtn, "flat");
        gtk_widget_add_css_class(delBtn, "destructive-action");
        auto* delData = new ManageDelData{this, f.url, dialog};
        g_object_set_data_full(G_OBJECT(delBtn), "data", delData, [](gpointer d) { delete static_cast<ManageDelData*>(d); });
        g_signal_connect(delBtn, "clicked", G_CALLBACK(onManageDelBtn), nullptr);
        gtk_box_append(GTK_BOX(row), delBtn);
        
        gtk_list_box_append(GTK_LIST_BOX(listBox), row);
    }
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), listBox);
    gtk_box_append(GTK_BOX(mainBox), scrolled);
    
    GtkWidget* btnBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(btnBox, 16);
    gtk_widget_set_margin_end(btnBox, 16);
    gtk_widget_set_margin_top(btnBox, 8);
    gtk_widget_set_margin_bottom(btnBox, 16);
    gtk_widget_set_halign(btnBox, GTK_ALIGN_END);
    
    GtkWidget* addBtn = gtk_button_new_with_label("Add Feed");
    gtk_widget_add_css_class(addBtn, "suggested-action");
    g_object_set_data(G_OBJECT(addBtn), "panel", this);
    g_object_set_data(G_OBJECT(addBtn), "dialog", dialog);
    g_signal_connect(addBtn, "clicked", G_CALLBACK(onManageAddBtn), nullptr);
    gtk_box_append(GTK_BOX(btnBox), addBtn);
    
    GtkWidget* closeBtn = gtk_button_new_with_label("Close");
    g_signal_connect_swapped(closeBtn, "clicked", G_CALLBACK(gtk_window_close), dialog);
    gtk_box_append(GTK_BOX(btnBox), closeBtn);
    gtk_box_append(GTK_BOX(mainBox), btnBox);
    
    gtk_window_set_child(GTK_WINDOW(dialog), mainBox);
    gtk_window_present(GTK_WINDOW(dialog));
}

// Edit feed dialog
struct EditFeedData {
    RSSPanel* panel;
    std::string origUrl;
    GtkWidget* nameEntry;
    GtkWidget* urlEntry;
    GtkWidget* catCombo;
    GtkWidget* dialog;
    GtkWidget* parentDlg;
    std::vector<Category> cats;
};

static void onEditSaveBtn(GtkButton* btn, gpointer) {
    auto* sd = static_cast<EditFeedData*>(g_object_get_data(G_OBJECT(btn), "data"));
    if (!sd) return;
    std::string newName = gtk_entry_buffer_get_text(gtk_entry_get_buffer(GTK_ENTRY(sd->nameEntry)));
    std::string newUrl = gtk_entry_buffer_get_text(gtk_entry_get_buffer(GTK_ENTRY(sd->urlEntry)));
    guint idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(sd->catCombo));
    std::string newCat = idx < sd->cats.size() ? sd->cats[idx].id : "general";
    Config::getInstance().removeFeed(sd->origUrl);
    FeedInfo newFeed{newUrl, newName, newCat, true};
    Config::getInstance().addFeed(newFeed);
    gtk_window_close(GTK_WINDOW(sd->dialog));
    if (sd->parentDlg) gtk_window_close(GTK_WINDOW(sd->parentDlg));
    sd->panel->showFeedManagementDialog();
    sd->panel->refresh();
}

void RSSPanel::showEditFeedDialog(const std::string& feedUrl, GtkWidget* parentDialog) {
    auto feeds = Config::getInstance().getFeeds();
    auto categories = Config::getInstance().getCategories();
    const FeedInfo* feed = nullptr;
    for (const auto& f : feeds) if (f.url == feedUrl) { feed = &f; break; }
    if (!feed) return;
    
    GtkWidget* dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Edit Feed");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 450, 350);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(gtk_widget_get_root(mainPaned_)));
    
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    
    GtkWidget* header = gtk_label_new("Edit Feed");
    gtk_widget_add_css_class(header, "title-2");
    gtk_box_append(GTK_BOX(box), header);
    
    GtkWidget* nameBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* nameLbl = gtk_label_new("Feed Name");
    gtk_label_set_xalign(GTK_LABEL(nameLbl), 0);
    gtk_widget_add_css_class(nameLbl, "dim-label");
    gtk_box_append(GTK_BOX(nameBox), nameLbl);
    GtkWidget* nameEntry = gtk_entry_new();
    gtk_entry_buffer_set_text(gtk_entry_get_buffer(GTK_ENTRY(nameEntry)), feed->name.c_str(), -1);
    gtk_box_append(GTK_BOX(nameBox), nameEntry);
    gtk_box_append(GTK_BOX(box), nameBox);
    
    GtkWidget* urlBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* urlLbl = gtk_label_new("Feed URL");
    gtk_label_set_xalign(GTK_LABEL(urlLbl), 0);
    gtk_widget_add_css_class(urlLbl, "dim-label");
    gtk_box_append(GTK_BOX(urlBox), urlLbl);
    GtkWidget* urlEntry = gtk_entry_new();
    gtk_entry_buffer_set_text(gtk_entry_get_buffer(GTK_ENTRY(urlEntry)), feed->url.c_str(), -1);
    gtk_box_append(GTK_BOX(urlBox), urlEntry);
    gtk_box_append(GTK_BOX(box), urlBox);
    
    GtkWidget* catBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* catLbl = gtk_label_new("Category");
    gtk_label_set_xalign(GTK_LABEL(catLbl), 0);
    gtk_widget_add_css_class(catLbl, "dim-label");
    gtk_box_append(GTK_BOX(catBox), catLbl);
    GtkWidget* catCombo = gtk_drop_down_new(nullptr, nullptr);
    GtkStringList* catList = gtk_string_list_new(nullptr);
    guint selIdx = 0;
    std::vector<Category> filteredCats;
    for (const auto& c : categories) {
        if (c.id != "all" && c.id != "saved") {
            filteredCats.push_back(c);
        }
    }
    for (guint i = 0; i < filteredCats.size(); i++) {
        gtk_string_list_append(catList, filteredCats[i].name.c_str());
        if (filteredCats[i].id == feed->category) selIdx = i;
    }
    gtk_drop_down_set_model(GTK_DROP_DOWN(catCombo), G_LIST_MODEL(catList));
    gtk_drop_down_set_selected(GTK_DROP_DOWN(catCombo), selIdx);
    gtk_box_append(GTK_BOX(catBox), catCombo);
    gtk_box_append(GTK_BOX(box), catBox);
    g_object_unref(catList);
    
    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(box), spacer);
    
    GtkWidget* btnBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btnBox, GTK_ALIGN_END);
    GtkWidget* cancelBtn = gtk_button_new_with_label("Cancel");
    g_signal_connect_swapped(cancelBtn, "clicked", G_CALLBACK(gtk_window_close), dialog);
    gtk_box_append(GTK_BOX(btnBox), cancelBtn);
    
    GtkWidget* saveBtn = gtk_button_new_with_label("Save");
    gtk_widget_add_css_class(saveBtn, "suggested-action");
    auto* sd = new EditFeedData{this, feedUrl, nameEntry, urlEntry, catCombo, dialog, parentDialog, filteredCats};
    g_object_set_data_full(G_OBJECT(saveBtn), "data", sd, 
        [](gpointer d) { delete static_cast<EditFeedData*>(d); });
    g_signal_connect(saveBtn, "clicked", G_CALLBACK(onEditSaveBtn), nullptr);
    gtk_box_append(GTK_BOX(btnBox), saveBtn);
    gtk_box_append(GTK_BOX(box), btnBox);
    
    gtk_window_set_child(GTK_WINDOW(dialog), box);
    gtk_window_present(GTK_WINDOW(dialog));
}

// Add feed dialog
struct AddFeedData {
    RSSPanel* panel;
    GtkWidget* urlEntry;
    GtkWidget* nameEntry;
    GtkWidget* catCombo;
    GtkWidget* dialog;
    std::vector<Category> cats;
};

static void onAddFeedBtn(GtkButton* btn, gpointer) {
    auto* d = static_cast<AddFeedData*>(g_object_get_data(G_OBJECT(btn), "data"));
    if (!d) return;
    std::string url = gtk_entry_buffer_get_text(gtk_entry_get_buffer(GTK_ENTRY(d->urlEntry)));
    std::string name = gtk_entry_buffer_get_text(gtk_entry_get_buffer(GTK_ENTRY(d->nameEntry)));
    if (url.empty()) return;
    
    // Add https:// if missing
    if (url.find("://") == std::string::npos) url = "https://" + url;
    
    // Check if URL looks like a direct RSS feed (ends with .rss, .xml, /feed, /rss, etc)
    bool looksLikeFeed = (url.find(".rss") != std::string::npos ||
                         url.find(".xml") != std::string::npos ||
                         url.find("/feed") != std::string::npos ||
                         url.find("/rss") != std::string::npos ||
                         url.find("/atom") != std::string::npos);
    
    // If it doesn't look like a feed URL, try to discover feeds first
    if (!looksLikeFeed) {
        auto feeds = d->panel->discoverFeeds(url);
        if (!feeds.empty()) {
            gtk_window_close(GTK_WINDOW(d->dialog));
            d->panel->showFeedDiscoveryDialog(feeds, name, "uncategorized");
            return;
        }
    }
    
    guint idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->catCombo));
    std::string cat = idx < d->cats.size() ? d->cats[idx].id : "uncategorized";
    FeedInfo feed{url, name.empty() ? url : name, cat, true};
    Config::getInstance().addFeed(feed);
    gtk_window_close(GTK_WINDOW(d->dialog));
    d->panel->refresh();
}

void RSSPanel::showAddFeedDialog() {
    GtkWidget* dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Add Feed");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 450, 380);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(gtk_widget_get_root(mainPaned_)));
    
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    
    GtkWidget* header = gtk_label_new("Add/Discover Feed");
    gtk_widget_add_css_class(header, "title-2");
    gtk_box_append(GTK_BOX(box), header);
    
    GtkWidget* urlBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* urlLbl = gtk_label_new("Feed URL or Website");
    gtk_label_set_xalign(GTK_LABEL(urlLbl), 0);
    gtk_widget_add_css_class(urlLbl, "dim-label");
    gtk_box_append(GTK_BOX(urlBox), urlLbl);
    
    GtkWidget* urlRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* urlEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(urlEntry), "https://example.com or RSS URL");
    gtk_widget_set_hexpand(urlEntry, TRUE);
    gtk_box_append(GTK_BOX(urlRow), urlEntry);
    
    GtkWidget* discoverBtn = gtk_button_new_with_label("Discover");
    g_object_set_data(G_OBJECT(discoverBtn), "panel", this);
    g_object_set_data(G_OBJECT(discoverBtn), "entry", urlEntry);
    g_object_set_data(G_OBJECT(discoverBtn), "dialog", dialog);
    g_signal_connect(discoverBtn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer) {
        RSSPanel* panel = static_cast<RSSPanel*>(g_object_get_data(G_OBJECT(btn), "panel"));
        GtkWidget* entry = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(btn), "entry"));
        GtkWidget* dlg = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(btn), "dialog"));
        if (!panel || !entry) return;
        std::string url = gtk_entry_buffer_get_text(gtk_entry_get_buffer(GTK_ENTRY(entry)));
        if (url.empty()) return;
        auto feeds = panel->discoverFeeds(url);
        if (!feeds.empty()) {
            gtk_window_close(GTK_WINDOW(dlg));
            panel->showFeedDiscoveryDialog(feeds, "", "uncategorized");
        }
    }), nullptr);
    gtk_box_append(GTK_BOX(urlRow), discoverBtn);
    gtk_box_append(GTK_BOX(urlBox), urlRow);
    gtk_box_append(GTK_BOX(box), urlBox);
    
    GtkWidget* nameBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* nameLbl = gtk_label_new("Feed Name (optional)");
    gtk_label_set_xalign(GTK_LABEL(nameLbl), 0);
    gtk_widget_add_css_class(nameLbl, "dim-label");
    gtk_box_append(GTK_BOX(nameBox), nameLbl);
    GtkWidget* nameEntry = gtk_entry_new();
    gtk_box_append(GTK_BOX(nameBox), nameEntry);
    gtk_box_append(GTK_BOX(box), nameBox);
    
    GtkWidget* catBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* catLbl = gtk_label_new("Category");
    gtk_label_set_xalign(GTK_LABEL(catLbl), 0);
    gtk_widget_add_css_class(catLbl, "dim-label");
    gtk_box_append(GTK_BOX(catBox), catLbl);
    GtkWidget* catCombo = gtk_drop_down_new(nullptr, nullptr);
    GtkStringList* catList = gtk_string_list_new(nullptr);
    auto categories = Config::getInstance().getCategories();
    std::vector<Category> filteredCats;
    for (const auto& c : categories) {
        if (c.id != "all" && c.id != "saved") {
            filteredCats.push_back(c);
            gtk_string_list_append(catList, c.name.c_str());
        }
    }
    gtk_drop_down_set_model(GTK_DROP_DOWN(catCombo), G_LIST_MODEL(catList));
    gtk_box_append(GTK_BOX(catBox), catCombo);
    gtk_box_append(GTK_BOX(box), catBox);
    g_object_unref(catList);
    
    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(box), spacer);
    
    GtkWidget* btnBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btnBox, GTK_ALIGN_END);
    GtkWidget* cancelBtn = gtk_button_new_with_label("Cancel");
    g_signal_connect_swapped(cancelBtn, "clicked", G_CALLBACK(gtk_window_close), dialog);
    gtk_box_append(GTK_BOX(btnBox), cancelBtn);
    
    GtkWidget* addBtn = gtk_button_new_with_label("Add Feed");
    gtk_widget_add_css_class(addBtn, "suggested-action");
    auto* data = new AddFeedData{this, urlEntry, nameEntry, catCombo, dialog, filteredCats};
    g_object_set_data_full(G_OBJECT(addBtn), "data", data,
        [](gpointer d) { delete static_cast<AddFeedData*>(d); });
    g_signal_connect(addBtn, "clicked", G_CALLBACK(onAddFeedBtn), nullptr);
    gtk_box_append(GTK_BOX(btnBox), addBtn);
    gtk_box_append(GTK_BOX(box), btnBox);
    
    gtk_window_set_child(GTK_WINDOW(dialog), box);
    gtk_window_present(GTK_WINDOW(dialog));
}

// Add category dialog
struct AddCatData {
    RSSPanel* panel;
    GtkWidget* nameEntry;
    GtkWidget* dialog;
};

static void onAddCatBtn(GtkButton* btn, gpointer) {
    auto* cd = static_cast<AddCatData*>(g_object_get_data(G_OBJECT(btn), "data"));
    if (!cd) return;
    std::string name = gtk_entry_buffer_get_text(gtk_entry_get_buffer(GTK_ENTRY(cd->nameEntry)));
    if (name.empty()) return;
    std::string id = name;
    std::transform(id.begin(), id.end(), id.begin(), ::tolower);
    std::replace(id.begin(), id.end(), ' ', '-');
    Category cat{id, name, "folder-symbolic", 0};
    Config::getInstance().addCategory(cat);
    gtk_window_close(GTK_WINDOW(cd->dialog));
    cd->panel->updateSidebar();
}

void RSSPanel::showAddCategoryDialog() {
    GtkWidget* dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Add Category");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 350, 200);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(gtk_widget_get_root(mainPaned_)));
    
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    
    GtkWidget* header = gtk_label_new("Add Category");
    gtk_widget_add_css_class(header, "title-2");
    gtk_box_append(GTK_BOX(box), header);
    
    GtkWidget* nameBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* nameLbl = gtk_label_new("Category Name");
    gtk_label_set_xalign(GTK_LABEL(nameLbl), 0);
    gtk_widget_add_css_class(nameLbl, "dim-label");
    gtk_box_append(GTK_BOX(nameBox), nameLbl);
    GtkWidget* nameEntry = gtk_entry_new();
    gtk_box_append(GTK_BOX(nameBox), nameEntry);
    gtk_box_append(GTK_BOX(box), nameBox);
    
    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(box), spacer);
    
    GtkWidget* btnBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btnBox, GTK_ALIGN_END);
    GtkWidget* cancelBtn = gtk_button_new_with_label("Cancel");
    g_signal_connect_swapped(cancelBtn, "clicked", G_CALLBACK(gtk_window_close), dialog);
    gtk_box_append(GTK_BOX(btnBox), cancelBtn);
    
    GtkWidget* addBtn = gtk_button_new_with_label("Add");
    gtk_widget_add_css_class(addBtn, "suggested-action");
    auto* cd = new AddCatData{this, nameEntry, dialog};
    g_object_set_data_full(G_OBJECT(addBtn), "data", cd,
        [](gpointer d) { delete static_cast<AddCatData*>(d); });
    g_signal_connect(addBtn, "clicked", G_CALLBACK(onAddCatBtn), nullptr);
    gtk_box_append(GTK_BOX(btnBox), addBtn);
    gtk_box_append(GTK_BOX(box), btnBox);
    
    gtk_window_set_child(GTK_WINDOW(dialog), box);
    gtk_window_present(GTK_WINDOW(dialog));
}

// Feed discovery
std::vector<DiscoveredFeed> RSSPanel::discoverFeeds(const std::string& url) {
    std::vector<DiscoveredFeed> feeds;
    std::string fullUrl = url;
    if (fullUrl.find("://") == std::string::npos) fullUrl = "https://" + fullUrl;
    
    CURL* curl = curl_easy_init();
    if (!curl) return feeds;
    
    std::string html;
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "InfoDash/1.0");
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK || html.empty()) return feeds;
    
    if (isRSSFeed(html)) {
        feeds.push_back({fullUrl, "Direct RSS Feed", "rss"});
        return feeds;
    }
    
    htmlDocPtr doc = htmlReadMemory(html.c_str(), html.size(), fullUrl.c_str(), nullptr,
        HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (!doc) return feeds;
    
    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (!ctx) { xmlFreeDoc(doc); return feeds; }
    
    xmlXPathObjectPtr result = xmlXPathEvalExpression(
        (const xmlChar*)"//link[@rel='alternate'][@type='application/rss+xml' or @type='application/atom+xml']",
        ctx);
    
    if (result && result->nodesetval) {
        for (int i = 0; i < result->nodesetval->nodeNr; i++) {
            xmlNodePtr node = result->nodesetval->nodeTab[i];
            xmlChar* href = xmlGetProp(node, (const xmlChar*)"href");
            xmlChar* title = xmlGetProp(node, (const xmlChar*)"title");
            xmlChar* type = xmlGetProp(node, (const xmlChar*)"type");
            
            if (href) {
                std::string feedUrl = (const char*)href;
                if (feedUrl.find("://") == std::string::npos) {
                    if (feedUrl[0] == '/') feedUrl = fullUrl.substr(0, fullUrl.find('/', 8)) + feedUrl;
                    else feedUrl = fullUrl + "/" + feedUrl;
                }
                std::string feedTitle = title ? (const char*)title : feedUrl;
                std::string feedType = type ? (const char*)type : "rss";
                feeds.push_back({feedUrl, feedTitle, feedType});
                xmlFree(href);
            }
            if (title) xmlFree(title);
            if (type) xmlFree(type);
        }
    }
    if (result) xmlXPathFreeObject(result);
    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);
    
    return feeds;
}

bool RSSPanel::isRSSFeed(const std::string& content) {
    return content.find("<rss") != std::string::npos || 
           content.find("<feed") != std::string::npos ||
           content.find("<channel>") != std::string::npos;
}

// Feed discovery results dialog
struct SelectFeedsData {
    RSSPanel* panel;
    std::vector<GtkWidget*> checkboxes;
    std::vector<DiscoveredFeed> feeds;
    GtkWidget* dialog;
    GtkWidget* catCombo;
    std::vector<Category> cats;
};

static void onSelectFeedsAdd(GtkButton* btn, gpointer) {
    auto* d = static_cast<SelectFeedsData*>(g_object_get_data(G_OBJECT(btn), "data"));
    if (!d) return;
    guint catIdx = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->catCombo));
    std::string cat = catIdx < d->cats.size() ? d->cats[catIdx].id : "uncategorized";
    for (size_t i = 0; i < d->checkboxes.size(); i++) {
        if (gtk_check_button_get_active(GTK_CHECK_BUTTON(d->checkboxes[i]))) {
            FeedInfo feed{d->feeds[i].url, d->feeds[i].title, cat, true};
            Config::getInstance().addFeed(feed);
        }
    }
    gtk_window_close(GTK_WINDOW(d->dialog));
    d->panel->refresh();
}

void RSSPanel::showFeedDiscoveryDialog(const std::vector<DiscoveredFeed>& feeds, 
                                       const std::string&, const std::string&) {
    GtkWidget* dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Discovered Feeds");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(gtk_widget_get_root(mainPaned_)));
    
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    
    GtkWidget* header = gtk_label_new("Select Feeds to Add");
    gtk_widget_add_css_class(header, "title-2");
    gtk_box_append(GTK_BOX(box), header);
    
    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    
    GtkWidget* listBox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listBox), GTK_SELECTION_NONE);
    gtk_widget_add_css_class(listBox, "boxed-list");
    
    std::vector<GtkWidget*> checkboxes;
    for (const auto& f : feeds) {
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_margin_start(row, 12);
        gtk_widget_set_margin_end(row, 12);
        gtk_widget_set_margin_top(row, 8);
        gtk_widget_set_margin_bottom(row, 8);
        
        GtkWidget* check = gtk_check_button_new();
        gtk_check_button_set_active(GTK_CHECK_BUTTON(check), TRUE);
        gtk_box_append(GTK_BOX(row), check);
        checkboxes.push_back(check);
        
        GtkWidget* infoBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_hexpand(infoBox, TRUE);
        GtkWidget* titleLbl = gtk_label_new(f.title.c_str());
        gtk_label_set_xalign(GTK_LABEL(titleLbl), 0);
        gtk_label_set_ellipsize(GTK_LABEL(titleLbl), PANGO_ELLIPSIZE_END);
        gtk_box_append(GTK_BOX(infoBox), titleLbl);
        GtkWidget* urlLbl = gtk_label_new(f.url.c_str());
        gtk_widget_add_css_class(urlLbl, "dim-label");
        gtk_widget_add_css_class(urlLbl, "caption");
        gtk_label_set_xalign(GTK_LABEL(urlLbl), 0);
        gtk_label_set_ellipsize(GTK_LABEL(urlLbl), PANGO_ELLIPSIZE_END);
        gtk_box_append(GTK_BOX(infoBox), urlLbl);
        gtk_box_append(GTK_BOX(row), infoBox);
        
        gtk_list_box_append(GTK_LIST_BOX(listBox), row);
    }
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), listBox);
    gtk_box_append(GTK_BOX(box), scrolled);
    
    GtkWidget* catBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* catLbl = gtk_label_new("Category:");
    gtk_box_append(GTK_BOX(catBox), catLbl);
    GtkWidget* catCombo = gtk_drop_down_new(nullptr, nullptr);
    GtkStringList* catList = gtk_string_list_new(nullptr);
    auto categories = Config::getInstance().getCategories();
    std::vector<Category> filteredCats;
    for (const auto& c : categories) {
        if (c.id != "all" && c.id != "saved") {
            filteredCats.push_back(c);
            gtk_string_list_append(catList, c.name.c_str());
        }
    }
    gtk_drop_down_set_model(GTK_DROP_DOWN(catCombo), G_LIST_MODEL(catList));
    gtk_widget_set_hexpand(catCombo, TRUE);
    gtk_box_append(GTK_BOX(catBox), catCombo);
    gtk_box_append(GTK_BOX(box), catBox);
    g_object_unref(catList);
    
    GtkWidget* btnBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btnBox, GTK_ALIGN_END);
    GtkWidget* cancelBtn = gtk_button_new_with_label("Cancel");
    g_signal_connect_swapped(cancelBtn, "clicked", G_CALLBACK(gtk_window_close), dialog);
    gtk_box_append(GTK_BOX(btnBox), cancelBtn);
    
    GtkWidget* addBtn = gtk_button_new_with_label("Add Selected");
    gtk_widget_add_css_class(addBtn, "suggested-action");
    auto* data = new SelectFeedsData{this, checkboxes, feeds, dialog, catCombo, filteredCats};
    g_object_set_data_full(G_OBJECT(addBtn), "data", data,
        [](gpointer d) { delete static_cast<SelectFeedsData*>(d); });
    g_signal_connect(addBtn, "clicked", G_CALLBACK(onSelectFeedsAdd), nullptr);
    gtk_box_append(GTK_BOX(btnBox), addBtn);
    gtk_box_append(GTK_BOX(box), btnBox);
    
    gtk_window_set_child(GTK_WINDOW(dialog), box);
    gtk_window_present(GTK_WINDOW(dialog));
}

// Static callbacks
void RSSPanel::onAddFeedClicked(GtkButton*, gpointer data) {
    static_cast<RSSPanel*>(data)->showAddFeedDialog();
}

void RSSPanel::onManageFeedsClicked(GtkButton*, gpointer data) {
    static_cast<RSSPanel*>(data)->showFeedManagementDialog();
}

void RSSPanel::onCategorySelected(GtkListBox*, GtkListBoxRow* row, gpointer data) {
    if (!row) return;
    const char* catId = static_cast<const char*>(g_object_get_data(G_OBJECT(row), "category-id"));
    if (catId) static_cast<RSSPanel*>(data)->selectCategory(catId);
}

void RSSPanel::onMarkAllReadClicked(GtkButton*, gpointer data) {
    auto* panel = static_cast<RSSPanel*>(data);
    auto& config = Config::getInstance();
    for (const auto& item : panel->allItems_) {
        config.markArticleRead(item.link);
    }
    panel->updateSidebar();
    panel->loadFeedsForCategory(panel->currentCategory_);
}

void RSSPanel::onAddCategoryClicked(GtkButton*, gpointer data) {
    static_cast<RSSPanel*>(data)->showAddCategoryDialog();
}

} // namespace InfoDash
