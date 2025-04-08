#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include <Aulib/Stream.h>

#include "engine/sound_defs.hpp"
#include "utils/log.hpp"
#include "utils/stdcompat/shared_ptr_array.hpp"

#ifdef __DREAMCAST__
#include <dc/sound/sfxmgr.h>
#include <dc/sound/sound.h>
#include <dc/sound/stream.h>
#include <wav/sndwav.h>
#endif

namespace devilution {

class SoundSample final {
public:
	SoundSample() = default;
	SoundSample(SoundSample &&) noexcept = default;
	SoundSample &operator=(SoundSample &&) noexcept = default;

	[[nodiscard]] bool IsLoaded() const
	{
#ifdef __DREAMCAST__
		return stream_ != SND_STREAM_INVALID || file_data_ != SFXHND_INVALID;
#else
		return stream_ != nullptr;
#endif
	}

	void Release();
	bool IsPlaying();

	// Returns 0 on success.
	int SetChunkStream(std::string filePath, bool isMp3, bool logErrors = true);

	void SetFinishCallback(Aulib::Stream::Callback &&callback)
	{
#ifdef __DREAMCAST__
		Log("SetFinishCallback not implemented yet");
#else
		stream_->setFinishCallback(std::forward<Aulib::Stream::Callback>(callback));
#endif
	}

	/**
	 * @brief Sets the sample's WAV, FLAC, or Ogg/Vorbis data.
	 * @param fileData Buffer containing the data
	 * @param dwBytes Length of buffer
	 * @param isMp3 Whether the data is an MP3
	 * @return 0 on success, -1 otherwise
	 */
#ifdef __DREAMCAST__
	int SetChunk(std::string filePath, std::size_t dwBytes, bool isMp3);
#else
	int SetChunk(ArraySharedPtr<std::uint8_t> fileData, std::size_t dwBytes, bool isMp3);
#endif

	[[nodiscard]] bool IsStreaming() const
	{
		Log("IsStreaming {}: {}", file_path_, stream_ != SND_STREAM_INVALID);
#ifdef __DREAMCAST__
		return stream_ != SND_STREAM_INVALID;
#else
		return file_data_ == nullptr;
#endif
	}

	int DuplicateFrom(const SoundSample &other)
	{
		if (other.IsStreaming())
			return SetChunkStream(other.file_path_, other.isMp3_);
#ifdef __DREAMCAST__
		return SetChunk(other.file_path_, other.file_data_size_, other.isMp3_);
#else
		return SetChunk(other.file_data_, other.file_data_size_, other.isMp3_);
#endif
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
		if (IsStreaming()) {
#ifdef __DREAMCAST__
			Log("wav_stop({})", file_path_);
			wav_stop(stream_);
#else
			stream_->stop();
#endif
		} else {
			Log("snd_sfx_stop({})", file_path_);
			snd_sfx_stop(channel_);
		}
	}

	void SetVolume(int logVolume, int logMin, int logMax);
	void SetStereoPosition(int logPan);

	void Mute()
	{
		if (IsStreaming()) {
#ifdef __DREAMCAST__
			previousVolume_ = volume_;
			volume_ = 0;
			wav_volume(stream_, volume_);
#else
			stream_->mute();
#endif
		} else {
			previousVolume_ = volume_;
			volume_ = 0;
		}
	}

	void Unmute()
	{
		if (IsStreaming()) {
#ifdef __DREAMCAST__
			volume_ = previousVolume_;
			previousVolume_ = 0;
			wav_volume(stream_, volume_);
#else
			stream_->unmute();
#endif
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
#ifdef __DREAMCAST__
	sfxhnd_t file_data_ = SFXHND_INVALID;
	int channel_ = -1;
	int volume_ = 0;
	int previousVolume_ = 0;
	int pan_ = 128;
#else
	ArraySharedPtr<std::uint8_t> file_data_;
#endif
	std::size_t file_data_size_;

	// Set for streaming audio to allow for duplicating it:
	std::string file_path_;

	bool isMp3_;

#ifdef __DREAMCAST__
	wav_stream_hnd_t stream_ = SND_STREAM_INVALID;
#else
	std::unique_ptr<Aulib::Stream> stream_;
#endif
};

} // namespace devilution
