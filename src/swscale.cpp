#include "swscale.h"

FFSWScale::FFSWScale(
    int src_width, int src_height, AVPixelFormat src_pix_fmt,
    int dst_width, int dst_height, AVPixelFormat dst_pix_fmt,
    int flags
) : src_width_(src_width), src_height_(src_height)
  , dst_width_(dst_width), dst_height_(dst_height)
  , src_pix_fmt_(src_pix_fmt), dst_pix_fmt_(dst_pix_fmt)
  , flags_(flags) {
}

bool FFSWScale::SetSrcFilter(
    float luma_gblur, float chroma_gblur, float luma_sharpen,
    float chroma_sharpen, float chroma_hshift, float chroma_vshift,
    int verbose
) {
    if (context_) return false;

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    SwsFilter* filter = sws_getDefaultFilter(
        luma_gblur, chroma_gblur, luma_sharpen,
        chroma_sharpen,  chroma_hshift,  chroma_vshift, verbose);
    if (!filter)
        return false;

    src_filter_ = SwsFilterPtr(filter, [](SwsFilter *fp) {
        sws_freeFilter(fp);
    });
    return true;
}

bool FFSWScale::SetDstFilter(
    float luma_gblur, float chroma_gblur, float luma_sharpen,
    float chroma_sharpen, float chroma_hshift, float chroma_vshift,
    int verbose
) {
    if (context_) return false;

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    SwsFilter* filter = sws_getDefaultFilter(
        luma_gblur, chroma_gblur, luma_sharpen,
        chroma_sharpen,  chroma_hshift,  chroma_vshift, verbose);
    if (!filter)
        return false;

    dst_filter_ = SwsFilterPtr(filter, [](SwsFilter *fp) {
        sws_freeFilter(fp);
    });
    return true;
}

bool FFSWScale::SetParams(const std::vector<double>& params) {
    if (context_) return false;

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    params_ = params;
    return true;
}

bool FFSWScale::Init() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    SwsFilter *src_filter = src_filter_ ? src_filter_.get() : nullptr;
    SwsFilter *dst_filter = dst_filter_ ? dst_filter_.get() : nullptr;
    const double *params = params_.empty() ? nullptr : params_.data();
    struct SwsContext* context = sws_getContext(
        src_width_, src_height_, src_pix_fmt_,
        dst_width_, dst_height_, dst_pix_fmt_,
        flags_, src_filter, dst_filter, params
    );
    if (!context)
        return false;

    context_ = SwsContextPtr(context, [](SwsContext *ctx) {
        sws_freeContext(ctx);
    });
    return true;
}

std::shared_ptr<AVFrame> FFSWScale::Scale(
    std::shared_ptr<AVFrame> src_frame,
    int src_index_y, int src_height, int dst_align
) {
    if (!context_) return nullptr;

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    AVFrame *dst_frame = av_frame_alloc();
    if (!dst_frame)
        return nullptr;

    int ret = av_image_alloc(
        dst_frame->data, dst_frame->linesize,
        dst_width_, dst_height_, dst_pix_fmt_, dst_align);
    if (ret < 0) {
        av_frame_free(&dst_frame);
        return nullptr;
    }

    ret = sws_scale(context_.get(),
        src_frame->data, src_frame->linesize, src_index_y, src_height,
        dst_frame->data, dst_frame->linesize);
    if (ret < 0) {
        av_freep(&dst_frame->data[0]);
        av_frame_free(&dst_frame);
        return nullptr;
    }

    std::shared_ptr<AVFrame> dst_frame_ptr(dst_frame, [](AVFrame *p) {
        av_freep(&p->data[0]);
        av_frame_free(&p);
    });
    return dst_frame_ptr;
}
