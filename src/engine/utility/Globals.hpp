#ifndef VULCANO_SRC_ENGINE_UTILITY_GLOBALS_HPP
#define VULCANO_SRC_ENGINE_UTILITY_GLOBALS_HPP

#include <cstddef>
#include <cstdint>

namespace vulc::globals
{

constexpr std::size_t NUM_MODELS{3};
constexpr std::uint32_t MAX_FRMES_IN_FLIGHT{2};

constexpr auto CAMERA_FOV{45.f};
constexpr auto CAMERA_NEAR_PLANE{0.1f};
constexpr auto CAMERA_FAR_PLANE{32.f};
constexpr auto INSTANCE_OFFSET_MUL{3.f};

} // namespace vulc::globals

#endif // !VULCANO_SRC_ENGINE_UTILITY_GLOBALS_HPP

