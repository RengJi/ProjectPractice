#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include "V4L2Camera.h"
#include "Logger.hpp"

V4L2Camera::V4L2Camera() = default;
V4L2Camera::~V4L2Camera()
{
    Close();
}

/**
 * @brief 初始化摄像头设备
 * @param devicePath 摄像头设备路径（例如 "/dev/video0"）
 * @param expousetime_ms 可选的曝光时间（毫秒），如果为0则使用默认曝光时间
 * @return 0表示成功，-1表示失败
 */
int V4L2Camera::Init(const std::string &devicePath, int expousetime_ms)
{
    if (Open(devicePath) != 0)
    {
        LOG_ERROR("V4L2", "Failed to open camera device: ", devicePath);
        return -1;
    }

    if (SetCameraFormat() != 0)
    {
        LOG_ERROR("V4L2", "Camera device does not meet required capabilities: ", devicePath);
        Close();
        return -1;
    }

    if (InitBuffer() != 0)
    {
        LOG_ERROR("V4L2", "Failed to initialize camera buffers: ", devicePath);
        Close();
        return -1;
    }

    if (expousetime_ms > 0)
    {
        if (SetExposureTime(expousetime_ms) != 0)
        {
            LOG_WARNING("V4L2", "Failed to set exposure time, continuing with default: ", devicePath);
        }
    }

    LOG_INFO("V4L2", "Camera initialized successfully: ", devicePath,
             "(", captureConfig.width, "x", captureConfig.height,
             "@", captureConfig.fpsNum / (captureConfig.fpsDen > 0 ? captureConfig.fpsDen : 1), "fps)");

    return 0;
}

/**
 * @brief 打开摄像头设备并查询其能力
 * @param devicePath 摄像头设备路径
 * @return 0表示成功，-1表示失败
 */
int V4L2Camera::Open(const std::string &devicePath)
{
    // 1. 打开摄像头设备文件
    cameraFd = open(devicePath.c_str(), O_RDWR | O_NONBLOCK, 0);
    if (cameraFd < 0)
    {
        LOG_ERROR("V4L2", "Failed to open camera device: ", devicePath, " Error: ", strerror(errno));
        return -1;
    }

    // 2. 查询摄像头设备能力
    if (ioctl(cameraFd, VIDIOC_QUERYCAP, &cameraCapability) < 0)
    {
        LOG_ERROR("V4L2", "Failed to query camera capabilities: ", devicePath, " Error: ", strerror(errno));
        Close();
        return -1;
    }

    if ((cameraCapability.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) != 0)
    {
        isMultiplanar = true;
        LOG_INFO("V4L2", "Camera device supports Video Capture Multiplanar mode");
    }
    else if ((cameraCapability.capabilities & V4L2_CAP_VIDEO_CAPTURE) != 0)
    {
        isMultiplanar = false;
        LOG_INFO("V4L2", "Camera device supports Video Capture mode");
    }
    else
    {
        LOG_ERROR("V4L2", "Camera device does not support video capture: ", devicePath);
        Close();
        return -1;
    }

    if ((cameraCapability.capabilities & V4L2_CAP_STREAMING) == 0) // 检查是否支持流式I/O
    {
        LOG_ERROR("V4L2", "Camera device does not support streaming I/O: ", devicePath);
        Close();
        return -1;
    }

    return 0;
}

/**
 * @brief 检查摄像头设备的格式支持和设置捕获参数
 * @return 0表示成功，-1表示失败
 */
