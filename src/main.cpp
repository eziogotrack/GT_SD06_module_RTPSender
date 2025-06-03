#include "RTPSendingSession.hh"
#include <chrono>
#include <thread>

// Suppose you have your own H264 frame source (e.g., an encoding loop from a camera)
// When a new frame is available, just call the callback that was returned.

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: %s <server_ip> <rtp_port>\n", argv[0]);
        return 1;
    }

    const char* serverIp = argv[1];
    int rtpPort = atoi(argv[2]);

    // 1) Create session
    RTPSendingSession session;

    // 2) Prepare a variable to receive the callback
    RTPSendingSession::FrameInjectCallback pushFrameCb;

    // 3) Call start(), and get pushFrameCb
    if (!session.start(serverIp, rtpPort, pushFrameCb)) {
        printf("Failed to start RTPSendingSession\n");
        return 1;
    }

    // 4) Simulate "reading" or "creating" an H264 frame and push it to the callback
    //    Here, we just wait 1s and push a dummy frame for illustration
    for (int i = 0; i < 100 && true; ++i) {
        // In practice, you would get data from the encoder, for example:
        // unsigned char* data = ...; unsigned size = ...; struct timeval ts = ...;
        // pushFrameCb(data, size, ts);
        std::this_thread::sleep_for(std::chrono::milliseconds(40));

        // Simulate: a dummy frame (NOT VIEWABLE)
        // Just to illustrate how to call the callback
        const char* dummy = "\x00\x00\x00\x01\x65..."; // H264 NALU data (sample only)
        unsigned dummySize = 5; 
        struct timeval tv;
        gettimeofday(&tv, nullptr);

        pushFrameCb((unsigned char*)dummy, dummySize, tv);
    }

    // 5) When you want to stop:
    session.stop();
    printf("Session stopped.\n");
    return 0;
}
