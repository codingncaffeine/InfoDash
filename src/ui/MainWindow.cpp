#include "ui/MainWindow.hpp"
#include "ui/RSSPanel.hpp"
#include "ui/WeatherPanel.hpp"
#include "ui/StockPanel.hpp"
#include "utils/ThemeManager.hpp"
#include "utils/Config.hpp"
#include <iostream>

namespace InfoDash {

MainWindow::MainWindow(GtkApplication* app)
    : window_(nullptr), headerBar_(nullptr), mainStack_(nullptr), stackSwitcher_(nullptr) {
    
    window_ = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window_), "InfoDash");
    gtk_window_set_default_size(GTK_WINDOW(window_), 1400, 900);
    
    // Apply theme from ThemeManager
    ThemeManager::getInstance().applyTheme();
    
    setupUI();
}

MainWindow::~MainWindow() = default;

void MainWindow::show() {
    gtk_window_present(GTK_WINDOW(window_));
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

void MainWindow::onSettingsClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<MainWindow*>(userData);
    self->showSettingsDialog();
}

void MainWindow::showSettingsDialog() {
    // Create a GTK4 Window instead of deprecated Dialog
    GtkWidget* dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Settings");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window_));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 550);
    
    GtkWidget* mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(dialog), mainBox);
    
    // Create notebook for tabs
    GtkWidget* notebook = gtk_notebook_new();
    gtk_widget_set_vexpand(notebook, TRUE);
    gtk_box_append(GTK_BOX(mainBox), notebook);
    
    // ==================== APPEARANCE TAB ====================
    GtkWidget* appearanceBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(appearanceBox, 24);
    gtk_widget_set_margin_end(appearanceBox, 24);
    gtk_widget_set_margin_top(appearanceBox, 20);
    gtk_widget_set_margin_bottom(appearanceBox, 20);
    
    // Mode Selection (Dark/Light/System)
    GtkWidget* modeLabel = gtk_label_new("MODE");
    gtk_widget_add_css_class(modeLabel, "theme-section-title");
    gtk_widget_set_halign(modeLabel, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(appearanceBox), modeLabel);
    
    GtkWidget* modeBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_append(GTK_BOX(appearanceBox), modeBox);
    
    auto& themeMgr = ThemeManager::getInstance();
    ThemeMode currentMode = themeMgr.getThemeMode();
    
    // Dark mode button
    GtkWidget* darkBtn = gtk_button_new();
    GtkWidget* darkBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(darkBox, GTK_ALIGN_CENTER);
    GtkWidget* darkIcon = gtk_label_new("🌙");
    gtk_widget_add_css_class(darkIcon, "mode-button-icon");
    GtkWidget* darkLabel = gtk_label_new("Dark");
    gtk_widget_add_css_class(darkLabel, "mode-button-label");
    gtk_box_append(GTK_BOX(darkBox), darkIcon);
    gtk_box_append(GTK_BOX(darkBox), darkLabel);
    gtk_button_set_child(GTK_BUTTON(darkBtn), darkBox);
    gtk_widget_add_css_class(darkBtn, "mode-button");
    if (currentMode == ThemeMode::Dark) gtk_widget_add_css_class(darkBtn, "selected");
    g_object_set_data(G_OBJECT(darkBtn), "mode", GINT_TO_POINTER(static_cast<int>(ThemeMode::Dark)));
    g_signal_connect(darkBtn, "clicked", G_CALLBACK(onModeButtonClicked), dialog);
    gtk_box_append(GTK_BOX(modeBox), darkBtn);
    
    // Light mode button
    GtkWidget* lightBtn = gtk_button_new();
    GtkWidget* lightBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(lightBox, GTK_ALIGN_CENTER);
    GtkWidget* lightIcon = gtk_label_new("☀️");
    gtk_widget_add_css_class(lightIcon, "mode-button-icon");
    GtkWidget* lightLabel = gtk_label_new("Light");
    gtk_widget_add_css_class(lightLabel, "mode-button-label");
    gtk_box_append(GTK_BOX(lightBox), lightIcon);
    gtk_box_append(GTK_BOX(lightBox), lightLabel);
    gtk_button_set_child(GTK_BUTTON(lightBtn), lightBox);
    gtk_widget_add_css_class(lightBtn, "mode-button");
    if (currentMode == ThemeMode::Light) gtk_widget_add_css_class(lightBtn, "selected");
    g_object_set_data(G_OBJECT(lightBtn), "mode", GINT_TO_POINTER(static_cast<int>(ThemeMode::Light)));
    g_signal_connect(lightBtn, "clicked", G_CALLBACK(onModeButtonClicked), dialog);
    gtk_box_append(GTK_BOX(modeBox), lightBtn);
    
    // System mode button
    GtkWidget* systemBtn = gtk_button_new();
    GtkWidget* systemBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(systemBox, GTK_ALIGN_CENTER);
    GtkWidget* systemIcon = gtk_label_new("💻");
    gtk_widget_add_css_class(systemIcon, "mode-button-icon");
    GtkWidget* systemLabel = gtk_label_new("System");
    gtk_widget_add_css_class(systemLabel, "mode-button-label");
    gtk_box_append(GTK_BOX(systemBox), systemIcon);
    gtk_box_append(GTK_BOX(systemBox), systemLabel);
    gtk_button_set_child(GTK_BUTTON(systemBtn), systemBox);
    gtk_widget_add_css_class(systemBtn, "mode-button");
    if (currentMode == ThemeMode::System) gtk_widget_add_css_class(systemBtn, "selected");
    g_object_set_data(G_OBJECT(systemBtn), "mode", GINT_TO_POINTER(static_cast<int>(ThemeMode::System)));
    g_signal_connect(systemBtn, "clicked", G_CALLBACK(onModeButtonClicked), dialog);
    gtk_box_append(GTK_BOX(modeBox), systemBtn);
    
    // Store mode buttons for easy access
    g_object_set_data(G_OBJECT(dialog), "darkBtn", darkBtn);
    g_object_set_data(G_OBJECT(dialog), "lightBtn", lightBtn);
    g_object_set_data(G_OBJECT(dialog), "systemBtn", systemBtn);
    
    // Color Scheme Selection
    GtkWidget* schemeLabel = gtk_label_new("COLOR SCHEME");
    gtk_widget_add_css_class(schemeLabel, "theme-section-title");
    gtk_widget_set_halign(schemeLabel, GTK_ALIGN_START);
    gtk_widget_set_margin_top(schemeLabel, 16);
    gtk_box_append(GTK_BOX(appearanceBox), schemeLabel);
    
    // Create scrollable grid for color schemes
    GtkWidget* schemeScroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(schemeScroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(schemeScroll, TRUE);
    
    GtkWidget* schemeGrid = gtk_flow_box_new();
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(schemeGrid), 4);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(schemeGrid), GTK_SELECTION_SINGLE);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(schemeGrid), 12);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(schemeGrid), 12);
    
    ColorScheme currentScheme = themeMgr.getColorScheme();
    
    // Define color schemes with their preview colors
    struct SchemeInfo {
        ColorScheme scheme;
        const char* name;
        const char* desc;
        const char* color1;  // Primary/accent
        const char* color2;  // Background
        const char* color3;  // Secondary
    };
    
    SchemeInfo schemes[] = {
        {ColorScheme::Default, "Default", "Original theme", "#e94560", "#1a1a2e", "#16213e"},
        {ColorScheme::Ocean, "Ocean", "Deep blue & teal", "#64ffda", "#0a192f", "#112240"},
        {ColorScheme::Forest, "Forest", "Green & emerald", "#50fa7b", "#1a2f1a", "#243524"},
        {ColorScheme::Sunset, "Sunset", "Warm orange tones", "#ff6b35", "#1f1135", "#2d1b4e"},
        {ColorScheme::Midnight, "Midnight", "Pure dark purple", "#bb86fc", "#0d0d0d", "#151515"},
        {ColorScheme::Nord, "Nord", "Arctic palette", "#88c0d0", "#2e3440", "#3b4252"},
        {ColorScheme::Dracula, "Dracula", "Vibrant dark", "#bd93f9", "#282a36", "#44475a"},
        {ColorScheme::Solarized, "Solarized", "Precision colors", "#268bd2", "#002b36", "#073642"},
        {ColorScheme::Rose, "Rosé", "Soft pink tones", "#f472b6", "#1f1a24", "#2a232f"},
    };
    
    for (const auto& info : schemes) {
        GtkWidget* schemeBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_add_css_class(schemeBox, "theme-preview");
        if (info.scheme == currentScheme) {
            gtk_widget_add_css_class(schemeBox, "selected");
        }
        
        // Color swatches row
        GtkWidget* swatchRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_halign(swatchRow, GTK_ALIGN_CENTER);
        
        // Create color swatches using a unique class per swatch
        static int swatchCounter = 0;
        for (const char* color : {info.color1, info.color2, info.color3}) {
            GtkWidget* swatch = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_widget_set_size_request(swatch, 24, 24);
            
            std::string swatchClass = "swatch-" + std::to_string(swatchCounter++);
            gtk_widget_add_css_class(swatch, swatchClass.c_str());
            gtk_widget_add_css_class(swatch, "color-swatch");
            
            // Apply color via display provider
            GtkCssProvider* provider = gtk_css_provider_new();
            std::string css = "." + swatchClass + " { background-color: " + color + "; border-radius: 50%; }";
            gtk_css_provider_load_from_string(provider, css.c_str());
            gtk_style_context_add_provider_for_display(
                gdk_display_get_default(),
                GTK_STYLE_PROVIDER(provider),
                GTK_STYLE_PROVIDER_PRIORITY_USER
            );
            g_object_unref(provider);
            
            gtk_box_append(GTK_BOX(swatchRow), swatch);
        }
        gtk_box_append(GTK_BOX(schemeBox), swatchRow);
        
        // Scheme name
        GtkWidget* nameLabel = gtk_label_new(info.name);
        gtk_widget_add_css_class(nameLabel, "theme-preview-name");
        gtk_box_append(GTK_BOX(schemeBox), nameLabel);
        
        // Scheme description
        GtkWidget* descLabel = gtk_label_new(info.desc);
        gtk_widget_add_css_class(descLabel, "theme-preview-desc");
        gtk_box_append(GTK_BOX(schemeBox), descLabel);
        
        // Store scheme enum value
        g_object_set_data(G_OBJECT(schemeBox), "scheme", GINT_TO_POINTER(static_cast<int>(info.scheme)));
        
        gtk_flow_box_append(GTK_FLOW_BOX(schemeGrid), schemeBox);
    }
    
    g_signal_connect(schemeGrid, "child-activated", G_CALLBACK(onSchemeSelected), dialog);
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(schemeScroll), schemeGrid);
    gtk_box_append(GTK_BOX(appearanceBox), schemeScroll);
    
    g_object_set_data(G_OBJECT(dialog), "schemeGrid", schemeGrid);
    
    GtkWidget* appearanceLabel = gtk_label_new("Appearance");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), appearanceBox, appearanceLabel);
    
    // ==================== ABOUT TAB ====================
    GtkWidget* aboutBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(aboutBox, 24);
    gtk_widget_set_margin_end(aboutBox, 24);
    gtk_widget_set_margin_top(aboutBox, 32);
    gtk_widget_set_halign(aboutBox, GTK_ALIGN_CENTER);
    
    GtkWidget* appIcon = gtk_label_new("📊");
    PangoAttrList* attrList = pango_attr_list_new();
    pango_attr_list_insert(attrList, pango_attr_scale_new(4.0));
    gtk_label_set_attributes(GTK_LABEL(appIcon), attrList);
    pango_attr_list_unref(attrList);
    gtk_box_append(GTK_BOX(aboutBox), appIcon);
    
    GtkWidget* appName = gtk_label_new("InfoDash");
    gtk_widget_add_css_class(appName, "title-2");
    gtk_box_append(GTK_BOX(aboutBox), appName);
    
    GtkWidget* version = gtk_label_new("Version 0.08");
    gtk_widget_add_css_class(version, "dim-label");
    gtk_box_append(GTK_BOX(aboutBox), version);
    
    GtkWidget* desc = gtk_label_new("A modern GTK4 dashboard for RSS feeds,\nweather, and stock information.");
    gtk_widget_set_margin_top(desc, 12);
    gtk_label_set_justify(GTK_LABEL(desc), GTK_JUSTIFY_CENTER);
    gtk_box_append(GTK_BOX(aboutBox), desc);
    
    GtkWidget* featureLabel = gtk_label_new("Features:");
    gtk_widget_add_css_class(featureLabel, "heading");
    gtk_widget_set_margin_top(featureLabel, 24);
    gtk_box_append(GTK_BOX(aboutBox), featureLabel);
    
    GtkWidget* features = gtk_label_new(
        "• 9 beautiful color themes with dark/light modes\n"
        "• RSS feed aggregation with category support\n"
        "• Multi-location weather with forecasts\n"
        "• Real-time stock tracking"
    );
    gtk_label_set_justify(GTK_LABEL(features), GTK_JUSTIFY_LEFT);
    gtk_widget_add_css_class(features, "dim-label");
    gtk_box_append(GTK_BOX(aboutBox), features);
    
    GtkWidget* aboutLabel = gtk_label_new("About");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), aboutBox, aboutLabel);
    
    // Close button at bottom
    GtkWidget* buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(buttonBox, GTK_ALIGN_END);
    gtk_widget_set_margin_start(buttonBox, 24);
    gtk_widget_set_margin_end(buttonBox, 24);
    gtk_widget_set_margin_top(buttonBox, 12);
    gtk_widget_set_margin_bottom(buttonBox, 16);
    
    GtkWidget* closeBtn = gtk_button_new_with_label("Close");
    gtk_widget_add_css_class(closeBtn, "suggested-action");
    g_signal_connect_swapped(closeBtn, "clicked", G_CALLBACK(gtk_window_destroy), dialog);
    gtk_box_append(GTK_BOX(buttonBox), closeBtn);
    
    gtk_box_append(GTK_BOX(mainBox), buttonBox);
    
    gtk_window_present(GTK_WINDOW(dialog));
}

