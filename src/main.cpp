#include <GL/gl3w.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sync.h>
#include <track.h>

#include "audioStream.hpp"
#include "error.hpp"
#include "frameBuffer.hpp"
#include "gpuProfiler.hpp"
#include "gui.hpp"
#include "quad.hpp"
#include "shader.hpp"
#include "timer.hpp"
#include "window.hpp"
#include <cstdio>

// Comment out to compile in demo-mode, so close when music stops etc.
// #define DEMO_MODE
#ifndef DEMO_MODE
// Comment out to load sync from files
// Uncomment to use rocket editor
// #define TCPROCKET
#endif // !DEMO_MODE

namespace
{

#ifdef TCPROCKET
// Set up audio callbacks for rocket
sync_cb sAudioSync = {
    AudioStream::pauseStream, AudioStream::setStreamRow,
    AudioStream::isStreamPlaying};
#endif // TCPROCKET

const uint32_t sXRes = 1920;
const uint32_t sYRes = 1080;

const float sBeatsPerMinute = 175.f;
const int32_t sRowsPerBeat = 8;

std::string fullResPath(const std::string &pathInRes)
{
    // RES_DIRECTORY is a constant set by cmake based on the build config.
    return RES_DIRECTORY + pathInRes;
}

} // namespace

#ifdef DEMO_MODE

#define UPDATE_COMMON_UNIFORMS(shader)                                         \
    do                                                                         \
    {                                                                          \
        shader.setFloat("uTime", currentTimeS);                                \
        shader.setVec2(                                                        \
            "uRes", (GLfloat)window.width(), (GLfloat)window.height());        \
        shader.setFloat(                                                       \
            "uAspectRatio",                                                    \
            (GLfloat)window.width() / (GLfloat)window.height());               \
    } while (0)

#else // !DEMO_NODE
#define UPDATE_COMMON_UNIFORMS(shader)                                         \
    do                                                                         \
    {                                                                          \
        shader.setFloat(                                                       \
            "uTime",                                                           \
            gui.useSliderTime() ? gui.sliderTime() : globalTime.getSeconds()); \
        shader.setVec2(                                                        \
            "uRes", (GLfloat)window.width(), (GLfloat)window.height());        \
        shader.setFloat(                                                       \
            "uAspectRatio",                                                    \
            (GLfloat)window.width() / (GLfloat)window.height());               \
    } while (0)
#endif // DEMO_MODE

#if defined(DEMO_MODE) && defined(_WIN32)

