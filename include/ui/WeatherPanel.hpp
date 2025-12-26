#pragma once
#include <gtk/gtk.h>
#include <memory>
#include <vector>
#include "services/WeatherService.hpp"

namespace InfoDash {

class WeatherPanel {
public:
    WeatherPanel();
    ~WeatherPanel();
    GtkWidget* getWidget() const { return widget_; }
    void refresh();

private:
    void setupUI();
    void updateWeather(const std::vector<WeatherData>& data);
    void updateTempUnitButton();
    void updateLocationsBar();
    void showLoading(bool show);
    
    static void onAddLocationClicked(GtkButton* button, gpointer userData);
    static void onRemoveLocationClicked(GtkButton* button, gpointer userData);
    static void onTempUnitToggled(GtkButton* button, gpointer userData);
    static gboolean updateUICallback(gpointer userData);

    GtkWidget* widget_;
    GtkWidget* weatherBox_;
    GtkWidget* zipEntry_;
    GtkWidget* tempUnitBtn_;
    GtkWidget* locationsBox_;
    GtkWidget* loadingSpinner_;
    GtkWidget* loadingLabel_;
    std::unique_ptr<WeatherService> service_;
    std::vector<WeatherData> pendingData_;
};

}
