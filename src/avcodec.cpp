#include <iomanip>
#include <sstream>
#include "avcodec.h"

bool FFAVCodec::initialize(const AVCodec *codec) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    AVCodecContext *context = avcodec_alloc_context3(codec);
    if (!context)
        return false;

    codec_ = std::shared_ptr<const AVCodec>(codec, [](auto){});
    context_ = std::shared_ptr<AVCodecContext>(context, [](AVCodecContext* ctx) {
        avcodec_free_context(&ctx);
    });
    return true;
}

std::shared_ptr<AVPacket> FFAVCodec::transformPacket(std::shared_ptr<AVPacket> packet) {
    if (av_codec_is_encoder(codec_.get()))
        packet->time_base = context_->time_base;
    return packet;
}

std::shared_ptr<AVFrame> FFAVCodec::transformFrame(std::shared_ptr<AVFrame> frame) {
    frame_count_++;
    if (av_codec_is_decoder(codec_.get())) {
        if (frame->pts == AV_NOPTS_VALUE)
            frame->pts = frame->best_effort_timestamp;
        if (frame->time_base.num == 0 || frame->time_base.den == 0)
            frame->time_base = context_->pkt_timebase;
    } else if (av_codec_is_encoder(codec_.get())) {
        auto newframe = std::shared_ptr<AVFrame>(
            av_frame_clone(frame.get()),
            [&](AVFrame *p) {
                av_frame_unref(p);
                av_frame_free(&p);
            }
        );
        if (av_cmp_q(frame->time_base, context_->time_base) != 0) {
            newframe->time_base = context_->time_base;
            newframe->pts = av_rescale_q_rnd(frame->pts, frame->time_base, context_->time_base,
                static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            newframe->pkt_dts = av_rescale_q_rnd(frame->pkt_dts, frame->time_base, context_->time_base,
                static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            newframe->duration = av_rescale_q(frame->duration, frame->time_base, context_->time_base);
        }
        frame = newframe;
    }

    if (debug_.load()) {
        std::cout << "[";
        if (av_codec_is_decoder(codec_.get()))
            std::cout << "D";
        else if (av_codec_is_encoder(codec_.get()))
            std::cout << "E";
        else
            std::cout << "-";
        std::cout << ":" << avcodec_get_name(codec_->id)
            << ":" << frame_count_.load()
            << "]" << DumpAVFrame(frame.get())
            << std::endl;
    }
    if (swscale_)
        frame = swscale_->Scale(frame, 0, context_->height, 32);
    return frame;
}

std::shared_ptr<const AVCodec> FFAVCodec::GetCodec() const {
    return codec_;
}

std::shared_ptr<AVCodecContext> FFAVCodec::GetContext() const {
    return context_;
}

std::shared_ptr<AVCodecParameters> FFAVCodec::GetParameters() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    AVCodecParameters *params = avcodec_parameters_alloc();
    int ret = avcodec_parameters_from_context(params, context_.get());
    if (ret < 0) {
        avcodec_parameters_free(&params);
        return nullptr;
    }
    return std::shared_ptr<AVCodecParameters>(params, [](AVCodecParameters *p) {
        avcodec_parameters_free(&p);
    });
}

std::shared_ptr<FFSWScale> FFAVCodec::GetSWScale() const {
    return swscale_;
}

int64_t FFAVCodec::GetFrameCount() const {
    return frame_count_.load();
}

void FFAVCodec::SetDebug(bool debug) {
    debug_.store(debug);
}

bool FFAVCodec::SetSWScale(int dst_width, int dst_height, AVPixelFormat dst_pix_fmt, int flags) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto params = GetParameters();
    auto swscale = std::make_shared<FFSWScale>(
        params->width, params->height, (AVPixelFormat)params->format,
        dst_width, dst_height, dst_pix_fmt, flags
    );
    if (!swscale->Init())
        return false;

    swscale_ = swscale;
    return true;
}

bool FFAVCodec::Open() {
    if (opened_.load())
        return true;

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    int ret = avcodec_open2(context_.get(), codec_.get(), nullptr);
    if (ret < 0) {
        return false;
    }

    opened_.store(true);
    return true;
}

