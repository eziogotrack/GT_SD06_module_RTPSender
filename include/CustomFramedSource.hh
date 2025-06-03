#ifndef CUSTOM_FRAMED_SOURCE_HH
#define CUSTOM_FRAMED_SOURCE_HH

#include <liveMedia.hh>
#include <queue>
#include <mutex>

class CustomFramedSource : public FramedSource {
public:
    static CustomFramedSource* createNew(UsageEnvironment& env);
    void deliverFrame(unsigned char* frame, unsigned frameSize, struct timeval timestamp);

protected:
    CustomFramedSource(UsageEnvironment& env);
    virtual ~CustomFramedSource();
    virtual void doGetNextFrame();

private:
    std::queue<std::pair<unsigned char*, unsigned>> frameQueue;
    std::mutex queueMutex;
    struct timeval lastTimestamp;
};

#endif