int V4L2Camera::SetCameraFormat()
{
    bool nv12Supported = false;
    bool nv21Supported = false;
    bool yuyvSupported = false;
    bool mjpegSupported = false;

    struct v4l2_fmtdesc fmtdesc{};
    memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.type = isMultiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    // 1. 枚举摄像头支持的格式，检查是否支持NV12、NV21、YUYV或MJPEG格式
    while (ioctl(cameraFd, VIDIOC_ENUM_FMT, &fmtdesc) == 0)
    {
        if (fmtdesc.pixelformat == V4L2_PIX_FMT_NV12)
        {
            nv12Supported = true;
        }
        else if (fmtdesc.pixelformat == V4L2_PIX_FMT_NV21)
        {
            nv21Supported = true;
        }
        else if (fmtdesc.pixelformat == V4L2_PIX_FMT_YUYV)
        {
            yuyvSupported = true;
        }
        else if (fmtdesc.pixelformat == V4L2_PIX_FMT_MJPEG)
        {
            mjpegSupported = true;
        }
        fmtdesc.index++;
    }

    // 2. 根据支持的格式优先选择NV12、NV21、YUYV或MJPEG格式，并设置摄像头格式参数
    uint32_t pixelFormat = 0;
    if (nv12Supported)
    {
        pixelFormat = V4L2_PIX_FMT_NV12; // 优先使用NV12
        LOG_INFO("V4L2", "Camera supports NV12 format, using NV12 for capture");
    }
    else if (nv21Supported)
    {
        pixelFormat = V4L2_PIX_FMT_NV21; // 其次使用NV21
        LOG_INFO("V4L2", "Camera supports NV21 format, using NV21 for capture");
    }
    else if (yuyvSupported)
    {
        pixelFormat = V4L2_PIX_FMT_YUYV; // 其次使用YUYV
        LOG_INFO("V4L2", "Camera supports YUYV format, using YUYV for capture");
    }
    else if (mjpegSupported)
    {
        pixelFormat = V4L2_PIX_FMT_MJPEG; // 最后使用MJPEG
        LOG_INFO("V4L2", "Camera supports MJPEG format, using MJPEG for capture");
    }
    else
    {
        LOG_ERROR("V4L2", "Camera does not support any of the required pixel formats (NV12, NV21, YUYV, MJPEG)");
        return -1;
    }

    // 3. 设置摄像头格式参数，包括分辨率、像素格式和扫描方式
    memset(&cameraFormat, 0, sizeof(cameraFormat));
    
    if (isMultiplanar)
    {
        cameraFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;         // 多平面模式
        cameraFormat.fmt.pix_mp.width = CameraParameter::CaptureWidth;  // 设置捕获宽度
        cameraFormat.fmt.pix_mp.height = CameraParameter::CaptureHeight;// 设置捕获高度
        cameraFormat.fmt.pix_mp.pixelformat = pixelFormat;              // 设置像素格式
        cameraFormat.fmt.pix_mp.field = V4L2_FIELD_NONE;                // 设置无扫描方式
        cameraFormat.fmt.pix_mp.num_planes = 1;
        if (ioctl(cameraFd, VIDIOC_S_FMT, &cameraFormat) < 0)
        {
            LOG_ERROR("V4L2", "Failed to set camera format (MPLANE): ", strerror(errno));
            return -1;
        }
    }
    else
    {
        cameraFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cameraFormat.fmt.pix.width = CameraParameter::CaptureWidth;
        cameraFormat.fmt.pix.height = CameraParameter::CaptureHeight;
        cameraFormat.fmt.pix.pixelformat = pixelFormat;
        cameraFormat.fmt.pix.field = V4L2_FIELD_NONE;
        if (ioctl(cameraFd, VIDIOC_S_FMT, &cameraFormat) < 0)
        {
            LOG_ERROR("V4L2", "Failed to set camera format: ", strerror(errno));
            return -1;
        }
    }

    // 4. 设置摄像头流参数，包括帧率分子和分母
    memset(&cameraStreamParam, 0, sizeof(cameraStreamParam));
    cameraStreamParam.type = isMultiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cameraStreamParam.parm.capture.timeperframe.numerator = 1;
    cameraStreamParam.parm.capture.timeperframe.denominator = CameraParameter::CaptureFps;
    if (ioctl(cameraFd, VIDIOC_S_PARM, &cameraStreamParam) < 0)
    {
        LOG_WARNING("V4L2", "Failed to set camera stream parameters (FPS), using device default: ", strerror(errno));
        if (ioctl(cameraFd, VIDIOC_G_PARM, &cameraStreamParam) < 0)
        {
            LOG_WARNING("V4L2", "Failed to get camera stream parameters, using default values");
            cameraStreamParam.parm.capture.timeperframe.numerator = 1;                              // 默认值为1，表示1帧/秒
            cameraStreamParam.parm.capture.timeperframe.denominator = CameraParameter::CaptureFps;  // 默认值为1，表示1帧/秒
        }
    }

    // 5. 更新摄像头实际生效的配置参数到 captureConfig 结构体中
    if (isMultiplanar)
    {
        captureConfig.width = cameraFormat.fmt.pix_mp.width;
        captureConfig.height = cameraFormat.fmt.pix_mp.height;
        captureConfig.bytesPerLine = cameraFormat.fmt.pix_mp.plane_fmt[0].bytesperline;
        captureConfig.imageSize = cameraFormat.fmt.pix_mp.plane_fmt[0].sizeimage;
        captureConfig.pixelFormat = cameraFormat.fmt.pix_mp.pixelformat;
    }
    else
    {
        captureConfig.width = cameraFormat.fmt.pix.width;
        captureConfig.height = cameraFormat.fmt.pix.height;
        captureConfig.bytesPerLine = cameraFormat.fmt.pix.bytesperline;
        captureConfig.imageSize = cameraFormat.fmt.pix.sizeimage;
        captureConfig.pixelFormat = cameraFormat.fmt.pix.pixelformat;
    }
    captureConfig.fpsNum = cameraStreamParam.parm.capture.timeperframe.denominator;
    captureConfig.fpsDen = cameraStreamParam.parm.capture.timeperframe.numerator;
    captureConfig.valid = true;

    return 0;
}

