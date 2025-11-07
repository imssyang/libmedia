#include <iomanip>
#include "avformat.h"

std::shared_ptr<FFAVStream> FFAVStream::Create(
    std::shared_ptr<AVFormatContext> context,
    std::shared_ptr<AVStream> stream) {
    auto instance = std::shared_ptr<FFAVStream>(new FFAVStream());
    if (!instance->initialize(context, stream))
        return nullptr;
    return instance;
}

bool FFAVStream::initialize(
    std::shared_ptr<AVFormatContext> context,
    std::shared_ptr<AVStream> stream) {
    context_ = context;
    stream_ = stream;
    return true;
}

bool FFAVStream::flushStream() {
    return true;
}

int64_t FFAVStream::getPacketDts() const {
    return packet_dts_.load();
}

int64_t FFAVStream::getFramePts() const {
    return frame_pts_.load();
}

void FFAVStream::setFmtStartTime(int64_t start_time) {
    if (fmt_start_time_.load() == AV_NOPTS_VALUE) {
        auto fmt_starttime = av_rescale_q(start_time, AV_TIME_BASE_Q, stream_->time_base);
        fmt_start_time_.store(fmt_starttime);
        std::cout << "[FmtStartTime]" << context_->url
            << " index:" << stream_->index
            << " time_base:" << stream_->time_base.den
            << " start_time:" << fmt_starttime
            << std::endl;
    }
}

void FFAVStream::resetTimeBase(const AVRational& time_base) {
    start_time_.store(
        av_rescale_q(start_time_.load(), time_base, stream_->time_base));
    first_dts_.store(
        av_rescale_q(first_dts_.load(), time_base, stream_->time_base));
    pkt_duration_.store(
        av_rescale_q(pkt_duration_.load(), time_base, stream_->time_base));
    limit_duration_.store(
        av_rescale_q(limit_duration_.load(), time_base, stream_->time_base));
    std::cout << "[StartTimeReset]" << context_->url
        << " index:" << stream_->index
        << " time_base:" << stream_->time_base.den
        << " start_time:" << start_time_.load()
        << " first_dts:" << first_dts_.load()
        << " pkt_duration:" << pkt_duration_.load()
        << " limit_duration:" << limit_duration_.load()
        << std::endl;
}

std::shared_ptr<AVPacket> FFAVStream::setStartTime(std::shared_ptr<AVPacket> packet) {
    if (!packet)
        return packet;

    if (start_time_.load() != AV_NOPTS_VALUE)
        return packet;

    start_time_.store(packet->pts);
    first_dts_.store(packet->dts);
    pkt_duration_.store(packet->duration);
    limit_duration_.store(
        av_rescale_q(limit_duration_.load(), AV_TIME_BASE_Q, stream_->time_base));
    std::cout << "[StartTimeByPacket]" << context_->url
        << " index:" << stream_->index
        << " time_base:" << stream_->time_base.den
        << " start_time:" << start_time_.load()
        << " first_dts:" << first_dts_.load()
        << " pkt_duration:" << pkt_duration_.load()
        << " limit_duration:" << limit_duration_.load()
        << std::endl;
    return packet;
}

std::shared_ptr<AVFrame> FFAVStream::setStartTime(std::shared_ptr<AVFrame> frame) {
    if (!frame)
        return frame;

    if (start_time_.load() != AV_NOPTS_VALUE)
        return frame;

    start_time_.store(frame->pts);
    first_dts_.store(frame->pkt_dts);
    pkt_duration_.store(frame->duration);
    limit_duration_.store(
        av_rescale_q(limit_duration_.load(), AV_TIME_BASE_Q, stream_->time_base));
    std::cout << "[StartTimeByFrame]" << context_->url
        << " index:" << stream_->index
        << " time_base:" << stream_->time_base.den
        << " start_time:" << start_time_.load()
        << " first_dts:" << first_dts_.load()
        << " pkt_duration:" << pkt_duration_.load()
        << " limit_duration:" << limit_duration_.load()
        << std::endl;
    return frame;
}

