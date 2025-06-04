// main.cpp
#include "RTPSendingSession.hh"
#include "CustomFramedSource.hh"
#include "ze_rec_read.h"
#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <vector>

#define MAX_CHANNELS 4
#define FPS_SIMULATION 25

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <input.264> <server_ip> <base_rtp_port>\n";
        return 1;
    }

    std::string inputFile = argv[1];
    std::string serverIp = argv[2];
    int basePort = std::stoi(argv[3]);

    // Initialize RTP sessions and sources for each channel
    std::vector<RTPSendingSession> sessions(MAX_CHANNELS);
    std::vector<CustomFramedSource> sources(MAX_CHANNELS);

    for (int chn = 0; chn < MAX_CHANNELS; ++chn) {
        std::string streamId = "cam0" + std::to_string(chn);
        int rtpPort = basePort + chn * 2;

        if (!sessions[chn].start(serverIp, rtpPort, streamId)) {
            std::cerr << "[ERROR] Failed to start RTP session for channel " << chn << "\n";
            return 1;
        }

        sources[chn].setCallback([&sessions, chn](const uint8_t* data, int len, int64_t pts,
                                                 int channel, int stream_type, int frame_type) {
            sessions[chn].sendFrame(data, len, pts);
        });
    }

    // Open input file
    ze_mxm_read_t* reader = nullptr;
    ze_mxm_file_info_t* fileInfo = nullptr;
    ze_mxm_read_open(&reader, inputFile.c_str(), &fileInfo, 0);
    if (!reader) {
        std::cerr << "[ERROR] Failed to open file: " << inputFile << "\n";
        return 1;
    }

    // Read and dispatch frames
    int s_chn, s_stream_type, s_frame_type, s_frame_len = 0;
    s64 s_frame_pts = 0;
    char s_frame_buf[1024 * 1024];

    useconds_t sleep_duration = 1000000 / FPS_SIMULATION / MAX_CHANNELS;

    while (ze_mxm_read_frame(reader, &s_chn, &s_stream_type, &s_frame_type,
                              &s_frame_len, &s_frame_pts, s_frame_buf) == 0) {
        if (s_chn < 0 || s_chn >= MAX_CHANNELS || s_frame_len <= 0) continue;
        if (s_frame_type != FRAME_TYPE_H264I && s_frame_type != FRAME_TYPE_H264P) continue;

        // std::cout << "[chn " << s_chn << "] Frame: type="
        //           << (s_frame_type == FRAME_TYPE_H264I ? "I" : "P")
        //           << ", len=" << s_frame_len
        //           << ", pts=" << std::fixed << std::setprecision(3)
        //           << (s_frame_pts / 1000000.0) << "s\n";

        sources[s_chn].pushFrame(reinterpret_cast<uint8_t*>(s_frame_buf), s_frame_len, s_frame_pts,
                                 s_chn, s_stream_type, s_frame_type);
                                 
        // simulate frame rate: FPS_SIMULATION
        usleep(sleep_duration);
    }

    ze_mxm_read_close(reader);

    for (auto& session : sessions) {
        session.stop();
    }

    std::cout << "All streams finished.\n";
    return 0;
}
