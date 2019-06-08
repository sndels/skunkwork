#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif // _WIN32

#include <cmath>
#include <GL/gl3w.h>
#include <sync.h>
#include <track.h>

#include <glm/gtc/matrix_transform.hpp>

#include "audioStream.hpp"
#include "gpuProfiler.hpp"
#include "gui.hpp"
#include "log.hpp"
#include "marched.hpp"
#include "noise.h"
#include "quad.hpp"
#include "shader.hpp"
#include "timer.hpp"
#include "window.hpp"

using namespace glm;

// Comment out to disable autoplay without tcp-Rocket
//#define MUSIC_AUTOPLAY
// Comment out to load sync from files
#define TCPROCKET
// Comment out to draw gui
//#define DRAW_GUI

#ifdef TCPROCKET
//Set up audio callbacks for rocket
static struct sync_cb audioSync = {
    AudioStream::pauseStream,
    AudioStream::setStreamRow,
    AudioStream::isStreamPlaying
};
#endif // TCPROCKET

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
    if (!window.init(1280, 720, "skunkwork"))
        return -1;
#ifndef DRAW_GUI
    glfwSetInputMode(window.ptr(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
#endif

    // Setup imgui
    GUI gui;
    gui.init(window.ptr());

    Quad q;
    auto sdf = [&](const vec3& pos, const float time) {
        auto pos0 = pos * sin(time);
        return perlin_noise_3d(pos0.x + time, pos0.y + sin(time), pos0.z, 0.1f, 3, 1234);
    };
    Marched m(sdf);

#if (defined(TCPROCKET) || defined(MUSIC_AUTOPLAY))
    // Set up audio
    std::string musicPath(RES_DIRECTORY);
    musicPath += "music/foo.mp3";
    AudioStream::getInstance().init(musicPath, 175.0, 8);
    int32_t streamHandle = AudioStream::getInstance().getStreamHandle();
#endif // TCPROCKET || MUSIC_AUTOPLAY

    // Set up rocket
    sync_device *rocket = sync_create_device("sync");
    if (!rocket)
        ADD_LOG("[rocket] Failed to create device\n");

    // Set up scene
    std::string vertPath(RES_DIRECTORY);
    std::string fragPath(RES_DIRECTORY);
    vertPath += "shader/tri_vert.glsl";
    fragPath += "shader/tri_frag.glsl";
    Shader triShader("Scene", rocket, vertPath, fragPath, "");

#ifdef TCPROCKET
    // Try connecting to rocket-server
    int rocketConnected = sync_tcp_connect(rocket, "localhost", SYNC_DEFAULT_PORT) == 0;
    if (!rocketConnected)
        ADD_LOG("[rocket] Failed to connect to server\n");
#endif // TCPROCKET

    // Init rocket tracks here

    Timer reloadTime;
    Timer globalTime;
    Timer marchTime;
    GpuProfiler sceneProf(5);
    std::vector<std::pair<std::string, const GpuProfiler*>> profilers = {};

#ifdef MUSIC_AUTOPLAY
    AudioStream::getInstance().play();
#endif // MUSIC_AUTOPLAY


    glClearColor(0.2, 0.2, 0.2, 1.0);

    // Run the main loop
    while (window.open()) {
        window.startFrame();

        // Sync
        double syncRow = AudioStream::getInstance().getRow();

#ifdef TCPROCKET
        // Try re-connecting to rocket-server if update fails
        // Drops all the frames, if trying to connect on windows
        if (sync_update(rocket, (int)floor(syncRow), &audioSync, (void *)&streamHandle))
            sync_tcp_connect(rocket, "localhost", SYNC_DEFAULT_PORT);
#endif // TCPROCKET

#ifdef DRAW_GUI
        if (window.drawGUI())
            gui.startFrame(window.height(), triShader.dynamicUniforms(), profilers);
#endif 

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Try reloading the shader every 0.5s
        if (reloadTime.getSeconds() > 0.5f) {
            triShader.reload();
            reloadTime.reset();
        }

#ifdef DRAW_GUI
        //TODO: No need to reset before switch back
        if (gui.useSliderTime())
            globalTime.reset();
#endif

        marchTime.reset();
        m.update(uvec3(40), vec3(0, 0, 0), vec3(4, 4, 4), globalTime.getSeconds());

#ifdef DRAW_GUI
        ImGui::Begin("HAX");
        ImGui::Text("march time: %.1f", marchTime.getSeconds() * 1000);
        ImGui::End();
#endif

        glClearColor(0.2, 0.2, 0.2, 1.0);
        vec3 cameraPos(0, 0, 3);

        mat4 modelToWorld = mat4(1);
        mat3 normalToWorld = mat3(transpose(inverse(modelToWorld)));
        mat4 worldToClip =
            perspective(45.f, window.width() / float(window.height()), 0.01f, 10.f) *
            lookAt(cameraPos, vec3(0, 0, 0), vec3(0, 1, 0));

        triShader.bind(syncRow);
        glUniformMatrix4fv(glGetUniformLocation(triShader._progID, "uModelToWorld"), 1, false, (GLfloat*) &modelToWorld);
        glUniformMatrix3fv(glGetUniformLocation(triShader._progID, "uNormalToWorld"), 1, false, (GLfloat*) &normalToWorld);
        glUniformMatrix4fv(glGetUniformLocation(triShader._progID, "uWorldToClip"), 1, false, (GLfloat*) &worldToClip);
        glUniform3fv(glGetUniformLocation(triShader._progID, "uCameraToClip"), 1, (GLfloat*) &cameraPos);
        glEnable(GL_DEPTH_TEST);
        m.render();

#ifdef DRAW_GUI
        if (window.drawGUI())
            gui.endFrame();
#endif

        window.endFrame();

#ifdef MUSIC_AUTOPLAY
        if (!AudioStream::getInstance().isPlaying()) glfwSetWindowShouldClose(window.ptr(), GLFW_TRUE);
#endif // MUSIC_AUTOPLAY
    }

    // Save rocket tracks
    sync_save_tracks(rocket);

    // Release resources
    sync_destroy_device(rocket);

    gui.destroy();
    window.destroy();
    exit(EXIT_SUCCESS);
}