std::shared_ptr<AVPacket> FFAVStream::setLimitStatus(std::shared_ptr<AVPacket> packet) {
    if (reached_limit_.load())
        return packet;

    if (!packet)
        return packet;

    if (limit_duration_.load() > 0) {
        int64_t current_duration = packet->pts - start_time_.load() - pkt_duration_.load();
        if (current_duration >= limit_duration_.load()) {
            if (!flushStream())
                return packet;
            reached_limit_.store(true);
        }
    }
    return packet;
}

std::shared_ptr<AVPacket> FFAVStream::transformPacket(std::shared_ptr<AVPacket> packet) {
    if (!packet)
        return nullptr;

    if (context_->iformat) {
        if (packet->time_base.num == 0 || packet->time_base.den == 0)
            packet->time_base = stream_->time_base;
    }

    if (context_->oformat) {
        auto newpacket = std::shared_ptr<AVPacket>(
            av_packet_clone(packet.get()),
            [&](AVPacket *p) {
                av_packet_unref(p);
                av_packet_free(&p);
            }
        );
        newpacket->stream_index = stream_->index;
        newpacket->pos = -1;
        if (av_cmp_q(packet->time_base, stream_->time_base) != 0) {
            newpacket->time_base = stream_->time_base;
            newpacket->pts = av_rescale_q_rnd(packet->pts, packet->time_base, stream_->time_base,
                static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            newpacket->dts = av_rescale_q_rnd(packet->dts, packet->time_base, stream_->time_base,
                static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            newpacket->duration = av_rescale_q(packet->duration, packet->time_base, stream_->time_base);
        }
        packet = newpacket;
    }

    packet_dts_.store(packet->dts);
    return setStartTime(packet);
}

std::shared_ptr<AVFrame> FFAVStream::transformFrame(std::shared_ptr<AVFrame> frame) {
    if (!frame)
        return nullptr;

    if (context_->iformat) {
        frame->time_base = stream_->time_base;
    }

    if (context_->oformat) {
        auto newframe = std::shared_ptr<AVFrame>(
            av_frame_clone(frame.get()),
            [&](AVFrame *p) {
                av_frame_unref(p);
                av_frame_free(&p);
            }
        );
        if (av_cmp_q(frame->time_base, stream_->time_base) != 0) {
            newframe->time_base = stream_->time_base;
            newframe->pts = av_rescale_q_rnd(frame->pts, frame->time_base, stream_->time_base,
                static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            newframe->pkt_dts = av_rescale_q_rnd(frame->pkt_dts, frame->time_base, stream_->time_base,
                static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            newframe->duration = av_rescale_q(frame->duration, frame->time_base, stream_->time_base);
        }
        frame = newframe;
    }

    frame_pts_.store(frame->pts);
    return setStartTime(frame);
}

std::shared_ptr<AVPacket> FFAVStream::formatPacket(std::shared_ptr<AVPacket> packet) {
    packet_count_++;
    auto pkt = setLimitStatus(transformPacket(packet));

    if (debug_.load()) {
        std::cout << "[";
        if (context_->iformat)
            std::cout << "R";
        else if (context_->oformat)
            std::cout << "W";
        else
            std::cout << "-";
        std::cout << ":" << pkt->stream_index
            << ":" << packet_count_.load()
            << "]" << DumpAVPacket(pkt.get())
            << std::endl;
    }

    if (context_->oformat) {
        int64_t dts_offset = 0;
        if (first_dts_.load() > start_time_.load())
            dts_offset = first_dts_.load() - start_time_.load();

        pkt->pts -= fmt_start_time_.load();
        pkt->dts -= fmt_start_time_.load() + dts_offset;
    }
    return pkt;
}

std::shared_ptr<AVFormatContext> FFAVStream::GetContext() const {
    return context_;
}

std::shared_ptr<AVStream> FFAVStream::GetStream() const {
    return stream_;
}

std::shared_ptr<AVCodecParameters> FFAVStream::GetParameters() const {
    return std::shared_ptr<AVCodecParameters>(stream_->codecpar, [](auto){});
}

int FFAVStream::GetIndex() const {
    return stream_->index;
}

AVRational FFAVStream::GetTimeBase() const {
    return stream_->time_base;
}

uint64_t FFAVStream::GetPacketCount() const {
    return packet_count_;
}

std::string FFAVStream::GetMetadata(const std::string& metakey) const {
    if (!context_->iformat)
        return {};

    AVDictionaryEntry *entry = av_dict_get(stream_->metadata, metakey.c_str(), nullptr, AV_DICT_IGNORE_SUFFIX);
    if (!entry)
        return {};

    return entry->value;
}

bool FFAVStream::ReachLimit() const {
    return reached_limit_.load();
}

bool FFAVStream::SetMetadata(const std::unordered_map<std::string, std::string>& metadata) {
    if (!context_->oformat)
        return false;

    for (const auto& [key, value] : metadata) {
        int ret = av_dict_set(&stream_->metadata, key.c_str(), value.c_str(), 0);
        if (ret < 0)
            return false;
    }
    return true;
}

bool FFAVStream::SetParameters(const AVCodecParameters& params) {
    if (!context_->oformat)
        return false;

    int ret = avcodec_parameters_copy(stream_->codecpar, &params);
    if (ret < 0)
        return false;

    stream_->codecpar->codec_tag = 0;
    return true;
}

bool FFAVStream::SetDesiredTimeBase(const AVRational& time_base) {
    if (!context_->oformat)
        return false;

    stream_->time_base = time_base;
    return true;
}

void FFAVStream::SetDuration(double duration) {
    if (duration > 0) {
        limit_duration_.store(duration * AV_TIME_BASE);
    }
}

void FFAVStream::SetDebug(bool debug) {
    debug_.store(debug);
}

std::shared_ptr<FFAVDecodeStream> FFAVDecodeStream::Create(
    std::shared_ptr<AVFormatContext> context,
    std::shared_ptr<AVStream> stream) {
    auto instance = std::shared_ptr<FFAVDecodeStream>(new FFAVDecodeStream());
    if (!instance->initialize(context, stream))
        return nullptr;
    return instance;
}

bool FFAVDecodeStream::initialize(
    std::shared_ptr<AVFormatContext> context,
    std::shared_ptr<AVStream> stream) {
    if (!initDecoder(stream))
        return false;
    return FFAVStream::initialize(context, stream);
}

bool FFAVDecodeStream::initDecoder(std::shared_ptr<AVStream> stream) {
    if (decoder_)
        return true;

    AVCodecParameters *codecpar = stream->codecpar;
    AVCodecID codec_id = codecpar->codec_id;
    if (codec_id == AV_CODEC_ID_NONE)
        return false;

    auto decoder = FFAVDecoder::Create(codec_id);
    if (!decoder)
        return false;

    if (!decoder->SetParameters(*stream->codecpar))
        return false;

    decoder->SetTimeBase(stream->time_base);
    if (!decoder->Open())
        return false;

    decoder_ = decoder;
    return true;
}

bool FFAVDecodeStream::flushStream() {
    return decoder_->SendPacket(nullptr);
}

std::shared_ptr<FFAVDecoder> FFAVDecodeStream::GetDecoder() const {
    return decoder_;
}

bool FFAVDecodeStream::SendPacket(std::shared_ptr<AVPacket> packet) {
    if (stream_->index != packet->stream_index)
        return false;

    if (!decoder_->SendPacket(transformPacket(packet)))
        return false;

    return true;
}

std::shared_ptr<AVFrame> FFAVDecodeStream::RecvFrame() {
    auto frame = decoder_->RecvFrame();
    if (!frame)
        return nullptr;
    return transformFrame(frame);
}

std::shared_ptr<FFAVEncodeStream> FFAVEncodeStream::Create(
    std::shared_ptr<AVFormatContext> context,
    std::shared_ptr<AVStream> stream,
    std::shared_ptr<FFAVEncoder> encoder) {
    auto instance = std::shared_ptr<FFAVEncodeStream>(new FFAVEncodeStream());
    if (!instance->initialize(context, stream, encoder))
        return nullptr;
    return instance;
}

bool FFAVEncodeStream::initialize(
    std::shared_ptr<AVFormatContext> context,
    std::shared_ptr<AVStream> stream,
    std::shared_ptr<FFAVEncoder> encoder) {
    encoder_ = encoder;
    return FFAVStream::initialize(context, stream);
}

bool FFAVEncodeStream::openEncoder() {
    if (!encoder_)
        return false;

    if (openencoded_.load())
        return true;

    auto codec_ctx = encoder_->GetContext();
    if (context_->oformat->flags & AVFMT_GLOBALHEADER) {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (!encoder_->Open())
        return false;

    int ret = avcodec_parameters_from_context(stream_->codecpar, codec_ctx.get());
    if (ret < 0)
        return false;

    if (stream_->time_base.num == 0 || stream_->time_base.den == 0) {
        auto time_base = codec_ctx->time_base;
        if (codec_ctx->codec->type == AVMEDIA_TYPE_VIDEO)
            time_base = { 1, 90000 };
        stream_->time_base = time_base;
    }

    openencoded_.store(true);
    return true;
}

bool FFAVEncodeStream::flushStream() {
    return encoder_->SendFrame(nullptr);
}

std::shared_ptr<FFAVEncoder> FFAVEncodeStream::GetEncoder() const {
    return encoder_;
}

bool FFAVEncodeStream::SendFrame(std::shared_ptr<AVFrame> frame) {
    if (!openEncoder())
        return false;
    if (encoder_->FrameEOF())
        return true;
    return encoder_->SendFrame(transformFrame(frame));
}

std::shared_ptr<AVPacket> FFAVEncodeStream::RecvPacket() {
    auto packet = encoder_->RecvPacket();
    if (!packet)
        return nullptr;
    return transformPacket(packet);
}

FFAVFormat::AVFormatInitPtr FFAVFormat::inited_ = FFAVFormat::AVFormatInitPtr(
    new std::atomic_bool(false),
    [](std::atomic_bool *p) {
        if (p->load()) {
            avformat_network_deinit();
        }
    }
);

FFAVFormat::~FFAVFormat() {
    std::cout << uri_ << " exit." << std::endl;
    exit_.store(true);
}

bool FFAVFormat::initialize(const std::string& uri, std::shared_ptr<AVFormatContext> context) {
    if (!inited_->load()) {
        int ret = avformat_network_init();
        if (ret < 0)
            return false;
        inited_->store(true);
    }

    uri_ = uri;
    context_ = context;
    return true;
}

std::shared_ptr<AVPacket> FFAVFormat::setStartTime(std::shared_ptr<AVPacket> packet) {
    if (!packet)
        return packet;

    auto stream = GetStream(packet->stream_index);
    if (!stream)
        return packet;

    if (start_time_.load() == AV_NOPTS_VALUE) {
        auto start_time = av_rescale_q(packet->pts, stream->GetTimeBase(), AV_TIME_BASE_Q);
        start_time_.store(start_time);
        auto first_dts = av_rescale_q(packet->dts, stream->GetTimeBase(), AV_TIME_BASE_Q);
        first_dts_.store(first_dts);
        real_starttime_.store(av_gettime());
    }

    stream->setFmtStartTime(start_time_.load());
    return packet;
}

std::shared_ptr<AVPacket> FFAVFormat::formatPacket(std::shared_ptr<AVPacket> packet) {
    auto stream = GetStream(packet->stream_index);
    if (!stream)
        return nullptr;

    return stream->formatPacket(setStartTime(packet));
}

std::shared_ptr<AVFormatContext> FFAVFormat::GetContext() const {
    return context_;
}

std::string FFAVFormat::GetURI() const {
    return uri_;
}

std::vector<int> FFAVFormat::GetStreamIndexes() const {
    std::vector<int> indexes;
    std::transform(streams_.begin(), streams_.end(),
        std::back_inserter(indexes),
        [](const auto& pair) { return pair.first; });
    return indexes;
}

std::shared_ptr<FFAVStream> FFAVFormat::GetStream(int stream_index) const {
    return streams_.count(stream_index) ? streams_.at(stream_index) : nullptr;
}

bool FFAVFormat::PacketEOF() const {
    return packet_eof_.load();
}

bool FFAVFormat::FrameEOF() const {
    return frame_eof_.load();
}

void FFAVFormat::SetDebug(bool debug) {
    debug_.store(debug);
}

void FFAVFormat::SetDuration(double duration) {
    for (auto& item : streams_) {
        auto stream = item.second;
        stream->SetDuration(duration);
    }
}

void FFAVFormat::SetPlaySpeed(double speed) {
    play_speed_.store(speed * AV_TIME_BASE);
}

bool FFAVFormat::DropStream(int stream_index) {
    if (!GetStream(stream_index))
        return false;

    streams_.erase(stream_index);
    return true;
}

void FFAVFormat::DumpStreams() const {
    int is_output = context_->iformat ? 0 : 1;
    av_dump_format(context_.get(), 0, uri_.c_str(), is_output);
}

std::shared_ptr<FFAVDemuxer> FFAVDemuxer::Create(const std::string& uri) {
    auto instance = std::shared_ptr<FFAVDemuxer>(new FFAVDemuxer());
    if (!instance->initialize(uri))
        return nullptr;
    return instance;
}

bool FFAVDemuxer::initialize(const std::string& uri) {
    AVFormatContext *context = nullptr;
    int ret = avformat_open_input(&context, uri.c_str(), NULL, NULL);
    if (ret < 0) {
        std::cerr << "avformat_open_input(" << uri << "): " << AVErrorStr(ret) << std::endl;
        return false;
    }

    ret = avformat_find_stream_info(context, NULL);
    if (ret < 0) {
        std::cerr << "avformat_find_stream_info(" << uri << "): " << AVErrorStr(ret) << std::endl;
        avformat_close_input(&context);
        return false;
    }

    auto context_ptr = std::shared_ptr<AVFormatContext>(
        context,
        [&](AVFormatContext* ctx) {
            avformat_close_input(&ctx);
        }
    );

    if (!initDemuxStreams(context_ptr))
        return false;

    return FFAVFormat::initialize(uri, context_ptr);
}

bool FFAVDemuxer::initDemuxStreams(std::shared_ptr<AVFormatContext> context) {
    for (uint32_t i = 0; i < context->nb_streams; i++) {
        auto stream = std::shared_ptr<AVStream>(context->streams[i], [](auto){});
        auto demuxstream = FFAVStream::Create(context, stream);
        if (!demuxstream)
            return false;

        demuxstream->SetDebug(debug_.load());
        streams_[i] = demuxstream;
    }
    return true;
}

bool FFAVDemuxer::setPacketEOF() {
    bool flushed = std::all_of(streams_.begin(), streams_.end(), [&](auto item) {
        return item.second->flushStream();
    });
    if (!flushed)
        return false;

    packet_eof_.store(true);
    return true;
}

std::shared_ptr<FFAVDecodeStream> FFAVDemuxer::choseDecodeStream() {
    std::shared_ptr<FFAVDecodeStream> target;
    int64_t min_pts = AV_NOPTS_VALUE;
    for (auto& item : streams_) {
        auto stream = GetDecodeStream(item.first);
        if (!stream)
            continue;

        auto decoder = stream->GetDecoder();
        if (decoder->FrameEOF())
            continue;

        auto packet_count = stream->GetPacketCount();
        auto frame_pts = stream->getFramePts();
        if (packet_count > 0 && frame_pts == AV_NOPTS_VALUE) {
            target = stream;
            break;
        }

        auto pts = av_rescale_q(frame_pts, stream->GetTimeBase(), AV_TIME_BASE_Q);
        if (min_pts == AV_NOPTS_VALUE || min_pts > pts) {
            min_pts = pts;
            target = stream;
        }
    }
    return target;
}

std::shared_ptr<FFAVStream> FFAVDemuxer::GetDemuxStream(int stream_index) const {
    return GetStream(stream_index);
}

std::shared_ptr<FFAVDecodeStream> FFAVDemuxer::GetDecodeStream(int stream_index) {
    auto demuxstream = GetStream(stream_index);
    if (!demuxstream)
        return nullptr;

    auto decodestream = std::dynamic_pointer_cast<FFAVDecodeStream>(demuxstream);
    if (!decodestream) {
        decodestream = FFAVDecodeStream::Create(context_, demuxstream->GetStream());
        if (!decodestream)
            return nullptr;

        decodestream->debug_.store(debug_.load());
        streams_[stream_index] = decodestream;
    }
    return decodestream;
}

std::string FFAVDemuxer::GetMetadata(const std::string& metakey) const {
    AVDictionaryEntry *entry = av_dict_get(context_->metadata, metakey.c_str(), nullptr, AV_DICT_IGNORE_SUFFIX);
    if (!entry)
        return {};
    return entry->value;
}

std::shared_ptr<AVPacket> FFAVDemuxer::ReadPacket() {
    if (PacketEOF())
        return nullptr;

    AVPacket *packet = av_packet_alloc();
    if (!packet)
        return nullptr;

    while (true) {
        bool finished = std::all_of(streams_.begin(), streams_.end(), [](auto item) {
            return item.second->ReachLimit();
        });
        if (finished) {
            setPacketEOF();
            return nullptr;
        }

        int ret = av_read_frame(context_.get(), packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF)
                setPacketEOF();
            av_packet_free(&packet);
            return nullptr;
        }

        auto stream = GetStream(packet->stream_index);
        if (!stream) {
            av_packet_unref(packet);
            continue;
        }

        if (stream->ReachLimit()) {
            av_packet_unref(packet);
            continue;
        }

        if (play_speed_.load() == 0) {
            if (exit_.load())
                return nullptr;
            // Pause play, waiting restore.
            av_usleep(10*1000);
        }

        if (start_time_.load() != AV_NOPTS_VALUE) {
            int64_t elapsed_dts = packet->dts - first_dts_.load();
            int64_t elapsed_time = av_gettime() - real_starttime_.load();
            elapsed_dts = av_rescale_q(elapsed_dts, stream->GetTimeBase(), AV_TIME_BASE_Q);
            elapsed_time = elapsed_time * play_speed_.load() / AV_TIME_BASE;
            if (elapsed_dts > elapsed_time)
                av_usleep(elapsed_dts - elapsed_time);
        }

        break;
    }

    return formatPacket(std::shared_ptr<AVPacket>(packet, [&](AVPacket *p) {
        av_packet_unref(p);
        av_packet_free(&p);
    }));
}

std::pair<int, std::shared_ptr<AVFrame>> FFAVDemuxer::ReadFrame() {
    if (frame_eof_.load())
        return { -1, nullptr };

    int stream_index = -1;
    std::shared_ptr<AVFrame> frame;
    while (true) {
        auto packet = ReadPacket();
        if (!packet) {
            if (!PacketEOF())
                return { -1, nullptr };
        } else {
            auto stream = GetDecodeStream(packet->stream_index);
            stream->SendPacket(packet);
        }

        auto stream = choseDecodeStream();
        if (!stream) {
            frame_eof_.store(true);
            return { -1, nullptr };
        }

        frame = stream->RecvFrame();
        if (!frame) {
            auto decoder = stream->GetDecoder();
            if (decoder->LackedPacket())
                continue;
            if (decoder->FrameEOF())
                continue;
            return { -1, nullptr };
        }

        stream_index = stream->GetIndex();
        break;
    }

    return {stream_index, frame};
}

bool FFAVDemuxer::Seek(int stream_index, double timestamp) {
    int64_t timestamp_i = timestamp * AV_TIME_BASE;
    auto stream = GetStream(stream_index);
    if (stream) {
        timestamp_i = av_rescale_q(timestamp_i, AV_TIME_BASE_Q, stream->GetTimeBase());
    } else {
        stream_index = -1;
    }
    int ret = av_seek_frame(context_.get(), stream_index, timestamp_i, AVSEEK_FLAG_BACKWARD);
    if (ret < 0)
        return false;
    return true;
}

std::shared_ptr<FFAVMuxer> FFAVMuxer::Create(const std::string& uri, const std::string& mux_fmt) {
    auto instance = std::shared_ptr<FFAVMuxer>(new FFAVMuxer());
    if (!instance->initialize(uri, mux_fmt))
        return nullptr;
    return instance;
}

bool FFAVMuxer::initialize(const std::string& uri, const std::string& mux_fmt) {
    AVFormatContext *context = nullptr;
    const char *filename = uri.empty() ? NULL : uri.c_str();
    const char *format_name = mux_fmt.empty() ? NULL : mux_fmt.c_str();
    int ret = avformat_alloc_output_context2(&context, nullptr, format_name, filename);
    if (ret < 0) {
        std::cerr << "avformat_alloc_output_context2(" << format_name
            << ", " << filename
            << "): " << AVErrorStr(ret) << std::endl;
        return false;
    }

    return FFAVFormat::initialize(uri, std::shared_ptr<AVFormatContext>(
        context,
        [&](AVFormatContext* ctx) {
            if (openmuxed_.load()) {
                if (!(ctx->oformat->flags & AVFMT_NOFILE)) {
                    avio_closep(&ctx->pb);
                }
            }
            avformat_free_context(ctx);
        }
    ));
}

bool FFAVMuxer::openMuxer() {
    if (openmuxed_.load())
        return true;

    for (const auto& item : streams_) {
        auto stream = GetEncodeStream(item.first);
        if (!stream->openEncoder())
            return false;
    }

    if (!(context_->oformat->flags & AVFMT_NOFILE)) {
        int ret = avio_open2(&context_->pb, uri_.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr);
        if (ret < 0) {
            std::cerr << "avio_open2(" << uri_ << "): " << AVErrorStr(ret) << std::endl;
            return false;
        }
    }

    openmuxed_.store(true);
    return true;
}

bool FFAVMuxer::writeHeader() {
    if (headmuxed_.load())
        return true;

    if (!openMuxer())
        return false;

    if (debug_.load()) {
        std::cout << "[W:Header]";
        for (auto& [stream_index, stream] : streams_) {
            auto rawstream = stream->GetStream();
            std::cout << std::fixed << std::setprecision(6)
                << " index:" << stream_index
                << " time_base:" << rawstream->time_base.den;
        }
    }

    std::unordered_map<int, AVRational> time_bases;
    for (const auto& [stream_index, stream] : streams_) {
        time_bases[stream_index] = stream->GetTimeBase();
    }

    int ret = avformat_write_header(context_.get(), nullptr);
    if (ret < 0) {
        std::cerr << "avformat_write_header: " << AVErrorStr(ret) << std::endl;
        return false;
    }

    if (debug_.load()) {
        std::cout << " ->";
        for (auto& [stream_index, stream] : streams_) {
            auto rawstream = stream->GetStream();
            std::cout << std::fixed << std::setprecision(6)
                << " index:" << stream_index
                << " time_base:" << av_q2d(rawstream->time_base)
                << " (" << rawstream->time_base.den << ")";
        }
        std::cout << std::endl;
    }

    for (const auto& [stream_index, stream] : streams_) {
        const auto& time_base = time_bases[stream_index];
        if (av_cmp_q(time_base, stream->GetTimeBase()) != 0) {
            stream->resetTimeBase(time_base);
        }
    }

    headmuxed_.store(true);
    return true;
}

bool FFAVMuxer::writeTrailer() {
    if (trailmuxed_.load())
        return true;

    if (!headmuxed_.load())
        return false;

    int ret = av_write_trailer(context_.get());
    if (ret < 0) {
        std::cerr << "av_write_trailer: " << AVErrorStr(ret) << std::endl;
        return false;
    }

    if (debug_.load()) {
        std::cout << "[W:Tailer]"
            << "streams:" << streams_.size()
            << std::endl;
    }
    trailmuxed_.store(true);
    return true;
}

bool FFAVMuxer::setPacketEOF() {
    packet_eof_.store(true);
    return writeTrailer();
}

bool FFAVMuxer::setFrameEOF(std::shared_ptr<FFAVEncodeStream> stream) {
    if (frame_eof_.load())
        return true;

    if (!stream->flushStream())
        return false;

    bool finished = std::all_of(streams_.begin(), streams_.end(), [&](auto item) {
        auto s = GetEncodeStream(item.first);
        return s->GetEncoder()->FrameEOF();
    });
    if (finished)
        frame_eof_.store(true);
    return true;
}

std::shared_ptr<FFAVEncodeStream> FFAVMuxer::choseEncodeStream() {
    std::shared_ptr<FFAVEncodeStream> target;
    int64_t min_dts = AV_NOPTS_VALUE;
    for (auto& item : streams_) {
        auto stream = GetEncodeStream(item.first);
        if (!stream)
            continue;

        auto encoder = stream->GetEncoder();
        if (encoder->PacketEOF())
            continue;

        auto frame_count = stream->GetEncoder()->GetFrameCount();
        auto packet_dts = stream->getPacketDts();
        if (frame_count > 0 && packet_dts == AV_NOPTS_VALUE) {
            target = stream;
            break;
        }

        auto dts = av_rescale_q(packet_dts, stream->GetTimeBase(), AV_TIME_BASE_Q);
        if (min_dts == AV_NOPTS_VALUE || min_dts > dts) {
            min_dts = dts;
            target = stream;
        }
    }
    return target;
}

std::shared_ptr<FFAVStream> FFAVMuxer::GetMuxStream(int stream_index) const {
    return GetStream(stream_index);
}

std::shared_ptr<FFAVEncodeStream> FFAVMuxer::GetEncodeStream(int stream_index) const {
    auto muxstream = GetStream(stream_index);
    if (!muxstream)
        return nullptr;
    return std::dynamic_pointer_cast<FFAVEncodeStream>(muxstream);
}

std::shared_ptr<FFAVStream> FFAVMuxer::AddMuxStream() {
    auto stream = avformat_new_stream(context_.get(), nullptr);
    if (!stream)
        return nullptr;

    auto muxstream = FFAVStream::Create(
        context_,
        std::shared_ptr<AVStream>(stream, [](auto){}));
    if (!muxstream)
        return nullptr;

    muxstream->SetDebug(debug_.load());
    streams_[stream->index] = muxstream;
    return muxstream;
}

std::shared_ptr<FFAVEncodeStream> FFAVMuxer::AddEncodeStream(AVCodecID codec_id) {
    if (codec_id == AV_CODEC_ID_NONE)
        return nullptr;

    auto encoder = FFAVEncoder::Create(codec_id);
    if (!encoder)
        return nullptr;

    auto stream = avformat_new_stream(context_.get(), encoder->GetCodec().get());
    if (!stream)
        return nullptr;

    auto encodestream = FFAVEncodeStream::Create(
        context_,
        std::shared_ptr<AVStream>(stream, [](auto){}),
        encoder);
    if (!encodestream)
        return nullptr;

    encodestream->debug_.store(debug_.load());
    streams_[stream->index] = encodestream;
    return encodestream;
}

bool FFAVMuxer::SetMetadata(const std::unordered_map<std::string, std::string>& metadata) {
    for (const auto& [key, value] : metadata) {
        int ret = av_dict_set(&context_->metadata, key.c_str(), value.c_str(), 0);
        if (ret < 0)
            return false;
    }
    return true;
}

bool FFAVMuxer::WritePacket(std::shared_ptr<AVPacket> packet) {
    if (!packet)
        return setPacketEOF();

    if (!writeHeader())
        return false;

    auto stream = GetMuxStream(packet->stream_index);
    if (stream->ReachLimit())
        return true;

    packet = formatPacket(packet);
    if (!packet)
        return false;

    int ret = av_interleaved_write_frame(context_.get(), packet.get());
    if (ret < 0) {
        std::cerr << "av_interleaved_write_frame: " << AVErrorStr(ret) << std::endl;
        return false;
    }
    return true;
}

bool FFAVMuxer::WriteFrame(int stream_index, std::shared_ptr<AVFrame> frame) {
    auto stream = GetEncodeStream(stream_index);
    if (!stream)
        return false;

    if (!frame || stream->ReachLimit()) {
        if (!setFrameEOF(stream)) {
            return false;
        }
    }

    if (frame && !stream->SendFrame(frame))
        return false;

    while (true) {
        auto stream = choseEncodeStream();
        if (!stream)
            return WritePacket(nullptr);

        auto packet = stream->RecvPacket();
        if (!packet) {
            auto encoder = stream->GetEncoder();
            if (encoder->LackedFrame())
                break;
            else if (encoder->PacketEOF())
                continue;
            return false;
        }

        if (!WritePacket(packet))
            return false;
    }
    return true;
}