bool FFAVCodec::PacketEOF() const {
    return packet_eof_.load() && packets_.empty();
}

bool FFAVCodec::FrameEOF() const {
    return frame_eof_.load() && frames_.empty();
}

std::shared_ptr<FFAVDecoder> FFAVDecoder::Create(AVCodecID id) {
    auto instance = std::shared_ptr<FFAVDecoder>(new FFAVDecoder());
    if (!instance->initialize(id))
        return nullptr;
    return instance;
}

bool FFAVDecoder::initialize(AVCodecID id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const AVCodec *codec = avcodec_find_decoder(id);
    if (!codec)
        return false;

    return FFAVCodec::initialize(codec);
}

bool FFAVDecoder::sendPackets() {
    while (!packets_.empty()) {
        auto front = packets_.front();
        auto pkt = front ? front.get() : nullptr;
        int ret = avcodec_send_packet(context_.get(), pkt);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN)) {
            } else if (ret == AVERROR_EOF) {
                packet_eof_.store(true);
            }
            return false;
        }

        if (!pkt)
            packet_eof_.store(true);

        packets_.pop();
        recvFrames();
    }
    return true;
}

bool FFAVDecoder::recvFrames() {
    while (true) {
        AVFrame *frame = av_frame_alloc();
        if (!frame)
            return false;

        int ret = avcodec_receive_frame(context_.get(), frame);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN)) {
                lacked_packet_.store(true);
            } else if (ret == AVERROR_EOF) {
                frame_eof_.store(true);
            }
            av_frame_free(&frame);
            return false;
        }

        lacked_packet_.store(false);
        frames_.push(transformFrame(std::shared_ptr<AVFrame>(
            frame,
            [](AVFrame *p) {
                av_frame_unref(p);
                av_frame_free(&p);
            }
        )));
    }
    return true;
}

bool FFAVDecoder::flushPacket() {
    if (!flushed_packet_.load()) {
        packets_.push(nullptr);
        flushed_packet_.store(true);
    }

    while (true) {
        if (packet_eof_.load())
            break;

        if (!sendPackets())
            return false;
    }
    return true;
}

bool FFAVDecoder::SetParameters(const AVCodecParameters& params) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    int ret = avcodec_parameters_to_context(context_.get(), &params);
    if (ret < 0) {
        return false;
    }
    return true;
}

void FFAVDecoder::SetTimeBase(const AVRational& time_base) {
    context_->pkt_timebase = time_base;
}

bool FFAVDecoder::SendPacket(std::shared_ptr<AVPacket> packet) {
    if (!packet)
        return flushPacket();

    if (!sendPackets())
        return false;

    packets_.push(transformPacket(packet));
    return sendPackets();
}

std::shared_ptr<AVFrame> FFAVDecoder::RecvFrame() {
    recvFrames();
    if (frames_.empty())
        return nullptr;

    auto frame = frames_.front();
    frames_.pop();
    return frame;
}

bool FFAVDecoder::LackedPacket() const {
    return lacked_packet_.load();
}

std::shared_ptr<FFAVEncoder> FFAVEncoder::Create(AVCodecID id) {
    auto instance = std::shared_ptr<FFAVEncoder>(new FFAVEncoder());
    if (!instance->initialize(id))
        return nullptr;
    return instance;
}

bool FFAVEncoder::initialize(AVCodecID id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const AVCodec *codec = avcodec_find_encoder(id);
    if (!codec)
        return false;
    return FFAVCodec::initialize(codec);
}

template <typename T, typename Compare>
bool FFAVEncoder::checkConfig(AVCodecConfig config, const T& value, Compare compare) {
    int num_configs = 0;
    const T *configs = nullptr;
    int ret = avcodec_get_supported_config(
        context_.get(), nullptr, config, 0, reinterpret_cast<const void**>(&configs), &num_configs);
    if (ret < 0)
        return false;

    if (!configs || num_configs <= 0)
        return true;

    return std::any_of(configs, configs + num_configs,
        [&value, &compare](const T& item) {
            return compare(item, value);
        });
}

