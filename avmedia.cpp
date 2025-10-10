#include "avmedia.h"

std::shared_ptr<FFAVMedia> FFAVMedia::Create() {
    auto instance = std::shared_ptr<FFAVMedia>(new FFAVMedia());
    if (!instance->initialize())
        return nullptr;
    return instance;
}

bool FFAVMedia::initialize() {
    return true;
}

bool FFAVMedia::seekPacket(std::shared_ptr<FFAVDemuxer> demuxer) {
    const auto& uri = demuxer->GetURI();
    if (optseeks_.count(uri))
        return true;

    for (const auto& [stream_index, option] : options_[uri]) {
        if (option.seek_timestamp > 0) {
            if (!demuxer->Seek(stream_index, option.seek_timestamp)) {
                return false;
            }
        }
    }

    optseeks_.insert(uri);
    return true;
}

bool FFAVMedia::setDuration(std::shared_ptr<FFAVFormat> avformat) {
    const auto& uri = avformat->GetURI();
    if (optdurations_.count(uri))
        return true;

    for (const auto& [stream_index, option] : options_[uri]) {
        if (option.duration > 0) {
            if (stream_index < 0) {
                avformat->SetDuration(option.duration);
            } else {
                auto stream = avformat->GetStream(stream_index);
                if (!stream)
                    return false;
                stream->SetDuration(option.duration);
            }
        }
    }

    optdurations_.insert(uri);
    return true;
}

bool FFAVMedia::dropStreams() {
    for (const auto& [uri, rules] : rules_) {
        auto demuxer = GetDemuxer(uri);
        if (!demuxer)
            return false;

        for (auto index : demuxer->GetStreamIndexes()) {
            if (!rules.count(index)) {
                demuxer->DropStream(index);
            }
        }
    }
    return true;
}

std::shared_ptr<FFAVDemuxer> FFAVMedia::GetDemuxer(const std::string& uri) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return demuxers_.count(uri) ? demuxers_.at(uri) : nullptr;
}

std::shared_ptr<FFAVMuxer> FFAVMedia::GetMuxer(const std::string& uri) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return muxers_.count(uri) ? muxers_.at(uri) : nullptr;
}

void FFAVMedia::SetDebug(bool debug) {
    debug_.store(debug);
}

void FFAVMedia::DumpStreams(const std::string& uri) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto demuxer = GetDemuxer(uri);
    if (demuxer)
        demuxer->DumpStreams();

    auto muxer = GetMuxer(uri);
    if (muxer)
        muxer->DumpStreams();
}

std::shared_ptr<FFAVDemuxer> FFAVMedia::AddDemuxer(const std::string& uri) {
    auto demuxer = FFAVDemuxer::Create(uri);
    if (!demuxer)
        return nullptr;

    demuxer->SetDebug(debug_.load());
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    demuxers_[uri] = demuxer;
    return demuxer;
}

std::shared_ptr<FFAVMuxer> FFAVMedia::AddMuxer(const std::string& uri, const std::string& mux_fmt) {
    auto muxer = FFAVMuxer::Create(uri, mux_fmt);
    if (!muxer)
        return nullptr;

    muxer->SetDebug(debug_.load());
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    muxers_[uri] = muxer;
    return muxer;
}

bool FFAVMedia::DeleteFormat(const std::string& uri) {
    if (demuxers_.count(uri)) {
        demuxers_.erase(uri);
        return true;
    }

    if (muxers_.count(uri)) {
        muxers_.erase(uri);
        return true;
    }

    return false;
}

bool FFAVMedia::AddRule(const FFAVNode& src, const FFAVNode& dst) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!rules_.count(src.uri))
        rules_[src.uri] = {};
    rules_[src.uri][src.stream_index] = dst;
    return true;
}

bool FFAVMedia::SetOption(const FFAVOption& opt) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    options_[opt.uri][opt.stream_index] = opt;
    return true;
}

bool FFAVMedia::Remux() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (demuxers_.empty() || muxers_.empty() || rules_.empty())
        return false;

    if (!dropStreams())
        return false;

    std::unordered_set<std::string> endflags;
    while (endflags.size() != rules_.size()) {
        for (const auto& [uri, rules] : rules_) {
            if (endflags.count(uri))
                continue;

            auto demuxer = GetDemuxer(uri);
            if (!demuxer)
                return false;

            if (!seekPacket(demuxer))
                return false;

            if (!setDuration(demuxer))
                return false;

            auto packet = demuxer->ReadPacket();
            if (!packet) {
                if (demuxer->PacketEOF()) {
                    for (const auto& item : rules) {
                        const auto& target = item.second;
                        auto muxer = GetMuxer(target.uri);
                        if (!muxer)
                            return false;
                        if (!muxer->WritePacket(nullptr))
                            return false;
                    }
                    endflags.insert(uri);
                    continue;
                }
                return false;
            }

            const auto& target = rules.at(packet->stream_index);
            auto muxer = GetMuxer(target.uri);
            if (!muxer)
                return false;

            if (!setDuration(muxer))
                return false;

            packet->stream_index = target.stream_index;
            if (!muxer->WritePacket(packet))
                return false;
        }
    }
    return true;
}

bool FFAVMedia::Transcode() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (demuxers_.empty() || muxers_.empty() || rules_.empty())
        return false;

    if (!dropStreams())
        return false;

    std::unordered_set<std::string> endflags;
    while (endflags.size() != rules_.size()) {
        for (const auto& [uri, rules] : rules_) {
            auto demuxer = GetDemuxer(uri);
            if (!demuxer)
                return false;

            if (!seekPacket(demuxer))
                return false;

            if (!setDuration(demuxer))
                return false;

            auto [stream_index, frame] = demuxer->ReadFrame();
            if (!frame) {
                if (demuxer->FrameEOF()) {
                    for (const auto& item : rules) {
                        const auto& target = item.second;
                        auto muxer = GetMuxer(target.uri);
                        if (!muxer)
                            return false;
                        if (!muxer->WriteFrame(target.stream_index, nullptr))
                            return false;
                    }
                    endflags.insert(uri);
                    continue;
                }
                return false;
            }

            const auto& target = rules.at(stream_index);
            auto muxer = GetMuxer(target.uri);
            if (!muxer)
                return false;

            if (!setDuration(muxer))
                return false;

            if (!muxer->WriteFrame(target.stream_index, frame)) {
                return false;
            }
        }
    }

    return true;
}
