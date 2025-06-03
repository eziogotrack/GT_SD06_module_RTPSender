#include "RTPSendingSession.hh"
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <iostream>

RTPSendingSession::RTPSendingSession()
    : scheduler_(nullptr),
      env_(nullptr),
      customSource_(nullptr),
      videoSource_(nullptr),
      videoSink_(nullptr),
      rtcpInstance_(nullptr),
      tcpSocket_(-1),
      running_(0) {}

RTPSendingSession::~RTPSendingSession() {
    stop();
}

bool RTPSendingSession::start(const std::string& serverIp,
                              int rtpPort,
                              FrameInjectCallback& outCallback) {
    if (running_) {
        std::cerr << "[RTPSendingSession] Already running!\n";
        return false;
    }

    serverIp_ = serverIp;
    rtpPort_ = rtpPort;
    running_ = 1;

    // Initialize callback for user: just call sessionInject(frame, size, ts)
    injectCallback_ = [this](unsigned char* frame, unsigned frameSize, struct timeval timestamp) {
        this->deliverFrameToSource(frame, frameSize, timestamp);
    };

    // Return this callback to user
    outCallback = injectCallback_;

    // Create thread to run event loop (Live555 + RTP)
    loopThread_ = std::thread(&RTPSendingSession::eventLoop, this);
    return true;
}

void RTPSendingSession::stop() {
    if (!running_) return;
    running_ = 0;

    // Request Live555 to exit the event loop
    if (env_) {
        // Call trigger to exit doEventLoop faster
        env_->taskScheduler().triggerEvent(0, nullptr);
    }

    if (loopThread_.joinable()) {
        loopThread_.join();
    }
}

void RTPSendingSession::deliverFrameToSource(unsigned char* frame, unsigned frameSize, struct timeval timestamp) {
    std::lock_guard<std::mutex> locker(deliverMutex_);
    if (customSource_) {
        customSource_->deliverFrame(frame, frameSize, timestamp);
    }
}

std::string RTPSendingSession::generateSDP(const std::string& ip, int port) {
    char sdpBuf[1024];

#ifdef USE_RTP_OVER_TCP
    snprintf(sdpBuf, sizeof(sdpBuf),
             "v=0\r\n"
             "o=- 0 0 IN IP4 127.0.0.1\r\n"
             "s=Live Stream\r\n"
             "c=IN IP4 %s\r\n"
             "t=0 0\r\n"
             "m=video %d RTP/AVP/TCP 96\r\n"
             "a=rtpmap:96 H264/90000\r\n"
             "a=control:trackID=0\r\n"
             "a=rtsp-interleaved:0-1\r\n",
             ip.c_str(), port);
#else
    snprintf(sdpBuf, sizeof(sdpBuf),
             "v=0\r\n"
             "o=- 0 0 IN IP4 127.0.0.1\r\n"
             "s=Live Stream\r\n"
             "c=IN IP4 %s\r\n"
             "t=0 0\r\n"
             "m=video %d RTP/AVP 96\r\n"
             "a=rtpmap:96 H264/90000\r\n"
             "a=control:trackID=0\r\n",
             ip.c_str(), port);
#endif

    return std::string(sdpBuf);
}

void RTPSendingSession::sendSdpToZLMediaKit(const std::string& sdp, const std::string& serverUrl) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[RTPSendingSession] libcurl init failed\n";
        return;
    }

    std::string url = serverUrl + "/index/api/openRtpServer";
    std::string postData = "sdp=" + sdp;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "[RTPSendingSession] Failed to send SDP: "
                  << curl_easy_strerror(res) << "\n";
    } else {
        std::cout << "[RTPSendingSession] SDP sent successfully\n";
    }

    curl_easy_cleanup(curl);
}

