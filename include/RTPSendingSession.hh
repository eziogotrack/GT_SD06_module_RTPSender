// RTPSendingSession.hh
#ifndef RTP_SENDING_SESSION_HH
#define RTP_SENDING_SESSION_HH

#include <string>
#include <netinet/in.h>
#include <sys/time.h>
#include <mutex>


// ZLMediaKit secret key for authentication
#define ZLMEDIAKIT_SRT_KEY "2RY8OlPtstBt96XhkGREio2gW4haRG1E"

// Use RTP over TCP if set to 1, otherwise use UDP
#define RTP_OVER_TCP 1

/**
 * RTPSendingSession is responsible for sending H264 RTP packets (and RTCP)
 * to a target server via UDP or TCP. It performs RTP header packing, NALU fragmentation (FU-A),
 * and RTCP Sender Reports (for UDP).
 */
class RTPSendingSession {
public:
    RTPSendingSession();
    ~RTPSendingSession();

    /**
     * Start the RTP session.
     * @param ip        Target server IP
     * @param rtpPort   Port to send RTP to (RTCP will use rtpPort+1 if UDP)
     * @param streamId  ZLMediaKit stream ID (for openRtpServer API)
     */
    bool start(const std::string& ip, int rtpPort, const std::string& streamId);

    /**
     * Stop and cleanup the session.
     */
    void stop();

    /**
     * Push a raw H264 frame to be packetized and sent.
     * @param data         Pointer to raw H264 frame (one or more NALUs with start codes)
     * @param len          Length of the frame in bytes
     * @param pts          Presentation timestamp (microseconds)
     */
    void sendFrame(const uint8_t* data, int len, int64_t pts);

private:
    bool createSockets();
    void sendRtpPacket(const uint8_t* payload, size_t size, bool marker, uint32_t timestamp);
    void sendRtcpSr(uint32_t timestamp);
    void packetizeAndSendNalu(const uint8_t* nalu, int size, uint32_t timestamp);
    void parseAndSendFrame(const uint8_t* data, int len, int64_t pts);

private:
    int rtpSock_;
    int rtcpSock_;
    int tcpSock_;
    sockaddr_in rtpAddr_;
    sockaddr_in rtcpAddr_;
    std::string serverIp_;
    int rtpPort_;
    std::string streamId_;

    uint16_t rtpSeq_ = 0;
    uint32_t rtpSsrc_;
    int64_t basePts_ = -1;
    uint32_t pktCount_ = 0;
    uint32_t octetCount_ = 0;
    int64_t lastRtcpPts_ = 0;

    std::mutex sendMutex_;
};

#endif // RTP_SENDING_SESSION_HH
