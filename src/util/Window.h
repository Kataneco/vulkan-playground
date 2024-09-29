#pragma once
#include "util/VulkanUtils.h"

class Window {
public:
    Window(int width, int height, const char *title = "I love you.");
    ~Window();

    operator GLFWwindow*() const { return window; }

private:
    GLFWwindow *window;
};