#include "app/Application.hpp"
#include "ui/MainWindow.hpp"
#include "utils/Config.hpp"
#include <iostream>

namespace InfoDash {

Application* Application::instance_ = nullptr;

Application::Application() : app_(nullptr), mainWindow_(nullptr) {
    instance_ = this;
    app_ = gtk_application_new("com.infodash.app", G_APPLICATION_DEFAULT_FLAGS);
    
    g_signal_connect(app_, "activate", G_CALLBACK(onActivate), this);
    g_signal_connect(app_, "shutdown", G_CALLBACK(onShutdown), this);
}

Application::~Application() {
    if (app_) {
        g_object_unref(app_);
    }
    instance_ = nullptr;
}

Application* Application::getInstance() {
    return instance_;
}

int Application::run(int argc, char* argv[]) {
    return g_application_run(G_APPLICATION(app_), argc, argv);
}

void Application::onActivate(GtkApplication* app, gpointer userData) {
    auto* self = static_cast<Application*>(userData);
    
    // Load configuration
    Config::getInstance().load();
    
    // Create main window
    self->mainWindow_ = std::make_unique<MainWindow>(app);
    self->mainWindow_->show();
}

void Application::onShutdown(GtkApplication* /*app*/, gpointer /*userData*/) {
    // Save configuration on shutdown
    Config::getInstance().save();
}

} // namespace InfoDash
