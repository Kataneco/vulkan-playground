#include "Material.h"

Material::Material(VkPipeline pipeline, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet) : pipeline(pipeline), layout(pipelineLayout), set(descriptorSet) {}