bool FFAVEncoder::sendFrames() {
    while (!frames_.empty()) {
        auto front = frames_.front();
        auto frm = front ? front.get() : nullptr;
        int ret = avcodec_send_frame(context_.get(), frm);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN)) {
            } else if (ret == AVERROR_EOF) {
                frame_eof_.store(true);
            } else
                std::cerr << "avcodec_send_frame: " << AVErrorStr(ret) << std::endl;
            return false;
        }

        if (!frm)
            frame_eof_.store(true);

        frames_.pop();
        recvPackets();
    }
    return true;
}

bool FFAVEncoder::recvPackets() {
    while (true) {
        AVPacket *packet = av_packet_alloc();
        if (!packet)
            return false;

        int ret = avcodec_receive_packet(context_.get(), packet);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN))
                lacked_frame_.store(true);
            else if (ret == AVERROR_EOF)
                packet_eof_.store(true);
            else
                std::cerr << "avcodec_receive_packet: " << AVErrorStr(ret) << std::endl;
            av_packet_free(&packet);
            return false;
        }

        lacked_frame_.store(false);
        packets_.push(transformPacket(std::shared_ptr<AVPacket>(
            packet,
            [](AVPacket *p) {
                av_packet_unref(p);
                av_packet_free(&p);
            }
        )));
    }
    return true;
}

bool FFAVEncoder::flushFrame() {
    if (!flushed_frame_.load()) {
        frames_.push(nullptr);
        flushed_frame_.store(true);
    }

    while (true) {
        if (frame_eof_.load())
            break;

        if (!sendFrames())
            return false;
    }
    return true;
}

bool FFAVEncoder::SetParameters(const AVCodecParameters& params) {
    context_->bit_rate = params.bit_rate;
    if (params.codec_type == AVMEDIA_TYPE_VIDEO) {
        if (!checkConfig(AV_CODEC_CONFIG_PIX_FORMAT, (AVPixelFormat)params.format))
            return false;

        if (params.framerate.num == 0 || params.framerate.den == 0) {
            context_->time_base = AV_TIME_BASE_Q;
        } else {
            context_->framerate = params.framerate;
            context_->time_base = av_inv_q(params.framerate);
        }

        context_->width = params.width;
        context_->height = params.height;
        context_->sample_aspect_ratio = params.sample_aspect_ratio;
        context_->pix_fmt = (AVPixelFormat)params.format;
    } else if (params.codec_type == AVMEDIA_TYPE_AUDIO) {
        if (!checkConfig(AV_CODEC_CONFIG_SAMPLE_RATE, params.sample_rate))
            return false;

        if (!checkConfig(AV_CODEC_CONFIG_SAMPLE_FORMAT, (AVSampleFormat)params.format))
            return false;

        if (!checkConfig(AV_CODEC_CONFIG_CHANNEL_LAYOUT, params.ch_layout, [](auto a, auto b) {
            return av_channel_layout_compare(&a, &b) == 0;
        })) {
            return false;
        }

        context_->time_base = {1, params.sample_rate};
        context_->sample_fmt = (AVSampleFormat)params.format;
        context_->sample_rate = params.sample_rate;
        int ret = av_channel_layout_copy(&context_->ch_layout, &params.ch_layout);
        if (ret < 0)
            return false;
    }
    return true;
}

void FFAVEncoder::SetGopSize(int gop_size) {
    context_->gop_size = gop_size;
}

void FFAVEncoder::SetMaxBFrames(int max_b_frames) {
    context_->max_b_frames = max_b_frames;
}

void FFAVEncoder::SetFlags(int flags) {
    context_->flags |= flags;
}

bool FFAVEncoder::SetOption(const std::string& name, const std::string& val, int search_flags) {
    int ret = av_opt_set(context_->priv_data, name.c_str(), val.c_str(), search_flags);
    return bool(ret == 0);
}

bool FFAVEncoder::SetOptions(const std::unordered_map<std::string, std::string>& options) {
    AVDictionary *opts = nullptr;

    for (const auto& [key, value] : options) {
        int ret = av_dict_set(&opts, key.c_str(), value.c_str(), 0);
        if (ret < 0)
            return false;
    }

    int ret = av_opt_set_dict(context_->priv_data, &opts);
    if (ret < 0)
        return false;

    av_dict_free(&opts);
    return true;
}

