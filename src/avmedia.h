#pragma once

#include <cstdio>
#include <atomic>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>
#include "avutil.h"
#include "avformat.h"

struct FFAVNode {
    std::string uri;
    int stream_index;
};

struct FFAVOption : public FFAVNode {
    double seek_timestamp;
    double duration;
};

class FFAVMedia {
    using FFAVDemuxerMap = std::unordered_map<std::string, std::shared_ptr<FFAVDemuxer>>;
    using FFAVMuxerMap = std::unordered_map<std::string, std::shared_ptr<FFAVMuxer>>;
    using FFAVRuleMap = std::unordered_map<std::string, std::unordered_map<int, FFAVNode>>;
    using FFAVOptionMap = std::unordered_map<std::string, std::unordered_map<int, FFAVOption>>;

public:
    static std::shared_ptr<FFAVMedia> Create();
    std::shared_ptr<FFAVDemuxer> GetDemuxer(const std::string& uri) const;
    std::shared_ptr<FFAVMuxer> GetMuxer(const std::string& uri) const;
    void SetDebug(bool debug);
    void DumpStreams(const std::string& uri) const;
    std::shared_ptr<FFAVDemuxer> AddDemuxer(const std::string& uri);
    std::shared_ptr<FFAVMuxer> AddMuxer(const std::string& uri, const std::string& mux_fmt);
    bool DeleteFormat(const std::string& uri);
    bool AddRule(const FFAVNode& src, const FFAVNode& dst);
    bool SetOption(const FFAVOption& opt);
    bool Remux();
    bool Transcode();

private:
    FFAVMedia() = default;
    bool initialize();
    bool seekPacket(std::shared_ptr<FFAVDemuxer> demuxer);
    bool setDuration(std::shared_ptr<FFAVFormat> avformat);
    bool dropStreams();

private:
    mutable std::recursive_mutex mutex_;
    std::atomic_bool debug_{false};
    FFAVDemuxerMap demuxers_;
    FFAVMuxerMap muxers_;
    FFAVRuleMap rules_;
    FFAVOptionMap options_;
    std::unordered_set<std::string> optseeks_;
    std::unordered_set<std::string> optdurations_;
};
