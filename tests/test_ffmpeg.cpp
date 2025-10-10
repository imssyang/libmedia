#include "test_ffmpeg.h"
#include "../avcodec.h"

void test_demuxer(const std::string& uri) {
    uint32_t media_id = CreateMedia();

    assert(AddDemuxer(media_id, uri.c_str()));

    while (true) {
        auto packet = ReadPacket(media_id, uri.c_str());
        if (!packet)
            break;

        if (packet->dts > 10240)
            break;

        std::cout << DumpAVPacket(packet) << std::endl;
    }

    DeleteFormat(media_id, uri.c_str());
    DeleteMedia(media_id);
}
