#include "RTPSendingSession.hh"
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <curl/curl.h>
#include <chrono>
#include <thread>
#include <random>


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
        std::cout << "[Error]" << "[RTPSendingSession] Already running!\n";
        return false;
    }

    // Validate rtpPort
    if (rtpPort < 1024 || rtpPort > 65535) {
        std::cout << "[Error]" << "[RTPSendingSession] Invalid RTP port: " << rtpPort << ". Use a port like 10000.\n";
        return false;
    }

    serverIp_ = serverIp;
    rtpPort_ = rtpPort;
    running_ = 1;

    // Initialize callback for user
    injectCallback_ = [this](unsigned char* frame, unsigned frameSize, struct timeval timestamp) {
        this->deliverFrameToSource(frame, frameSize, timestamp);
    };

    // Return this callback to user
    outCallback = injectCallback_;

    // Create thread to run event loop
    loopThread_ = std::thread(&RTPSendingSession::eventLoop, this);
    return true;
}

void RTPSendingSession::stop() {
    if (!running_) return;
    running_ = 0;

    // Request to exit the event loop
    if (env_) {
        env_->taskScheduler().triggerEvent(0, nullptr);
    }

    if (loopThread_.joinable()) {
        loopThread_.join();
    }
}

void RTPSendingSession::deliverFrameToSource(unsigned char* frame, unsigned frameSize, struct timeval timestamp) {
    std::cout << "calling deliverFrameToSource with frame size: " << frameSize << "\n";
    std::lock_guard<std::mutex> locker(deliverMutex_);
    if (customSource_) {
        customSource_->deliverFrame(frame, frameSize, timestamp);
    } 
    else {
        std::cout << "[RTPSendingSession] CustomFramedSource not initialized, cannot deliver frame\n";
    }
}

// Function to start RTP push to ZLMediaKit via HTTP API
bool RTPSendingSession::startRtpPush(const std::string& serverIp, int rtpPort, const std::string& serverUrl) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cout << "[Error]" << "[RTPSendingSession] libcurl init failed\n";
        return false;
    }

    // Configurable parameters
    std::string vhost = "__defaultVhost__";
    std::string secret = "2RY8OlPtstBt96XhkGREio2gW4haRG1E"; // Replace with your secret

    // Generate random SSRC
    std::string ssrc = "1234"; // Placeholder, replace with actual SSRC generation logic if needed

    // Construct URL with query parameters
    std::string url = serverUrl + "/index/api/openRtpServer?";
    url += "secret=" + secret;
    url += "&port=" + std::to_string(rtpPort);
    url += "&stream_id=cam01";
#ifdef USE_RTP_OVER_TCP
    url += "&enable_tcp=1";
#else
    url += "&enable_tcp=0";
#endif

    std::cout << "[RTPSendingSession] Sending API request: " << url << "\n";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);


    CURLcode res = curl_easy_perform(curl);
    bool success = true;

    if (res != CURLE_OK) {
        std::cout << "[Error]" << "[RTPSendingSession] Failed to start RTP push: "
                  << curl_easy_strerror(res) << "\n";
        success = false;
    }

    curl_easy_cleanup(curl);
    return success;
}

