#ifndef VULCANO_SRC_ENGINE_UTILITY_COMMON_HPP
#define VULCANO_SRC_ENGINE_UTILITY_COMMON_HPP

#include <SDL3/SDL_error.h>
#include <ktx.h>
#include <spdlog/spdlog.h>
#include <utility>
#include <volk.h>
#include <vulkan/vulkan_core.h>

#include <cstdio>
#include <cstdlib>
#include <format>
#include <source_location>
#include <string_view>

// NOTE: Custom formatters. They all could use some C++26 reflection (will be available soon, right?!?!!)
namespace std
{

template<>
struct formatter<VkResult> : formatter<int>
{
    auto format(VkResult result, format_context& ctx) const
    {
        return formatter<int>::format(to_underlying(result), ctx);
    }
};

template<>
struct formatter<ktxResult> : formatter<unsigned int>
{
    auto format(ktxResult result, format_context& ctx) const
    {
        return formatter<unsigned int>::format(to_underlying(result), ctx);
    }
};

} // namespace std


// NOTE: Declare the SDLResult struct before defining the traits
namespace vulc
{

struct SDLResult
{
    bool value;
};

}

namespace
{

template<typename T>
struct chkTraits;

template<>
struct chkTraits<VkResult>
{
    static bool failed(VkResult result) {return result != VK_SUCCESS;}
    static std::string message(VkResult result) {return std::format("Vulkan call returned an error ({})", result);}
    static int exitCode(VkResult result) {return static_cast<int>(result);}
};

template<>
struct chkTraits<bool>
{
    static bool failed(bool result) {return !result;}
    static std::string message([[maybe_unused]] bool result) {return "Call returned an error";}
    static int exitCode([[maybe_unused]] bool result) {return 1;}
};

template<>
struct chkTraits<ktxResult>
{
    static bool failed(ktxResult result) {return result != KTX_SUCCESS;}
    static std::string message(ktxResult result) {return std::format("KTX call returned an error ({})", result);}
    static int exitCode(ktxResult result) {return static_cast<int>(result);}
};

template<>
struct chkTraits<vulc::SDLResult>
{
    static bool failed(vulc::SDLResult result) {return !result.value;}
    static std::string message([[maybe_unused]] vulc::SDLResult result) {return std::format("SDL call returned an error with the following message: {}", SDL_GetError());}
    static int exitCode([[maybe_unused]] vulc::SDLResult result) {return 1;}
};

} // namespace

#if !defined(VULCANO_DEBUG)
#    define VULCANO_DEBUG 0
#endif

namespace vulc
{

template<typename T>
inline void chk(T result, std::string_view msg = {})
{
#if VULCANO_DEBUG
    if(::chkTraits<T>::failed(result))
    {
        if(msg.empty())
            spdlog::critical("{}", ::chkTraits<T>::message(result));
        else
            spdlog::critical("{}: {}", ::chkTraits<T>::message(result), msg);

        std::exit(::chkTraits<T>::exitCode(result));
    }
#endif
}

inline void chk(VkResult result, bool& swapchainUpdateFlag, std::string_view msg = {})
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
            spdlog::critical("{}", ::chkTraits<VkResult>::message(result));
        else
            spdlog::critical("{}: {}", ::chkTraits<VkResult>::message(result), msg);

        std::exit(::chkTraits<VkResult>::exitCode(result));
    }
#endif
}

namespace assertion
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

} // namespace assertion

}; // namespace vulc


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

