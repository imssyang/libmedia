#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define __STDC_CONSTANT_MACROS
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>

uint32_t CreateMedia();
bool DeleteMedia(uint32_t media_id);
bool AddDemuxer(uint32_t media_id, const char* uri);
bool AddMuxer(uint32_t media_id, const char* uri, const char* mux_fmt);
bool DeleteFormat(uint32_t media_id, const char* uri);
const AVFormatContext* GetFormatContext(uint32_t media_id, const char* uri);
const AVStream* GetStream(uint32_t media_id, const char* uri, int32_t stream_index);
const AVCodecParameters* GetCodecParameters(uint32_t media_id, const char* uri, int32_t stream_index);
const AVPacket* ReadPacket(uint32_t media_id, const char* uri);
void FreePacket(AVPacket* packet);
bool SetPlaySpeed(uint32_t media_id, const char* uri, double speed);

const char* GetPacketSideDataTypeStr(const AVPacketSideData* side_data);
const char* GetFieldOrderStr(enum AVFieldOrder order);
const char* AllocChannelLayoutStr(const AVChannelLayout* ch_layout);

#ifdef __cplusplus
}
#endif
