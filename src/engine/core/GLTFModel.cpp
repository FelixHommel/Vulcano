#include "GLTFModel.hpp"

#include "core/Device.hpp"

#include <cstring>
#include <tiny_gltf.h>

namespace vulc
{

GLTFModel::GLTFModel(const Device& device)
    : m_device{device}
{
}

GLTFModel::~GLTFModel()
{
    for(auto& n : m_nodes)
        delete n;

    vkDestroyBuffer(m_device.handle(), vertices.buffer, nullptr);
    vkFreeMemory(m_device.handle(), vertices.memory, nullptr);
    vkDestroyBuffer(m_device.handle(), indices.buffer, nullptr);
    vkFreeMemory(m_device.handle(), indices.memory, nullptr);
}

void GLTFModel::loadImages(tinygltf::Model& input)
{
    m_images.resize(input.images.size());
    for(auto i{0}; i < input.images.size(); ++i)
    {
        auto& glTFImage{ input.images[i] };

        unsigned char* buffer{ nullptr };
        VkDeviceSize bufferSize{ 0 };
        bool deleteBuffer{ false };

        if(glTFImage.component == GLTF_RGB_COMPONENT_COUNT)
        {
            bufferSize = static_cast<VkDeviceSize>(glTFImage.width * glTFImage.height) * GLTF_RGBA_COMPONENT_COUNT;
            buffer = new unsigned char[bufferSize];
            
            unsigned char* rgba{ buffer };
            unsigned char* rgb{ glTFImage.image.data() };

            for(auto j{0u}; j < glTFImage.width * glTFImage.height; ++i)
            {
                std::memcpy(rgba, rgb, sizeof(unsigned char) * GLTF_RGB_COMPONENT_COUNT);
                rgba += GLTF_RGBA_COMPONENT_COUNT; // NOLINT
                rgb += GLTF_RGB_COMPONENT_COUNT; // NOLINT
            }
            deleteBuffer = true;
        }
        else
        {
            buffer = glTFImage.image.data();
            bufferSize = static_cast<VkDeviceSize>(glTFImage.image.size());
        }

        m_images[i].
    }
}

} // namespace vulc

