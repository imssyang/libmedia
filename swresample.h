#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
extern "C" {
#define __STDC_CONSTANT_MACROS
#include <libswresample/swresample.h>
}

class FFSWResample {
    using SwrContextPtr = std::unique_ptr<SwrContext, std::function<void(SwrContext*)>>;

public:
    FFSWResample(
        const AVChannelLayout& src_ch_layout, AVSampleFormat src_sample_fmt, int src_sample_rate,
        const AVChannelLayout& dst_ch_layout, AVSampleFormat dst_sample_fmt, int dst_sample_rate);
    bool Init();
    std::shared_ptr<AVFrame> Convert(std::shared_ptr<AVFrame> src_frame);

private:
    mutable std::recursive_mutex mutex_;
    int src_sample_rate_;
    int dst_sample_rate_;
    AVSampleFormat src_sample_fmt_;
    AVSampleFormat dst_sample_fmt_;
    AVChannelLayout src_ch_layout_;
    AVChannelLayout dst_ch_layout_;
    SwrContextPtr context_;
};
