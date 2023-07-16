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
#include "timer.hpp"
#include "window.hpp"

// Comment out to disable autoplay without tcp-Rocket
// #define MUSIC_AUTOPLAY
// Comment out to load sync from files
// #define TCPROCKET

#ifdef TCPROCKET
//Set up audio callbacks for rocket
static struct sync_cb audioSync = {
    AudioStream::pauseStream,
    AudioStream::setStreamRow,
    AudioStream::isStreamPlaying
};
#endif // TCPROCKET

int main()
{
    Window window;
    if (!window.init(1280, 720, "skunkwork"))
        return -1;

    // Setup imgui
    GUI gui;
    gui.init(window.ptr(), window.ctx());

    Quad q;

#if (defined(TCPROCKET) || defined(MUSIC_AUTOPLAY))
    // Set up audio
    std::string musicPath(RES_DIRECTORY);
    musicPath += "illegal_af.wav";
    if (!AudioStream::getInstance().init(musicPath, 175.0, 8))
    {
        gui.destroy();
        window.destroy();
        exit(EXIT_FAILURE);
    }
#endif // TCPROCKET || MUSIC_AUTOPLAY

    // Set up rocket
    sync_device *rocket = sync_create_device("sync");
    if (!rocket)
        printf("[rocket] Failed to create device\n");

    // Set up scene
    std::string vertPath(RES_DIRECTORY);
    vertPath += "shader/basic_vert.glsl";
    std::string fragPath(RES_DIRECTORY);
    fragPath += "shader/basic_frag.glsl";
    Shader shader("Scene", rocket, vertPath, fragPath);

#ifdef TCPROCKET
    // Try connecting to rocket-server
    int rocketConnected = sync_connect(rocket, "localhost", SYNC_DEFAULT_PORT) == 0;
    if (!rocketConnected)
        printf("[rocket] Failed to connect to server\n");
#endif // TCPROCKET

    // Init rocket tracks here

    Timer reloadTime;
    Timer globalTime;
    GpuProfiler sceneProf(5);
    std::vector<std::pair<std::string, const GpuProfiler*>> profilers = 
        {{"Scene", &sceneProf}};

#ifdef MUSIC_AUTOPLAY
    AudioStream::getInstance().play();
#endif // MUSIC_AUTOPLAY

    // Run the main loop
    while (window.open()) {
        window.startFrame();

        // Sync
        double syncRow = AudioStream::getInstance().getRow();

#ifdef TCPROCKET
        // Try re-connecting to rocket-server if update fails
        // Drops all the frames, if trying to connect on windows
        if (sync_update(rocket, (int)floor(syncRow), &audioSync, AudioStream::getInstance().getMusic()))
            sync_connect(rocket, "localhost", SYNC_DEFAULT_PORT);
#endif // TCPROCKET

        if (window.drawGUI())
            gui.startFrame(window.height(), shader.dynamicUniforms(), profilers);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Try reloading the shader every 0.5s
        if (reloadTime.getSeconds() > 0.5f) {
            shader.reload();
            reloadTime.reset();
        }

        //TODO: No need to reset before switch back
        if (gui.useSliderTime())
            globalTime.reset();

        sceneProf.startSample();
        shader.bind(syncRow);
        shader.setFloat(
            "uTime",
            gui.useSliderTime() ? gui.sliderTime() : globalTime.getSeconds()
        );
        shader.setVec2("uRes", (GLfloat)window.width(), (GLfloat)window.height());
        q.render();
        sceneProf.endSample();

        if (window.drawGUI())
            gui.endFrame();

        window.endFrame();

#ifdef MUSIC_AUTOPLAY
        if (!AudioStream::getInstance().isPlaying())
            window.setClose();
#endif // MUSIC_AUTOPLAY
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
