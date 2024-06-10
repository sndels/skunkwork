#ifndef AUDIOSTREAM_HPP
#define AUDIOSTREAM_HPP

#include <SDL_mixer.h>

#include <chrono>
#include <string>

class AudioStream
{
  public:
    static AudioStream &getInstance()
    {
        // Only one instance should exist as this uses the Mix_ interfaces and
        // the current implementation doesn't support two streams with
        // independent behavior.
        static AudioStream instance;
        return instance;
    }

    AudioStream(AudioStream const &) = delete;
    AudioStream(AudioStream &&) = delete;
    void operator=(AudioStream const &) = delete;
    void operator=(AudioStream &&) = delete;

    // Static interface for rocket
    static void pauseStream(void *data, int32_t flag);
    static void setStreamRow(void *data, int32_t row);
    static int32_t isStreamPlaying(void *data);

    void init(const std::string &filePath, double bpm, int32_t rpb);
    void destroy();

    bool hasMusic() const;
    Mix_Music *getMusic() const;
    void play();
    bool isPlaying();
    void pause();
    void stop();
    double getTimeS();
    void setTimeS(double timeS);
    double getRow();
    void setRow(int32_t row);

  private:
    AudioStream() = default;
    ~AudioStream() = default;

    Mix_Music *_music{nullptr};
    bool _shouldRestart{false};
    double _timeS{0.f};
    std::chrono::high_resolution_clock::time_point _prevTimeStamp;
};

#endif // AUDIOSTREAM_HPP
