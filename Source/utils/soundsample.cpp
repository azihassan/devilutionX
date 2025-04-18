#include "utils/soundsample.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <utility>

#include <Aulib/DecoderDrmp3.h>
#include <Aulib/DecoderDrwav.h>
#include <SDL.h>
#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#else
#include "utils/sdl2_backports.h"
#endif

#include "engine/assets.hpp"
#include "options.h"
#include "utils/aulib.hpp"
#include "utils/log.hpp"
#include "utils/math.h"
#include "utils/stubs.h"

namespace devilution {

namespace {

constexpr float LogBase = 10.0;

/**
 * Scaling factor for attenuating volume.
 * Picked so that a volume change of -10 dB results in half perceived loudness.
 * VolumeScale = -1000 / log(0.5)
 */
constexpr float VolumeScale = 3321.9281F;

/**
 * Min and max volume range, in millibel.
 * -100 dB (muted) to 0 dB (max. loudness).
 */
constexpr float MillibelMin = -10000.F;
constexpr float MillibelMax = 0.F;

/**
 * Stereo separation factor for left/right speaker panning. Lower values increase separation, moving
 * sounds further left/right, while higher values will pull sounds more towards the middle, reducing separation.
 * Current value is tuned to have ~2:1 mix for sounds that happen on the edge of a 640x480 screen.
 */
constexpr float StereoSeparation = 6000.F;

float PanLogToLinear(int logPan)
{
	if (logPan == 0)
		return 0;

	auto factor = std::pow(LogBase, static_cast<float>(-std::abs(logPan)) / StereoSeparation);

	return copysign(1.F - factor, static_cast<float>(logPan));
}

std::unique_ptr<Aulib::Decoder> CreateDecoder(bool isMp3)
{
	if (isMp3)
		return std::make_unique<Aulib::DecoderDrmp3>();
	return std::make_unique<Aulib::DecoderDrwav>();
}

std::unique_ptr<Aulib::Stream> CreateStream(SDL_RWops *handle, bool isMp3)
{
	auto decoder = CreateDecoder(isMp3);
	if (!decoder->open(handle)) // open for `getRate`
		return nullptr;
	auto resampler = CreateAulibResampler(decoder->getRate());
	return std::make_unique<Aulib::Stream>(handle, std::move(decoder), std::move(resampler), /*closeRw=*/true);
}

/**
 * @brief Converts log volume passed in into linear volume.
 * @param logVolume Logarithmic volume in the range [logMin..logMax]
 * @param logMin Volume range minimum (usually ATTENUATION_MIN for game sounds and VOLUME_MIN for volume sliders)
 * @param logMax Volume range maximum (usually 0)
 * @return Linear volume in the range [0..1]
 */
float VolumeLogToLinear(int logVolume, int logMin, int logMax)
{
	const auto logScaled = math::Remap(static_cast<float>(logMin), static_cast<float>(logMax), MillibelMin, MillibelMax, static_cast<float>(logVolume));
	return std::pow(LogBase, logScaled / VolumeScale); // linVolume
}

} // namespace

///// SoundSample /////

#ifndef __DREAMCAST__
void SoundSample::Release()
{
	stream_ = nullptr;
	file_data_ = nullptr;
	file_data_size_ = 0;
}

/**
 * @brief Check if a the sound is being played atm
 */
bool SoundSample::IsPlaying()
{
	return stream_ && stream_->isPlaying();
}

bool SoundSample::Play(int numIterations)
{
	if (!stream_->play(numIterations)) {
		LogError(LogCategory::Audio, "Aulib::Stream::play (from SoundSample::Play): {}", SDL_GetError());
		return false;
	}
	return true;
}

int SoundSample::SetChunkStream(std::string filePath, bool isMp3, bool logErrors)
{
	SDL_RWops *handle = OpenAssetAsSdlRwOps(filePath.c_str(), /*threadsafe=*/true);
	if (handle == nullptr) {
		if (logErrors)
			LogError(LogCategory::Audio, "OpenAsset failed (from SoundSample::SetChunkStream) for {}: {}", filePath, SDL_GetError());
		return -1;
	}
	file_path_ = std::move(filePath);
	isMp3_ = isMp3;
	stream_ = CreateStream(handle, isMp3);
	if (!stream_->open()) {
		stream_ = nullptr;
		if (logErrors)
			LogError(LogCategory::Audio, "Aulib::Stream::open (from SoundSample::SetChunkStream) for {}: {}", file_path_, SDL_GetError());
		return -1;
	}
	return 0;
}

int SoundSample::SetChunk(ArraySharedPtr<std::uint8_t> fileData, std::size_t dwBytes, bool isMp3)
{
	isMp3_ = isMp3;
	file_data_ = std::move(fileData);
	file_data_size_ = dwBytes;
	SDL_RWops *buf = SDL_RWFromConstMem(file_data_.get(), static_cast<int>(dwBytes));
	if (buf == nullptr) {
		return -1;
	}

	stream_ = CreateStream(buf, isMp3_);
	if (!stream_->open()) {
		stream_ = nullptr;
		file_data_ = nullptr;
		LogError(LogCategory::Audio, "Aulib::Stream::open (from SoundSample::SetChunk): {}", SDL_GetError());
		return -1;
	}

	return 0;
}

void SoundSample::SetVolume(int logVolume, int logMin, int logMax)
{
	stream_->setVolume(VolumeLogToLinear(logVolume, logMin, logMax));
}

void SoundSample::SetStereoPosition(int logPan)
{
	stream_->setStereoPosition(PanLogToLinear(logPan));
}

int SoundSample::GetLength() const
{
	if (!stream_)
		return 0;
	return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(stream_->duration()).count());
}

