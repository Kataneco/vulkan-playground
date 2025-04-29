#pragma once
#include "util/VulkanUtils.h"

#include "resource/Texture.h"

class Window {
public:
    static void initialize();
    static void terminate();

    Window(int width, int height, const char *title = "your life");
    ~Window();

    static std::vector<const char*> getRequiredInstanceExtensions();

    VkResult createSurface(VkInstance instance);
    const VkSurfaceKHR getSurface();
    void destroySurface();

    void pollEvents();
    void waitEvents();

    VkExtent2D getWindowExtent();
    void setWindowExtent(uint32_t width, uint32_t height);

    VkOffset2D getWindowPosition();
    void setWindowPosition(int32_t x, int32_t y);

    bool windowShouldClose();
    bool windowIconified();

    void setWindowIcon(const Texture &icon, const Texture &icon_small);
    //void setWindowCursor(const Texture &cursor);

    operator GLFWwindow *() const { return window; }

private:
    VkInstance instance;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    GLFWwindow *window;

    GLFWimage icon_images[2];
    //GLFWimage cursor_image;
    //GLFWcursor* cursor = nullptr;
};