void MainWindow::onModeButtonClicked(GtkButton* button, gpointer dialogPtr) {
    GtkWidget* dialog = GTK_WIDGET(dialogPtr);
    ThemeMode mode = static_cast<ThemeMode>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "mode")));
    
    // Update button states
    GtkWidget* darkBtn = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "darkBtn"));
    GtkWidget* lightBtn = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "lightBtn"));
    GtkWidget* systemBtn = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "systemBtn"));
    
    gtk_widget_remove_css_class(darkBtn, "selected");
    gtk_widget_remove_css_class(lightBtn, "selected");
    gtk_widget_remove_css_class(systemBtn, "selected");
    gtk_widget_add_css_class(GTK_WIDGET(button), "selected");
    
    // Apply theme
    ThemeManager::getInstance().setThemeMode(mode);
}

void MainWindow::onSchemeSelected(GtkFlowBox*, GtkFlowBoxChild* child, gpointer) {
    GtkWidget* schemeBox = gtk_flow_box_child_get_child(child);
    ColorScheme scheme = static_cast<ColorScheme>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(schemeBox), "scheme")));
    
    // Update visual selection (remove from all, add to selected)
    GtkWidget* flowbox = gtk_widget_get_parent(GTK_WIDGET(child));
    for (GtkWidget* c = gtk_widget_get_first_child(flowbox); c != nullptr; c = gtk_widget_get_next_sibling(c)) {
        GtkWidget* box = gtk_flow_box_child_get_child(GTK_FLOW_BOX_CHILD(c));
        gtk_widget_remove_css_class(box, "selected");
    }
    gtk_widget_add_css_class(schemeBox, "selected");
    
    // Apply theme
    ThemeManager::getInstance().setColorScheme(scheme);
}

}
