#include <cassert>
#include "test_ffmpeg.h"
#include "../avmedia.h"

void test_demux(const std::string& uri) {
    auto origin = FFAVDemuxer::Create(uri);
    if (origin) {
        origin->DumpStreams();
        for (auto i : origin->GetStreamIndexes()) {
            std::shared_ptr<FFAVStream> demuxstream = origin->GetDemuxStream(i);
            std::shared_ptr<AVStream> stream = demuxstream->GetStream();
            AVCodecParameters *codecpar = stream->codecpar;
            if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                printf("Found video stream at index %d\n", i);
            } else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                printf("Found audio stream at index %d\n", i);
            }
            std::cout << DumpAVCodecParameters(codecpar) << std::endl;
        }
        for (int i = 0; i < 20; i++) {
            std::shared_ptr<AVPacket> packet = origin->ReadPacket();
            std::cout << DumpAVPacket(packet.get()) << std::endl;
        }
    }
}

void test_remux(
    const std::string& src_uri,
    const std::string& dst_uri,
    const std::string& mux_fmt,
    double seek_timestamp,
    double duration
) {
    bool debug = true;
    auto m = FFAVMedia::Create();

    auto demuxer = m->AddDemuxer(src_uri);
    demuxer->DumpStreams();

    auto muxer = m->AddMuxer(dst_uri, mux_fmt);
    muxer->SetDebug(debug);
    muxer->SetMetadata({
        { "title", "FFmpeg Remux Example" },
        { "author", "Quincy Yang" }
    });

    for (auto i : demuxer->GetStreamIndexes()) {
        auto src_muxstream = demuxer->GetDemuxStream(i);
        src_muxstream->SetDebug(debug);

        auto src_stream = src_muxstream->GetStream();
        auto src_language = src_muxstream->GetMetadata("language");
        auto src_time_base = src_stream->time_base;
        auto src_codecpar = src_stream->codecpar;
        if (src_codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            auto src_video = FFAVNode{ src_uri, src_stream->index };
            auto dst_muxstream = muxer->AddMuxStream();
            dst_muxstream->SetDebug(debug);
            dst_muxstream->SetMetadata({
                { "language", src_language },
            });
            dst_muxstream->SetParameters(*src_codecpar);
            dst_muxstream->SetDesiredTimeBase( { src_time_base.num, src_time_base.den * 2 } );
            auto dst_video = FFAVNode{ dst_uri, dst_muxstream->GetIndex() };
            m->AddRule(src_video, dst_video);
            //m->SetOption({ { src_uri, src_stream->index }, seek_timestamp, duration });
            //m->SetOption({ { dst_uri, dst_muxstream->GetIndex() }, seek_timestamp, duration });
        } else if (src_codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            auto src_audio = FFAVNode{ src_uri, src_stream->index };
            auto dst_muxstream = muxer->AddMuxStream();
            dst_muxstream->SetDebug(debug);
            dst_muxstream->SetParameters(*src_codecpar);
            dst_muxstream->SetDesiredTimeBase( { src_time_base.num, src_time_base.den / 2 } );
            auto dst_audio = FFAVNode{ dst_uri, dst_muxstream->GetIndex() };
            m->AddRule(src_audio, dst_audio);
            //m->SetOption({ { src_uri, src_stream->index }, seek_timestamp, duration });
            //m->SetOption({ { dst_uri, dst_muxstream->GetIndex() }, seek_timestamp, duration });
        }
    }

    m->SetOption({ { src_uri, -1 }, seek_timestamp, duration });
    if (!m->Remux()) {
        std::cout << "remux fail." << std::endl;
        return;
    }

    muxer->DumpStreams();
}