/**
 * @brief 初始化摄像头缓冲区，使用内存映射方式
 * @return 0表示成功，-1表示失败
 */
int V4L2Camera::InitBuffer()
{
    // 1. 请求摄像头缓冲区，并检查请求的缓冲区数量是否满足要求
    struct v4l2_requestbuffers reqBuf{};
    memset(&reqBuf, 0, sizeof(reqBuf));
    reqBuf.count = CameraParameter::CaptureRequestBufferCount;                                      // 请求的缓冲区数量
    reqBuf.type = isMultiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE; // 缓冲区类型根据是否为多平面模式设置
    reqBuf.memory = V4L2_MEMORY_MMAP;                                                               // 使用内存映射方式

    if (ioctl(cameraFd, VIDIOC_REQBUFS, &reqBuf) < 0)
    {
        LOG_ERROR("V4L2", "Failed to request camera buffers: ", strerror(errno));
        return -1;
    }

    if (reqBuf.count < 2)
    {
        LOG_ERROR("V4L2", "Insufficient camera buffers allocated: ", reqBuf.count);
        return -1;
    }

    // 2. 查询每个缓冲区的信息，并使用 mmap 将缓冲区映射到用户空间，同时尝试使用 EXPBUF 导出 DMA-BUF fd 以供后续高效处理
    bufferCount = static_cast<int>(reqBuf.count);

    for (int i = 0; i < bufferCount; ++i)
    {
        struct v4l2_buffer buf{};       // 缓冲区查询结构体
        struct v4l2_plane planes[1];    // 多平面模式下的平面信息结构体数组，单平面模式下只使用第一个元素
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        
        buf.type = isMultiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (isMultiplanar)
        {
            buf.m.planes = planes;
            buf.length = 1;
        }

        // 查询缓冲区信息，获取缓冲区的长度和偏移量等参数
        if (ioctl(cameraFd, VIDIOC_QUERYBUF, &buf) < 0)
        {
            LOG_ERROR("V4L2", "Failed to query camera buffer: ", strerror(errno));
            return -1;
        }

        frameDesArray[i].index = i;
        
        // 尝试使用 EXPBUF 导出 DMA-BUF fd
        struct v4l2_exportbuffer expbuf{};
        memset(&expbuf, 0, sizeof(expbuf));
        expbuf.type = isMultiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
        expbuf.index = i;
        expbuf.flags = O_RDWR | O_CLOEXEC;  // 请求读写权限和自动关闭标志
        
        if (ioctl(cameraFd, VIDIOC_EXPBUF, &expbuf) == 0)
        {
            frameDesArray[i].fd = expbuf.fd;
            LOG_INFO("V4L2", "Exported DMA-BUF fd ", expbuf.fd, " for buffer ", i);
        }
        else
        {
            frameDesArray[i].fd = -1;
            LOG_WARNING("V4L2", "VIDIOC_EXPBUF failed for buffer ", i, ", falling back to mmap only");
        }
        
        if (isMultiplanar)
        {
            frameDesArray[i].length = buf.m.planes[0].length;
            frameDesArray[i].payloadSize = captureConfig.imageSize;
            LOG_INFO("V4L2", "Buffer ", i, ": length=", buf.m.planes[0].length, 
                     ", bytesperline=", captureConfig.bytesPerLine,
                     ", sizeimage=", captureConfig.imageSize);
            // 如果成功导出 DMA-BUF fd 则使用该 fd 进行 mmap，否则使用传统的 offset 方式进行 mmap
            if (frameDesArray[i].fd >= 0)
            {
                frameDesArray[i].base = mmap(nullptr, buf.m.planes[0].length, PROT_READ | PROT_WRITE, 
                                             MAP_SHARED, frameDesArray[i].fd, 0);
            }
            else
            {
                frameDesArray[i].base = mmap(nullptr, buf.m.planes[0].length, PROT_READ | PROT_WRITE, 
                                             MAP_SHARED, cameraFd, buf.m.planes[0].m.mem_offset);
            }
        }
        else
        {
            frameDesArray[i].length = buf.length;
            frameDesArray[i].payloadSize = captureConfig.imageSize;
            LOG_INFO("V4L2", "Buffer ", i, ": length=", buf.length,
                     ", bytesperline=", captureConfig.bytesPerLine,
                     ", sizeimage=", captureConfig.imageSize);
            if (frameDesArray[i].fd >= 0)
            {
                frameDesArray[i].base = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, 
                                             MAP_SHARED, frameDesArray[i].fd, 0);
            }
            else
            {
                frameDesArray[i].base = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, 
                                             MAP_SHARED, cameraFd, buf.m.offset);
            }
        }
        
        if (frameDesArray[i].base == MAP_FAILED)
        {
            LOG_ERROR("V4L2", "Failed to mmap camera buffer: ", strerror(errno));
            frameDesArray[i].base = nullptr;
            return -1;
        }
        LOG_INFO("V4L2", "Buffer ", i, " mmap success: base=", frameDesArray[i].base);
    }

    LOG_INFO("V4L2", "Camera buffers initialized successfully: ", bufferCount, " buffers allocated");
    return 0;
}

