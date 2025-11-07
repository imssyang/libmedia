#include "ffmpeg.h"
#include "avmanage.h"

uint32_t CreateMedia() {
    auto& manager = MediaManager::GetInstance();
    auto [media_id, media_obj] = manager.CreateMedia();
    if (!media_obj)
        return 0;

    return media_id;
}

bool DeleteMedia(uint32_t media_id) {
    auto& manager = MediaManager::GetInstance();
    return manager.DeleteMedia(media_id);
}

bool AddDemuxer(uint32_t media_id, const char* uri) {
    auto media = MediaManager::GetInstance().GetMedia(media_id);
    if (!media)
        return false;

    return bool(media->AddDemuxer(uri));
}

bool AddMuxer(uint32_t media_id, const char* uri, const char* mux_fmt) {
    auto media = MediaManager::GetInstance().GetMedia(media_id);
    if (!media)
        return false;

    return bool(media->AddMuxer(uri, mux_fmt));
}

bool DeleteFormat(uint32_t media_id, const char* uri) {
    auto media = MediaManager::GetInstance().GetMedia(media_id);
    if (!media)
        return false;
    return media->DeleteFormat(uri);
}

const AVFormatContext* GetFormatContext(uint32_t media_id, const char* uri) {
    auto media = MediaManager::GetInstance().GetMedia(media_id);
    if (!media)
        return nullptr;

    auto demuxer = media->GetDemuxer(uri);
    if (demuxer)
        return demuxer->GetContext().get();

    auto muxer = media->GetMuxer(uri);
    if (muxer)
        return muxer->GetContext().get();

    return nullptr;
}

const AVStream* GetStream(uint32_t media_id, const char* uri, int32_t stream_index) {
    auto formatCtx = GetFormatContext(media_id, uri);
    if (!formatCtx)
        return nullptr;

    if (stream_index < 0 || stream_index >= (int32_t)formatCtx->nb_streams)
        return nullptr;

    return formatCtx->streams[stream_index];
}

const AVCodecParameters* GetCodecParameters(uint32_t media_id, const char* uri, int32_t stream_index) {
    auto stream = GetStream(media_id, uri, stream_index);
    if (!stream)
        return nullptr;

    return stream->codecpar;
}

const AVPacket* ReadPacket(uint32_t media_id, const char* uri) {
    auto media = MediaManager::GetInstance().GetMedia(media_id);
    if (!media)
        return nullptr;

    auto demuxer = media->GetDemuxer(uri);
    if (!demuxer)
        return nullptr;

    auto packet = demuxer->ReadPacket();
    if (!packet)
        return nullptr;

    return av_packet_clone(packet.get());
}

void FreePacket(AVPacket* packet) {
    if (packet) {
        av_packet_unref(packet);
        av_packet_free(&packet);
    }
}

bool SetPlaySpeed(uint32_t media_id, const char* uri, double speed) {
    auto media = MediaManager::GetInstance().GetMedia(media_id);
    if (!media)
        return false;

    auto demuxer = media->GetDemuxer(uri);
    if (demuxer) {
        demuxer->SetPlaySpeed(speed);
        return true;
    }

    auto muxer = media->GetMuxer(uri);
    if (muxer) {
        muxer->SetPlaySpeed(speed);
        return true;
    }

    return false;
}

const char* GetPacketSideDataTypeStr(const AVPacketSideData* side_data) {
    if (!side_data)
        return nullptr;
    return av_packet_side_data_name(side_data->type);
}

const char* GetFieldOrderStr(enum AVFieldOrder order) {
    switch (order) {
        case AV_FIELD_UNKNOWN: return "Unknown";
        case AV_FIELD_PROGRESSIVE: return "Progressive";
        case AV_FIELD_TT: return "TT";
        case AV_FIELD_BB: return "BB";
        case AV_FIELD_TB: return "TB";
        case AV_FIELD_BT: return "BT";
        default: return "Invalid";
    }
}

const char* AllocChannelLayoutStr(const AVChannelLayout* ch_layout) {
    char *buf = (char*)malloc(AV_ERROR_MAX_STRING_SIZE * sizeof(char));
    av_channel_layout_describe(ch_layout, buf, AV_ERROR_MAX_STRING_SIZE);
    return buf;
}
