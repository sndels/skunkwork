#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif // _WIN32

#include <cmath>
#include <GL/gl3w.h>
#include <sync.h>
#include <track.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

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
#define DRAW_GUI

#ifdef TCPROCKET
//Set up audio callbacks for rocket
static struct sync_cb audioSync = {
    AudioStream::pauseStream,
    AudioStream::setStreamRow,
    AudioStream::isStreamPlaying
};
#endif // TCPROCKET

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

static float seaSdf(const vec3& pos, const float time) {
        const vec3 pos0 = pos * vec3(3);
        return pos.y + 1 * cos(pos0.x + time) + 1 * cos(pos0.x * 3 + time) + 1 * sin(pos0.z + time);
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
    if (!window.init(1280, 720, "skunkwork"))
        return -1;
#ifndef DRAW_GUI
    glfwSetInputMode(window.ptr(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
#endif

    // Setup imgui
    GUI gui;
    gui.init(window.ptr());

    Quad q;

    Marched cloudMarch(cloudSdf);
    Marched seaMarch(seaSdf);
    Marched duneMarch(duneSdf);
    Marched plantMarch(plantSdf);

#if (defined(TCPROCKET) || defined(MUSIC_AUTOPLAY))
    // Set up audio
    std::string musicPath(RES_DIRECTORY);
    musicPath += "music/aawikko_0230_final.wav";
    AudioStream::getInstance().init(musicPath, 130.0, 8);
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
    const sync_track* timeTrack = sync_get_track(rocket, "time");
    const sync_track* sceneTrack = sync_get_track(rocket, "scene");

    Timer reloadTime;
    Timer globalTime;
    Timer marchTime;
    GpuProfiler sceneProf(5);
    std::vector<std::pair<std::string, const GpuProfiler*>> profilers = {};

#ifdef MUSIC_AUTOPLAY
    AudioStream::getInstance().play();
#endif // MUSIC_AUTOPLAY


    glEnable(GL_DEPTH_TEST);
    vec3 sky(0.3, 0.5, 0.8);
    vec3 cameraPos(-30, -20, 30);
    vec3 cameraTgt(0, -20.5, 0);
    // Run the main loop
    while (window.open()) {
        window.startFrame();

        // Sync
        double syncRow = AudioStream::getInstance().getRow();
        float rTime = (float)sync_get_val(timeTrack, syncRow);
        size_t scene = min(max((int)sync_get_val(sceneTrack, syncRow), 0), 2);

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

        vec3 clearColor = sky;
        if (rTime < 1)
            clearColor = mix(vec3(0), sky, rTime);
        else if (rTime > 121.f && rTime < 123.93f)
            clearColor = mix(sky, vec3(0), (rTime - 121.f) / 4.f);
        else if (rTime > 123.93f)
            clearColor = vec3(0);
        glClearColor(clearColor.x, clearColor.y, clearColor.z, 1.0);

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
        if (scene == 0 || scene == 1)
            cloudMarch.update(uvec3(30), vec3(-32), vec3(32), rTime);
        if (scene == 1)
            seaMarch.update(uvec3(30), vec3(-32), vec3(32), rTime);
        if (scene == 2) {
            duneMarch.update(uvec3(60, 60, 40), vec3(-15), vec3(15), rTime / 10);
            plantMarch.update(uvec3(40, 60, 40), vec3(-15), vec3(15), rTime / 10);
        }

#ifdef DRAW_GUI
        ImGui::Begin("HAX");
        ImGui::DragFloat3("campos", (float*)&cameraPos, 0.01f);
        ImGui::DragFloat3("camtgt", (float*)&cameraTgt, 0.01f);
        ImGui::DragFloat3("cs", (float*)&cs, 0.001f);
        ImGui::Text("march time: %.1f", marchTime.getSeconds() * 1000);
        ImGui::End();
#endif


        if (scene == 0) {
            cameraPos = vec3(-40, -15, -10);
            cameraTgt = vec3(0, -10.5, -10);
        } else if (scene == 1) {
            cameraPos = vec3(-30, -20, 20);
            cameraTgt = vec3(0, -20.5, 0);
        } else {
            cameraPos = vec3(10, 5, 16);
            cameraTgt = vec3(0, 0, 0);
        }
        mat4 worldToClip =
            perspective(45.f, window.width() / float(window.height()), 0.01f, 100.f) *
            lookAt(cameraPos, cameraTgt, vec3(0, 1, 0));

        triShader.bind(syncRow);
        glUniform1f(glGetUniformLocation(triShader._progID, "uTime"), rTime);
        glUniformMatrix4fv(glGetUniformLocation(triShader._progID, "uWorldToClip"), 1, false, (GLfloat*) &worldToClip);
        glUniform3fv(glGetUniformLocation(triShader._progID, "uEye"), 1, (GLfloat*) &cameraPos);

        /* Clouds */
        if (scene == 0 || scene == 1) {
        vec3 lightDir = vec3(-1, 1, -1);
        mat4 modelToWorld = mat4(1);
        mat3 normalToWorld = mat3(transpose(inverse(modelToWorld)));
        glUniformMatrix4fv(glGetUniformLocation(triShader._progID, "uModelToWorld"), 1, false, (GLfloat*) &modelToWorld);
        glUniformMatrix3fv(glGetUniformLocation(triShader._progID, "uNormalToWorld"), 1, false, (GLfloat*) &normalToWorld);
        glUniform3fv(glGetUniformLocation(triShader._progID, "uLightDir"),
                     1, (GLfloat*) &lightDir);
        vec3 additionalColor = vec3(0.1, 0.2, 0.7);
        glUniform3fv(glGetUniformLocation(triShader._progID, "uAdditionalColor"),
                     1, (GLfloat*) &additionalColor);
        vec3 color = vec3(1);
        glUniform3fv(glGetUniformLocation(triShader._progID, "uColor"), 1, (GLfloat*) &color);
        cloudMarch.render();
        }

        /* Sea */
        if (scene == 1) {
        vec3 lightDir = vec3(-1, -1, -1);
        mat4 modelToWorld = translate(vec3(0, -24, 0));
        mat4 normalToWorld = mat3(transpose(inverse(modelToWorld)));
        glUniformMatrix4fv(glGetUniformLocation(triShader._progID, "uModelToWorld"), 1, false, (GLfloat*) &modelToWorld);
        glUniformMatrix3fv(glGetUniformLocation(triShader._progID, "uNormalToWorld"), 1, false, (GLfloat*) &normalToWorld);
        glUniform3fv(glGetUniformLocation(triShader._progID, "uLightDir"),
                     1, (GLfloat*) &lightDir);
        vec3 color = vec3(0, 0, 1);
        glUniform3fv(glGetUniformLocation(triShader._progID, "uColor"), 1, (GLfloat*) &color);
        seaMarch.render();
        }

        /* Dunes */
        if (scene == 2) {
        vec3 lightDir = vec3(-1, -1, -1);
        mat4 modelToWorld = mat4(1);
        mat3 normalToWorld = mat3(transpose(inverse(modelToWorld)));
        glUniformMatrix4fv(glGetUniformLocation(triShader._progID, "uModelToWorld"), 1, false, (GLfloat*) &modelToWorld);
        glUniformMatrix3fv(glGetUniformLocation(triShader._progID, "uNormalToWorld"), 1, false, (GLfloat*) &normalToWorld);
        glUniform3fv(glGetUniformLocation(triShader._progID, "uLightDir"),
                     1, (GLfloat*) &lightDir);
        vec3 color = vec3(0.87, 0.62, 0);
        glUniform3fv(glGetUniformLocation(triShader._progID, "uColor"), 1, (GLfloat*) &color);
        duneMarch.render();
        color = vec3(0, 0.87, 0.11);
        glUniform3fv(glGetUniformLocation(triShader._progID, "uColor"), 1, (GLfloat*) &color);
        plantMarch.render();
        }

        sceneProf.endSample();

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
