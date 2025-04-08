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

void SoundSample::Release()
{
#ifdef __DREAMCAST__
	if (stream_ != SND_STREAM_INVALID) {
		wav_destroy(stream_);
		stream_ = SND_STREAM_INVALID;
	}
	if (file_data_ != SFXHND_INVALID) {
		snd_sfx_unload(file_data_);
		file_data_ = SFXHND_INVALID;
		channel_ = -1;
	}
	volume_ = 0;
	previousVolume_ = 0;
#else
	stream_ = nullptr;
	file_data_ = nullptr;
#endif
	file_data_size_ = 0;
}

/**
 * @brief Check if a the sound is being played atm
 */
bool SoundSample::IsPlaying()
{
	if (IsStreaming()) {
#ifdef __DREAMCAST__
		Log("wav_is_playing({}) = {}", file_path_, wav_is_playing(stream_));
		return wav_is_playing(stream_);
#else
		return stream_->isPlaying();
#endif
	} else {
		return snd_is_playing(channel_);
	}
}

bool SoundSample::Play(int numIterations)
{
	if (IsStreaming()) {
#ifdef __DREAMCAST__
		Log("Streaming {} with audio 255", file_path_);
		wav_volume(stream_, 255);
		wav_play(stream_);
		wav_volume(stream_, 255);
		return true;
#else
		if (!stream_->play(numIterations)) {
			LogError(LogCategory::Audio, "Aulib::Stream::play (from SoundSample::Play): {}", SDL_GetError());
			return false;
		}
		return true;
#endif
	}
	volume_ = 255;
	pan_ = 128;
	Log("snd_sfx_play({}, {}, {})", file_path_, volume_, pan_);
	int channel = snd_sfx_play(file_data_, volume_, pan_);
	if (channel == -1) {
		LogError(LogCategory::Audio, "Aulib::Stream::play (from SoundSample::Play): {}", SDL_GetError());
		return false;
	}
	channel_ = channel;
	return true;
}

int SoundSample::SetChunkStream(std::string filePath, bool isMp3, bool logErrors)
{
#ifdef __DREAMCAST__
	stream_ = wav_create(filePath.c_str(), 1);
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
#else
	SDL_RWops *handle = OpenAssetAsSdlRwOps(filePath.c_str(), /*threadsafe=*/true);
	if (handle == nullptr) {
		if (logErrors)
			LogError(LogCategory::Audio, "OpenAsset failed (from SoundSample::SetChunkStream) for {}: {}", filePath, SDL_GetError());
		return -1;
	}
	file_path_ = filePath;
	isMp3_ = isMp3;
	stream_ = CreateStream(handle, isMp3);
	if (!stream_->open()) {
		stream_ = nullptr;
		if (logErrors)
			LogError(LogCategory::Audio, "Aulib::Stream::open (from SoundSample::SetChunkStream) for {}: {}", file_path_, SDL_GetError());
		return -1;
	}
	return 0;
#endif
}

#ifdef __DREAMCAST__
int SoundSample::SetChunk(std::string filePath, std::size_t dwBytes, bool isMp3)
{
	isMp3_ = isMp3;
	file_path_ = filePath;
	Log("snd_sfx_load({})", filePath);
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
#else
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
#endif

void SoundSample::SetVolume(int logVolume, int logMin, int logMax)
{
	if (IsStreaming()) {
#ifdef __DREAMCAST__
		previousVolume_ = volume_;
		volume_ = VolumeLogToLinear(logVolume, logMin, logMax);
		wav_volume(stream_, 255);
#else
		stream_->setVolume(VolumeLogToLinear(logVolume, logMin, logMax));
#endif
	} else {
		previousVolume_ = volume_;
		volume_ = VolumeLogToLinear(logVolume, logMin, logMax);
	}
}

void SoundSample::SetStereoPosition(int logPan)
{
	if (IsStreaming()) {
#ifdef __DREAMCAST__
		Log("pan is not supported in libwav on the Dreamcast");
		snd_stream_pan(stream_, 128, 128);
#else
		stream_->setStereoPosition(PanLogToLinear(logPan));
#endif
	} else {
		pan_ = PanLogToLinear(logPan);
	}
}

int SoundSample::GetLength() const
{
#ifdef __DREAMCAST__
	if (IsStreaming())
		return 10000;
#else
	if (IsStreaming())
		return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(stream_->duration()).count());
#endif
	return 1000;
}

} // namespace devilution
