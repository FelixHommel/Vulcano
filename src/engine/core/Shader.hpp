#ifndef VULCANO_SRC_ENGINE_CORE_SHADER_HPP
#define VULCANO_SRC_ENGINE_CORE_SHADER_HPP

#include "core/Device.hpp"

#include <volk.h>

#include <slang-com-ptr.h>
#include <slang.h>
#include <vulkan/vulkan_core.h>

#include <filesystem>
#include <string>

namespace
{

// NOTE: This is not optimal, but much easier compared to forcing clients to provide a global session for now.
Slang::ComPtr<slang::IGlobalSession>& global()
{
    static Slang::ComPtr<slang::IGlobalSession> instance = [] {
        Slang::ComPtr<slang::IGlobalSession> s;
        slang::createGlobalSession(s.writeRef());

        return s;
    }();

    return instance;
}

} // namespace

namespace vulc
{

class SlangContext
{
public:
    SlangContext(slang::IGlobalSession* globalSession = ::global());
    ~SlangContext() = default;

    SlangContext(const SlangContext&) = delete;
    SlangContext& operator=(const SlangContext&) = delete;
    SlangContext(SlangContext&&) = delete;
    SlangContext& operator=(SlangContext&&) = delete;

    [[nodiscard]] slang::IModule* loadFromSource(const std::string& moduleName, const std::filesystem::path& filepath);

private:
    Slang::ComPtr<slang::IGlobalSession> m_globalSession{ nullptr };
    Slang::ComPtr<slang::ISession> m_slangSession{ nullptr };
};

class Shader
{
public:
    Shader(
        const Device& device,
        SlangContext& slangContext,
        const std::string& moduleName,
        const std::filesystem::path& filepath
    );
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&&) = delete;
    Shader& operator=(Shader&&) = delete;

    [[nodiscard]] VkShaderModule handle() noexcept { return m_shaderModule; }
    [[nodiscard]] VkShaderModule handle() const noexcept { return m_shaderModule; }

private:
    const Device& m_device;
    SlangContext& m_slangContext;

    VkShaderModule m_shaderModule{ VK_NULL_HANDLE };
};

} // namespace vulc

#endif // !VULCANO_SRC_ENGINE_CORE_SHADER_HPP

