#include "ui/WeatherPanel.hpp"
#include "utils/Config.hpp"
#include <cstring>

namespace InfoDash {

WeatherPanel::WeatherPanel() : widget_(nullptr), weatherBox_(nullptr), 
                               zipEntry_(nullptr), tempUnitBtn_(nullptr),
                               locationsBox_(nullptr), loadingSpinner_(nullptr),
                               loadingLabel_(nullptr) {
    service_ = std::make_unique<WeatherService>();
    setupUI();
    refresh();
}

WeatherPanel::~WeatherPanel() = default;

void WeatherPanel::setupUI() {
    widget_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(widget_, 16);
    gtk_widget_set_margin_end(widget_, 16);
    gtk_widget_set_margin_top(widget_, 8);
    gtk_widget_set_margin_bottom(widget_, 8);

    // Top bar with location entry and settings
    GtkWidget* topBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(topBar, "panel-card");

    zipEntry_ = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(zipEntry_), "Enter city name or ZIP code...");
    gtk_widget_set_hexpand(zipEntry_, TRUE);
    gtk_box_append(GTK_BOX(topBar), zipEntry_);

    GtkWidget* addBtn = gtk_button_new_with_label("Add Location");
    gtk_widget_add_css_class(addBtn, "add-button");
    g_signal_connect(addBtn, "clicked", G_CALLBACK(onAddLocationClicked), this);
    gtk_box_append(GTK_BOX(topBar), addBtn);

    // Temperature unit toggle button
    tempUnitBtn_ = gtk_button_new();
    gtk_widget_add_css_class(tempUnitBtn_, "flat");
    gtk_widget_set_tooltip_text(tempUnitBtn_, "Toggle temperature unit");
    g_signal_connect(tempUnitBtn_, "clicked", G_CALLBACK(onTempUnitToggled), this);
    gtk_box_append(GTK_BOX(topBar), tempUnitBtn_);
    updateTempUnitButton();

    gtk_box_append(GTK_BOX(widget_), topBar);

    // Current locations indicator
    locationsBox_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_top(locationsBox_, 4);
    gtk_box_append(GTK_BOX(widget_), locationsBox_);
    updateLocationsBar();

    // Loading indicator
    GtkWidget* loadingBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(loadingBox, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(loadingBox, 8);
    
    loadingSpinner_ = gtk_spinner_new();
    gtk_box_append(GTK_BOX(loadingBox), loadingSpinner_);
    
    loadingLabel_ = gtk_label_new("Fetching weather data...");
    gtk_widget_add_css_class(loadingLabel_, "loading-label");
    gtk_box_append(GTK_BOX(loadingBox), loadingLabel_);
    
    gtk_box_append(GTK_BOX(widget_), loadingBox);

    // Weather cards area
    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);

    weatherBox_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), weatherBox_);
    gtk_box_append(GTK_BOX(widget_), scroll);
}

void WeatherPanel::updateLocationsBar() {
    GtkWidget* child;
    while ((child = gtk_widget_get_first_child(locationsBox_)) != nullptr)
        gtk_box_remove(GTK_BOX(locationsBox_), child);

    auto locations = Config::getInstance().getWeatherLocations();
    
    GtkWidget* label = gtk_label_new("Locations: ");
    gtk_widget_add_css_class(label, "locations-label");
    gtk_box_append(GTK_BOX(locationsBox_), label);

    for (const auto& loc : locations) {
        GtkWidget* tag = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_widget_add_css_class(tag, "location-tag");

        GtkWidget* tagLabel = gtk_label_new(loc.c_str());
        gtk_box_append(GTK_BOX(tag), tagLabel);

        if (locations.size() > 1) {
            GtkWidget* removeBtn = gtk_button_new_from_icon_name("window-close-symbolic");
            gtk_widget_add_css_class(removeBtn, "location-remove-btn");
            gtk_widget_add_css_class(removeBtn, "flat");
            gtk_widget_add_css_class(removeBtn, "circular");
            gtk_widget_set_size_request(removeBtn, 16, 16);
            
            char* locCopy = g_strdup(loc.c_str());
            g_object_set_data_full(G_OBJECT(removeBtn), "location", locCopy, g_free);
            g_signal_connect(removeBtn, "clicked", G_CALLBACK(onRemoveLocationClicked), this);
            
            gtk_box_append(GTK_BOX(tag), removeBtn);
        }

        gtk_box_append(GTK_BOX(locationsBox_), tag);
    }
}