void RTPSendingSession::eventLoop() {
    // 1) Register stream proxy
    std::string serverUrl = "http://" + serverIp_ + ":80";

    // 2) Start RTP push via HTTP API
    // if (!startRtpPush(serverIp_, rtpPort_, serverUrl)) {
    //     std::cout << "[Error]" << "[RTPSendingSession] Failed to start RTP push, exiting event loop\n";
    //     running_ = 0;
    //     return;
    // }

    std::cout << "[RTPSendingSession] Started RTP push to " << serverUrl << "\n";

    // 3) Initialize Live555 environment
    scheduler_ = BasicTaskScheduler::createNew();

    std::cout << "[RTPSendingSession] Created BasicTaskScheduler" << (scheduler_ == nullptr ? " (failed)" : " (success)") << "\n";

    env_ = BasicUsageEnvironment::createNew(*scheduler_);

    std::cout << "[RTPSendingSession] Created BasicUsageEnvironment" << (env_ == nullptr ? " (failed)" : " (success)") << "\n";

    // 4) Create CustomFramedSource and H264VideoStreamFramer
    customSource_ = CustomFramedSource::createNew(*env_);
    if (!customSource_) {
        std::cout << "[Error]" << "[RTPSendingSession] Failed to create CustomFramedSource\n";
        goto cleanup;
    }

    std::cout << "[RTPSendingSession] Created CustomFramedSource" << (customSource_ == 0 ? " (failed)" : " (success)") << "\n";

    videoSource_ = H264VideoStreamFramer::createNew(*env_, customSource_);
    if (!videoSource_) {
        std::cout << "[Error]" << "[RTPSendingSession] Failed to create H264VideoStreamFramer\n";
        goto cleanup;
    }

    std::cout << "[RTPSendingSession] Created CustomFramedSource" << (customSource_ == 0 ? " (failed)" : " (success)") << "\n";

    // 5) Create RTP sink (TCP or UDP depending on #define)
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
    serverAddr.sin_port = htons(rtpPort_);
    if (inet_pton(AF_INET, serverIp_.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cout << "[Error]" << "[RTPSendingSession] Invalid server IP\n";
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
    sockaddr_in* dummy_in = (sockaddr_in*)&dummyAddr;
    memset(&dummyAddr, 0, sizeof(dummyAddr));
    dummy_in->sin_family = AF_INET;
    dummy_in->sin_addr.s_addr = inet_addr("127.0.0.1");
    dummy_in->sin_port = 0;

    rtpGroupsock_ = new Groupsock(*env_, dummyAddr, Port(0), 255);
    rtcpGroupsock_ = new Groupsock(*env_, dummyAddr, Port(0), 255);

    videoSink_ = H264VideoRTPSink::createNew(*env_, rtpGroupsock_, 96);
    if (!videoSink_) {
        std::cout << "[Error]" << "[RTPSendingSession] Failed to create H264VideoRTPSink\n";
        ::close(tcpSocket_);
        goto cleanup;
    }

    // Assign TCP socket (channel 0 = RTP)
    videoSink_->addStreamSocket(tcpSocket_, 0, nullptr);

    std::cout << "[RTPSendingSession] Created H264VideoRTPSink for RTP-over-TCP\n";

    const unsigned char* cname = reinterpret_cast<const unsigned char*>("emchienchillchill");

    rtcpInstance_ = RTCPInstance::createNew(*env_, rtcpGroupsock_, 5000, cname, videoSink_, nullptr, False);
    if (!rtcpInstance_) {
        std::cout << "[Error]" << "[RTPSendingSession] Failed to create RTCPInstance\n";
        ::close(tcpSocket_);
        goto cleanup;
    }

    std::cout << "[RTPSendingSession] Created RTCPInstance for RTP-over-TCP\n";
}
#else
{
    // --- RTP-over-UDP mode ---
    sockaddr_storage rtpAddr{};
    sockaddr_in* rtp_in = (sockaddr_in*)&rtpAddr;
    memset(&rtpAddr, 0, sizeof(rtpAddr));
    rtp_in->sin_family = AF_INET;
    rtp_in->sin_addr.s_addr = inet_addr(serverIp_.c_str());
    rtp_in->sin_port = htons(rtpPort_);

    sockaddr_storage rtcpAddr{};
    sockaddr_in* rtcp_in = (sockaddr_in*)&rtcpAddr;
    memset(&rtcpAddr, 0, sizeof(rtcpAddr));
    rtcp_in->sin_family = AF_INET;
    rtcp_in->sin_addr.s_addr = inet_addr(serverIp_.c_str());
    rtcp_in->sin_port = htons(rtpPort_ + 1);

    Groupsock rtpGroupsock(*env_, rtpAddr, Port(htons(rtpPort_)), 255);
    Groupsock rtcpGroupsock(*env_, rtcpAddr, Port(htons(rtpPort_ + 1)), 255);

    videoSink_ = H264VideoRTPSink::createNew(*env_, &rtpGroupsock, 96);
    if (!videoSink_) {
        std::cout << "[Error]" << "[RTPSendingSession] Failed to create H264VideoRTPSink\n";
        goto cleanup;
    }

    rtcpInstance_ = RTCPInstance::createNew(*env_, &rtcpGroupsock, 5000, nullptr, videoSink_, nullptr, False);
    if (!rtcpInstance_) {
        std::cout << "[Error]" << "[RTPSendingSession] Failed to create RTCPInstance\n";
        goto cleanup;
    }
}
#endif

    // 6) Start streaming (Live555 will pull data from CustomFramedSource)
    videoSink_->startPlaying(*videoSource_, nullptr, nullptr);
    std::cout << "[RTPSendingSession] Started streaming RTP to " << serverIp_ << "\n";

    // 7) Set up RTCP handlers (if needed, here left as default)
    rtcpInstance_->setByeHandler(nullptr, nullptr);
    rtcpInstance_->setSRHandler(nullptr, nullptr);
    rtcpInstance_->setRRHandler(nullptr, nullptr);

    // 8) Run Live555 Event Loop until running_ == false
    env_->taskScheduler().doEventLoop(&running_);

    // When reaching here: running_ == false (user called stop())
    std::cout << "[RTPSendingSession] Exiting event loop...\n";

cleanup:
    // Cleanup resources
    if (videoSink_) { Medium::close(videoSink_); videoSink_ = nullptr; }
    if (videoSource_) { Medium::close(videoSource_); videoSource_ = nullptr; }
    if (customSource_) { Medium::close(customSource_); customSource_ = nullptr; }
    if (rtcpInstance_) { Medium::close(rtcpInstance_); rtcpInstance_ = nullptr; }

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