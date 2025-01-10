#include "SDL_movie_internal.h"

#include <webm/webm_parser.h>
#include <webm/callback.h>
#include <webm/istream_reader.h>

static constexpr int kWebmReaderError = 1;
static constexpr int kWebmReaderEof = 2;

class SDLWebmIoReader : public webm::Reader
{
public:
    SDLWebmIoReader(SDL_IOStream *io) : m_io(io)
    {
        m_position = 0;
        SDL_SeekIO(m_io, 0, SDL_IO_SEEK_SET);
    }

    void Seek(std::uint64_t position)
    {
        SDL_SeekIO(m_io, position, SDL_IO_SEEK_SET);
        m_position = position;
    }

    webm::Status Skip(std::uint64_t num_to_skip,
                      std::uint64_t *num_actually_skipped)
    {
        const auto oldPosition = m_position;
        SDL_SeekIO(m_io, num_to_skip, SDL_IO_SEEK_CUR);
        m_position += num_to_skip;
        const auto bytesSkipped = m_position - oldPosition;
        *num_actually_skipped = bytesSkipped;

        if (bytesSkipped == num_to_skip)
        {
            return webm::Status(webm::Status::kOkCompleted);
        }
        else if (bytesSkipped > 0)
        {
            return webm::Status(webm::Status::kOkPartial);
        }
        else
        {
            return webm::Status(kWebmReaderError);
        }
    }

    webm::Status Read(std::size_t num_to_read, std::uint8_t *buffer,
                      std::uint64_t *num_actually_read)
    {
        const auto bytesRead = SDL_ReadIO(m_io, buffer, num_to_read);

        *num_actually_read = bytesRead;
        m_position += bytesRead;

        if (bytesRead > 0 && bytesRead == num_to_read)
        {
            return webm::Status(webm::Status::kOkCompleted);
        }
        else if (bytesRead > 0 && bytesRead < num_to_read)
        {
            return webm::Status(webm::Status::kOkPartial);
        }
        else if (bytesRead == 0)
        {
            const auto status = SDL_GetIOStatus(m_io);

            if (status == SDL_IO_STATUS_EOF)
            {
                return webm::Status(kWebmReaderEof);
            }
            else
            {
                *num_actually_read = 0;
                return webm::Status(webm::Status::kInvalidElementSize);
            }
        }

        return webm::Status(webm::Status::kOkPartial);
    }

    std::uint64_t Position() const
    {
        return m_position;
    }

private:
    SDL_IOStream *m_io;
    std::uint64_t m_position;
};

class SDLMovieWebmCallback : public webm::Callback
{
public:
    SDLMovieWebmCallback(SDL_Movie *movie)
    {
        m_movie = movie;
    }

    webm::Status OnInfo(const webm::ElementMetadata &metadata, const webm::Info &info) override
    {
        m_movie->timecode_scale = info.timecode_scale.value();

        return webm::Status(webm::Status::kOkCompleted);
    }

    webm::Status OnSimpleBlockBegin(const webm::ElementMetadata &metadata,
                                    const webm::SimpleBlock &simple_block,
                                    webm::Action *action) override
    {
        if (!simple_block.is_visible)
        {
            *action = webm::Action::kSkip;
            return webm::Status(webm::Status::kOkCompleted);
        }

        m_currentBlockTrack = SDLMovie_FindTrackByNumber(m_movie, simple_block.track_number);
        m_currentBlockTimecode = simple_block.timecode;
        *action = m_currentBlockTrack >= 0 ? webm::Action::kRead : webm::Action::kSkip;
        return webm::Status(webm::Status::kOkCompleted);
    }

    webm::Status OnBlockBegin(const webm::ElementMetadata &metadata,
                              const webm::Block &block, webm::Action *action) override
    {
        if (!block.is_visible)
        {
            *action = webm::Action::kSkip;
            return webm::Status(webm::Status::kOkCompleted);
        }

        if (block.num_frames == 0)
        {
            *action = webm::Action::kSkip;
            return webm::Status(webm::Status::kOkCompleted);
        }

        m_currentBlockTrack = SDLMovie_FindTrackByNumber(m_movie, block.track_number);
        m_currentBlockTimecode = block.timecode;
        *action = m_currentBlockTrack >= 0 ? webm::Action::kRead : webm::Action::kSkip;
        return webm::Status(webm::Status::kOkCompleted);
    }

