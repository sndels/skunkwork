#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif // _WIN32

#include <GL/gl3w.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "imgui.h"

#include "gpuProfiler.hpp"
#include "gui.hpp"
#include "marched.hpp"
#include "quad.hpp"
#include "shader.hpp"
#include "timer.hpp"
#include "window.hpp"
#include "noise.h"

using namespace glm;

vec3 cs = vec3(0.132, 0.257, 0.231);
static float cloudSdf(const vec3& pos, const float time)
{
    vec3 pos0 = pos * cs + vec3(10, 0, 0);
    float ns = perlin_noise_3d(pos0.x + time, pos0.y, pos0.z, 0.01f, 2, 2345);
    return ns + abs(pos0.y + 1);
}

static float duneSdf (const vec3& pos, const float time) {
    auto pos0 = rotate(0.5f, vec3(0, 1, 0)) * vec4(pos * vec3(0.5), 1);
    float ns = perlin_noise_3d(pos0.x - time * 8.01, pos0.y, pos0.z, 0.1f, 2,
                               1234);
    return ns + (pos.y - 2);
}

static float derp(float a, float b, float t)
{
    return a * (1.0f - t) + b * t;
}

static float plantSdf (const vec3& pos, const float time) {
    float t = fmodf(time, 1.0f);
    float amnt = derp(3.2f, -0.15f, t); /* TODO sync */
    auto pos0 = rotate(0.5f, vec3(0, 1, 0)) * vec4(pos * vec3(0.5), 1);
    float ns = perlin_noise_3d(pos0.x - time * 8.01, pos0.y, pos0.z, 0.3, 2,
                               1234);
    return ns + (pos.y - 2 + amnt);
}


#ifdef _WIN32
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
    (void) hInstance;
    (void) hPrevInstance;
    (void) lpCmdLine;
    (void) nCmdShow;
#else
int main()
{
#endif // _WIN32
    // Init GLFW-context
    Window window;
    if (!window.init(1280, 720, "skunktoy"))
        return -1;

    // Setup imgui
    GUI gui;
    gui.init(window.ptr());

    Quad q;

    Marched cloudMarch(cloudSdf);
    Marched duneMarch(duneSdf);
    Marched plantMarch(plantSdf);


    // Set up scene
    std::string vertPath(RES_DIRECTORY);
    std::string fragPath(RES_DIRECTORY);
    vertPath += "shader/tri_vert.glsl";
    fragPath += "shader/tri_frag.glsl";
    Shader triShader(vertPath, fragPath, "");

    Timer reloadTime;
    Timer globalTime;
    Timer marchTime;
    GpuProfiler sceneProf(5);
    std::vector<std::pair<std::string, const GpuProfiler*>> profilers = {};

    glEnable(GL_DEPTH_TEST);

    glClearColor(0.3, 0.5, 0.8, 1.0);
    vec3 cameraPos(-30, -20, 30);
    vec3 cameraTgt(0, -20.5, 0);
    // Run the main loop
    int t = 0;
    while (window.open()) {
        window.startFrame();

        if (window.drawGUI())
            gui.startFrame(window.height(), triShader.dynamicUniforms(), profilers);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Try reloading the shader every 0.5s
        if (reloadTime.getSeconds() > 0.5f) {
            triShader.reload();
            reloadTime.reset();
        }

        // TODO: No need to reset before switch back
        if (gui.useSliderTime())
            globalTime.reset();

        marchTime.reset();
        cloudMarch.update(uvec3(30), vec3(-32), vec3(32), (t * 16) / 1000.0);
        duneMarch.update(uvec3(30, 50, 30), vec3(-8), vec3(8), (t * 16) / 2000.0);
        plantMarch.update(uvec3(30, 50, 30), vec3(-8), vec3(8), (t * 16) / 2000.0);

        ImGui::Begin("HAX");
        ImGui::DragFloat3("campos", (float*)&cameraPos, 0.01f);
        ImGui::DragFloat3("camtgt", (float*)&cameraTgt, 0.01f);
        ImGui::DragFloat3("cs", (float*)&cs, 0.001f);
        ImGui::Text("march time: %.1f", marchTime.getSeconds() * 1000);
        ImGui::End();

        mat4 modelToWorld = mat4(1);
        mat3 normalToWorld = mat3(transpose(inverse(modelToWorld)));
        mat4 worldToClip =
            perspective(45.f, window.width() / float(window.height()), 0.01f, 100.f) *
            lookAt(cameraPos, cameraTgt, vec3(0, 1, 0));

        triShader.bind();
        glUniformMatrix4fv(glGetUniformLocation(triShader._progID, "uModelToWorld"), 1, false, (GLfloat*) &modelToWorld);
        glUniformMatrix3fv(glGetUniformLocation(triShader._progID, "uNormalToWorld"), 1, false, (GLfloat*) &normalToWorld);
        glUniformMatrix4fv(glGetUniformLocation(triShader._progID, "uWorldToClip"), 1, false, (GLfloat*) &worldToClip);
        glUniform3fv(glGetUniformLocation(triShader._progID, "uCameraToClip"), 1, (GLfloat*) &cameraPos);

        /* Clouds */
        vec3 lightDir = vec3(-1, 1, -1);
        glUniform3fv(glGetUniformLocation(triShader._progID, "uLightDir"),
                     1, (GLfloat*) &lightDir);
        vec3 additionalColor = vec3(0.1, 0.2, 0.7);
        glUniform3fv(glGetUniformLocation(triShader._progID, "uAdditionalColor"),
                     1, (GLfloat*) &additionalColor);
        vec3 color = vec3(1);
        glUniform3fv(glGetUniformLocation(triShader._progID, "uColor"), 1, (GLfloat*) &color);
        cloudMarch.render();


        /* Dunes */
        lightDir = vec3(-1, -1, -1);
        glUniform3fv(glGetUniformLocation(triShader._progID, "uLightDir"),
                     1, (GLfloat*) &lightDir);
        color = vec3(0.87, 0.62, 0);
        glUniform3fv(glGetUniformLocation(triShader._progID, "uColor"), 1, (GLfloat*) &color);
        duneMarch.render();
        color = vec3(0, 0.87, 0.11);
        glUniform3fv(glGetUniformLocation(triShader._progID, "uColor"), 1, (GLfloat*) &color);
        plantMarch.render();
        sceneProf.endSample();

        if (window.drawGUI())
            gui.endFrame();

        window.endFrame();
        ++t;
    }

    gui.destroy();
    window.destroy();
    exit(EXIT_SUCCESS);
}
