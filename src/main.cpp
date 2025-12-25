#include "app/Application.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        InfoDash::Application app;
        return app.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
