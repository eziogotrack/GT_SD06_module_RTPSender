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
      rtpGroupsock_(nullptr),
      rtcpGroupsock_(nullptr),
      running_(0) {}

RTPSendingSession::~RTPSendingSession() {
    stop();
}

bool RTPSendingSession::start(const std::string& serverIp,
                              int rtpPort,
                              FrameInjectCallback& outCallback) {
    if (running_) {
        std::cout << "[Error] [RTPSendingSession] Already running!\n";
        return false;
    }

    if (rtpPort < 1024 || rtpPort > 65535) {
        std::cout << "[Error] [RTPSendingSession] Invalid RTP port: " << rtpPort << "\n";
        return false;
    }

    serverIp_ = serverIp;
    rtpPort_ = rtpPort;
    running_ = 1;

    injectCallback_ = [this](unsigned char* frame, unsigned frameSize, struct timeval timestamp) {
        this->deliverFrameToSource(frame, frameSize, timestamp);
    };
    outCallback = injectCallback_;

    loopThread_ = std::thread(&RTPSendingSession::eventLoop, this);
    return true;
}

void RTPSendingSession::stop() {
    if (!running_) return;
    running_ = 0;
    if (env_) {
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
    } else {
        std::cout << "[RTPSendingSession] CustomFramedSource not initialized, cannot deliver frame\n";
    }
}

bool RTPSendingSession::startRtpPush(const std::string& serverIp, int rtpPort, const std::string& serverUrl) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cout << "[Error] [RTPSendingSession] libcurl init failed\n";
        return false;
    }
    std::string vhost = "__defaultVhost__";
    std::string secret = "2RY8OlPtstBt96XhkGREio2gW4haRG1E";
    std::string url = serverUrl + "/index/api/openRtpServer?";
    url += "secret=" + secret;
    url += "&port=" + std::to_string(rtpPort);
    url += "&stream_id=cam01";
#ifdef USE_RTP_OVER_TCP
{
    url += "&enable_tcp=1";
}
#else
{
    url += "&enable_tcp=0";
}
#endif
    std::cout << "[RTPSendingSession] Sending API request: " << url << "\n";
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    CURLcode res = curl_easy_perform(curl);
    bool success = (res == CURLE_OK);
    if (!success) {
        std::cout << "[Error] [RTPSendingSession] Failed to start RTP push: " << curl_easy_strerror(res) << "\n";
    }
    curl_easy_cleanup(curl);
    return success;
}

void RTPSendingSession::eventLoop() {
    std::string serverUrl = "http://" + serverIp_ + ":80";
    std::cout << "[RTPSendingSession] Started RTP push to " << serverUrl << "\n";

    scheduler_ = BasicTaskScheduler::createNew();
    std::cout << "[RTPSendingSession] Created BasicTaskScheduler (" << (scheduler_ ? "success" : "failed") << ")\n";
    env_ = BasicUsageEnvironment::createNew(*scheduler_);
    std::cout << "[RTPSendingSession] Created BasicUsageEnvironment (" << (env_ ? "success" : "failed") << ")\n";

    customSource_ = CustomFramedSource::createNew(*env_);
    if (!customSource_) goto cleanup;
    std::cout << "[RTPSendingSession] Created CustomFramedSource (success)\n";

    videoSource_ = H264VideoStreamFramer::createNew(*env_, customSource_);
    if (!videoSource_) goto cleanup;

#ifdef USE_RTP_OVER_TCP
{
    // TCP branch omitted for brevity
}
#else
{
    sockaddr_storage rtpAddr{}, rtcpAddr{};
    sockaddr_in* rtp_in = (sockaddr_in*)&rtpAddr;
    sockaddr_in* rtcp_in = (sockaddr_in*)&rtcpAddr;

    memset(rtp_in, 0, sizeof(sockaddr_in));
    rtp_in->sin_family = AF_INET;
    rtp_in->sin_addr.s_addr = inet_addr(serverIp_.c_str());
    rtp_in->sin_port = htons(rtpPort_);

    memset(rtcp_in, 0, sizeof(sockaddr_in));
    rtcp_in->sin_family = AF_INET;
    rtcp_in->sin_addr.s_addr = inet_addr(serverIp_.c_str());
    rtcp_in->sin_port = htons(rtpPort_ + 1);

    rtpGroupsock_ = new Groupsock(*env_, rtpAddr, Port(htons(rtpPort_)), 255);
    rtcpGroupsock_ = new Groupsock(*env_, rtcpAddr, Port(htons(rtpPort_ + 1)), 255);

    videoSink_ = H264VideoRTPSink::createNew(*env_, rtpGroupsock_, 96);
    if (!videoSink_) goto cleanup;

    const unsigned char* cname = reinterpret_cast<const unsigned char*>("live555");

    rtcpInstance_ = RTCPInstance::createNew(*env_, rtcpGroupsock_, 5000, cname, videoSink_, nullptr, False);
    if (!rtcpInstance_) goto cleanup;
}
#endif

    videoSink_->startPlaying(*videoSource_, nullptr, nullptr);
    std::cout << "[RTPSendingSession] Started streaming RTP to " << serverIp_ << "\n";

    if (rtcpInstance_) {
        rtcpInstance_->setByeHandler(nullptr, nullptr);
        rtcpInstance_->setSRHandler(nullptr, nullptr);
        rtcpInstance_->setRRHandler(nullptr, nullptr);
    }

    env_->taskScheduler().doEventLoop(&running_);
    std::cout << "[RTPSendingSession] Exiting event loop...\n";

cleanup:
    if (videoSink_) { Medium::close(videoSink_); videoSink_ = nullptr; }
    if (videoSource_) { Medium::close(videoSource_); videoSource_ = nullptr; }
    if (customSource_) { Medium::close(customSource_); customSource_ = nullptr; }
    if (rtcpInstance_) { Medium::close(rtcpInstance_); rtcpInstance_ = nullptr; }
    if (rtpGroupsock_) { delete rtpGroupsock_; rtpGroupsock_ = nullptr; }
    if (rtcpGroupsock_) { delete rtcpGroupsock_; rtcpGroupsock_ = nullptr; }
    if (env_) { env_->reclaim(); env_ = nullptr; }
    if (scheduler_) { delete scheduler_; scheduler_ = nullptr; }
}
