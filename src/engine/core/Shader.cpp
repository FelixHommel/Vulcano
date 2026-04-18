#include "Shader.hpp"

#include "core/Device.hpp"
#include "utility/Common.hpp"

#include <volk.h>

#include <slang-com-ptr.h>
#include <slang.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace vulc
{

SlangContext::SlangContext(slang::IGlobalSession* globalSession) : m_globalSession(globalSession)
{
    VULCANO_ASSERT(m_globalSession != nullptr, "The global slang session must be a valid session");

    auto slangTargets{
        std::to_array<slang::TargetDesc>(
            {{.format = SLANG_SPIRV, .profile = m_globalSession->findProfile("spirv_1_4")}}
        )
    };
    auto slangOptions{
        std::to_array<slang::CompilerOptionEntry>(
            {{.name = slang::CompilerOptionName::EmitSpirvDirectly,
              .value = {.kind = slang::CompilerOptionValueKind::Int, .intValue0 = 1}}}
        )
    };
    const slang::SessionDesc slangSessionDesc{
        .targets = slangTargets.data(),
        .targetCount = static_cast<std::uint32_t>(slangTargets.size()),
        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
        .compilerOptionEntries = slangOptions.data(),
        .compilerOptionEntryCount = static_cast<std::uint32_t>(slangOptions.size())
    };
    m_globalSession->createSession(slangSessionDesc, m_slangSession.writeRef());
}

slang::IModule* SlangContext::loadFromSource(const std::string& moduleName, const std::filesystem::path& filepath)
{
    return m_slangSession->loadModuleFromSource(moduleName.c_str(), filepath.c_str(), nullptr, nullptr);
}

Shader::Shader(
    const Device& device,
    SlangContext& slangContext,
    const std::string& moduleName,
    const std::filesystem::path& filepath
)
    : m_device{device}, m_slangContext{slangContext}
{
    const Slang::ComPtr<slang::IModule> slangModule{m_slangContext.loadFromSource(moduleName, filepath)};

    Slang::ComPtr<ISlangBlob> spirv;
    slangModule->getTargetCode(0, spirv.writeRef());

    const VkShaderModuleCreateInfo shaderModuleCI{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirv->getBufferSize(),
        .pCode = static_cast<const std::uint32_t*>(spirv->getBufferPointer())
    };
    chk(vkCreateShaderModule(m_device.handle(), &shaderModuleCI, nullptr, &m_shaderModule));
}

Shader::~Shader()
{
    vkDestroyShaderModule(m_device.handle(), m_shaderModule, nullptr);
}

} // namespace vulc

