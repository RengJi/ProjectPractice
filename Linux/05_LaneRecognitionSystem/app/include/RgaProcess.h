#pragma once

#include <rockchip/mpp_frame.h>
#include "DataType.h"
#include <mutex>
#include <array>
#include <queue>

class RgaProcessor
{
private:
    struct ImageConfig
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t horStride = 0;
        uint32_t verStride = 0;
        uint32_t format = MPP_FMT_YUV420SP;
        bool valid = false;
    };

    enum class Operation
    {
        Copy,
        FlipHorizontal,
        FlipVertical,
        Resize,
        ConvertFormat
    };

    int InitDmaBuf(IoFd *output, size_t size);
    void ReleaseDmaBuf(IoFd *output);
    bool IsConfigValid(const ImageConfig &config) const;
    bool LoadConfig(const IoFd &io, ImageConfig &config);
    int InitOutputBuffers(const ImageConfig &config);
    void ReleaseOutputBuffers();
    bool IsOutputMember(const IoFd *buffer) const;
    IoFd *AquireOutputBuffer();
    IoFd *TransformInternal(const IoFd *src, Operation op, uint32_t dst_width = 0, uint32_t dst_height = 0, uint32_t dst_format = 0);
    size_t CalcNv12ImageSize(uint32_t hor_stride, uint32_t ver_stride) const;

public:
    RgaProcessor() = default;
    ~RgaProcessor();
    int Init();
    IoFd *Copy(const IoFd *src);
    IoFd *FlipHorizontal(const IoFd *src);
    IoFd *FlipVertical(const IoFd *src);
    IoFd *Resize(const IoFd *src, uint32_t dst_width, uint32_t dst_height);
    IoFd *ConvertFormat(const IoFd *src, uint32_t dst_format);
    int QueueOutputToRecycler(const IoFd *buffer);


private:
    bool initialized = false;
    bool outReady = false;
    std::mutex outMtx;
    std::array<bool, ResourceLimit::RgaOutputBufferCount> outBufferInUse{};
    std::queue<IoFd *> availablePositions;
public:
    IoFd outBuffers[ResourceLimit::RgaOutputBufferCount]{};
};