#else

void SoundSample::Release()
{
        // Log("SoundSample::Release({})", file_path_);
        if(stream_ != SND_STREAM_INVALID) {
            wav_destroy(stream_);
            stream_ = SND_STREAM_INVALID;
        }
        if(file_data_ != SFXHND_INVALID) {
            snd_sfx_unload(file_data_);
            file_data_ = SFXHND_INVALID;
            channel_ = -1;
        }
        volume_ = 0;
        previousVolume_ = 0;
	file_data_size_ = 0;
}

/**
 * @brief Check if a the sound is being played atm
 */
bool SoundSample::IsPlaying()
{
	// Log("SoundSample::IsPlaying({})", file_path_);
	if(IsStreaming()) {
		// Log("wav_is_playing({}) = {}", file_path_, wav_is_playing(stream_));
		return wav_is_playing(stream_);
	}
	//todo figure out why snd_is_playing(channel_) always returns true after sound effect is played the first time
	// Log("snd_is_playing({}, {}) = {}", channel_, file_data_, file_data_ != SFXHND_INVALID && channel_ != -1 && snd_is_playing(channel_));
	return file_data_ != SFXHND_INVALID && channel_ != -1 && snd_is_playing(channel_);
}

bool SoundSample::Play(int numIterations)
{
    // Log("SoundSample::Play({}, {})", file_path_, numIterations);
    if(IsStreaming()) {
        // Log("Streaming {} with audio 255", file_path_);
        wav_volume(stream_, 255);
        wav_play(stream_);
        return true;
    }
    volume_ = 255;
    // Log("snd_sfx_play({}, {}, {})", file_path_, volume_, pan_);
    channel_ = snd_sfx_play(file_data_, volume_, pan_);
    if(channel_ == -1) {
		LogError(LogCategory::Audio, "Aulib::Stream::play (from SoundSample::Play): {}", SDL_GetError());
                return false;
    }
    return true;
}

int SoundSample::SetChunkStream(std::string filePath, bool isMp3, bool logErrors)
{
        // Log("SoundSample::SetChunkStream({}, {}, {})", filePath, isMp3, logErrors);

	stream_ = wav_create(filePath.c_str(), filePath.find("music") != std::string::npos);
        // Log("stream_ = {} for {}", stream_, filePath);
	if (stream_ == SND_STREAM_INVALID) {
		if (logErrors)
			LogError(LogCategory::Audio, "wav_create failed (from SoundSample::SetChunkStream) for {}", filePath);
		return -1;
	}
	file_path_ = filePath;
	isMp3_ = isMp3;
        file_data_ = SFXHND_INVALID;
        volume_ = 255;
        pan_ = 128;
        wav_volume(stream_, volume_);
	snd_stream_pan(stream_, 128, 128);
        return 0;
}

int SoundSample::SetChunk(std::string filePath, std::size_t dwBytes, bool isMp3)
{
        // Log("SoundSample::SetChunk({}, {}, {})", filePath, dwBytes, isMp3_);
	isMp3_ = isMp3;
	file_path_ = filePath;
	// Log("snd_sfx_load({})", filePath);
	file_data_ = snd_sfx_load(filePath.c_str());
	file_data_size_ = dwBytes;
	if (file_data_ == SFXHND_INVALID) {
		LogError(LogCategory::Audio, "snd_sfx_load returned -1 for {}", filePath);
		return -1;
	}

        volume_ = 255;
        pan_ = 128;
        stream_ = SND_STREAM_INVALID;
	return 0;
}

void SoundSample::SetVolume(int logVolume, int logMin, int logMax)
{
        // Log("SoundSample::SetVolume({}, {}, {}, {}) (vol = {})", file_path_, logVolume, logMin, logMax, VolumeLogToLinear(logVolume, logMin, logMax));
    if(IsStreaming()) {
        previousVolume_ = volume_;
        volume_ = VolumeLogToLinear(logVolume, logMin, logMax);
        wav_volume(stream_, 255);
    } else {
        previousVolume_ = volume_;
        volume_ = VolumeLogToLinear(logVolume, logMin, logMax);
    }
}

void SoundSample::SetStereoPosition(int logPan)
{
        // Log("SoundSample::SetStereoPosition({}, {}) (stereo = {})", file_path_, logPan, PanLogToLinear(logPan));
    if(IsStreaming()) {
	//Log("pan is not supported in libwav on the Dreamcast");
	snd_stream_pan(stream_, 128, 128);
    } else {
        pan_ = PanLogToLinear(logPan);
    }
}

int SoundSample::GetLength() const
{
        //Log("SoundSample::GetLength({})", file_path_);
	if (IsStreaming())
		return 10000;
	return 1000;
}
#endif
} // namespace devilution