void WeatherPanel::updateTempUnitButton() {
    TempUnit unit = Config::getInstance().getTempUnit();
    gtk_button_set_label(GTK_BUTTON(tempUnitBtn_), unit == TempUnit::Fahrenheit ? "F" : "C");
}

void WeatherPanel::showLoading(bool show) {
    if (show) {
        gtk_spinner_start(GTK_SPINNER(loadingSpinner_));
        gtk_widget_set_visible(gtk_widget_get_parent(loadingSpinner_), TRUE);
    } else {
        gtk_spinner_stop(GTK_SPINNER(loadingSpinner_));
        gtk_widget_set_visible(gtk_widget_get_parent(loadingSpinner_), FALSE);
    }
}

void WeatherPanel::refresh() {
    showLoading(true);
    service_->fetchAllLocations([this](std::vector<WeatherData> data) {
        pendingData_ = data;
        g_idle_add(updateUICallback, this);
    });
}

gboolean WeatherPanel::updateUICallback(gpointer userData) {
    auto* self = static_cast<WeatherPanel*>(userData);
    self->showLoading(false);
    self->updateWeather(self->pendingData_);
    return FALSE;
}

void WeatherPanel::updateWeather(const std::vector<WeatherData>& data) {
    GtkWidget* child;
    while ((child = gtk_widget_get_first_child(weatherBox_)) != nullptr)
        gtk_box_remove(GTK_BOX(weatherBox_), child);

    for (const auto& w : data) {
        GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_add_css_class(card, "weather-card");

        // Weather alerts (if any)
        for (const auto& alert : w.alerts) {
            GtkWidget* alertBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            gtk_widget_add_css_class(alertBox, "weather-alert");
            
            GtkWidget* alertIcon = gtk_image_new_from_icon_name("dialog-warning-symbolic");
            gtk_image_set_pixel_size(GTK_IMAGE(alertIcon), 20);
            gtk_box_append(GTK_BOX(alertBox), alertIcon);
            
            GtkWidget* alertText = gtk_label_new(alert.headline.c_str());
            gtk_widget_add_css_class(alertText, "alert-text");
            gtk_label_set_wrap(GTK_LABEL(alertText), TRUE);
            gtk_label_set_xalign(GTK_LABEL(alertText), 0);
            gtk_widget_set_hexpand(alertText, TRUE);
            gtk_box_append(GTK_BOX(alertBox), alertText);
            
            gtk_box_append(GTK_BOX(card), alertBox);
        }

        // Location header with country
        std::string locationStr = w.location;
        if (!w.country.empty()) {
            locationStr += ", " + w.country;
        } else if (locationStr.empty()) {
            locationStr = w.zipCode;
        }
        GtkWidget* loc = gtk_label_new(locationStr.c_str());
        gtk_widget_add_css_class(loc, "weather-location");
        gtk_label_set_xalign(GTK_LABEL(loc), 0);
        gtk_box_append(GTK_BOX(card), loc);

        // Current conditions row
        GtkWidget* currentBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
        gtk_widget_set_margin_top(currentBox, 8);
        
        // Weather icon
        std::string iconName = WeatherService::getWeatherIcon(w.conditionCode);
        GtkWidget* weatherIcon = gtk_image_new_from_icon_name(iconName.c_str());
        gtk_image_set_pixel_size(GTK_IMAGE(weatherIcon), 64);
        gtk_widget_add_css_class(weatherIcon, "weather-icon");
        gtk_box_append(GTK_BOX(currentBox), weatherIcon);
        
        // Large temperature
        GtkWidget* temp = gtk_label_new(w.temperature.c_str());
        gtk_widget_add_css_class(temp, "weather-temp");
        gtk_box_append(GTK_BOX(currentBox), temp);
        
        // Condition and details
        GtkWidget* detailsBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_set_hexpand(detailsBox, TRUE);
        
        GtkWidget* cond = gtk_label_new(w.condition.c_str());
        gtk_widget_add_css_class(cond, "weather-condition");
        gtk_label_set_xalign(GTK_LABEL(cond), 0);
        gtk_box_append(GTK_BOX(detailsBox), cond);

        std::string feelsStr = "Feels like " + w.feelsLike;
        GtkWidget* feels = gtk_label_new(feelsStr.c_str());
        gtk_widget_add_css_class(feels, "weather-feels");
        gtk_label_set_xalign(GTK_LABEL(feels), 0);
        gtk_box_append(GTK_BOX(detailsBox), feels);

        std::string details = "Humidity: " + w.humidity + " | Wind: " + w.wind;
        GtkWidget* det = gtk_label_new(details.c_str());
        gtk_widget_add_css_class(det, "weather-details");
        gtk_label_set_xalign(GTK_LABEL(det), 0);
        gtk_box_append(GTK_BOX(detailsBox), det);
        
        gtk_box_append(GTK_BOX(currentBox), detailsBox);
        gtk_box_append(GTK_BOX(card), currentBox);

        // Forecast section
        if (!w.forecast.empty()) {
            GtkWidget* forecastLabel = gtk_label_new("3-Day Forecast");
            gtk_widget_add_css_class(forecastLabel, "forecast-header");
            gtk_label_set_xalign(GTK_LABEL(forecastLabel), 0);
            gtk_widget_set_margin_top(forecastLabel, 16);
            gtk_box_append(GTK_BOX(card), forecastLabel);
            
            GtkWidget* forecastBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            gtk_widget_set_margin_top(forecastBox, 8);
            
            for (const auto& f : w.forecast) {
                GtkWidget* dayCard = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
                gtk_widget_add_css_class(dayCard, "forecast-day");
                gtk_widget_set_hexpand(dayCard, TRUE);
                
                // Day name
                GtkWidget* dayName = gtk_label_new(f.day.c_str());
                gtk_widget_add_css_class(dayName, "forecast-day-name");
                gtk_box_append(GTK_BOX(dayCard), dayName);
                
                // Weather icon for forecast
                std::string forecastIcon = WeatherService::getWeatherIcon(f.conditionCode);
                GtkWidget* fIcon = gtk_image_new_from_icon_name(forecastIcon.c_str());
                gtk_image_set_pixel_size(GTK_IMAGE(fIcon), 32);
                gtk_widget_add_css_class(fIcon, "forecast-icon");
                gtk_box_append(GTK_BOX(dayCard), fIcon);
                
                // High temp
                GtkWidget* hi = gtk_label_new(f.high.c_str());
                gtk_widget_add_css_class(hi, "forecast-temp-high");
                gtk_box_append(GTK_BOX(dayCard), hi);
                
                // Low temp
                GtkWidget* lo = gtk_label_new(f.low.c_str());
                gtk_widget_add_css_class(lo, "forecast-temp-low");
                gtk_box_append(GTK_BOX(dayCard), lo);
                
                gtk_box_append(GTK_BOX(forecastBox), dayCard);
            }
            
            gtk_box_append(GTK_BOX(card), forecastBox);
        }

        gtk_box_append(GTK_BOX(weatherBox_), card);
    }
}

