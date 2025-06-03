#include "RTPSendingSession.hh"
#include <ze_rec_read.h>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input.264> <server_ip> [rtp_port]\n";
        return 1;
    }

    const char* inputFile = argv[1];
    std::string serverIp = argv[2];
    int rtpPort = (argc > 3) ? std::stoi(argv[3]) : 10086; // default port

    // Open .264 file using ze_mxm
    ze_mxm_read_t* reader = nullptr;
    ze_mxm_file_info_t* file_info = nullptr;
    reader = ze_mxm_open(&reader, inputFile, &file_info, 0);
    if (!reader) {
        std::cerr << "[ERROR] Failed to open file: " << inputFile << "\n";
        return 1;
    }

    // Create RTP session
    RTPSendingSession session;
    FrameInjectCallback injectCallback;

    if (!session.start(serverIp, rtpPort, injectCallback)) {
        std::cerr << "[ERROR] Failed to start RTP session\n";
        ze_mxm_close(reader);
        return 1;
    }

    // Prepare variables for reading frames
    int s_chn, s_stream_type, s_frame_type, s_frame_len = 0;
    s64 s_frame_pts = 0;
    char s_frame_buf[1024 * 1024]; // 1MB buffer
    int chn = 0; // target channel

    // Read frames and inject into RTP session
    while (ze_mxm_read_frame(reader, &s_chn, &s_stream_type, &s_frame_type,
                              &s_frame_len, &s_frame_pts, s_frame_buf) == MP4CV_OK) {
        if (s_chn != chn || s_frame_len <= 0) continue;

        if (s_frame_type != FRAME_TYPE_H264I && s_frame_type != FRAME_TYPE_H264P) {
            continue;
        }

        // Convert timestamp
        struct timeval timestamp;
        timestamp.tv_sec = s_frame_pts / 1000000;
        timestamp.tv_usec = s_frame_pts % 1000000;

        // Send frame using callback
        injectCallback(reinterpret_cast<unsigned char*>(s_frame_buf), s_frame_len, timestamp);

        // Optional: Sleep to simulate real-time streaming
        usleep(33000); // ~30 FPS
    }

    // Clean up
    ze_mxm_close(reader);
    session.stop();

    std::cout << "Streaming finished.\n";
    return 0;
}
