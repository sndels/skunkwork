#include <cmath>
#include <GL/gl3w.h>
#include <sync.h>
#include <track.h>

#include "audioStream.hpp"
#include "gpuProfiler.hpp"
#include "gui.hpp"
#include <cstdio>
#include "quad.hpp"
#include "shader.hpp"
#include "frameBuffer.hpp"
#include "timer.hpp"
#include "window.hpp"

// Comment out to compile in demo-mode, so close when music stops etc.
// #define DEMO_MODE
#ifndef DEMO_MODE
// Comment out to load sync from files
// #define TCPROCKET
#endif // !DEMO_MODE

#ifdef TCPROCKET
//Set up audio callbacks for rocket
static struct sync_cb audioSync = {
    AudioStream::pauseStream,
    AudioStream::setStreamRow,
    AudioStream::isStreamPlaying
};
#endif // TCPROCKET

#define XRES 1920
#define YRES 1080

int main()
{
    Window window;
    if (!window.init(XRES, YRES, "skunkwork"))
        return -1;

#ifdef DEMO_MODE
    SDL_SetWindowFullscreen(window.ptr(), SDL_WINDOW_FULLSCREEN);
#endif // DEMO_MODE

    // Setup imgui
    GUI gui;
    gui.init(window.ptr(), window.ctx());

    Quad q;

    // Set up audio
    std::string musicPath(RES_DIRECTORY);
    musicPath += "illegal_af.wav";
    if (!AudioStream::getInstance().init(musicPath, 175.0, 8))
    {
        gui.destroy();
        window.destroy();
        exit(EXIT_FAILURE);
    }

    // Set up rocket
    sync_device *rocket = sync_create_device(RES_DIRECTORY "rocket/sync");
    if (!rocket)
        printf("[rocket] Failed to create device\n");

    // Set up scene
    std::string vertPath(RES_DIRECTORY);
    vertPath += "shader/basic_vert.glsl";
    std::string basicFragPath(RES_DIRECTORY);
    basicFragPath += "shader/basic_frag.glsl";
    std::string rayMarchingFragPath(RES_DIRECTORY);
    rayMarchingFragPath += "shader/ray_marching_frag.glsl";
    std::string compositeFragPath(RES_DIRECTORY);
    compositeFragPath += "shader/composite_frag.glsl";
    Shader basicShader("Basic", rocket, vertPath, basicFragPath);
    Shader rayMarchingShader("RayMarch", rocket, vertPath, rayMarchingFragPath);
    Shader compositeShader("Composite", rocket, vertPath, compositeFragPath);

#ifdef TCPROCKET
    // Try connecting to rocket-server
    int rocketConnected = sync_connect(rocket, "localhost", SYNC_DEFAULT_PORT) == 0;
    if (!rocketConnected)
        printf("[rocket] Failed to connect to server\n");
#endif // TCPROCKET

    // Init rocket tracks here

    Timer reloadTime;
    Timer globalTime;
    GpuProfiler scenePingProf(5);
    GpuProfiler scenePongProf(5);
    GpuProfiler compositeProf(5);
    std::vector<std::pair<std::string, const GpuProfiler*>> profilers = {
            {"ScenePing", &scenePingProf},
            {"ScenePong", &scenePingProf},
            {"Composite", &compositeProf}
    };

    TextureParams rgba16fParams = {GL_RGBA16F, GL_RGBA, GL_FLOAT,
                                  GL_LINEAR, GL_LINEAR,
                                  GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER};


    // Generate framebuffer for main rendering
    std::vector<TextureParams> sceneTexParams({rgba16fParams});

    FrameBuffer scenePingFbo(XRES, YRES, sceneTexParams);
    FrameBuffer scenePongFbo(XRES, YRES, sceneTexParams);

    AudioStream::getInstance().play();

    // Run the main loop
    while (window.open()) {
        bool const resized = window.startFrame();

#ifndef DEMO_MODE
        if (window.playPausePressed())
        {
            if (AudioStream::getInstance().isPlaying())
                AudioStream::getInstance().pause();
            else
                AudioStream::getInstance().play();
        }
#endif // !DEMO_MODE

        if (resized) {
            scenePingFbo.resize(window.width(), window.height());
            scenePongFbo.resize(window.width(), window.height());
        }

        // Sync
        double syncRow = AudioStream::getInstance().getRow();

#ifdef TCPROCKET
        // Try re-connecting to rocket-server if update fails
        // Drops all the frames, if trying to connect on windows
        if (sync_update(rocket, (int)floor(syncRow), &audioSync, AudioStream::getInstance().getMusic()))
            sync_connect(rocket, "localhost", SYNC_DEFAULT_PORT);
#endif // TCPROCKET

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#ifndef DEMO_MODE
        if (window.drawGUI())
        {
            float const currentTimeS = AudioStream::getInstance().getTimeS();
            float uiTimeS = currentTimeS;
            gui.startFrame(window.height(), uiTimeS, {&basicShader, &rayMarchingShader, &compositeShader}, profilers);
            if (uiTimeS != currentTimeS)
                AudioStream::getInstance().setTimeS(uiTimeS);
        }   

        // Try reloading the shader every 0.5s
        if (reloadTime.getSeconds() > 0.5f) {
            basicShader.reload();
            rayMarchingShader.reload();
            compositeShader.reload();
            reloadTime.reset();
        }

        //TODO: No need to reset before switch back
        if (gui.useSliderTime())
            globalTime.reset();
#endif //! DEMO_MODE

        scenePingProf.startSample();
        basicShader.bind(syncRow);
        scenePingFbo.bindWrite();
        basicShader.setFloat(
            "uTime",
            gui.useSliderTime() ? gui.sliderTime() : globalTime.getSeconds()
        );
        basicShader.setVec2("uRes", (GLfloat)window.width(), (GLfloat)window.height());
        q.render();
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        scenePingProf.endSample();

        scenePongProf.startSample();
        rayMarchingShader.bind(syncRow);
        scenePongFbo.bindWrite();
        rayMarchingShader.setFloat(
            "uTime",
            gui.useSliderTime() ? gui.sliderTime() : globalTime.getSeconds()
        );
        rayMarchingShader.setVec2("uRes", (GLfloat)window.width(), (GLfloat)window.height());
        q.render();
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        scenePongProf.endSample();

        compositeProf.startSample();
        compositeShader.bind(syncRow);
        compositeShader.setFloat(
            "uTime",
            gui.useSliderTime() ? gui.sliderTime() : globalTime.getSeconds()
        );
        compositeShader.setVec2("uRes", (GLfloat)window.width(), (GLfloat)window.height());
        scenePingFbo.bindRead(0, GL_TEXTURE0, compositeShader.getUniformLocation("uScenePingColorDepth"));
        scenePongFbo.bindRead(0, GL_TEXTURE1, compositeShader.getUniformLocation("uScenePongColorDepth"));
        q.render();
        compositeProf.endSample();

#ifndef DEMO_MODE
        if (window.drawGUI())
            gui.endFrame();
#endif // DEMO_MODE

        window.endFrame();

#ifdef DEMO_MODE
        if (!AudioStream::getInstance().isPlaying())
            window.setClose();
#endif // DEMO_MODE
    }

    // Save rocket tracks
    sync_save_tracks(rocket);

    // Release resources
    sync_destroy_device(rocket);

    AudioStream::getInstance().destroy();
    gui.destroy();
    window.destroy();
    exit(EXIT_SUCCESS);
}
