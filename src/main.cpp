#include <iostream>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <iomanip>
#include <sstream>
#include <vector>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <sys/time.h>
#include "ze_rec_read.h"

#define RTP_VERSION 2
#define RTP_PAYLOAD_TYPE_H264 98
#define RTP_MTU 1400

struct RtpHeader {
    uint8_t vpxcc;
    uint8_t mpt;
    uint16_t seq;
    uint32_t timestamp;
    uint32_t ssrc;
} __attribute__((packed));

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
} __attribute__((packed));

static uint16_t rtp_seq = 0;
static uint32_t rtp_ssrc = 0x12345678;
static int64_t base_pts = -1;

void send_open_rtpserver_to_zlm(const std::string& ip, int port, const std::string& stream_id) {
    std::ostringstream url;
    url << "http://" << ip << ":80/index/api/openRtpServer?";
    url << "secret=2RY8OlPtstBt96XhkGREio2gW4haRG1E";
    url << "&port=" << port;
    url << "&enable_tcp=0";
    url << "&stream_id=" << stream_id;

    std::cout << "[DEBUG] Sending request to: " << url.str() << "\n";

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[ERROR] Failed to init libcurl\n";
        return;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (res != CURLE_OK || http_code != 200) {
        std::cerr << "[ERROR] Failed to call openRtpServer: " << curl_easy_strerror(res)
                  << ", HTTP status: " << http_code << "\n";
    } else {
        std::cout << "[INFO] Sent openRtpServer API to ZLMediaKit successfully\n";
    }

    curl_easy_cleanup(curl);
}

int create_udp_socket(const std::string& ip, int port, sockaddr_in& out_addr) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    memset(&out_addr, 0, sizeof(out_addr));
    out_addr.sin_family = AF_INET;
    out_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &out_addr.sin_addr);
    return sock;
}

void send_rtp_packet_udp(int sock, sockaddr_in& addr, const uint8_t* payload, size_t payload_size, bool marker, uint32_t timestamp) {
    RtpHeader header;
    header.vpxcc = (RTP_VERSION << 6);
    header.mpt = (marker ? 0x80 : 0x00) | (RTP_PAYLOAD_TYPE_H264 & 0x7F);
    header.seq = htons(rtp_seq++);
    header.timestamp = htonl(timestamp);
    header.ssrc = htonl(rtp_ssrc);

    std::vector<uint8_t> packet(sizeof(RtpHeader) + payload_size);
    memcpy(packet.data(), &header, sizeof(RtpHeader));
    memcpy(packet.data() + sizeof(RtpHeader), payload, payload_size);

    std::cout << "[DEBUG] Sending RTP packet: seq=" << ntohs(header.seq)
              << ", timestamp=" << ntohl(header.timestamp)
              << ", size=" << packet.size() << " bytes\n";

    sendto(sock, packet.data(), packet.size(), 0, (sockaddr*)&addr, sizeof(addr));
}

void send_rtcp_sr(int sock, sockaddr_in& addr, uint32_t timestamp, uint32_t pkt_count, uint32_t octet_count) {
    struct timeval now;
    gettimeofday(&now, nullptr);

    RtcpHeader sr;
    sr.version_p_count = (RTP_VERSION << 6);
    sr.packet_type = 200;  // SR
    sr.length = htons(6);
    sr.ssrc = htonl(rtp_ssrc);
    sr.ntp_sec = htonl(now.tv_sec + 2208988800U);
    sr.ntp_frac = htonl((uint32_t)(((uint64_t)now.tv_usec << 32) / 1000000));
    sr.rtp_timestamp = htonl(timestamp);
    sr.packet_count = htonl(pkt_count);
    sr.octet_count = htonl(octet_count);

    sendto(sock, &sr, sizeof(sr), 0, (sockaddr*)&addr, sizeof(addr));
}

