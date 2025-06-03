#include "CustomFramedSource.hh"

CustomFramedSource::CustomFramedSource(UsageEnvironment& env) : FramedSource(env) {}

CustomFramedSource::~CustomFramedSource() {
    while (!frameQueue.empty()) {
        delete[] frameQueue.front().first;
        frameQueue.pop();
    }
}

CustomFramedSource* CustomFramedSource::createNew(UsageEnvironment& env) {
    return new CustomFramedSource(env);
}

void CustomFramedSource::deliverFrame(unsigned char* frame, unsigned frameSize, struct timeval timestamp) {
    std::lock_guard<std::mutex> lock(queueMutex);
    unsigned char* frameCopy = new unsigned char[frameSize];
    memcpy(frameCopy, frame, frameSize);
    frameQueue.push({frameCopy, frameSize});
    lastTimestamp = timestamp;
    if (isCurrentlyAwaitingData()) {
        doGetNextFrame();
    }
}

void CustomFramedSource::doGetNextFrame() {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (!frameQueue.empty()) {
        auto framePair = frameQueue.front();
        unsigned char* frame = framePair.first;
        unsigned frameSize = framePair.second;
        if (frameSize > fMaxSize) {
            fFrameSize = fMaxSize;
            fNumTruncatedBytes = frameSize - fMaxSize;
        } else {
            fFrameSize = frameSize;
            fNumTruncatedBytes = 0;
        }
        memcpy(fTo, frame, fFrameSize);
        fPresentationTime = lastTimestamp;
        frameQueue.pop();
        delete[] frame;
        afterGetting(this);
    } else {
        handleClosure(this);
    }
}