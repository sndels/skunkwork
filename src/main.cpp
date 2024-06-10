#include <GL/gl3w.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sync.h>
#include <track.h>

#include "audioStream.hpp"
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

#ifdef TCPROCKET
// Set up audio callbacks for rocket
static struct sync_cb audioSync = {
    AudioStream::pauseStream, AudioStream::setStreamRow,
    AudioStream::isStreamPlaying};
#endif // TCPROCKET

#define XRES 1920
#define YRES 1080

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
int APIENTRY WinMain(
    HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    int argc = __argc;
    char **argv = __argv;
#else  // !DEMO_MODE || !_WIN32
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
            fprintf(
                stderr, "Unexpected CLI argument, only '1', '2' is supported "
                        "for selecting second or third connected display \n");
            exit(EXIT_FAILURE);
        }
    }
    Window window;
    if (!window.init(XRES, YRES, "skunkwork", displayIndex))
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
    std::string musicPath(RES_DIRECTORY);
    musicPath += "no_music_path_given";

    AudioStream &audioStream = AudioStream::getInstance();

    float beatsPerMinute = 175.0;
    int32_t rowsPerBeat = 8;
    audioStream.init(musicPath, beatsPerMinute, rowsPerBeat);
    if (!audioStream.hasMusic())
    {
#ifdef DEMO_MODE
        exit(EXIT_FAILURE);
#else  //  !DEMO_MODE
        fprintf(
            stderr, "Running without rocket and timeline scrubbing.\n",
            musicPath.c_str());
#endif // DEMO_MODE
    }

    // Set up rocket
    sync_device *rocket = nullptr;
    if (audioStream.hasMusic())
    {
        rocket = sync_create_device(
            std::filesystem::relative(
                std::filesystem::path{RES_DIRECTORY "rocket/sync"},
                std::filesystem::current_path())
                .lexically_normal()
                .generic_string()
                .c_str());
        if (!rocket)
            fprintf(stderr, "[rocket] Failed to create device\n");
    }

    // Set up scene
    std::string vertPath{RES_DIRECTORY "shader/basic_vert.glsl"};
    std::vector<Shader> sceneShaders;
    sceneShaders.emplace_back(
        "Basic", rocket, vertPath, RES_DIRECTORY "shader/basic_frag.glsl");
    sceneShaders.emplace_back(
        "RayMarch", rocket, vertPath,
        RES_DIRECTORY "shader/ray_marching_frag.glsl");
    sceneShaders.emplace_back(
        "Text", rocket, vertPath, RES_DIRECTORY "shader/text_frag.glsl");
    Shader compositeShader(
        "Composite", rocket, vertPath,
        RES_DIRECTORY "shader/composite_frag.glsl");

#ifdef TCPROCKET
    if (rocket != nullptr)
    {
        // Try connecting to rocket-server
        int rocketConnected =
            sync_tcp_connect(rocket, "localhost", SYNC_DEFAULT_PORT) == 0;
        if (!rocketConnected)
            fprintf(stderr, "[rocket] Failed to connect to server\n");
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

    TextureParams rgba16fParams = {
        GL_RGBA16F,         GL_RGBA,           GL_FLOAT, GL_LINEAR, GL_LINEAR,
        GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER};

    // Generate framebuffer for main rendering
    std::vector<TextureParams> sceneTexParams({rgba16fParams});

    FrameBuffer scenePingFbo(XRES, YRES, sceneTexParams);
    FrameBuffer scenePongFbo(XRES, YRES, sceneTexParams);

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
                    rocket, (int)floor(syncRow), &audioSync, &audioStream))
                sync_tcp_connect(rocket, "localhost", SYNC_DEFAULT_PORT);
        }
#endif // TCPROCKET

        int32_t pingIndex = 0;
        if (pingScene != nullptr)
            pingIndex = std::clamp(
                (int32_t)(float)sync_get_val(pingScene, syncRow), 0,
                (int32_t)sceneShaders.size() - 1);
        int32_t pongIndex = 0;
        if (pongScene != nullptr)
            pongIndex = std::clamp(
                (int32_t)(float)sync_get_val(pongScene, syncRow), 0,
                (int32_t)sceneShaders.size() - 1);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

        // TODO: No need to reset before switch back
        if (gui.shouldResetTime())
            globalTime.reset();

        if (overrideIndex >= 0)
        {
            scenePingProf.startSample();
            sceneShaders[overrideIndex].bind(syncRow);
            UPDATE_COMMON_UNIFORMS(sceneShaders[overrideIndex]);
            q.render();
            scenePingProf.endSample();
        }
        else
#endif //! DEMO_MODE
        {
            scenePingProf.startSample();

            sceneShaders[pingIndex].bind(syncRow);
            scenePingFbo.bindWrite();

            UPDATE_COMMON_UNIFORMS(sceneShaders[pingIndex]);

            q.render();

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            scenePingProf.endSample();

            scenePongProf.startSample();

            sceneShaders[pongIndex].bind(syncRow);
            scenePongFbo.bindWrite();

            UPDATE_COMMON_UNIFORMS(sceneShaders[pongIndex]);

            q.render();

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            scenePongProf.endSample();

            compositeProf.startSample();
            compositeShader.bind(syncRow);
            compositeShader.setFloat(
                "uTime",
#ifdef DEMO_MODE
                currentTimeS
#else  // DEMO_NODE
                gui.useSliderTime() ? gui.sliderTime() : globalTime.getSeconds()
#endif // DEMO_MODE
            );
            compositeShader.setVec2(
                "uRes", (GLfloat)window.width(), (GLfloat)window.height());
            scenePingFbo.bindRead(
                0, GL_TEXTURE0,
                compositeShader.getUniformLocation("uScenePingColorDepth"));
            scenePongFbo.bindRead(
                0, GL_TEXTURE1,
                compositeShader.getUniformLocation("uScenePongColorDepth"));
            q.render();
            compositeProf.endSample();
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
