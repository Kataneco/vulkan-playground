#include "Window.h"

Window::Window(int width, int height, const char *title) {
    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
}

Window::~Window() {}

void Window::initialize() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

void Window::terminate() {
    glfwTerminate();
}
