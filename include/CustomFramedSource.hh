// CustomFramedSource.hh
#ifndef CUSTOM_FRAMED_SOURCE_HH
#define CUSTOM_FRAMED_SOURCE_HH

#include <functional>
#include <cstdint>
#include <mutex>
#include <queue>
#include <vector>

/**
 * CustomFramedSource acts as a buffer that accepts H264 frames via pushFrame,
 * and delivers them to a user-defined callback. When full, it drops the oldest frame.
 * Frame metadata such as channel, stream_type, and frame_type are also retained.
 */
class CustomFramedSource {
public:
    using FrameCallback = std::function<void(const uint8_t* data, int len, int64_t pts,
                                             int chn, int stream_type, int frame_type)>;

    CustomFramedSource();

    // Set the callback that will be triggered when a frame is to be delivered
    void setCallback(FrameCallback cb);

    // Push a frame into the buffer; oldest will be dropped if full
    void pushFrame(const uint8_t* data, int len, int64_t pts,
                   int chn, int stream_type, int frame_type);

private:
    struct Frame {
        std::vector<uint8_t> buff;
        int64_t pts;
        int chn;
        int stream_type;
        int frame_type;
    };

    static constexpr size_t MAX_QUEUE_SIZE = 30;

    std::queue<Frame> frameQueue_;
    FrameCallback callback_ = nullptr;
    std::mutex mutex_;

    void deliverNext();
};

#endif // CUSTOM_FRAMED_SOURCE_HH