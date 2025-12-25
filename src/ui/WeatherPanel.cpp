#include "ui/WeatherPanel.hpp"
#include "utils/Config.hpp"
#include <cstring>

namespace InfoDash {

WeatherPanel::WeatherPanel() : widget_(nullptr), weatherBox_(nullptr), zipEntry_(nullptr) {
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

    // Add location section
    GtkWidget* addBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(addBox, "panel-card");

    zipEntry_ = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(zipEntry_), "Enter ZIP code or city...");
    gtk_widget_set_hexpand(zipEntry_, TRUE);
    gtk_box_append(GTK_BOX(addBox), zipEntry_);

    GtkWidget* addBtn = gtk_button_new_with_label("Add Location");
    gtk_widget_add_css_class(addBtn, "add-button");
    g_signal_connect(addBtn, "clicked", G_CALLBACK(onAddLocationClicked), this);
    gtk_box_append(GTK_BOX(addBox), addBtn);

    gtk_box_append(GTK_BOX(widget_), addBox);

    // Weather cards area
    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);

    weatherBox_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), weatherBox_);
    gtk_box_append(GTK_BOX(widget_), scroll);
}

void WeatherPanel::refresh() {
    service_->fetchAllLocations([this](std::vector<WeatherData> data) {
        pendingData_ = data;
        g_idle_add(updateUICallback, this);
    });
}

gboolean WeatherPanel::updateUICallback(gpointer userData) {
    auto* self = static_cast<WeatherPanel*>(userData);
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

        GtkWidget* loc = gtk_label_new(w.location.empty() ? w.zipCode.c_str() : w.location.c_str());
        gtk_widget_add_css_class(loc, "weather-location");
        gtk_label_set_xalign(GTK_LABEL(loc), 0);
        gtk_box_append(GTK_BOX(card), loc);

        GtkWidget* temp = gtk_label_new(w.temperature.c_str());
        gtk_widget_add_css_class(temp, "weather-temp");
        gtk_label_set_xalign(GTK_LABEL(temp), 0);
        gtk_box_append(GTK_BOX(card), temp);

        GtkWidget* cond = gtk_label_new(w.condition.c_str());
        gtk_widget_add_css_class(cond, "weather-condition");
        gtk_label_set_xalign(GTK_LABEL(cond), 0);
        gtk_box_append(GTK_BOX(card), cond);

        std::string details = "Humidity: " + w.humidity + " | Wind: " + w.wind;
        GtkWidget* det = gtk_label_new(details.c_str());
        gtk_widget_add_css_class(det, "weather-condition");
        gtk_label_set_xalign(GTK_LABEL(det), 0);
        gtk_box_append(GTK_BOX(card), det);

        // Forecast row
        GtkWidget* forecastBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_top(forecastBox, 12);
        for (const auto& f : w.forecast) {
            GtkWidget* dayCard = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
            gtk_widget_add_css_class(dayCard, "forecast-day");
            
            GtkWidget* dayName = gtk_label_new(f.day.c_str());
            gtk_widget_add_css_class(dayName, "forecast-day-name");
            gtk_box_append(GTK_BOX(dayCard), dayName);
            
            GtkWidget* hi = gtk_label_new(f.high.c_str());
            gtk_widget_add_css_class(hi, "forecast-temp-high");
            gtk_box_append(GTK_BOX(dayCard), hi);
            
            gtk_box_append(GTK_BOX(forecastBox), dayCard);
        }
        gtk_box_append(GTK_BOX(card), forecastBox);

        gtk_box_append(GTK_BOX(weatherBox_), card);
    }
}

void WeatherPanel::onAddLocationClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<WeatherPanel*>(userData);
    const char* zip = gtk_editable_get_text(GTK_EDITABLE(self->zipEntry_));
    if (zip && strlen(zip) > 0) {
        Config::getInstance().setWeatherLocation(zip);
        Config::getInstance().save();
        gtk_editable_set_text(GTK_EDITABLE(self->zipEntry_), "");
        self->refresh();
    }
}

}
