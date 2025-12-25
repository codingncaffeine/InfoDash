#pragma once

#include <gtk/gtk.h>
#include <memory>
#include <string>

namespace InfoDash {

class MainWindow;

class Application {
public:
    Application();
    ~Application();

    int run(int argc, char* argv[]);

    static Application* getInstance();
    GtkApplication* getGtkApp() const { return app_; }

private:
    static void onActivate(GtkApplication* app, gpointer userData);
    static void onShutdown(GtkApplication* app, gpointer userData);

    GtkApplication* app_;
    std::unique_ptr<MainWindow> mainWindow_;
    
    static Application* instance_;
};

} // namespace InfoDash
