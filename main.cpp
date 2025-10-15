#define KITTY_MAIN
#include "util/VulkanUtils.h"
#include "app/Application.h"
#include "util/Window.h"

int main(int argc, char** argv) {
    try {
        Window::initialize();
        Application app(argc, argv);
        app.run();
        Window::terminate();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        Window::terminate();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