void packetize_and_send_nalu_udp(int sock, sockaddr_in& addr, const uint8_t* nalu, int size, uint32_t timestamp, uint32_t& pkt_count, uint32_t& octet_count) {
    const int max_payload = RTP_MTU;
    uint8_t nal_header = nalu[0];
    uint8_t nal_type = nal_header & 0x1F;

    if (nal_type == 7) std::cout << "  → Found SPS\n";
    if (nal_type == 8) std::cout << "  → Found PPS\n";
    if (nal_type == 5) std::cout << "  → Found IDR (I-frame)\n";

    if (size <= max_payload) {
        std::cout << "  NALU found: type=" << (int)nal_type << ", size=" << size << " bytes\n";
        send_rtp_packet_udp(sock, addr, nalu, size, true, timestamp);
        pkt_count++;
        octet_count += size;
        return;
    }

    std::cout << "  NALU found (fragmented): type=" << (int)nal_type << ", size=" << size << " bytes\n";

    uint8_t fu_indicator = (nal_header & 0xE0) | 28;
    uint8_t fu_header_start = 0x80 | (nal_type & 0x1F);
    uint8_t fu_header_middle = nal_type & 0x1F;
    uint8_t fu_header_end = 0x40 | (nal_type & 0x1F);

    int pos = 1;
    while (pos < size) {
        int remaining = size - pos;
        int payload_len = std::min(remaining, max_payload - 2);

        uint8_t fu_header = (pos == 1) ? fu_header_start : ((remaining - payload_len == 0) ? fu_header_end : fu_header_middle);

        std::vector<uint8_t> fu_packet(2 + payload_len);
        fu_packet[0] = fu_indicator;
        fu_packet[1] = fu_header;
        memcpy(fu_packet.data() + 2, nalu + pos, payload_len);

        send_rtp_packet_udp(sock, addr, fu_packet.data(), fu_packet.size(), (fu_header & 0x40) != 0, timestamp);
        pkt_count++;
        octet_count += payload_len + 2;
        pos += payload_len;
    }
}

void parse_and_send_frame_udp(int sock, sockaddr_in& addr, const uint8_t* data, int len, int64_t pts, uint32_t& pkt_count, uint32_t& octet_count) {
    if (base_pts < 0) base_pts = pts;
    int64_t delta_pts = pts - base_pts;
    uint32_t timestamp = static_cast<uint32_t>((delta_pts / 1000000.0) * 90000);

    int pos = 0;
    while (pos + 4 < len) {
        if (data[pos] == 0x00 && data[pos+1] == 0x00 && data[pos+2] == 0x00 && data[pos+3] == 0x01) {
            int start = pos + 4;
            int end = len;
            for (int i = start; i + 4 < len; ++i) {
                if (data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x00 && data[i+3] == 0x01) {
                    end = i;
                    break;
                }
            }
            packetize_and_send_nalu_udp(sock, addr, data + start, end - start, timestamp, pkt_count, octet_count);
            if (data[start] == 0x68) usleep(20000);
            pos = end;
        } else {
            ++pos;
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <input.264> <server_ip> <rtp_port>\n";
        return 1;
    }

    const char* inputFile = argv[1];
    std::string serverIp = argv[2];
    int rtpPort = std::stoi(argv[3]);
    std::string pathName = "cam01";

    send_open_rtpserver_to_zlm(serverIp, rtpPort, pathName);

    sockaddr_in rtp_addr, rtcp_addr;
    int sock = create_udp_socket(serverIp, rtpPort, rtp_addr);
    int rtcp_sock = create_udp_socket(serverIp, rtpPort + 1, rtcp_addr);
    if (sock < 0 || rtcp_sock < 0) return 1;

    ze_mxm_read_t* reader = nullptr;
    ze_mxm_file_info_t* file_info = nullptr;
    ze_mxm_read_open(&reader, inputFile, &file_info, 0);
    if (!reader) {
        std::cerr << "[ERROR] Failed to open file: " << inputFile << "\n";
        return 1;
    }

    int s_chn, s_stream_type, s_frame_type, s_frame_len = 0;
    s64 s_frame_pts = 0;
    char s_frame_buf[1024 * 1024];
    int chn = 0;
    uint32_t pkt_count = 0;
    uint32_t octet_count = 0;
    int64_t last_rtcp_pts = 0;

    while (ze_mxm_read_frame(reader, &s_chn, &s_stream_type, &s_frame_type,
                              &s_frame_len, &s_frame_pts, s_frame_buf) == 0) {
        if (s_chn != chn || s_frame_len <= 0) continue;
        if (s_frame_type != FRAME_TYPE_H264I && s_frame_type != FRAME_TYPE_H264P) continue;

        std::cout << "Read frame: chn=" << s_chn
                  << ", type=" << (s_frame_type == FRAME_TYPE_H264I ? "I" : "P")
                  << ", len=" << s_frame_len
                  << ", pts=" << std::fixed << std::setprecision(3) << (s_frame_pts / 1000000.0) << "s\n";

        parse_and_send_frame_udp(sock, rtp_addr, reinterpret_cast<uint8_t*>(s_frame_buf), s_frame_len, s_frame_pts, pkt_count, octet_count);

        if (base_pts >= 0 && (s_frame_pts - last_rtcp_pts) >= 1000000) {
            uint32_t timestamp = static_cast<uint32_t>(((s_frame_pts - base_pts) / 1000000.0) * 90000);
            send_rtcp_sr(rtcp_sock, rtcp_addr, timestamp, pkt_count, octet_count);
            last_rtcp_pts = s_frame_pts;
        }

        usleep(40000);
    }

    ze_mxm_read_close(reader);
    close(sock);
    close(rtcp_sock);
    return 0;
}
