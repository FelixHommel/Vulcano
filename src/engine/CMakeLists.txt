set(NAME "VulcanoEngine")
include(${PROJECT_SOURCE_DIR}/cmake/StaticAnalyzers.cmake)

add_library(${NAME}
    ./core/CommandPool.cpp
    ./core/Device.cpp
    ./core/FrameResources.cpp
    ./core/GLTFModel.cpp
    ./core/ObjModel.cpp
    ./core/Pipeline.cpp
    ./core/Renderer.cpp
    ./core/Swapchain.cpp
    ./core/Shader.cpp
    ./core/Texture.cpp
    ./core/Window.cpp
    ./external/TinyGLTFImplementation.cpp
    ./external/TinyObjLoaderImplementation.cpp
    ./external/VmaImplementation.cpp
    ./external/VolkImplementation.cpp
)

target_sources(${NAME}
    PRIVATE
        FILE_SET HEADERS
        FILES
            ./core/CommandPool.hpp
            ./core/Device.hpp
            ./core/FrameResources.hpp
            ./core/GLTFModel.hpp
            ./core/ObjModel.hpp
            ./core/Pipeline.hpp
            ./core/Renderer.hpp
            ./core/Swapchain.hpp
            ./core/Shader.hpp
            ./core/Texture.hpp
            ./core/Window.hpp
            ./utility/Common.hpp
            ./utility/Globals.hpp
            ./utility/Vertex.hpp
)

target_compile_features(${NAME} PRIVATE cxx_std_23)

if(MSVC)
    target_compile_options(${NAME} PRIVATE
        /external:W0
        /external:anglebrackets
        /external:templates-
    )
endif()

target_link_libraries(${NAME}
    PUBLIC
        spdlog::spdlog
    PRIVATE
        project_warnings
        fmt::fmt
        GPUOpen::VulkanMemoryAllocator
        glm::glm-header-only
        KTX::ktx
        KTX::astcenc-avx2-static
        SDL3::SDL3
        slang::slang
        slang::gfx
        tinyobjloader::tinyobjloader
        volk::volk
        volk::volk_headers
        Vulkan::Vulkan
)

target_include_directories(${NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${TINYGLTF_INCLUDE_DIRS}
)

target_compile_definitions(${NAME}
    PUBLIC
        # Commonly needed GLM configuration to work smoothly with Vulkan. Must be PUBLIC because targets need them to be defined as well
        GLM_FORCE_RADIANS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
)
