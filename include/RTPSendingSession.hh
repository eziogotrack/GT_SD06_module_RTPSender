#ifndef RTP_SENDING_SESSION_HH
#define RTP_SENDING_SESSION_HH

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include "CustomFramedSource.hh"

//
// If you want to switch to UDP, just comment the line below
//
#define USE_RTP_OVER_TCP

class RTPSendingSession {
public:
    // Callback type that the user will call to push a frame in
    // - frame: pointer to H264 data
    // - frameSize: size of the frame
    // - timestamp: presentation time
    using FrameInjectCallback = std::function<void(unsigned char* frame, unsigned frameSize, struct timeval timestamp)>;

    RTPSendingSession();
    ~RTPSendingSession();

    /**
     * Start the session:
     * @param serverIp    : IP or hostname of the media server (ZLMediaKit)
     * @param rtpPort     : RTP port (RTCP default = rtpPort+1)
     * @param outCallback : reference to a std::function provided by the user,
     *                      the class will assign a lambda to outCallback for the user to call to push frames.
     * @return true if started successfully, false if there was an error.
     */
    bool start(const std::string& serverIp,
               int rtpPort,
               FrameInjectCallback& outCallback);

    /**
     * Stop the session (pause streaming, close socket, etc.).
     */
    void stop();

private:
    // Function running in a separate thread, contains all Live555 logic + SDP sending + event loop
    void eventLoop();

    // Generate SDP content (uses #ifdef USE_RTP_OVER_TCP)
    std::string generateSDP(const std::string& ip, int port);

    // Send SDP to ZLMediaKit (HTTP POST)
    void sendSdpToZLMediaKit(const std::string& sdp, const std::string& serverUrl);

    // Internal function to deliver a frame to CustomFramedSource
    void deliverFrameToSource(unsigned char* frame, unsigned frameSize, struct timeval timestamp);

private:
    std::string                   serverIp_;
    int                           rtpPort_;
    std::thread                   loopThread_;
    EventLoopWatchVariable        running_;

    // Live555 objects
    TaskScheduler*                scheduler_;
    UsageEnvironment*             env_;
    CustomFramedSource*           customSource_;
    H264VideoStreamFramer*        videoSource_;
    H264VideoRTPSink*             videoSink_;
    RTCPInstance*                 rtcpInstance_;

    // TCP socket (if using RTP-over-TCP)
    int                           tcpSocket_;

    // Callback for the user to call (this will be a lambda assigned in start())
    FrameInjectCallback           injectCallback_;

    // Mutex to protect deliverFrame (in case of multithreading)
    std::mutex                    deliverMutex_;
};

#endif // RTP_SENDING_SESSION_HH
