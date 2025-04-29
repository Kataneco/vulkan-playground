#include "Window.h"

Window::Window(int width, int height, const char *title) {
    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
}

Window::~Window() {
    destroySurface();
    glfwDestroyWindow(window);
}

void Window::initialize() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

void Window::terminate() {
    glfwTerminate();
}

std::vector<const char*> Window::getRequiredInstanceExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    return std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
}

VkResult Window::createSurface(VkInstance surface_instance) {
    if (surface != VK_NULL_HANDLE) return VK_ERROR_TOO_MANY_OBJECTS;
    instance = surface_instance;
    return glfwCreateWindowSurface(instance, window, nullptr, &surface);
}

const VkSurfaceKHR Window::getSurface() {
    return surface;
}

void Window::destroySurface() {
    if (surface == VK_NULL_HANDLE) return;
    vkDestroySurfaceKHR(instance, surface, nullptr);
    surface = VK_NULL_HANDLE;
}

void Window::pollEvents() {
    glfwPollEvents();
}

void Window::waitEvents() {
    glfwWaitEvents();
}

VkExtent2D Window::getWindowExtent() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

void Window::setWindowExtent(uint32_t width, uint32_t height) {
    glfwSetWindowSize(window, width, height);
}

VkOffset2D Window::getWindowPosition() {
    VkOffset2D position;
    glfwGetWindowPos(window, &position.x, &position.y);
    return position;
}

void Window::setWindowPosition(int32_t x, int32_t y) {
    glfwSetWindowPos(window, x, y);
}

bool Window::windowShouldClose() {
    return glfwWindowShouldClose(window);
}

bool Window::windowIconified() {
    return glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE ? true : false;
}

void Window::setWindowIcon(const Texture &icon, const Texture &icon_small) {
    icon_images[0].width = icon.width;
    icon_images[0].height = icon.height;
    icon_images[0].pixels = const_cast<unsigned char*>(icon.pixels.data());

    icon_images[1].width = icon_small.width;
    icon_images[1].height = icon_small.height;
    icon_images[1].pixels = const_cast<unsigned char*>(icon_small.pixels.data());

    glfwSetWindowIcon(window, 2, icon_images);
}