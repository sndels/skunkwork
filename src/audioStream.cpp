#include "audioStream.hpp"

#include <cassert>
#include <cstdio>

namespace
{

static float ROW_PER_S = 0.f;

} // namespace

void AudioStream::pauseStream(void *data, int32_t flag)
{
    AudioStream *audioStream = static_cast<AudioStream *>(data);

    if (!audioStream->hasMusic())
        return;

    if (flag)
        Mix_PauseMusic();
    else if (Mix_PlayingMusic())
        Mix_ResumeMusic();
    else
    {
        Mix_Music *music = (Mix_Music *)data;
        Mix_PlayMusic(music, 0);
    }
}

void AudioStream::setStreamRow(void *data, int32_t row)
{
    AudioStream *audioStream = static_cast<AudioStream *>(data);

    if (!audioStream->hasMusic())
        return;

    double const timeS = row / (double)ROW_PER_S;
    Mix_SetMusicPosition(timeS);
}

int32_t AudioStream::isStreamPlaying(void *data)
{
    AudioStream *audioStream = static_cast<AudioStream *>(data);

    if (!audioStream->hasMusic())
        return 0;

    return Mix_PlayingMusic();
}

void AudioStream::init(const std::string &filePath, double bpm, int32_t rpb)
{
    assert(_music == nullptr && "Already initialized");

    _prevTimeStamp = std::chrono::high_resolution_clock::now();

    int audio_rate = 44100;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    uint16_t audio_format = AUDIO_S16LSB;
#else
    uint16_t audio_format = AUDIO_S16MSB;
#endif
    int audio_channels = 2;

    if (Mix_OpenAudio(audio_rate, audio_format, audio_channels, 4096) < 0)
    {
        fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        return;
    }

    Mix_QuerySpec(&audio_rate, &audio_format, &audio_channels);
    printf(
        "Opened audio at %d Hz %d bit%s %s\n", audio_rate,
        (audio_format & 0xFF),
        (SDL_AUDIO_ISFLOAT(audio_format) ? " (float)" : ""),
        (audio_channels > 2)   ? "surround"
        : (audio_channels > 1) ? "stereo"
                               : "mono");

    _music = Mix_LoadMUS(filePath.c_str());
    if (_music == nullptr)
    {
        fprintf(stderr, "Failed to open music from %s\n", filePath.c_str());
        fprintf(stderr, "%s\n", SDL_GetError());
        Mix_CloseAudio();
        return;
    }

    ROW_PER_S = (float)bpm / 60.f * (float)rpb;
}

void AudioStream::destroy()
{
    Mix_CloseAudio();
    if (_music != nullptr)
    {
        Mix_FreeMusic(_music);
        _music = nullptr;
    }
}

bool AudioStream::hasMusic() const { return _music != nullptr; }

Mix_Music *AudioStream::getMusic() const { return _music; }

void AudioStream::play()
{
    if (_music == nullptr)
        return;

    if (_shouldRestart || !Mix_PausedMusic())
        Mix_PlayMusic(_music, 0);
    else
        Mix_ResumeMusic();
    _shouldRestart = false;
}

bool AudioStream::isPlaying()
{
    if (_music == nullptr)
        return false;

    return Mix_PlayingMusic() == 1 && Mix_PausedMusic() != 1;
}

void AudioStream::pause()
{
    if (_music == nullptr)
        return;

    Mix_PauseMusic();
}

void AudioStream::stop()
{
    if (_music == nullptr)
        return;

    Mix_PauseMusic();
    _shouldRestart = true;
}

double AudioStream::getRow()
{
    if (_music == nullptr)
        return 0.0;

    double timeS = getTimeS();
    return timeS * ROW_PER_S;
}

double AudioStream::getTimeS()
{
    if (_music == nullptr)
        return 0.0;

    if (isPlaying())
    {
        double const mixTimeS = Mix_GetMusicPosition(_music);

        // Smooth over the coarse API timestamps for animation
        // Idea from SoLoud docs https://solhsa.com/soloud/coremisc.html
        auto const now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> const dt = now - _prevTimeStamp;
        // Fudge a polling-rate based history weight
        // This seems to work at various demo refresh rates
        double const alpha = 1.0 - std::min(10 * dt.count(), 1.0);
        _timeS = (_timeS * alpha + mixTimeS * (1 - alpha));

        _prevTimeStamp = now;
    }

    return _timeS;
}

void AudioStream::setTimeS(double timeS)
{
    if (_music == nullptr)
        return;

    _prevTimeStamp = std::chrono::high_resolution_clock::now();
    Mix_SetMusicPosition(timeS);
    _timeS = timeS;
}

void AudioStream::setRow(int32_t row)
{
    if (_music == nullptr)
        return;

    double const timeS = row / (double)ROW_PER_S;
    Mix_SetMusicPosition(timeS);
    _timeS = timeS;
}