/**
 * @brief 解除摄像头缓冲区的内存映射并重置相关状态
 * @return 0表示成功，-1表示失败
 */
int V4L2Camera::DeinitBuffer()
{
    for (int i = 0; i < bufferCount; ++i)
    {
        // 释放内存
        if (frameDesArray[i].base != nullptr && frameDesArray[i].base != MAP_FAILED)
        {
            munmap(frameDesArray[i].base, frameDesArray[i].length);
            frameDesArray[i].base = nullptr;
        }

        // 关闭通过 EXPBUF 导出的 DMA-BUF fd
        if (frameDesArray[i].fd >= 0)   
        {
            close(frameDesArray[i].fd);
            frameDesArray[i].fd = -1;
        }
    }

    bufferCount = 0;
    return 0;
}

/**
 * @brief 控制摄像头的流式传输，启动或停止视频流
 * @param enable true表示启动流式传输，false表示停止流式传输
 * @return 0表示成功，-1表示失败
 */
int V4L2Camera::StreamControl(bool enable)
{
    enum v4l2_buf_type type = isMultiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    if (enable && !isStreaming)
    {
        for (int i = 0; i < bufferCount; ++i)
        {
            struct v4l2_buffer buf{};
            struct v4l2_plane planes[1];
            memset(&buf, 0, sizeof(buf));
            memset(planes, 0, sizeof(planes));
            
            buf.type = type;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (isMultiplanar)
            {
                buf.m.planes = planes;
                buf.length = 1;
            }

            if (ioctl(cameraFd, VIDIOC_QBUF, &buf) < 0)
            {
                LOG_ERROR("V4L2", "Failed to queue camera buffer: ", strerror(errno));
                return -1;
            }
        }

        if (ioctl(cameraFd, VIDIOC_STREAMON, &type) < 0)
        {
            LOG_ERROR("V4L2", "Failed to start camera streaming: ", strerror(errno));
            return -1;
        }

        isStreaming = true;
        LOG_INFO("V4L2", "Camera streaming started successfully");
    }
    else if (!enable && isStreaming)
    {
        if (ioctl(cameraFd, VIDIOC_STREAMOFF, &type) < 0)
        {
            LOG_ERROR("V4L2", "Failed to stop camera streaming: ", strerror(errno));
            return -1;
        }

        isStreaming = false;
        LOG_INFO("V4L2", "Camera streaming stopped successfully");
    }

    return 0;
}