bool FFAVEncoder::SendFrame(std::shared_ptr<AVFrame> frame) {
    if (!frame)
        return flushFrame();

    if (!sendFrames())
        return false;

    frames_.push(transformFrame(frame));
    return sendFrames();
}

std::shared_ptr<AVPacket> FFAVEncoder::RecvPacket() {
    recvPackets();
    if (packets_.empty())
        return nullptr;

    auto packet = packets_.front();
    packets_.pop();
    return packet;
}

bool FFAVEncoder::LackedFrame() const {
    return lacked_frame_.load();
}

std::string DumpAVPacket(const AVPacket* packet) {
    if (!packet)
        return {};

    std::ostringstream ss;
    ss << "packet"
        << " index:" << packet->stream_index
        << " dts:" << packet->dts
        << " pts:" << packet->pts
        << " duration:" << packet->duration
        << " time_base:" << packet->time_base.num << "/" << packet->time_base.den
        << " size:" << packet->size
        << " pos:" << packet->pos;

    ss << " flags:";
    if (packet->flags & AV_PKT_FLAG_KEY) ss << "K";
    if (packet->flags & AV_PKT_FLAG_CORRUPT) ss << "C";
    if (packet->flags & AV_PKT_FLAG_DISCARD) ss << "D";
    if (packet->flags & AV_PKT_FLAG_TRUSTED) ss << "T";
    if (packet->flags & AV_PKT_FLAG_DISPOSABLE) ss << "P";

    ss << " side_data:";
    for (int i = 0; i < packet->side_data_elems; i++) {
        const AVPacketSideData& sd = packet->side_data[i];
        ss << av_packet_side_data_name(sd.type) << "@" << sd.size << " ";
    }

    if (packet->size > 0) {
        ss << " head:";
        for (int i = 0; i < std::min(packet->size, 8); ++i) {
            ss << std::hex << std::uppercase
                << std::setw(2) << std::setfill('0')
                << static_cast<int>(packet->data[i]) << " ";
        }
        ss << std::dec;
    }
    return ss.str();
}

std::string DumpAVCodecParameters(const AVCodecParameters* params) {
    if (!params)
        return {};

    std::ostringstream ss;
    ss << av_get_media_type_string(params->codec_type)
        << " " << avcodec_get_name(params->codec_id)
        << "/" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << params->codec_tag << std::dec
        << " bit_rate:" << params->bit_rate;

    if (params->codec_type == AVMEDIA_TYPE_VIDEO) {
        ss << " pix_fmt:" << av_get_pix_fmt_name(static_cast<AVPixelFormat>(params->format))
            << " width:" << params->width
            << " height:" << params->height
            << " SAR:" << params->sample_aspect_ratio.num << "/" << params->sample_aspect_ratio.den
            << " framerate:" << params->framerate.num << "/" << params->framerate.den
            << " field_order:" << params->field_order
            << " color_range:" << av_color_range_name(params->color_range)
            << " color_primaries:" << av_color_primaries_name(params->color_primaries)
            << " color_trc:" << av_color_transfer_name(params->color_trc)
            << " color_space:" << av_color_space_name(params->color_space)
            << " chroma_location:" << av_chroma_location_name(params->chroma_location)
            << " video_delay:" << params->video_delay;
    } else if (params->codec_type == AVMEDIA_TYPE_AUDIO) {
        ss << " sample_fmt:" << av_get_sample_fmt_name(static_cast<AVSampleFormat>(params->format))
            << " sample_rate:" << params->sample_rate
            << " ch_layout:" << AVChannelLayoutStr(&params->ch_layout)
            << " size:" << params->frame_size;
    } else if (params->codec_type == AVMEDIA_TYPE_SUBTITLE) {
        ss << " width:" << params->width
            << " height:" << params->height;
    }
    if (params->extradata && params->extradata_size > 0) {
        ss << " extradata:";
        for (int i = 0; i < params->extradata_size; i++) {
            ss << std::hex << std::uppercase
                << std::setw(2) << std::setfill('0')
                << (int)params->extradata[i] << " ";
        }
        ss << std::dec;
    }
    return ss.str();
}
