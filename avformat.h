#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <deque>
#include <queue>
#include <string>
#include <unordered_set>
#include "avutil.h"
#include "avcodec.h"
extern "C" {
#define __STDC_CONSTANT_MACROS
#include <libavformat/avformat.h>
}

class FFAVStream {
public:
    static std::shared_ptr<FFAVStream> Create(
        std::shared_ptr<AVFormatContext> context,
        std::shared_ptr<AVStream> stream);
    std::shared_ptr<AVFormatContext> GetContext() const;
    std::shared_ptr<AVStream> GetStream() const;
    std::shared_ptr<AVCodecParameters> GetParameters() const;
    int GetIndex() const;
    AVRational GetTimeBase() const;
    uint64_t GetPacketCount() const;
    std::string GetMetadata(const std::string& metakey) const;
    bool ReachLimit() const;
    bool SetMetadata(const std::unordered_map<std::string, std::string>& metadata);
    bool SetParameters(const AVCodecParameters& params);
    bool SetDesiredTimeBase(const AVRational& time_base);
    void SetDuration(double duration);
    void SetDebug(bool debug);
    virtual ~FFAVStream() = default;

protected:
    FFAVStream() = default;
    bool initialize(
        std::shared_ptr<AVFormatContext> context,
        std::shared_ptr<AVStream> stream);
    virtual bool flushStream();
    int64_t getPacketDts() const;
    int64_t getFramePts() const;
    void setFmtStartTime(int64_t start_time);
    void resetTimeBase(const AVRational& time_base);
    std::shared_ptr<AVPacket> setStartTime(std::shared_ptr<AVPacket> packet);
    std::shared_ptr<AVFrame> setStartTime(std::shared_ptr<AVFrame> frame);
    std::shared_ptr<AVPacket> setLimitStatus(std::shared_ptr<AVPacket> packet);
    std::shared_ptr<AVPacket> transformPacket(std::shared_ptr<AVPacket> packet);
    std::shared_ptr<AVFrame> transformFrame(std::shared_ptr<AVFrame> frame);
    std::shared_ptr<AVPacket> formatPacket(std::shared_ptr<AVPacket> packet);

protected:
    std::atomic_bool debug_{false};
    std::atomic_bool reached_limit_{false};
    std::atomic_int64_t packet_count_{0};
    std::atomic_int64_t limit_duration_{0};
    std::atomic_int64_t pkt_duration_{0};
    std::atomic_int64_t fmt_start_time_{AV_NOPTS_VALUE};
    std::atomic_int64_t start_time_{AV_NOPTS_VALUE};
    std::atomic_int64_t first_dts_{AV_NOPTS_VALUE};
    std::atomic_int64_t packet_dts_{AV_NOPTS_VALUE};
    std::atomic_int64_t frame_pts_{AV_NOPTS_VALUE};
    std::shared_ptr<AVStream> stream_;
    std::shared_ptr<AVFormatContext> context_;
    friend class FFAVFormat;
    friend class FFAVDemuxer;
    friend class FFAVMuxer;
};

class FFAVDecodeStream : public FFAVStream {
public:
    static std::shared_ptr<FFAVDecodeStream> Create(
        std::shared_ptr<AVFormatContext> context,
        std::shared_ptr<AVStream> stream);
    std::shared_ptr<FFAVDecoder> GetDecoder() const;
    bool SetParameters(const AVCodecParameters& params) = delete;
    bool SetDesiredTimeBase(const AVRational& time_base) = delete;
    bool SendPacket(std::shared_ptr<AVPacket> packet);
    std::shared_ptr<AVFrame> RecvFrame();

private:
    FFAVDecodeStream() = default;
    bool initialize(
        std::shared_ptr<AVFormatContext> context,
        std::shared_ptr<AVStream> stream);
    bool initDecoder(std::shared_ptr<AVStream> stream);
    bool flushStream() override;

private:
    std::shared_ptr<FFAVDecoder> decoder_;
};

