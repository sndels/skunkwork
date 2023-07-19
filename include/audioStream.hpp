#ifndef AUDIOSTREAM_HPP
#define AUDIOSTREAM_HPP

#include <SDL_mixer.h>

#include <string>
#include <chrono>

class AudioStream
{
public:
    static AudioStream& getInstance()
    {
        // The only instance
        static AudioStream instance;
        return instance;
    }

    // Enforce singleton
    AudioStream(AudioStream const&) = delete;
    void operator=(AudioStream const&) = delete;

    // Static interface for rocket
    static void pauseStream(void* data, int32_t flag);
    static void setStreamRow(void* data, int32_t row);
    static int32_t isStreamPlaying(void* data);

    bool init(const std::string& filePath, double bpm, int32_t rpb);
    void destroy();
    Mix_Music* getMusic() const;
    void play();
    bool isPlaying();
    void pause();
    void stop();
    double getTimeS();
    void setTimeS(double timeS);
    double getRow();
    void setRow(int32_t row);

private:
    AudioStream();
    ~AudioStream();

    Mix_Music* _music{nullptr};
    bool _shouldRestart{false};
    double _timeS{0.f};
    std::chrono::high_resolution_clock::time_point _prevTimeStamp;
};

#endif // AUDIOSTREAM_HPP
