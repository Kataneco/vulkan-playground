#pragma once
#include "util/VulkanUtils.h"

#include "core/Device.h"

class Resource {
public:
    Resource(VkDevice device, const std::string& name);
    virtual ~Resource() = default;

    virtual void destroy() = 0;

    const std::string& getName() const { return name; }

protected:
    VkDevice device;
    std::string name;
};
