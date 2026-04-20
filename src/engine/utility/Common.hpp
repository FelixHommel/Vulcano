#ifndef VULCANO_SRC_ENGINE_UTILITY_COMMON_HPP
#define VULCANO_SRC_ENGINE_UTILITY_COMMON_HPP

#include <SDL3/SDL_error.h>
#include <fmt/base.h>
#include <fmt/format.h>
#include <ktx.h>
#include <spdlog/spdlog.h>
#include <volk.h>
#include <vulkan/vulkan_core.h>

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string_view>

// NOTE: Custom formatter to easily print VkResult enum members
template<>
struct fmt::formatter<VkResult> : formatter<std::string_view>
{
    constexpr format_context::iterator format(VkResult result, format_context& ctx) const
    {
        return formatter<string_view>::format(to_string(result), ctx);
    }
};

template<>
struct fmt::formatter<ktxResult> : formatter<std::string_view>
{
    constexpr format_context::iterator format([[maybe_unused]] ktxResult result, format_context& ctx) const
    {
        // TODO: Implement different way to convert ktxResult to string (reflection soon?!?!?)
        return formatter<string_view>::format("ktx error", ctx);
    }
};

#if !defined(VULCANO_DEBUG)
#    define VULCANO_DEBUG 0
#endif

namespace vulc
{

inline void chk(VkResult result, std::string_view msg = {})
{
#if VULCANO_DEBUG
    if(result != VK_SUCCESS)
    {
        if(msg.empty())
            spdlog::critical("Vulkan call returned an error ({})", result);
        else
            spdlog::critical("Vulkan call returned an error ({}): {}", result, msg);

        std::exit(result);
    }
#endif
}

inline void chkSwapchain(VkResult result, bool& swapchainUpdateFlag, std::string_view msg = {})
{
#if VULCANO_DEBUG
    if(result < VK_SUCCESS)
    {
        if(result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            swapchainUpdateFlag = true;
            return;
        }

        if(msg.empty())
            spdlog::critical("Vulkan call returned an error ({})", result);
        else
            spdlog::critical("Vulkan call returned an error ({}): {}", result, msg);

        std::exit(result);
    }
#endif
}

inline void chk(bool result, std::string_view msg = {})
{
#if VULCANO_DEBUG
    if(!result)
    {
        if(msg.empty())
            spdlog::critical("Call returned an error");
        else
            spdlog::critical("Call returned an error: {}", msg);

        std::exit(1);
    }
#endif
}

inline void chkSDL(bool result, std::string_view msg = {})
{
#if VULCANO_DEBUG
    if(!result)
    {
        if(msg.empty())
            spdlog::critical("Call returned an error: {}", SDL_GetError());
        else
            spdlog::critical("Call returned an error: {}\n{}", msg, SDL_GetError());

        std::exit(1);
    }
#endif
}

}; // namespace vulc

namespace vulc::assertion
{

/// \brief Assertion failure handler.
///
/// Document that an assertion failed and why, then calls std::abort.
///
/// \param pExpr the assertions condition
/// \param pUserMsg user provided message
/// \param loc(optional) std::source_location from where the function was called
[[noreturn]] inline void assertion_fail(
    const char* pExpr, const char* pUserMsg, std::source_location loc = std::source_location::current()
) noexcept
{
    // NOTE: Usage of fprintf because spdlog would not be a good alternative in an assertion macro

    if((pUserMsg != nullptr) && (*pUserMsg != 0))
        std::fprintf(
            stderr,
            "VR assertion failed: %s\n\tLocation: %s:%u (%s)\n\tMessage: %s",
            pExpr,
            loc.file_name(),
            loc.line(),
            loc.function_name(),
            pUserMsg
        );
    else
        std::fprintf(
            stderr,
            "VR assertion failed: %s\n\tLocation: %s:%u (%s)",
            pExpr,
            loc.file_name(),
            loc.line(),
            loc.function_name()
        );

    std::abort();
}

constexpr const char* msgOrNull() noexcept
{
    return nullptr;
}
constexpr const char* msgOrNull(const char* pMsg) noexcept
{
    return pMsg;
}

} // namespace vulc::assertion

#if !defined(VULCANO_ENABLE_ASSERTIONS)
#    define VULCANO_ENABLE_ASSERTIONS 0
#endif

// NOLINTBEGIN(cppcoreguidelines-macro-usage, cppcoreguidelines-avoid-do-while):
//      Definition of assertion macro this way is fine and not solvable by variadic template function,
//      Usage of do-while for this purpose is fine.
#if VULCANO_ENABLE_ASSERTIONS
/// \brief Assert on \p cond
///
/// Failure of the assertion results in std::abort() being called
///
/// \param cond the condition the assertion needs to pass
#    define VULCANO_ASSERT(cond, ...)                                                                 \
        do                                                                                       \
        {                                                                                        \
            if(!(cond))                                                                          \
            {                                                                                    \
                ::vulc::assertion::assertion_fail(#cond, ::vulc::assertion::msgOrNull(__VA_ARGS__)); \
            }                                                                                    \
        }                                                                                        \
        while(0)
#else
#    define VULCANO_ASSERT(cond, ...) ((void)0)
#endif
// NOLINTEND(cppcoreguidelines-macro-usage, cppcoreguidelines-avoid-do-while)


#endif // !VULCANO_SRC_ENGINE_UTILITY_COMMON_HPP

