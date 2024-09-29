#include "Window.h"

Window::Window(int width, int height, const char *title) {
    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
}

Window::~Window() {}