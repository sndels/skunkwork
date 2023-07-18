#ifndef AUDIOSTREAM_HPP
#define AUDIOSTREAM_HPP

#include <SDL_mixer.h>

#include <string>

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
    double getTimeS() const;
    void setTimeS(double timeS);
    double getRow() const;
    void setRow(int32_t row);

private:
    AudioStream() = default;
    ~AudioStream();

    Mix_Music* _music{nullptr};
    bool    _shouldRestart{false};

};

#endif // AUDIOSTREAM_HPP
