#include "ui/MainWindow.hpp"
#include "ui/RSSPanel.hpp"
#include "ui/WeatherPanel.hpp"
#include "ui/StockPanel.hpp"
#include <iostream>

namespace InfoDash {

MainWindow::MainWindow(GtkApplication* app)
    : window_(nullptr), headerBar_(nullptr), mainStack_(nullptr), stackSwitcher_(nullptr) {
    
    window_ = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window_), "InfoDash");
    gtk_window_set_default_size(GTK_WINDOW(window_), 1400, 900);
    
    applyCSS();
    setupUI();
}

MainWindow::~MainWindow() = default;

void MainWindow::show() {
    gtk_window_present(GTK_WINDOW(window_));
}

void MainWindow::applyCSS() {
    GtkCssProvider* cssProvider = gtk_css_provider_new();
    
    const char* css = R"(
        window {
            background-color: #1a1a2e;
        }
        
        .main-container {
            background-color: #1a1a2e;
        }
        
        /* Sidebar styles */
        .sidebar {
            background-color: #16213e;
            border-right: 1px solid #0f3460;
        }
        
        .sidebar-title {
            font-size: 20px;
            font-weight: bold;
            color: #ffffff;
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
            background-color: #0f3460;
        }
        
        .category-list row:hover:not(:selected) {
            background-color: rgba(15, 52, 96, 0.5);
        }
        
        .category-name {
            font-size: 14px;
            color: #ffffff;
        }
        
        .category-badge {
            font-size: 11px;
            color: #888888;
            background-color: #0f3460;
            padding: 2px 8px;
            border-radius: 10px;
        }
        
        .content-header {
            font-size: 24px;
            font-weight: bold;
            color: #ffffff;
        }
        
        /* Feedly-style article cards */
        .feedly-card {
            background-color: #16213e;
            border-radius: 12px;
        }
        
        .feedly-card:hover {
            background-color: #1a2744;
        }
        
        .article-read {
            opacity: 0.7;
        }
        
        .article-read .feedly-title {
            color: #888888;
        }
        
        .title-read {
            color: #888888 !important;
        }
        
        .unread-indicator {
            color: #e94560;
            font-size: 10px;
        }
        
        .feedly-image-container {
            background-color: #0f3460;
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
            background: linear-gradient(135deg, #e94560 0%, #0f3460 50%, #16213e 100%);
            min-height: 180px;
        }
        
        .feedly-content {
            background-color: transparent;
        }
        
        .feedly-source {
            font-size: 11px;
            font-weight: 600;
            color: #e94560;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        
        .feedly-separator {
            font-size: 10px;
            color: #666666;
        }
        
        .feedly-date {
            font-size: 11px;
            color: #888888;
        }
        
        .feedly-title {
            font-size: 15px;
            font-weight: 700;
            color: #ffffff;
            line-height: 1.3;
            margin-top: 6px;
        }
        
        .feedly-description {
            font-size: 13px;
            color: #aaaaaa;
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
            color: #ffffff;
        }
        
        .boxed-list {
            background-color: #0f3460;
            border-radius: 12px;
        }
        
        .boxed-list row {
            background-color: transparent;
            border-bottom: 1px solid #16213e;
        }
        
        .boxed-list row:last-child {
            border-bottom: none;
        }
        
        .heading {
            font-size: 14px;
            font-weight: 600;
            color: #ffffff;
        }
        
        .dim-label {
            font-size: 12px;
            color: #888888;
        }
        
        .destructive-action {
            color: #ff4444;
        }
        
        .suggested-action {
            background-color: #e94560;
            color: white;
        }
        
        .suggested-action:hover {
            background-color: #ff6b6b;
        }
        
        /* Original styles */
        .panel-card {
            background-color: #16213e;
            border-radius: 12px;
            padding: 16px;
            margin: 8px;
        }
        
        .panel-title {
            font-size: 18px;
            font-weight: bold;
            color: #e94560;
            margin-bottom: 12px;
        }
        
        .article-card {
            background-color: #0f3460;
            border-radius: 8px;
            padding: 12px;
            margin: 6px 0;
        }
        
        .article-card:hover {
            background-color: #1a4a7a;
        }
        
        .article-title {
            font-size: 14px;
            font-weight: bold;
            color: #ffffff;
        }
        
        .article-source {
            font-size: 11px;
            color: #888888;
        }
        
        .article-date {
            font-size: 10px;
            color: #666666;
        }
        
        .weather-card {
            background-color: #0f3460;
            border-radius: 12px;
            padding: 16px;
            margin: 8px;
        }
        
        .weather-temp {
            font-size: 48px;
            font-weight: bold;
            color: #ffffff;
        }
        
        .weather-location {
            font-size: 16px;
            color: #e94560;
        }
        
        .weather-condition {
            font-size: 14px;
            color: #cccccc;
        }
        
        .stock-ticker {
            background-color: #0f3460;
            padding: 8px 16px;
            border-radius: 8px;
        }
        
        .stock-symbol {
            font-size: 14px;
            font-weight: bold;
            color: #ffffff;
        }
        
        .stock-price {
            font-size: 16px;
            color: #ffffff;
        }
        
        .stock-up {
            color: #00ff88;
        }
        
        .stock-down {
            color: #ff4444;
        }
        
        .add-button {
            background-color: #e94560;
            color: white;
            border-radius: 8px;
            padding: 8px 16px;
        }
        
        .add-button:hover {
            background-color: #ff6b6b;
        }
        
        headerbar {
            background-color: #16213e;
            color: white;
        }
        
        stackswitcher button {
            background-color: #0f3460;
            color: white;
            border-radius: 8px;
            margin: 4px;
        }
        
        stackswitcher button:checked {
            background-color: #e94560;
        }
        
        entry {
            background-color: #0f3460;
            color: white;
            border-radius: 6px;
            padding: 8px;
            border: 1px solid #16213e;
        }
        
        entry:focus {
            border-color: #e94560;
        }
        
        scrolledwindow {
            background-color: transparent;
        }
        
        button.flat {
            background-color: transparent;
            color: #cccccc;
        }
        
        button.flat:hover {
            background-color: rgba(255, 255, 255, 0.1);
        }
        
        dropdown {
            background-color: #0f3460;
            color: white;
            border-radius: 6px;
        }
        
        dropdown button {
            background-color: #0f3460;
            color: white;
        }
        
        dropdown popover {
            background-color: #16213e;
        }
        
        dropdown popover listview row {
            color: white;
        }
        
        dropdown popover listview row:selected {
            background-color: #e94560;
        }
        
        .forecast-day {
            background-color: #0f3460;
            border-radius: 8px;
            padding: 12px;
            margin: 4px;
        }
        
        .forecast-day-name {
            font-size: 12px;
            color: #888888;
        }
        
        .forecast-temp-high {
            font-size: 16px;
            font-weight: bold;
            color: #ffffff;
        }
        
        .forecast-temp-low {
            font-size: 14px;
            color: #888888;
        }

        /* Article card styles */
        .card {
            background-color: #16213e;
            border-radius: 12px;
            transition: opacity 0.2s;
        }

        .card:hover {
            background-color: #1a2744;
        }

        .card.read {
            opacity: 0.55;
        }

        .card.read:hover {
            opacity: 0.75;
        }

        .card.saved {
            box-shadow: inset 0 0 0 2px #e94560;
        }

        /* List layout styles */
        .list-item {
            background-color: #16213e;
            border-radius: 8px;
            transition: opacity 0.2s, background-color 0.2s;
        }

        .list-item:hover {
            background-color: #1a2744;
        }

        .list-item.read {
            opacity: 0.55;
        }

        .list-item.read:hover {
            opacity: 0.75;
        }

        .list-item.saved {
            box-shadow: inset 0 0 0 2px #e94560;
        }

        .boxed-list {
            background-color: transparent;
        }

        .boxed-list row {
            background-color: transparent;
            padding: 4px 0;
        }

        .badge {
            font-size: 11px;
            font-weight: 600;
            background-color: #e94560;
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
            color: #e94560;
        }

    )";
    
    gtk_css_provider_load_from_string(cssProvider, css);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(cssProvider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    
    g_object_unref(cssProvider);
}

void MainWindow::setupUI() {
    setupHeaderBar();
    setupMainContent();
}

void MainWindow::setupHeaderBar() {
    headerBar_ = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(window_), headerBar_);
    
    GtkWidget* titleLabel = gtk_label_new("InfoDash");
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(headerBar_), titleLabel);
    
    GtkWidget* refreshButton = gtk_button_new_from_icon_name("view-refresh-symbolic");
    gtk_widget_set_tooltip_text(refreshButton, "Refresh all data");
    g_signal_connect(refreshButton, "clicked", G_CALLBACK(onRefreshClicked), this);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(headerBar_), refreshButton);
    
    GtkWidget* settingsButton = gtk_button_new_from_icon_name("emblem-system-symbolic");
    gtk_widget_set_tooltip_text(settingsButton, "Settings");
    g_signal_connect(settingsButton, "clicked", G_CALLBACK(onSettingsClicked), this);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(headerBar_), settingsButton);
}