void WeatherPanel::onAddLocationClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<WeatherPanel*>(userData);
    const char* input = gtk_editable_get_text(GTK_EDITABLE(self->zipEntry_));
    if (input && strlen(input) > 0) {
        Config::getInstance().addWeatherLocation(input);
        Config::getInstance().save();
        gtk_editable_set_text(GTK_EDITABLE(self->zipEntry_), "");
        self->updateLocationsBar();
        self->refresh();
    }
}

void WeatherPanel::onRemoveLocationClicked(GtkButton* button, gpointer userData) {
    auto* self = static_cast<WeatherPanel*>(userData);
    const char* location = static_cast<const char*>(g_object_get_data(G_OBJECT(button), "location"));
    if (location) {
        Config::getInstance().removeWeatherLocation(location);
        Config::getInstance().save();
        self->updateLocationsBar();
        self->refresh();
    }
}

void WeatherPanel::onTempUnitToggled(GtkButton*, gpointer userData) {
    auto* self = static_cast<WeatherPanel*>(userData);
    TempUnit current = Config::getInstance().getTempUnit();
    TempUnit newUnit = (current == TempUnit::Fahrenheit) ? TempUnit::Celsius : TempUnit::Fahrenheit;
    Config::getInstance().setTempUnit(newUnit);
    self->updateTempUnitButton();
    self->refresh();
}

}