class FFAVEncodeStream : public FFAVStream {
public:
    static std::shared_ptr<FFAVEncodeStream> Create(
        std::shared_ptr<AVFormatContext> context,
        std::shared_ptr<AVStream> stream,
        std::shared_ptr<FFAVEncoder> encoder);
    std::shared_ptr<FFAVEncoder> GetEncoder() const;
    bool SetParameters(const AVCodecParameters& params) = delete;
    bool SendFrame(std::shared_ptr<AVFrame> frame);
    std::shared_ptr<AVPacket> RecvPacket();

private:
    FFAVEncodeStream() = default;
    bool initialize(
        std::shared_ptr<AVFormatContext> context,
        std::shared_ptr<AVStream> stream,
        std::shared_ptr<FFAVEncoder> encoder);
    bool openEncoder();
    bool flushStream() override;

private:
    std::atomic_bool openencoded_{false};
    std::shared_ptr<FFAVEncoder> encoder_;
    friend class FFAVMuxer;
};

class FFAVFormat {
protected:
    using AVFormatInitPtr = std::unique_ptr<std::atomic_bool, std::function<void(std::atomic_bool*)>>;
    using FFAVStreamMap = std::map<int, std::shared_ptr<FFAVStream>>;

public:
    virtual ~FFAVFormat();
    std::shared_ptr<AVFormatContext> GetContext() const;
    std::string GetURI() const;
    std::vector<int> GetStreamIndexes() const;
    std::shared_ptr<FFAVStream> GetStream(int stream_index) const;
    bool PacketEOF() const;
    bool FrameEOF() const;
    void SetDebug(bool debug);
    void SetDuration(double duration);
    void SetPlaySpeed(double speed);
    bool DropStream(int stream_index);
    void DumpStreams() const;

protected:
    FFAVFormat() = default;
    bool initialize(const std::string& uri, std::shared_ptr<AVFormatContext> context);
    std::shared_ptr<AVPacket> setStartTime(std::shared_ptr<AVPacket> packet);
    std::shared_ptr<AVPacket> formatPacket(std::shared_ptr<AVPacket> packet);

protected:
    static AVFormatInitPtr inited_;
    std::string uri_;
    std::atomic_bool exit_{false};
    std::atomic_bool debug_{false};
    std::atomic_bool packet_eof_{false};
    std::atomic_bool frame_eof_{false};
    std::atomic_int64_t start_time_{AV_NOPTS_VALUE};
    std::atomic_int64_t first_dts_{AV_NOPTS_VALUE};
    std::atomic_int64_t real_starttime_{0};
    std::atomic_int64_t play_speed_{AV_TIME_BASE};
    std::shared_ptr<AVFormatContext> context_;
    FFAVStreamMap streams_;
};

class FFAVDemuxer final : public FFAVFormat {
public:
    static std::shared_ptr<FFAVDemuxer> Create(const std::string& uri);
    std::shared_ptr<FFAVStream> GetDemuxStream(int stream_index) const;
    std::shared_ptr<FFAVDecodeStream> GetDecodeStream(int stream_index);
    std::string GetMetadata(const std::string& metakey) const;
    std::shared_ptr<AVPacket> ReadPacket();
    std::pair<int, std::shared_ptr<AVFrame>> ReadFrame();
    bool Seek(int stream_index, double timestamp);

private:
    FFAVDemuxer() = default;
    bool initialize(const std::string& uri);
    bool initDemuxStreams(std::shared_ptr<AVFormatContext> context);
    bool setPacketEOF();
    std::shared_ptr<FFAVDecodeStream> choseDecodeStream();
};

class FFAVMuxer final : public FFAVFormat {
public:
    static std::shared_ptr<FFAVMuxer> Create(const std::string& uri, const std::string& mux_fmt);
    std::shared_ptr<FFAVStream> GetMuxStream(int stream_index) const;
    std::shared_ptr<FFAVEncodeStream> GetEncodeStream(int stream_index) const;
    std::shared_ptr<FFAVStream> AddMuxStream();
    std::shared_ptr<FFAVEncodeStream> AddEncodeStream(AVCodecID codec_id);
    bool SetMetadata(const std::unordered_map<std::string, std::string>& metadata);
    bool WritePacket(std::shared_ptr<AVPacket> packet);
    bool WriteFrame(int stream_index, std::shared_ptr<AVFrame> frame);

private:
    FFAVMuxer() = default;
    bool initialize(const std::string& uri, const std::string& mux_fmt);
    bool openMuxer();
    bool writeHeader();
    bool writeTrailer();
    bool setPacketEOF();
    bool setFrameEOF(std::shared_ptr<FFAVEncodeStream> stream);
    std::shared_ptr<FFAVEncodeStream> choseEncodeStream();

private:
    std::atomic_bool openmuxed_{false};
    std::atomic_bool headmuxed_{false};
    std::atomic_bool trailmuxed_{false};
};