void MainWindow::setupMainContent() {
    GtkWidget* mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(mainBox, "main-container");
    gtk_window_set_child(GTK_WINDOW(window_), mainBox);
    
    mainStack_ = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(mainStack_), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(mainStack_), 200);
    
    stackSwitcher_ = gtk_stack_switcher_new();
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(stackSwitcher_), GTK_STACK(mainStack_));
    gtk_widget_set_halign(stackSwitcher_, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(stackSwitcher_, 12);
    gtk_widget_set_margin_bottom(stackSwitcher_, 12);
    
    gtk_box_append(GTK_BOX(mainBox), stackSwitcher_);
    
    rssPanel_ = std::make_unique<RSSPanel>();
    weatherPanel_ = std::make_unique<WeatherPanel>();
    stockPanel_ = std::make_unique<StockPanel>();
    
    gtk_stack_add_titled(GTK_STACK(mainStack_), rssPanel_->getWidget(), "rss", "📰 RSS Feeds");
    gtk_stack_add_titled(GTK_STACK(mainStack_), weatherPanel_->getWidget(), "weather", "🌤️ Weather");
    gtk_stack_add_titled(GTK_STACK(mainStack_), stockPanel_->getWidget(), "stocks", "📈 Stocks");
    
    gtk_widget_set_vexpand(mainStack_, TRUE);
    gtk_widget_set_hexpand(mainStack_, TRUE);
    gtk_box_append(GTK_BOX(mainBox), mainStack_);
}

void MainWindow::onRefreshClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<MainWindow*>(userData);
    self->rssPanel_->refresh();
    self->weatherPanel_->refresh();
    self->stockPanel_->refresh();
}

void MainWindow::onSettingsClicked(GtkButton*, gpointer) {
    // TODO: Show settings dialog
}

}