// Use WinMain (no terminal) in demo mode to ensure that the fullscreen app gets
// focus instead of the potential newly opened terminal window.
int APIENTRY WinMain(
    HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, PSTR /*lpCmdLine*/,
    INT /*nCmdShow*/)
{
    int argc = __argc;
    char **argv = __argv;

#else // !DEMO_MODE || !_WIN32

int main(int argc, char *argv[])
{

#endif // DEMO_MODE && _WIN32

    int displayIndex = 0;
    if (argc == 2)
    {
        if (strncmp(argv[1], "1", 1) == 0)
            displayIndex = 1;
        else if (strncmp(argv[1], "2", 1) == 0)
            displayIndex = 2;
        else
        {
            reportError("Unexpected CLI argument, only '1', '2' is supported "
                        "for selecting second or third connected display \n");
            exitWithErrors(nullptr);
        }
    }
    Window window;
    // This also sets up our gl profile and backbuffer bits
    if (!window.init(sXRes, sYRes, "skunkwork", displayIndex))
        return -1;

#ifdef DEMO_MODE
    SDL_SetWindowFullscreen(window.ptr(), SDL_WINDOW_FULLSCREEN);
    SDL_ShowCursor(false);
#endif // DEMO_MODE

    // Setup imgui
    GUI gui;
    gui.init(window.ptr(), window.ctx());

    Quad q;

    // Set up audio
    const std::string musicPath = fullResPath("no_music_path_given");

    AudioStream &audioStream = AudioStream::getInstance();

    audioStream.init(musicPath, sBeatsPerMinute, sRowsPerBeat);
    if (!audioStream.hasMusic())
    {
#ifdef DEMO_MODE
        exitWithErrors(&window);
#else  //  !DEMO_MODE
        reportError("Running without rocket and timeline scrubbing");
#endif // DEMO_MODE
    }

    // Set up rocket
    sync_device *rocket = nullptr;
    if (audioStream.hasMusic())
    {
        rocket = sync_create_device(
            std::filesystem::relative(
                std::filesystem::path{fullResPath("rocket/sync")},
                std::filesystem::current_path())
                .lexically_normal()
                .generic_string()
                .c_str());
        if (!rocket)
            reportError("[rocket] Failed to create device");
    }

    // Set up shaders
    const std::string vertPath{fullResPath("shader/basic_vert.glsl")};
    std::vector<Shader> sceneShaders;
    sceneShaders.emplace_back(
        "Basic", rocket, vertPath, fullResPath("shader/basic_frag.glsl"));
    sceneShaders.emplace_back(
        "RayMarch", rocket, vertPath,
        fullResPath("shader/ray_marching_frag.glsl"));
    sceneShaders.emplace_back(
        "Text", rocket, vertPath, fullResPath("shader/text_frag.glsl"));
    Shader compositeShader(
        "Composite", rocket, vertPath,
        fullResPath("shader/composite_frag.glsl"));

#ifdef TCPROCKET
    if (rocket != nullptr)
    {
        // Try connecting to rocket-server
        int rocketConnected =
            sync_tcp_connect(rocket, "localhost", SYNC_DEFAULT_PORT) == 0;
        if (!rocketConnected)
            reportError("[rocket] Failed to connect to server");
    }
#endif // TCPROCKET

    // Init rocket tracks here
    sync_track const *pingScene = nullptr;
    sync_track const *pongScene = nullptr;
    if (rocket != nullptr)
    {
        pingScene = sync_get_track(rocket, "pingScene");
        pongScene = sync_get_track(rocket, "pongScene");
    }

    Timer reloadTime;
    Timer globalTime;
    GpuProfiler scenePingProf(5);
    GpuProfiler scenePongProf(5);
    GpuProfiler compositeProf(5);
    std::vector<std::pair<std::string, const GpuProfiler *>> profilers = {
        {"ScenePing", &scenePingProf},
        {"ScenePong", &scenePongProf},
        {"Composite", &compositeProf}};

    // Generate framebuffers for main rendering
    TextureParams rgba16fParams = {
        GL_RGBA16F,         GL_RGBA,           GL_FLOAT, GL_LINEAR, GL_LINEAR,
        GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER};
    std::vector<TextureParams> sceneTexParams({rgba16fParams});
    FrameBuffer scenePingFbo(sXRes, sYRes, sceneTexParams);
    FrameBuffer scenePongFbo(sXRes, sYRes, sceneTexParams);

    // You'll want to enable depth test any FrameBuffer has depth and some pass
    // writes into it. Either toggle glEnable/glDisable around the pass that
    // should test, if some passes shouldn't, or just enable here and leave it
    // if all passes can have it enabled.

    if (audioStream.hasMusic())
        audioStream.play();

    int32_t overrideIndex = -1;
    float currentTimeS = -1.f;

    // Run the main loop
    while (window.open())
    {
        bool const resized = window.startFrame();

#ifndef DEMO_MODE
        if (window.playPausePressed())
        {
            if (audioStream.hasMusic())
            {
                if (audioStream.isPlaying())
                    audioStream.pause();
                else
                    audioStream.play();
            }
        }
#endif // !DEMO_MODE

        if (resized)
        {
            scenePingFbo.resize(window.width(), window.height());
            scenePongFbo.resize(window.width(), window.height());
        }

        // Sync
        double syncRow = 0.0;
        if (audioStream.hasMusic())
            syncRow = audioStream.getRow();

#ifdef TCPROCKET
        if (rocket != nullptr)
        {
            // Try re-connecting to rocket-server if update fails
            // NOTE: Framerate grinds to a halt if trying to connect on windows
            if (sync_update(
                    rocket, (int)floor(syncRow), &sAudioSync, &audioStream))
                sync_tcp_connect(rocket, "localhost", SYNC_DEFAULT_PORT);
        }
#endif // TCPROCKET

        int32_t pingIndex = 0;
        if (pingScene != nullptr)
            pingIndex = std::clamp(
                (int32_t)sync_get_val(pingScene, syncRow), 0,
                (int32_t)sceneShaders.size() - 1);
        int32_t pongIndex = 0;
        if (pongScene != nullptr)
            pongIndex = std::clamp(
                (int32_t)sync_get_val(pongScene, syncRow), 0,
                (int32_t)sceneShaders.size() - 1);

        if (audioStream.hasMusic())
            currentTimeS = (float)audioStream.getTimeS();
#ifndef DEMO_MODE
        if (window.drawGUI())
        {
            float uiTimeS = currentTimeS;

            std::vector<Shader *> shaders{&compositeShader};
            for (Shader &s : sceneShaders)
                shaders.push_back(&s);

            gui.startFrame(overrideIndex, uiTimeS, shaders, profilers);
            overrideIndex =
                std::clamp(overrideIndex, -1, (int32_t)sceneShaders.size() - 1);

            if (uiTimeS != currentTimeS)
            {
                if (audioStream.hasMusic())
                    audioStream.setTimeS(uiTimeS);
                else
                    currentTimeS = uiTimeS;
            }
        }

        // Try reloading the shader every 0.5s
        if (reloadTime.getSeconds() > 0.5f)
        {
            compositeShader.reload();
            for (Shader &s : sceneShaders)
                s.reload();
            reloadTime.reset();
        }

        if (gui.shouldResetTime())
            globalTime.reset();

        if (overrideIndex >= 0)
        {
            scenePingProf.startSample();
            sceneShaders[overrideIndex].bind(syncRow);

            // Draw into custom fbo in case we rely on enough alpha bits etc.
            // This assumes that pingFbo satisfies all scenes' requirements
            scenePingFbo.bindWrite();

            // Clearing is not strictly necessary for all shaders but it's fast
            // enough that we probably don't care about the extra work
            glClearColor(0.f, 0.f, 0.f, 0.f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            UPDATE_COMMON_UNIFORMS(sceneShaders[overrideIndex]);
            q.render();

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

            // Blit result into the backbuffer as we don't have the special
            // composite pass here
            glDrawBuffer(GL_BACK);

            glClearColor(0.f, 0.f, 0.f, 0.f);
            glClear(GL_COLOR_BUFFER_BIT);

            scenePingFbo.bindBlitRead();
            glBlitFramebuffer(
                0, 0, window.width(), window.height(), 0, 0, window.width(),
                window.height(), GL_COLOR_BUFFER_BIT, GL_NEAREST);

            scenePingProf.endSample();
        }
        else
#endif // !DEMO_MODE
        {
            {
                // Draw the first scene offscreen
                scenePingProf.startSample();

                sceneShaders[pingIndex].bind(syncRow);
                scenePingFbo.bindWrite();

                // Clearing is not strictly necessary for all shaders but it's
                // fast enough that we probably don't care about the extra work
                glClearColor(0.f, 0.f, 0.f, 0.f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                UPDATE_COMMON_UNIFORMS(sceneShaders[pingIndex]);

                q.render();

                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                scenePingProf.endSample();
            }

            {
                // Draw the second scene offscreen
                scenePongProf.startSample();

                sceneShaders[pongIndex].bind(syncRow);
                scenePongFbo.bindWrite();

                // Clearing is not strictly necessary for all shaders but it's
                // fast enough that we probably don't care about the extra work
                glClearColor(0.f, 0.f, 0.f, 0.f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                UPDATE_COMMON_UNIFORMS(sceneShaders[pongIndex]);

                q.render();

                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                scenePongProf.endSample();
            }

            {
                // Composite the two offscreen images together onto the
                // backbuffer
                compositeProf.startSample();
                compositeShader.bind(syncRow);

                // Clearing is not strictly necessary for all shaders but it's
                // fast enough that we probably don't care about the extra work
                glClearColor(0.f, 0.f, 0.f, 0.f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                UPDATE_COMMON_UNIFORMS(compositeShader);

                scenePingFbo.bindRead(
                    0, GL_TEXTURE0,
                    compositeShader.getUniformLocation("uScenePingColorDepth"));
                scenePongFbo.bindRead(
                    0, GL_TEXTURE1,
                    compositeShader.getUniformLocation("uScenePongColorDepth"));

                q.render();

                compositeProf.endSample();
            }
        }

#ifndef DEMO_MODE
        if (window.drawGUI())
            gui.endFrame();
#endif // DEMO_MODE

        window.endFrame();

#ifdef DEMO_MODE
        if (!audioStream.isPlaying())
            window.setClose();
#endif // DEMO_MODE
    }

#ifdef TCPROCKET
    // Save rocket tracks
    if (rocket != nullptr)
        sync_save_tracks(rocket);
#endif // TCPROCKET

    // Release resources

    if (rocket != nullptr)
        sync_destroy_device(rocket);

    gui.destroy();
    window.destroy();

    exit(EXIT_SUCCESS);
}
