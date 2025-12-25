#pragma once

#include <gtk/gtk.h>
#include <memory>

namespace InfoDash {

class RSSPanel;
class WeatherPanel;
class StockPanel;

class MainWindow {
public:
    explicit MainWindow(GtkApplication* app);
    ~MainWindow();

    void show();
    GtkWidget* getWidget() const { return window_; }

private:
    void setupUI();
    void setupHeaderBar();
    void setupMainContent();
    void applyCSS();
    
    static void onRefreshClicked(GtkButton* button, gpointer userData);
    static void onSettingsClicked(GtkButton* button, gpointer userData);

    GtkWidget* window_;
    GtkWidget* headerBar_;
    GtkWidget* mainStack_;
    GtkWidget* stackSwitcher_;
    
    std::unique_ptr<RSSPanel> rssPanel_;
    std::unique_ptr<WeatherPanel> weatherPanel_;
    std::unique_ptr<StockPanel> stockPanel_;
};

} // namespace InfoDash
