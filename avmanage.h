#pragma once

#include <iostream>
#include <map>
#include <memory>
#include "avmedia.h"

class MediaManager {
public:
    static MediaManager& GetInstance();
    std::pair<uint32_t, std::shared_ptr<FFAVMedia>> CreateMedia();
    bool DeleteMedia(uint32_t media_id);
    std::shared_ptr<FFAVMedia> GetMedia(uint32_t media_id) const;

private:
    MediaManager() = default;
    MediaManager(const MediaManager&) = delete;
    MediaManager& operator=(const MediaManager&) = delete;

private:
    uint32_t media_id_{0};
    std::map<uint32_t, std::shared_ptr<FFAVMedia>> medias_;
};
