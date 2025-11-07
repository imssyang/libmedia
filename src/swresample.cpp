#include "swresample.h"

FFSWResample::FFSWResample(
    const AVChannelLayout& src_ch_layout, AVSampleFormat src_sample_fmt, int src_sample_rate,
    const AVChannelLayout& dst_ch_layout, AVSampleFormat dst_sample_fmt, int dst_sample_rate
) : src_sample_rate_(src_sample_rate), dst_sample_rate_(dst_sample_rate)
  , src_sample_fmt_(src_sample_fmt), dst_sample_fmt_(dst_sample_fmt)
  , src_ch_layout_(src_ch_layout), dst_ch_layout_(dst_ch_layout) {
}

bool FFSWResample::Init() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    SwrContext *context = nullptr;
    int ret = swr_alloc_set_opts2(&context,
        &dst_ch_layout_, dst_sample_fmt_, dst_sample_rate_,
        &src_ch_layout_, src_sample_fmt_, src_sample_rate_,
        0, nullptr);
    if (ret < 0)
        return false;

    if (!context) {
        swr_free(&context);
        return false;
    }

    if (swr_init(context) < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "Error initializing SwrContext: %s\n", errbuf);
        swr_free(&context);
        return false;
    }

    context_ = SwrContextPtr(context, [](SwrContext *ctx) {
        swr_free(&ctx);
    });
    return true;
}

std::shared_ptr<AVFrame> FFSWResample::Convert(std::shared_ptr<AVFrame> src_frame) {
    if (!context_) return nullptr;

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (src_sample_rate_ != src_frame->sample_rate
        || src_sample_fmt_ != (AVSampleFormat)src_frame->format
        || src_ch_layout_.order != src_frame->ch_layout.order
        || src_ch_layout_.nb_channels != src_frame->ch_layout.nb_channels)
        return nullptr;

    AVFrame *dst_frame = av_frame_alloc();
    if (!dst_frame)
        return nullptr;

    int src_nb_samples_delay = swr_get_delay(context_.get(), src_frame->sample_rate);
    int dst_nb_samples = av_rescale_rnd(
        src_frame->nb_samples + src_nb_samples_delay,
        dst_sample_rate_,
        src_frame->sample_rate,
        AV_ROUND_UP
    );

    av_frame_make_writable(dst_frame);
    dst_frame->nb_samples = dst_nb_samples;
    dst_frame->ch_layout = dst_ch_layout_;
    dst_frame->format = dst_sample_fmt_;
    dst_frame->sample_rate = dst_sample_rate_;

    int ret = swr_convert(
        context_.get(),
        dst_frame->data,
        dst_frame->nb_samples,
        (const uint8_t**)src_frame->data,
        src_frame->nb_samples
    );
    if (ret < 0) {
        av_frame_free(&dst_frame);
        return nullptr;
    }

    std::shared_ptr<AVFrame> dst_frame_ptr(dst_frame, [](AVFrame *p) {
        av_frame_free(&p);
    });
    return dst_frame_ptr;
}
