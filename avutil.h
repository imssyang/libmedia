#pragma once

#include <iostream>
#include <string>

extern "C" {
#define __STDC_CONSTANT_MACROS
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libavutil/time.h>
}

std::string AVErrorStr(int errnum);
std::string AVChannelLayoutStr(const AVChannelLayout* ch_layout);
std::string DumpAVFrame(const AVFrame* frame, int stream_index = -1, bool detailed = false);
