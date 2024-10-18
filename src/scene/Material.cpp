#include "Material.h"

Material::Material(VkPipeline pipeline, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, uint32_t layer) : pipeline(pipeline), layout(pipelineLayout), set(descriptorSet), layer(layer) {}