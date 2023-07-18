#ifndef SKUNKWORK_WINDOW_HPP
#define SKUNKWORK_WINDOW_HPP

#include <GL/gl3w.h>
#include <SDL.h>
#include <string>

class Window
{
public:
    Window() {};
    bool init(int w, int h, const std::string& title);
    void destroy();

    Window(const Window& other) = delete;
    Window(Window&& other);
    Window operator=(const Window& other) = delete;

    bool open() const;
    void setClose();
    SDL_Window* ptr() const;
    SDL_GLContext ctx() const;
    int width() const;
    int height() const;
    bool drawGUI() const;
    bool playPausePressed() const;

    // Returns true if backbuffer was resized
    bool startFrame();
    void endFrame() const;

private:
    bool handleWindow(SDL_Event const& event);
    void handleKey(SDL_Event const& event);

    SDL_Window* _window{nullptr};
    SDL_GLContext _context{nullptr};
    int _w{0};
    int _h{0};
    bool _drawGUI{false};
    bool _playPausePressed{false};
    bool _shouldClose{false};
};

#endif // SKUNKWORK_WINDOW_HPP
