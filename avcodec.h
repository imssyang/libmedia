
#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include "avutil.h"
#include "swscale.h"
extern "C" {
#define __STDC_CONSTANT_MACROS
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

class FFAVCodec {
public:
    std::shared_ptr<const AVCodec> GetCodec() const;
    std::shared_ptr<AVCodecContext> GetContext() const;
    std::shared_ptr<AVCodecParameters> GetParameters() const;
    std::shared_ptr<FFSWScale> GetSWScale() const;
    int64_t GetFrameCount() const;
    void SetDebug(bool debug);
    bool SetSWScale(int dst_width, int dst_height, AVPixelFormat dst_pix_fmt, int flags);
    bool Open();
    bool PacketEOF() const;
    bool FrameEOF() const;

protected:
    FFAVCodec() = default;
    bool initialize(const AVCodec *codec);
    std::shared_ptr<AVPacket> transformPacket(std::shared_ptr<AVPacket> packet);
    std::shared_ptr<AVFrame> transformFrame(std::shared_ptr<AVFrame> frame);

protected:
    mutable std::recursive_mutex mutex_;
    std::atomic_bool debug_{false};
    std::atomic_bool packet_eof_{false};
    std::atomic_bool frame_eof_{false};
    std::atomic_int64_t frame_count_{0};
    std::shared_ptr<const AVCodec> codec_;
    std::shared_ptr<AVCodecContext> context_;
    std::shared_ptr<FFSWScale> swscale_;
    std::queue<std::shared_ptr<AVPacket>> packets_;
    std::queue<std::shared_ptr<AVFrame>> frames_;

private:
    std::atomic_bool opened_;
};

class FFAVDecoder final : public FFAVCodec {
public:
    static std::shared_ptr<FFAVDecoder> Create(AVCodecID id);
    bool SetParameters(const AVCodecParameters& params);
    void SetTimeBase(const AVRational& time_base);
    bool SendPacket(std::shared_ptr<AVPacket> packet);
    std::shared_ptr<AVFrame> RecvFrame();
    bool LackedPacket() const;

private:
    FFAVDecoder() = default;
    bool initialize(AVCodecID id);
    bool sendPackets();
    bool recvFrames();
    bool flushPacket();

private:
    std::atomic_bool lacked_packet_{false};
    std::atomic_bool flushed_packet_{false};
};

class FFAVEncoder final : public FFAVCodec {
public:
    static std::shared_ptr<FFAVEncoder> Create(AVCodecID id);
    bool SetParameters(const AVCodecParameters& params);
    void SetGopSize(int gop_size);
    void SetMaxBFrames(int max_b_frames);
    void SetFlags(int flags);
    bool SetOption(const std::string& name, const std::string& val, int search_flags);
    bool SetOptions(const std::unordered_map<std::string, std::string>& options);
    bool SendFrame(std::shared_ptr<AVFrame> frame);
    std::shared_ptr<AVPacket> RecvPacket();
    bool LackedFrame() const;

private:
    FFAVEncoder() = default;
    bool initialize(AVCodecID id);
    template <typename T, typename Compare = std::equal_to<T>>
    bool checkConfig(AVCodecConfig config, const T& value, Compare compare = Compare());
    bool sendFrames();
    bool recvPackets();
    bool flushFrame();

private:
    std::atomic_bool lacked_frame_{false};
    std::atomic_bool flushed_frame_{false};
};

std::string DumpAVPacket(const AVPacket* packet);
std::string DumpAVCodecParameters(const AVCodecParameters* params);
