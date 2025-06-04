// main.cpp
#include "RTPSendingSession.hh"
#include "CustomFramedSource.hh"
#include "ze_rec_read.h"
#include <iostream>
#include <iomanip>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <input.264> <server_ip> <rtp_port>\n";
        return 1;
    }

    std::string inputFile = argv[1];
    std::string serverIp = argv[2];
    int rtpPort = std::stoi(argv[3]);
    std::string streamId = "cam01";

    // Initialize RTP session
    RTPSendingSession session;
    if (!session.start(serverIp, rtpPort, streamId)) {
        std::cerr << "[ERROR] Failed to start RTP session\n";
        return 1;
    }

    // Initialize frame source and bind callback to session
    CustomFramedSource source;
    source.setCallback([&](const uint8_t* data, int len, int64_t pts,
                           int chn, int stream_type, int frame_type) {
        session.sendFrame(data, len, pts);
    });

    // Read frames from file
    ze_mxm_read_t* reader = nullptr;
    ze_mxm_file_info_t* fileInfo = nullptr;
    ze_mxm_read_open(&reader, inputFile.c_str(), &fileInfo, 0);
    if (!reader) {
        std::cerr << "[ERROR] Failed to open file: " << inputFile << "\n";
        return 1;
    }

    int chn = 0;
    int s_chn, s_stream_type, s_frame_type, s_frame_len = 0;
    s64 s_frame_pts = 0;
    char s_frame_buf[1024 * 1024];

    while (ze_mxm_read_frame(reader, &s_chn, &s_stream_type, &s_frame_type,
                              &s_frame_len, &s_frame_pts, s_frame_buf) == 0) {
        if (s_chn != chn || s_frame_len <= 0) continue;
        if (s_frame_type != FRAME_TYPE_H264I && s_frame_type != FRAME_TYPE_H264P) continue;

        std::cout << "Read frame: chn=" << s_chn
                  << ", type=" << (s_frame_type == FRAME_TYPE_H264I ? "I" : "P")
                  << ", len=" << s_frame_len
                  << ", pts=" << std::fixed << std::setprecision(3)
                  << (s_frame_pts / 1000000.0) << "s\n";

        source.pushFrame(reinterpret_cast<uint8_t*>(s_frame_buf), s_frame_len, s_frame_pts,
                         s_chn, s_stream_type, s_frame_type);

        usleep(40000); // simulate 25fps
    }

    ze_mxm_read_close(reader);
    session.stop();
    std::cout << "Streaming finished.\n";
    return 0;
}
