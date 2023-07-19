
#ifndef SKUNKWORK_GUI_HPP
#define SKUNKWORK_GUI_HPP

#include <GL/gl3w.h>
#include <SDL.h>
#include <string>
#include <utility>
#include <vector>

#include "gpuProfiler.hpp"
#include "shader.hpp"

class GUI
{
public:
    GUI();

    void init(SDL_Window* window, SDL_GLContext context);
    void destroy();
    bool useSliderTime() const;
    float sliderTime() const;

    void startFrame(int windowHeight,
                    int32_t& sceneOverride,
                    float& timeS,
                    std::vector<Shader*> const& shaders,
                    const std::vector<std::pair<std::string, const GpuProfiler*>>& timers);
    void endFrame();

private:
    SDL_Window* _window{nullptr};
    bool _useSliderTime{false};
    float _sliderTime{0.f};
};

#endif // SKUNKWORK_GUI_HPP
