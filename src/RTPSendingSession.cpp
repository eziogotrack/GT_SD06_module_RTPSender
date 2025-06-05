// RTPSendingSession.cpp
#include "RTPSendingSession.hh"
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
#include <random>
#include <curl/curl.h>
#include <sys/socket.h>
#include <sys/time.h>

#define RTP_VERSION 2
#define RTP_PAYLOAD_TYPE_H264 98
#define RTP_MTU 1400

#pragma pack(push, 1)
struct RtpHeader {
    uint8_t vpxcc;
    uint8_t mpt;
    uint16_t seq;
    uint32_t timestamp;
    uint32_t ssrc;
};

struct RtcpHeader {
    uint8_t version_p_count;
    uint8_t packet_type;
    uint16_t length;
    uint32_t ssrc;
    uint32_t ntp_sec;
    uint32_t ntp_frac;
    uint32_t rtp_timestamp;
    uint32_t packet_count;
    uint32_t octet_count;
};
#pragma pack(pop)

RTPSendingSession::RTPSendingSession() : tcpSock_(-1) {}

RTPSendingSession::~RTPSendingSession() {
    stop();
}

bool RTPSendingSession::start(const std::string& ip, int port, const std::string& streamId) {
    serverIp_ = ip;
    rtpPort_ = port;
    streamId_ = streamId;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(1, 0xFFFFFFFF);
    rtpSsrc_ = dist(gen);

    // std::ostringstream url;
    // url << "http://" << ip << ":80/index/api/openRtpServer?"
    //     << "secret=dxIISf0mTqoCIyqVPPDpfGEfx3nRKaeZ"
    //     << "&port=" << port
    //     << "&enable_tcp=1"
    //     << "&stream_id=" << streamId;

    // CURL* curl = curl_easy_init();
    // if (!curl) return false;

    // curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
    // curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    // curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    // CURLcode res = curl_easy_perform(curl);
    // curl_easy_cleanup(curl);

    // if (res != CURLE_OK) return false;

    return createSockets();
}

void RTPSendingSession::stop() {
    if (tcpSock_ >= 0) {
        close(tcpSock_);
        tcpSock_ = -1;
    }
}

bool RTPSendingSession::createSockets() {
    std::cout << "Using RTP/RTCP over TCP interleaved\n";
    tcpSock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSock_ < 0) return false;

    sockaddr_in servAddr{};
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(rtpPort_);
    inet_pton(AF_INET, serverIp_.c_str(), &servAddr.sin_addr);

    if (connect(tcpSock_, (sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
        close(tcpSock_);
        tcpSock_ = -1;
        return false;
    }

    return true;
}

void RTPSendingSession::sendFrame(const uint8_t* data, int len, int64_t pts) {
    std::lock_guard<std::mutex> lock(sendMutex_);
    if (basePts_ < 0) basePts_ = pts;

    int64_t delta = pts - basePts_;
    uint32_t timestamp = static_cast<uint32_t>((delta / 1000000.0) * 90000);
    parseAndSendFrame(data, len, pts);

    if ((pts - lastRtcpPts_) >= 1000000) {
        sendRtcpSr(timestamp);
        lastRtcpPts_ = pts;
    }
}

void RTPSendingSession::sendRtpPacket(const uint8_t* payload, size_t size, bool marker, uint32_t timestamp) {
    RtpHeader header;
    header.vpxcc = (RTP_VERSION << 6);
    header.mpt = (marker ? 0x80 : 0x00) | (RTP_PAYLOAD_TYPE_H264 & 0x7F);
    header.seq = htons(rtpSeq_++);
    header.timestamp = htonl(timestamp);
    header.ssrc = htonl(rtpSsrc_);

    std::vector<uint8_t> packet(sizeof(RtpHeader) + size);
    memcpy(packet.data(), &header, sizeof(header));
    memcpy(packet.data() + sizeof(header), payload, size);

    uint8_t channel = 0;
    uint16_t pktLen = htons(packet.size());
    std::vector<uint8_t> tcpFrame(4 + packet.size());
    tcpFrame[0] = '$';
    tcpFrame[1] = channel;
    memcpy(&tcpFrame[2], &pktLen, 2);
    memcpy(&tcpFrame[4], packet.data(), packet.size());

    send(tcpSock_, tcpFrame.data(), tcpFrame.size(), 0);

    pktCount_++;
    octetCount_ += size;
}

void RTPSendingSession::sendRtcpSr(uint32_t timestamp) {
    struct timeval now;
    gettimeofday(&now, nullptr);

    RtcpHeader sr;
    sr.version_p_count = (RTP_VERSION << 6);
    sr.packet_type = 200;
    sr.length = htons(6);
    sr.ssrc = htonl(rtpSsrc_);
    sr.ntp_sec = htonl(now.tv_sec + 2208988800U);
    sr.ntp_frac = htonl((uint32_t)(((uint64_t)now.tv_usec << 32) / 1000000));
    sr.rtp_timestamp = htonl(timestamp);
    sr.packet_count = htonl(pktCount_);
    sr.octet_count = htonl(octetCount_);

    std::vector<uint8_t> tcpFrame(4 + sizeof(RtcpHeader));
    tcpFrame[0] = '$';
    tcpFrame[1] = 1; // channel 1 for RTCP
    uint16_t len = htons(sizeof(RtcpHeader));
    memcpy(&tcpFrame[2], &len, 2);
    memcpy(&tcpFrame[4], &sr, sizeof(RtcpHeader));

    send(tcpSock_, tcpFrame.data(), tcpFrame.size(), 0);
}

void RTPSendingSession::packetizeAndSendNalu(const uint8_t* nalu, int size, uint32_t timestamp) {
    const int maxPayload = RTP_MTU;
    uint8_t nalHeader = nalu[0];
    uint8_t nalType = nalHeader & 0x1F;

    if (size <= maxPayload) {
        sendRtpPacket(nalu, size, true, timestamp);
        return;
    }

    uint8_t fuIndicator = (nalHeader & 0xE0) | 28;
    uint8_t fuHeaderStart = 0x80 | nalType;
    uint8_t fuHeaderMid = nalType;
    uint8_t fuHeaderEnd = 0x40 | nalType;

    int pos = 1;
    while (pos < size) {
        int remaining = size - pos;
        int payloadLen = std::min(remaining, maxPayload - 2);
        uint8_t fuHeader = (pos == 1) ? fuHeaderStart : ((remaining - payloadLen == 0) ? fuHeaderEnd : fuHeaderMid);

        std::vector<uint8_t> buf(2 + payloadLen);
        buf[0] = fuIndicator;
        buf[1] = fuHeader;
        memcpy(buf.data() + 2, nalu + pos, payloadLen);

        sendRtpPacket(buf.data(), buf.size(), (fuHeader & 0x40) != 0, timestamp);
        pos += payloadLen;
    }
}

void RTPSendingSession::parseAndSendFrame(const uint8_t* data, int len, int64_t pts) {
    int pos = 0;
    while (pos + 4 < len) {
        if (data[pos] == 0x00 && data[pos + 1] == 0x00 && data[pos + 2] == 0x00 && data[pos + 3] == 0x01) {
            int start = pos + 4;
            int end = len;
            for (int i = start; i + 4 < len; ++i) {
                if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x00 && data[i + 3] == 0x01) {
                    end = i;
                    break;
                }
            }
            packetizeAndSendNalu(data + start, end - start, static_cast<uint32_t>((pts - basePts_) / 1000000.0 * 90000));
            pos = end;
        } else {
            ++pos;
        }
    }
}
