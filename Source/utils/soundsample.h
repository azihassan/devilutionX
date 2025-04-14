#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include <Aulib/Stream.h>

#include "engine/sound_defs.hpp"
#include "utils/log.hpp"
#include "utils/stdcompat/shared_ptr_array.hpp"

#ifdef __DREAMCAST__
#include "libwav.h"
#include "sndwav.h"
#include <dc/sound/sfxmgr.h>
#include <dc/sound/sound.h>
#include <dc/sound/stream.h>
#include <kos/fs.h>
#endif

#ifndef __DREAMCAST__
namespace devilution {
class SoundSample final {
public:
	SoundSample() = default;
	SoundSample(SoundSample &&) noexcept = default;
	SoundSample &operator=(SoundSample &&) noexcept = default;

	[[nodiscard]] bool IsLoaded() const
	{
		return stream_ != nullptr;
	}

	void Release();
	bool IsPlaying();

	// Returns 0 on success.
	int SetChunkStream(std::string filePath, bool isMp3, bool logErrors = true);

	void SetFinishCallback(Aulib::Stream::Callback &&callback)
	{
		stream_->setFinishCallback(std::forward<Aulib::Stream::Callback>(callback));
	}

	/**
	 * @brief Sets the sample's WAV, FLAC, or Ogg/Vorbis data.
	 * @param fileData Buffer containing the data
	 * @param dwBytes Length of buffer
	 * @param isMp3 Whether the data is an MP3
	 * @return 0 on success, -1 otherwise
	 */
	int SetChunk(ArraySharedPtr<std::uint8_t> fileData, std::size_t dwBytes, bool isMp3);

	[[nodiscard]] bool IsStreaming() const
	{
		return file_data_ == nullptr;
	}

	int DuplicateFrom(const SoundSample &other)
	{
		if (other.IsStreaming())
			return SetChunkStream(other.file_path_, other.isMp3_);
		return SetChunk(other.file_data_, other.file_data_size_, other.isMp3_);
	}

	/**
	 * @brief Start playing the sound for a given number of iterations (0 means loop).
	 */
	bool Play(int numIterations = 1);

	/**
	 * @brief Start playing the sound with the given sound and user volume, and a stereo position.
	 */
	bool PlayWithVolumeAndPan(int logSoundVolume, int logUserVolume, int logPan)
	{
		SetVolume(logSoundVolume + logUserVolume * (ATTENUATION_MIN / VOLUME_MIN), ATTENUATION_MIN, 0);
		SetStereoPosition(logPan);
		return Play();
	}

	/**
	 * @brief Stop playing the sound
	 */
	void Stop()
	{
		stream_->stop();
	}

	void SetVolume(int logVolume, int logMin, int logMax);
	void SetStereoPosition(int logPan);

	void Mute()
	{
		stream_->mute();
	}

	void Unmute()
	{
		stream_->unmute();
	}

	/**
	 * @return Audio duration in ms
	 */
	int GetLength() const;

private:
	// Non-streaming audio fields:
	ArraySharedPtr<std::uint8_t> file_data_;
	std::size_t file_data_size_;

	// Set for streaming audio to allow for duplicating it:
	std::string file_path_;

	bool isMp3_;

	std::unique_ptr<Aulib::Stream> stream_;
};
} // namespace devilution

#else
namespace devilution {
class SoundSample final {
public:
	SoundSample() = default;
	SoundSample(SoundSample &&) noexcept = default;
	SoundSample &operator=(SoundSample &&) noexcept = default;

	[[nodiscard]] bool IsLoaded() const
	{
		// Log("SoundSample::IsLoaded({})", file_path_);
		return stream_ != SND_STREAM_INVALID || file_data_ != SFXHND_INVALID;
	}

	void Release();
	bool IsPlaying();

	// Returns 0 on success.
	int SetChunkStream(std::string filePath, bool isMp3, bool logErrors = true);

	void SetFinishCallback(Aulib::Stream::Callback &&callback)
	{
		Log("SoundSample::SetFinishCallback({})", file_path_);
		Log("SetFinishCallback not implemented yet");
	}

	/**
	 * @brief Sets the sample's WAV, FLAC, or Ogg/Vorbis data.
	 * @param fileData Buffer containing the data
	 * @param dwBytes Length of buffer
	 * @param isMp3 Whether the data is an MP3
	 * @return 0 on success, -1 otherwise
	 */
	int SetChunk(std::string filePath, std::size_t dwBytes, bool isMp3);

	[[nodiscard]] bool IsStreaming() const
	{
		// Log("SoundSample::IsStreaming({}) = {}", file_path_, stream_ != SND_STREAM_INVALID);
		return stream_ != SND_STREAM_INVALID;
	}

	int DuplicateFrom(const SoundSample &other)
	{
		// Log("SoundSample::DuplicateFrom({}, {})", file_path_, other.file_path_);
		if (other.IsStreaming())
			return SetChunkStream(other.file_path_, other.isMp3_);
		return SetChunk(other.file_path_, other.file_data_size_, other.isMp3_);
	}

	/**
	 * @brief Start playing the sound for a given number of iterations (0 means loop).
	 */
	bool Play(int numIterations = 1);

	/**
	 * @brief Start playing the sound with the given sound and user volume, and a stereo position.
	 */
	bool PlayWithVolumeAndPan(int logSoundVolume, int logUserVolume, int logPan)
	{
		// Log("SoundSample::PlayWithVolumeAndPan({}, {}, {}, {})", file_path_, logSoundVolume, logUserVolume, logPan);
		SetVolume(logSoundVolume + logUserVolume * (ATTENUATION_MIN / VOLUME_MIN), ATTENUATION_MIN, 0);
		SetStereoPosition(logPan);
		return Play();
	}

	/**
	 * @brief Stop playing the sound
	 */
	void Stop()
	{
		// Log("SoundSample::Stop({})", file_path_);
		if (IsStreaming()) {
			// Log("wav_stop({})", file_path_);
			wav_stop(stream_);
		} else {
			// Log("snd_sfx_stop({})", file_path_);
			snd_sfx_stop(channel_);
		}
	}

	void SetVolume(int logVolume, int logMin, int logMax);
	void SetStereoPosition(int logPan);

	void Mute()
	{
		// Log("SoundSample::Mute({})", file_path_);
		if (IsStreaming()) {
			previousVolume_ = volume_;
			volume_ = 0;
			wav_volume(stream_, volume_);
		} else {
			previousVolume_ = volume_;
			volume_ = 0;
		}
	}

	void Unmute()
	{
		// Log("SoundSample::Unmute({})", file_path_);
		if (IsStreaming()) {
			volume_ = previousVolume_;
			previousVolume_ = 0;
			wav_volume(stream_, volume_);
		} else {
			volume_ = previousVolume_;
			previousVolume_ = 0;
		}
	}

	/**
	 * @return Audio duration in ms
	 */
	int GetLength() const;

private:
	// Non-streaming audio fields:
	sfxhnd_t file_data_ = SFXHND_INVALID;
	int channel_ = -1;
	int volume_ = 0;
	int previousVolume_ = 0;
	int pan_ = 128;
	std::size_t file_data_size_;

	// Set for streaming audio to allow for duplicating it:
	std::string file_path_;

	bool isMp3_;

	wav_stream_hnd_t stream_ = SND_STREAM_INVALID;
};
} // namespace devilution
#endif
