#ifndef VULCANO_SRC_ENGINE_CORE_WINDOW_HPP
#define VULCANO_SRC_ENGINE_CORE_WINDOW_HPP

#include <SDL3/SDL.h>

#include <memory>
#include <string>

namespace vulc
{

class Window
{
public:
    Window(const std::string& title, int width, int height);

    [[nodiscard]] const SDL_Window* handle() const { return m_window.get(); }
    [[nodiscard]] SDL_Window* handle() { return m_window.get(); }

private:
    struct SdlWindowDeleter
    {
        void operator()(SDL_Window* window) { SDL_DestroyWindow(window); }
    };

    std::unique_ptr<SDL_Window, SdlWindowDeleter> m_window;
};

} // namespace vulc

#endif // !VULCANO_SRC_ENGINE_CORE_WINDOW_HPP