void test_transcode(
    const std::string& src_uri,
    const std::string& dst_uri,
    const std::string& mux_fmt,
    double seek_timestamp,
    double duration
) {
    bool debug = true;
    auto m = FFAVMedia::Create();

    auto demuxer = m->AddDemuxer(src_uri);
    demuxer->DumpStreams();

    auto muxer = m->AddMuxer(dst_uri, mux_fmt);
    muxer->SetDebug(debug);

    for (auto i : demuxer->GetStreamIndexes()) {
        auto decodestream = demuxer->GetDecodeStream(i);
        decodestream->SetDebug(debug);

        auto decoder = decodestream->GetDecoder();
        decoder->SetDebug(debug);

        auto src_stream = decodestream->GetStream();
        auto src_codecpar = src_stream->codecpar;
        std::cout << DumpAVCodecParameters(src_codecpar) << std::endl;

        if (src_codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            auto src_video = FFAVNode{ src_uri, src_stream->index };

            AVCodecParameters dst_codecpar{};
            dst_codecpar.codec_type = src_codecpar->codec_type;
            dst_codecpar.codec_id = AV_CODEC_ID_H264;
            dst_codecpar.format = AV_PIX_FMT_YUV420P;
            dst_codecpar.bit_rate = 4000000;
            dst_codecpar.width = src_codecpar->width;
            dst_codecpar.height = src_codecpar->height;
            dst_codecpar.framerate = { 30, 1 };
            dst_codecpar.sample_aspect_ratio = { 1, 1 };
            //dst_codecpar.color_range = AVCOL_RANGE_MPEG;
            //dst_codecpar.color_primaries = AVCOL_PRI_BT709;
            //dst_codecpar.color_trc = AVCOL_TRC_BT709;
            //dst_codecpar.color_space = AVCOL_SPC_BT709;
            //dst_codecpar.chroma_location = AVCHROMA_LOC_LEFT;
            //dst_codecpar.video_delay = 1;

            auto encodestream = muxer->AddEncodeStream(dst_codecpar.codec_id);
            encodestream->SetDebug(debug);
            //encodestream->SetDesiredTimeBase({ 1, 90000 });
            auto encoder = encodestream->GetEncoder();
            encoder->SetDebug(debug);
            assert(encoder->SetParameters(dst_codecpar));
            //encoder->SetGopSize(25);
            //encoder->SetMaxBFrames(2);
            //encoder->SetOptions({
            //    { "force_key_frames", "expr:gte(t,n_forced*2)" },
            //    { "x264-params", "keyint=25:min-keyint=2:no-scenecut" },
            //    { "preset", "fast" }
            //});

            auto dst_video = FFAVNode{ dst_uri, encodestream->GetIndex() };
            m->AddRule(src_video, dst_video);
            //m->SetOption({ { src_uri, src_stream->index }, seek_timestamp, duration });
            //m->SetOption({ { dst_uri, encodestream->GetIndex() }, seek_timestamp, duration });
        } else if (src_codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            auto src_audio = FFAVNode{ src_uri, src_stream->index };

            AVCodecParameters dst_codecpar{};
            dst_codecpar.codec_type = src_codecpar->codec_type;
            dst_codecpar.codec_id = AV_CODEC_ID_AAC;
            dst_codecpar.bit_rate = 102400;
            dst_codecpar.format = AV_SAMPLE_FMT_FLTP;
            dst_codecpar.ch_layout = AV_CHANNEL_LAYOUT_MONO;
            dst_codecpar.sample_rate = 48000;

            auto encodestream = muxer->AddEncodeStream(dst_codecpar.codec_id);
            encodestream->SetDebug(debug);
            //encodestream->SetDesiredTimeBase({ 1, dst_codecpar.sample_rate });
            auto encoder = encodestream->GetEncoder();
            encoder->SetDebug(debug);
            assert(encoder->SetParameters(dst_codecpar));

            auto dst_audio = FFAVNode{ dst_uri, encodestream->GetIndex() };
            m->AddRule(src_audio, dst_audio);
            //m->SetOption({ { src_uri, src_stream->index }, seek_timestamp, duration });
            //m->SetOption({ { dst_uri, encodestream->GetIndex() }, seek_timestamp, duration });
        }
    }

    m->SetOption({ { src_uri, -1 }, seek_timestamp, duration });
    if (!m->Transcode()) {
        std::cout << "Transcode fail." << std::endl;
        return;
    }

    muxer->DumpStreams();
}

int main() {
    try {
        //av_log_set_level(AV_LOG_DEBUG);

        test_demuxer("/opt/ffmpeg/sample/tiny/oceans.mp4");

        //test_demux("/opt/app/gweb/tests/play-from-disk/output.ivf");
        //test_demux("/opt/ffmpeg/sample/tiny/oceans.mp4");
        //test_remux(
        //    "/opt/ffmpeg/sample/tiny/1.mp4",
        //    "/opt/ffmpeg/sample/tiny/1-o.flv",
        //    "flv", 9.0, 0.220
        //);
        //test_remux(
        //    "/opt/ffmpeg/sample/tiny/1.mp4",
        //    "/opt/ffmpeg/sample/tiny/1-o.mov",
        //    "mov", 9.0, 0.220
        //);
        //test_remux(
        //    "/opt/ffmpeg/sample/tiny/1.mp4",
        //    "/opt/ffmpeg/sample/tiny/1-o.mkv",
        //    "matroska", 9.0, 0.220
        //);
        //test_transcode(
        //    "/opt/ffmpeg/sample/tiny/1.mp4",
        //    "/opt/ffmpeg/sample/tiny/1-o.flv",
        //    "flv", 9.0, 0.520
        //);
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception occurred." << std::endl;
        return 1;
    }
    return 0;
}
