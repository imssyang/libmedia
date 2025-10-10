#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include "avutil.h"
extern "C" {
#define __STDC_CONSTANT_MACROS
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class FFSWScale {
    using SwsContextPtr = std::unique_ptr<SwsContext, std::function<void(SwsContext*)>>;
    using SwsFilterPtr = std::unique_ptr<SwsFilter, std::function<void(SwsFilter*)>>;

public:
    FFSWScale(
        int src_width, int src_height, AVPixelFormat src_pix_fmt,
        int dst_width, int dst_height, AVPixelFormat dst_pix_fmt,
        int flags);
    bool SetSrcFilter(
        float luma_gblur, float chroma_gblur, float luma_sharpen,
        float chroma_sharpen, float chroma_hshift, float chroma_vshift,
        int verbose);
    bool SetDstFilter(
        float luma_gblur, float chroma_gblur, float luma_sharpen,
        float chroma_sharpen, float chroma_hshift, float chroma_vshift,
        int verbose);
    bool SetParams(const std::vector<double>& params);
    bool Init();
    std::shared_ptr<AVFrame> Scale(
        std::shared_ptr<AVFrame> src_frame,
        int src_index_y, int src_height, int dst_align);

private:
    mutable std::recursive_mutex mutex_;
    int src_width_;
    int src_height_;
    int dst_width_;
    int dst_height_;
    AVPixelFormat src_pix_fmt_;
    AVPixelFormat dst_pix_fmt_;
    SwsFilterPtr src_filter_;
    SwsFilterPtr dst_filter_;
    int flags_;
    std::vector<double> params_;
    SwsContextPtr context_;
};