/**
 * @brief 从摄像头缓冲区读取一帧数据，并更新当前帧描述
 * @param outlength 输出参数，返回读取的帧数据长度
 * @param maxlength 可选参数，指定最大可读取的帧数据长度，默认为CameraParameter::MaxFrameSize
 * @return 0表示成功，-1表示失败
 */
int V4L2Camera::ReadFrame(int *outlength, size_t maxlength)
{
    unsigned int index = 0;
    unsigned int bufferSize = 0;

    if (DequeueFrame(index, bufferSize, maxlength) != CameraReadResult::ok) // 从摄像头缓冲区出队一帧数据
    {
        LOG_ERROR("V4L2", "Failed to read frame from camera buffer");
        return -1;
    }

    if (index >= static_cast<unsigned int>(bufferCount)) // 检查出队的缓冲区索引是否合法
    {
        LOG_ERROR("V4L2", "Invalid buffer index dequeued from camera: ", index);
        return -1;
    }

    curFrameDes = &frameDesArray[index];   // 更新当前帧描述指针
    curFrameDes->payloadSize = bufferSize; // 更新当前帧描述的有效载荷大小
    *outlength = bufferSize;               // 输出帧数据长度
    return 0;
}

/**
 * @brief 从摄像头缓冲区出队一帧数据，并检查数据大小是否超过最大限制
 * @param index 输出参数，返回出队的缓冲区索引
 * @param buffer_size 输出参数，返回出队的帧数据大小
 * @param max_buffer_size 输入参数，指定最大可接受的帧数据大小
 * @return CameraReadResult::ok表示成功读取一帧数据，CameraReadResult::retry表示读取失败但可以重试，CameraReadResult::timeout表示读取超时，CameraReadResult::error表示读取失败且不可重试
 */
