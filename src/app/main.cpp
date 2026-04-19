#include "core/CommandPool.hpp"
#include "core/Device.hpp"
#include "core/FrameResources.hpp"
#include "core/ObjModel.hpp"
#include "core/Pipeline.hpp"
#include "core/Renderer.hpp"
#include "core/Shader.hpp"
#include "core/Swapchain.hpp"
#include "core/Texture.hpp"
#include "core/Window.hpp"
#include "utility/Globals.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <SDL3/SDL.h>
#include <fmt/format.h>

#include <array>
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

using namespace vulc;

namespace
{

using namespace vulc::globals;

constexpr auto WINDOW_TITLE{"How To Vulkan"};
constexpr auto WINDOW_WIDTH_INIT{1280};
constexpr auto WINDOW_HEIGHT_INIT{720};

constexpr std::size_t NUM_TEXTURES{3};
constexpr std::size_t NUM_MODELS{3};
constexpr auto TIME_FACTOR{1000.f};
constexpr auto ZOOM_FACTOR{10.f};
constexpr auto DEFAULT_CAMERA_POS{glm::vec3(0.f, 0.f, -6.f)};

} // namespace

int main()
{
    // NOTE: Step 1 - Prepare Vulkan
    auto device{std::make_unique<vulc::Device>()};

    // NOTE: Step 2 - Preapre the Vulkan building pieces
    auto window{std::make_unique<vulc::Window>(WINDOW_TITLE, WINDOW_WIDTH_INIT, WINDOW_HEIGHT_INIT)};
    auto swapchain{std::make_unique<vulc::Swapchain>(*device, *window)};
    auto slangContext{std::make_unique<vulc::SlangContext>()};
    auto shader{
        std::make_unique<vulc::Shader>(*device, *slangContext, "meshShader", PROJ_ROOT "resources/shaders/shader.slang")
    };
    auto pipeline{std::make_unique<vulc::Pipeline>(*device, *shader, swapchain->imageFormats())};

    // NOTE: Step 3 - Prepare dynamic Vulkan resources (Renderer specific)
    auto commandPool{std::make_unique<vulc::CommandPool>(*device)};

    // NOTE: Step 4 - Load Resources
    auto mesh{std::make_unique<vulc::ObjModel>(*device, PROJ_ROOT "resources/models/suzanne.obj")};

    std::vector<std::unique_ptr<vulc::Texture>> textures{};
    textures.reserve(NUM_TEXTURES);
    for(auto i{0}; i < NUM_TEXTURES; ++i)
        textures.push_back(
            std::make_unique<vulc::Texture>(
                *device, *commandPool, fmt::format("{}resources/textures/suzanne{}.ktx", PROJ_ROOT, i)
            )
        );

    vulc::Renderer renderer{*device, *window, *swapchain, *pipeline, std::move(textures), *commandPool};

    vulc::ShaderData shaderData{};
    glm::vec3 cameraPos{::DEFAULT_CAMERA_POS};
    std::array<glm::vec3, ::NUM_MODELS> objectRotations{};

    bool quit{false};
    std::uint64_t lastTime{SDL_GetTicks()};
    while(!quit)
    {
        const float deltaTime{static_cast<float>(SDL_GetTicks() - lastTime) / ::TIME_FACTOR};
        lastTime = SDL_GetTicks();

        for(SDL_Event event; SDL_PollEvent(&event);)
        {
            if(event.type == SDL_EVENT_QUIT)
            {
                quit = true;
                break;
            }

            if(event.type == SDL_EVENT_MOUSE_MOTION)
            {
                if((event.motion.state & SDL_BUTTON_LMASK) != 0u)
                {
                    objectRotations.at(shaderData.selected).x -= event.motion.yrel * deltaTime;
                    objectRotations.at(shaderData.selected).y += event.motion.xrel * deltaTime;
                }
            }

            if(event.type == SDL_EVENT_MOUSE_WHEEL)
                cameraPos.z += event.wheel.y * deltaTime * ZOOM_FACTOR;

            if(event.type == SDL_EVENT_KEY_DOWN)
            {
                if(event.key.key == SDLK_PLUS || event.key.key == SDLK_KP_PLUS)
                    shaderData.selected = (shaderData.selected < ::NUM_MODELS - 1) ? shaderData.selected + 1 : 0;

                if(event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_PLUS)
                    shaderData.selected = (shaderData.selected > 0) ? shaderData.selected - 1 : ::NUM_MODELS - 1;
                
                if(event.key.key == SDLK_ESCAPE)
                    quit = true;
            }

            if(event.type == SDL_EVENT_WINDOW_RESIZED)
                renderer.requestSwapchainRecreate();
        }

        const auto extent{swapchain->extent()};
        shaderData.projection = glm::perspective(
            glm::radians(::CAMERA_FOV),
            static_cast<float>(extent.width) / static_cast<float>(extent.height),
            ::CAMERA_NEAR_PLANE,
            ::CAMERA_FAR_PLANE
        );
        shaderData.view = glm::translate(glm::mat4(1.f), cameraPos);

        for(auto i{0}; i < ::NUM_MODELS; ++i)
        {
            const auto instancePos{glm::vec3((static_cast<float>(i) - 1.f) * ::INSTANCE_OFFSET_MUL, 0.f, 0.f)};
            shaderData.model.at(i)
                = glm::translate(glm::mat4(1.f), instancePos) * glm::mat4_cast(glm::quat(objectRotations.at(i)));
        }

        renderer.draw(*mesh, shaderData);
    }
}
