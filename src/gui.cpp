#include "gui.hpp"

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_opengl3.h>

#include <cstdio>

namespace {
    float LOGW = 690.f;
    float LOGH = 210.f;
    float LOGM = 10.f;

    inline void uniformOffset()
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 27.f);
    }
}

GUI::GUI() :
    _useSliderTime(false),
    _sliderTime(0.f)
{ }

void GUI::init(SDL_Window* window, SDL_GLContext context)
{
    _window = window;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForOpenGL(_window, context);
    ImGui_ImplOpenGL3_Init("#version 410");
    ImGui::StyleColorsDark();
}

void GUI::destroy()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

bool GUI::useSliderTime() const
{
    return _useSliderTime;
}

float GUI::sliderTime() const
{
    return _sliderTime;
}

void GUI::startFrame(
    int windowHeight,
    float& timeS,
    std::vector<Shader*> const& shaders,
    const std::vector<std::pair<std::string, const GpuProfiler*>>& timers
)
{
    assert(_window != nullptr);

    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(_window);
    ImGui::NewFrame();

    // Uniform editor
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiSetCond_Once);
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiSetCond_Once);
    ImGui::SetNextWindowCollapsed(true, ImGuiSetCond_Once);
    ImGui::Begin("Skunkwork GUI");

    ImGui::Checkbox("##Use slider time", &_useSliderTime);
    ImGui::SameLine(); ImGui::DragFloat("uTime", &timeS, 0.01f);
    if (timeS < 0.f)
        timeS = 0.f;
    _sliderTime = timeS;

    for (auto* s : shaders)
    {
        assert(s != nullptr);
        if (ImGui::CollapsingHeader(s->name().c_str()))
        {
            for (auto& e : s->dynamicUniforms()) {
                std::string name = e.first + "##" + s->name();
                Uniform& uniform = e.second;
                switch (uniform.type) {
                case UniformType::Float:
                    uniformOffset();
                    ImGui::DragFloat(name.c_str(), uniform.value, 0.01f);
                    break;
                case UniformType::Vec2:
                    uniformOffset();
                    ImGui::DragFloat2(name.c_str(), uniform.value, 0.01f);
                    break;
                case UniformType::Vec3:
                    ImGui::ColorEdit3(
                        std::string("##" + name).c_str(),
                        uniform.value,
                        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueWheel
                    );
                    ImGui::SameLine(); ImGui::DragFloat3(name.c_str(), uniform.value, 0.01f);
                    break;
                default:
                    printf("[gui] Unknown dynamic uniform type\n");
                    break;
                }
            }
        }
    }

    ImGui::Text("Frame: %.1f", 1000.f / ImGui::GetIO().Framerate);
    for (auto& t : timers) {
        ImGui::SameLine(); ImGui::Text("%s: %.1f", t.first.c_str(), t.second->getAvg());
    }

    ImGui::End();
}

void GUI::endFrame()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
