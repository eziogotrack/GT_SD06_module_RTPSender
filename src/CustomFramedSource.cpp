// CustomFramedSource.cpp
#include "CustomFramedSource.hh"
#include <iostream>

CustomFramedSource::CustomFramedSource() {}

void CustomFramedSource::setCallback(FrameCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = cb;
}

void CustomFramedSource::pushFrame(const uint8_t* data, int len, int64_t pts,
                                   int chn, int stream_type, int frame_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Drop oldest if buffer is full
    if (frameQueue_.size() >= MAX_QUEUE_SIZE) {
        frameQueue_.pop();
    }

    Frame frame;
    frame.buff.assign(data, data + len);
    frame.pts = pts;
    frame.chn = chn;
    frame.stream_type = stream_type;
    frame.frame_type = frame_type;
    frameQueue_.push(std::move(frame));

    deliverNext();
}

void CustomFramedSource::deliverNext() {
    if (!callback_ || frameQueue_.empty()) return;

    const Frame& frame = frameQueue_.front();
    callback_(frame.buff.data(), static_cast<int>(frame.buff.size()), frame.pts,
              frame.chn, frame.stream_type, frame.frame_type);
    frameQueue_.pop();
}