int V4L2Camera::DequeueFrame(unsigned int &index, unsigned int &buffer_size, size_t max_buffer_size)
{
    // 1. 定义出队缓冲区的结构体
    struct v4l2_buffer buf{};
    struct v4l2_plane planes[1];
    memset(&buf, 0, sizeof(buf));
    memset(planes, 0, sizeof(planes));
    
    buf.type = isMultiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (isMultiplanar)
    {
        buf.m.planes = planes;
        buf.length = 1;
    }

    // 2. 使用 select 等待摄像头缓冲区可读，设置超时时间以避免长时间阻塞
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(cameraFd, &readFds);

    struct timeval timeOut{};
    timeOut.tv_sec = CameraParameter::DequeueTimeoutMs / 1000;
    timeOut.tv_usec = (CameraParameter::DequeueTimeoutMs % 1000) * 1000;

    int ret = select(cameraFd + 1, &readFds, nullptr, nullptr, &timeOut);
    if (ret < 0)
    {
        if (ret == EINTR)
        {
            LOG_WARNING("V4L2", "Select interrupted by signal, retrying...");
            return CameraReadResult::retryAble;
        }
        else
        {
            LOG_ERROR("V4L2", "Select error while waiting for camera frame: ", strerror(errno));
            return CameraReadResult::fatal;
        }
    }
    else if (ret == 0)
    {
        LOG_WARNING("V4L2", "Select timeout while waiting for camera frame");
        return CameraReadResult::timeOut;
    }

    if (ioctl(cameraFd, VIDIOC_DQBUF, &buf) < 0)
    {
        LOG_ERROR("V4L2", "Failed to dequeue camera buffer: ", strerror(errno));
        return CameraReadResult::fatal;
    }

    index = buf.index;
    buffer_size = isMultiplanar ? buf.m.planes[0].bytesused : buf.bytesused;
    
    if (buffer_size > max_buffer_size)
    {
        LOG_ERROR("V4L2", "Dequeued camera frame size exceeds maximum buffer size: ", buffer_size, " > ", max_buffer_size);
        return CameraReadResult::fatal;
    }

    frameDesArray[index].ptsUs = buf.timestamp.tv_sec * 1000000LL + buf.timestamp.tv_usec;

    return CameraReadResult::ok;
}

/**
 * @brief 将一帧数据重新入队到摄像头缓冲区，以便后续继续捕获
 * @param frame_des 要重新入队的帧描述
 * @return 0表示成功，-1表示失败
 */
int V4L2Camera::RequeueFrame(FrameDescription *frame_des)
{
    if (frame_des == nullptr)
    {
        LOG_ERROR("V4L2", "Invalid frame description pointer for requeue");
        return -1;
    }

    if (frame_des->index < 0 || frame_des->index >= bufferCount)
    {
        LOG_ERROR("V4L2", "Invalid frame index for requeue: ", frame_des->index);
        return -1;
    }

    struct v4l2_buffer buf{};
    struct v4l2_plane planes[1];
    memset(&buf, 0, sizeof(buf));
    memset(planes, 0, sizeof(planes));
    
    buf.type = isMultiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = static_cast<__u32>(frame_des->index);

    if (isMultiplanar)
    {
        buf.m.planes = planes;
        buf.length = 1;
    }

    if (ioctl(cameraFd, VIDIOC_QBUF, &buf) < 0)
    {
        LOG_ERROR("V4L2", "Failed to requeue camera buffer: ", strerror(errno));
        return -1;
    }

    return 0;
}

/**
 * @brief 设置摄像头的曝光时间
 * @param exposureTimeMs 曝光时间，单位为毫秒
 * @return 0表示成功，-1表示失败
 */
int V4L2Camera::SetExposureTime(int exposureTimeMs)
{
    if (exposureTimeMs < 0)
    {
        LOG_ERROR("V4L2", "Invalid exposure time: ", exposureTimeMs, " ms");
        return -1;
    }

    struct v4l2_control control{};
    memset(&control, 0, sizeof(control));
    control.id = V4L2_CID_EXPOSURE_ABSOLUTE;
    control.value = exposureTimeMs * 10;
    if (ioctl(cameraFd, VIDIOC_S_CTRL, &control) < 0)
    {
        LOG_ERROR("V4L2", "Failed to set camera exposure time: ", strerror(errno));
        return -1;
    }

    LOG_INFO("V4L2", "Camera exposure time set successfully: ", exposureTimeMs, " ms");
    return 0;
}

/**
 * @brief 关闭摄像头设备，停止流式传输并解除缓冲区映射
 * @return 0表示成功，-1表示失败
 */
int V4L2Camera::Close()
{
    if (isStreaming)
    {
        StreamControl(false); // 停止流式传输
    }

    DeinitBuffer(); // 解除缓冲区映射

    if (cameraFd >= 0)
    {
        close(cameraFd); // 关闭摄像头设备文件
        cameraFd = -1;   // 重置文件描述符
    }

    LOG_INFO("V4L2", "Camera device closed successfully");
    return 0;
}