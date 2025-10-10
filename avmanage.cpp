#include "avmanage.h"

MediaManager& MediaManager::GetInstance() {
    static MediaManager instance;
    return instance;
}

std::pair<uint32_t, std::shared_ptr<FFAVMedia>> MediaManager::CreateMedia() {
    auto media = FFAVMedia::Create();
    if (!media)
        return { 0, nullptr };

    medias_[++media_id_] = media;
    return { media_id_, media };
}

bool MediaManager::DeleteMedia(uint32_t media_id) {
    if (!medias_.count(media_id))
        return false;
    medias_.erase(media_id);
    return true;
}

std::shared_ptr<FFAVMedia> MediaManager::GetMedia(uint32_t media_id) const {
    return medias_.count(media_id) ? medias_.at(media_id) : nullptr;
}
