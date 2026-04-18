#include "Window.hpp"

#include <SDL3/SDL.h>

#include <memory>
#include <string>

namespace vulc
{

Window::Window(const std::string& title, int width, int height)
    : m_window{std::unique_ptr<SDL_Window, SdlWindowDeleter>(
          SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE), {}
      )}
{}

} // namespace vulc