void RTPSendingSession::eventLoop() {
    // 1) Create SDP and send to ZLMediaKit
    std::string sdpContent = generateSDP(serverIp_, rtpPort_);
    std::string serverUrl = "http://" + serverIp_ + ":80";
    sendSdpToZLMediaKit(sdpContent, serverUrl);

    // 2) Initialize Live555 environment
    scheduler_ = BasicTaskScheduler::createNew();
    env_       = BasicUsageEnvironment::createNew(*scheduler_);

    // 3) Create CustomFramedSource and H264VideoStreamFramer
    customSource_ = CustomFramedSource::createNew(*env_);
    if (!customSource_) {
        std::cerr << "[RTPSendingSession] Failed to create CustomFramedSource\n";
        goto cleanup;
    }

    videoSource_ = H264VideoStreamFramer::createNew(*env_, customSource_);
    if (!videoSource_) {
        std::cerr << "[RTPSendingSession] Failed to create H264VideoStreamFramer\n";
        goto cleanup;
    }

    // 4) Create RTP sink (TCP or UDP depending on #define)
#ifdef USE_RTP_OVER_TCP
{
    // --- RTP-over-TCP mode ---
    tcpSocket_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSocket_ < 0) {
        perror("[RTPSendingSession] socket");
        goto cleanup;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port   = htons(rtpPort_);
    if (inet_pton(AF_INET, serverIp_.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "[RTPSendingSession] Invalid server IP\n";
        ::close(tcpSocket_);
        goto cleanup;
    }

    if (::connect(tcpSocket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("[RTPSendingSession] connect");
        ::close(tcpSocket_);
        goto cleanup;
    }
    std::cout << "[RTPSendingSession] Connected to TCP " << serverIp_ << ":" << rtpPort_ << "\n";

    // Create dummy Groupsock to initialize sink
    sockaddr_storage dummyAddr{};
    sockaddr_in*       dummy_in = (sockaddr_in*)&dummyAddr;
    memset(&dummyAddr, 0, sizeof(dummyAddr));
    dummy_in->sin_family = AF_INET;
    dummy_in->sin_addr.s_addr = inet_addr("127.0.0.1");
    dummy_in->sin_port = 0;

    Groupsock  rtpGroupsock(*env_, dummyAddr, Port(0), 255);
    Groupsock  rtcpGroupsock(*env_, dummyAddr, Port(0), 255);

    videoSink_ = H264VideoRTPSink::createNew(*env_, &rtpGroupsock, 96);
    if (!videoSink_) {
        std::cerr << "[RTPSendingSession] Failed to create H264VideoRTPSink\n";
        ::close(tcpSocket_);
        goto cleanup;
    }

    // Assign TCP socket (channel 0 = RTP)
    videoSink_->addStreamSocket(tcpSocket_, 0, nullptr);

    rtcpInstance_ = RTCPInstance::createNew(*env_, &rtcpGroupsock, 5000, nullptr, videoSink_, nullptr, False);
    if (!rtcpInstance_) {
        std::cerr << "[RTPSendingSession] Failed to create RTCPInstance\n";
        ::close(tcpSocket_);
        goto cleanup;
    }
} 
#else 
{
    // --- RTP-over-UDP mode ---
    struct in_addr serverAddrIn{};
    serverAddrIn.s_addr = inet_addr(serverIp_.c_str());

    Groupsock rtpGroupsock(*env_, serverAddrIn, Port(htons(rtpPort_)), 255);
    Groupsock rtcpGroupsock(*env_, serverAddrIn, Port(htons(rtpPort_ + 1)), 255);

    videoSink_ = H264VideoRTPSink::createNew(*env_, &rtpGroupsock, 96);
    if (!videoSink_) {
        std::cerr << "[RTPSendingSession] Failed to create H264VideoRTPSink\n";
        goto cleanup;
    }

    rtcpInstance_ = RTCPInstance::createNew(*env_, &rtcpGroupsock, 5000, nullptr, videoSink_, nullptr, False);
    if (!rtcpInstance_) {
        std::cerr << "[RTPSendingSession] Failed to create RTCPInstance\n";
        goto cleanup;
    }
}
#endif

    // 5) Start streaming (Live555 will pull data from CustomFramedSource)
    videoSink_->startPlaying(*videoSource_, nullptr, nullptr);
    std::cout << "[RTPSendingSession] Started streaming RTP to " << serverIp_ << "\n";

    // 6) Set up RTCP handlers (if needed, here left as default)
    rtcpInstance_->setByeHandler(nullptr, nullptr);
    rtcpInstance_->setSRHandler(nullptr, nullptr);
    rtcpInstance_->setRRHandler(nullptr, nullptr);

    // 7) Run Live555 Event Loop until running_ == false
    env_->taskScheduler().doEventLoop(&running_);

    // When reaching here: running_ == false (user called stop())
    std::cout << "[RTPSendingSession] Exiting event loop...\n";

cleanup:
    // Cleanup resources
    if (videoSink_)      { Medium::close(videoSink_);      videoSink_ = nullptr; }
    if (videoSource_)    { Medium::close(videoSource_);    videoSource_ = nullptr; }
    if (customSource_)   { Medium::close(customSource_);   customSource_ = nullptr; }
    if (rtcpInstance_)   { Medium::close(rtcpInstance_);   rtcpInstance_ = nullptr; }

#ifdef USE_RTP_OVER_TCP
    if (tcpSocket_ >= 0) {
        ::close(tcpSocket_);
        tcpSocket_ = -1;
    }
#endif

    if (env_) {
        env_->reclaim();
        env_ = nullptr;
    }
    if (scheduler_) {
        delete scheduler_;
        scheduler_ = nullptr;
    }
}