    webm::Status OnFrame(const webm::FrameMetadata &metadata, webm::Reader *reader,
                         std::uint64_t *bytes_remaining) override
    {
        if (m_currentBlockTrack != -1)
        {
            SDLMovie_AddCachedFrame(
                m_movie,
                m_currentBlockTrack, m_currentBlockTimecode, metadata.position, metadata.size);
        }

        return Skip(reader, bytes_remaining);
    }

    webm::Status OnTrackEntry(const webm::ElementMetadata &metadata,
                              const webm::TrackEntry &track_entry) override
    {
        if (!track_entry.is_enabled.value())
        {
            return webm::Status(webm::Status::kOkCompleted);
        }

        if (m_movie->ntracks >= MAX_SDL_MOVIE_TRACKS)
        {
            return webm::Status(webm::Status::kOkCompleted);
        }

        const auto &trackCodecId = track_entry.codec_id.value();
        const auto trackType = track_entry.track_type.value();

        if (trackType != webm::TrackType::kVideo && trackType != webm::TrackType::kAudio)
        {
            return webm::Status(webm::Status::kOkCompleted);
        }

        if (trackType == webm::TrackType::kVideo && trackCodecId != "V_VP8")
        {
            return webm::Status(webm::Status::kOkCompleted);
        }

        if (trackType == webm::TrackType::kAudio && trackCodecId != "A_VORBIS")
        {
            return webm::Status(webm::Status::kOkCompleted);
        }

        SDL_MovieTrack *mt = &m_movie->tracks[m_movie->ntracks];

        m_movie->ntracks++;

        if (track_entry.name.is_present())
        {
            SDL_strlcpy(mt->name, track_entry.name.value().c_str(), sizeof(mt->name));
        }
        else
        {
            SDL_strlcpy(mt->name, "Unknown", sizeof(mt->name));
        }

        SDL_strlcpy(mt->codec_id, trackCodecId.c_str(), sizeof(mt->codec_id));

        SDL_strlcpy(mt->language, track_entry.language.is_present() ? track_entry.language.value().c_str() : "eng", sizeof(mt->language));

        mt->track_number = track_entry.track_number.value();

        mt->type = trackType == webm::TrackType::kVideo ? SDL_MOVIE_TRACK_TYPE_VIDEO : SDL_MOVIE_TRACK_TYPE_AUDIO;

        mt->lacing = track_entry.uses_lacing.value();

        if (mt->type == SDL_MOVIE_TRACK_TYPE_VIDEO)
        {
            const auto &video = track_entry.video.value();
            mt->video_width = video.pixel_width.value();
            mt->video_height = video.pixel_height.value();
            mt->video_frame_rate = video.frame_rate.value();
        }
        else if (mt->type == SDL_MOVIE_TRACK_TYPE_AUDIO)
        {
            const auto &audio = track_entry.audio.value();
            mt->audio_sample_frequency = audio.sampling_frequency.value();
            mt->audio_output_frequency = audio.output_frequency.value();
            mt->audio_channels = audio.channels.value();
            mt->audio_bit_depth = audio.bit_depth.value();
        }

        if (track_entry.codec_private.is_present())
        {
            const auto &codecPrivate = track_entry.codec_private.value();
            const auto codecPrivateData = codecPrivate.data();
            const auto codecPrivateSize = codecPrivate.size();
            if (codecPrivateSize > 0)
            {
                mt->codec_private_data = (Uint8 *)SDL_malloc(codecPrivateSize);
                SDL_memcpy(mt->codec_private_data, codecPrivateData, codecPrivateSize);
                mt->codec_private_size = codecPrivateSize;
            }
        }

        return webm::Status(webm::Status::kOkCompleted);
    }

private:
    SDL_Movie *m_movie;

    int m_currentBlockTrack;
    Uint64 m_currentBlockTimecode;
};

extern "C"
{
    bool SDLMovie_Parse_WebM(SDL_Movie *movie)
    {
        SDLWebmIoReader reader(movie->io);
        SDLMovieWebmCallback callback(movie);

        webm::WebmParser parser;

        auto result = parser.Feed(&callback, &reader);

        if (!result.completed_ok() && result.code != kWebmReaderEof)
        {
            SDLMovie_SetError("Failed to parse webm file, result code: %d", result.code);
            return false;
        }

        return true;
    }
}