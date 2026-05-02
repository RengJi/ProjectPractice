# LaneRecognitionSystem

基于 Rockchip 平台的实时车道线识别系统，采用 RKNN 深度学习推理框架，实现视频捕获、硬件编解码、车道线检测和实时显示功能。

## 项目概述

本系统是一个完整的端到端车道线识别解决方案，专为 Rockchip ARM 平台优化设计。系统采用多线程流水线架构，充分利用硬件加速能力（MPP、RGA、RKNN），实现低延迟、高帧率的实时车道线检测。

### 主要特性

- **硬件加速**: 使用 Rockchip MPP 进行视频编解码，RGA 进行图像处理
- **深度学习推理**: 基于 RKNN 的车道线语义分割模型
- **多线程流水线**: 模块化流水线设计，支持并行处理
- **图像增强**: 可选的 LIME 低光照图像增强算法
- **实时显示**: 支持车道线叠加显示和性能统计

## 系统架构

```
┌─────────────┐    ┌──────────────┐    ┌─────────────┐    ┌──────────────┐    ┌─────────────┐
│  V4L2Camera │───▶│  MppDecoder  │───▶│ RgaProcessor│───▶│ LaneDetector │───▶│   Display   │
│  (视频捕获)  │    │  (硬件解码)   │    │  (图像处理)  │    │  (车道线检测) │    │   (显示)    │
└─────────────┘    └──────────────┘    └─────────────┘    └──────────────┘    └─────────────┘
                                                                    │
                                                            ┌───────▼───────┐
                                                            │  LimeEnhance  │
                                                            │  (图像增强)    │
                                                            └───────────────┘
```

## 目录结构

```
LaneRecognitionSystem/
├── app/
│   ├── include/              # 头文件目录
│   │   ├── Config.h          # 配置参数定义
│   │   ├── DataType.h        # 数据结构定义
│   │   ├── V4L2Camera.h      # V4L2 摄像头接口
│   │   ├── MppDecoder.h      # MPP 解码器接口
│   │   ├── MppEncoder.h      # MPP 编码器接口
│   │   ├── RgaProcess.h      # RGA 图像处理接口
│   │   ├── LaneDetector.h    # 车道线检测器接口
│   │   ├── LimeEnhance.h     # LIME 图像增强接口
│   │   └── PipeLine.h        # 流水线管理接口
│   └── source/               # 源文件目录
│       ├── main.cpp          # 主程序入口
│       ├── V4L2Camera.cpp    # V4L2 摄像头实现
│       ├── MppDecoder.cpp    # MPP 解码器实现
│       ├── MppEncoder.cpp    # MPP 编码器实现
│       ├── RgaProcess.cpp    # RGA 图像处理实现
│       ├── LaneDetector.cpp  # 车道线检测实现
│       ├── LimeEnhance.cpp   # LIME 图像增强实现
│       └── PipeLine.cpp      # 流水线管理实现
├── 3rdparty/                 # 第三方库
│   └── rknn/                 # RKNN SDK
│       └── include/          # RKNN 头文件
├── build/                    # 构建输出目录
│   ├── bin/                  # 可执行文件输出
│   └── lib/                  # 库文件输出
├── model/                    # 模型文件目录
│   └── pp_liteseg.rknn       # 车道线分割模型
├── CMakeLists.txt            # CMake 构建配置
└── README.md                 # 项目说明文档
```

## 核心模块说明

### 1. V4L2Camera - 视频捕获模块

负责从 V4L2 设备捕获视频帧，支持：
- DMA-BUF 零拷贝缓冲区管理
- 多平面格式支持 (MPLANE)
- 可配置的分辨率、帧率
- 曝光时间控制

### 2. MppDecoder - 硬件解码模块

基于 Rockchip MPP 的硬件视频解码器：
- H.264/H.265 硬件解码
- 外部缓冲区导入 (DMA-BUF)
- 异步解码任务管理
- 缓冲区回收机制

### 3. MppEncoder - 硬件编码模块

基于 Rockchip MPP 的硬件视频编码器：
- H.264/H.265 硬件编码
- 可配置码率、GOP
- 实时编码输出

### 4. RgaProcessor - 图像处理模块

基于 Rockchip RGA 的硬件图像处理器：
- 图像缩放
- 格式转换
- 图像翻转
- DMA-BUF 零拷贝操作

### 5. LaneDetector - 车道线检测模块

基于 RKNN 的深度学习车道线检测：
- 语义分割模型推理
- 车道线轮廓提取
- 车道线类型分类
- 可视化叠加

### 6. LimeEnhance - 图像增强模块

LIME (Low-Light Image Enhancement) 算法实现：
- 低光照图像增强
- ADMM 优化求解
- 边缘感知权重

### 7. PipeLine - 流水线管理模块

多线程流水线管理器：
- 线程安全的队列管理
- 帧率控制和节流
- 性能统计
- 状态管理

## 依赖项

### 系统依赖

- **操作系统**: Linux (推荐 Ubuntu 20.04+)
- **平台**: Rockchip RK3588/RK356X 系列
- **编译器**: GCC 9.0+ (支持 C++17)

### 软件库

| 依赖库 | 版本要求 | 说明 |
|--------|----------|------|
| OpenCV | 4.0+ | 图像处理 |
| RKNN | 1.5.0+ | 深度学习推理 |
| MPP | 1.0.0+ | 多媒体处理 |
| RGA | 2.0+ | 图形加速 |
| OpenMP | 4.5+ | 并行计算 |

### 安装依赖 (Rockchip 平台)

```bash
# 安装 OpenCV
sudo apt-get install libopencv-dev

# RKNN、MPP、RGA 库需要从 Rockchip 官方获取并安装
# 通常预装在 Rockchip Linux 发行版中
```

## 构建说明

### 编译项目

```bash
# 创建构建目录
mkdir -p build && cd build

# 配置 CMake
cmake ..

# 编译
make -j$(nproc)

# 可执行文件输出到 build/bin/lane_recognition
```

### 构建选项

```bash
# Debug 构建
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release 构建 (默认)
cmake -DCMAKE_BUILD_TYPE=Release ..
```

## 运行说明

### 基本用法

```bash
# 使用默认配置运行
./build/bin/lane_recognition

# 指定摄像头设备
./build/bin/lane_recognition -d /dev/video0

# 指定分辨率和帧率
./build/bin/lane_recognition -w 1920 -h 1080 -f 30

# 指定模型路径
./build/bin/lane_recognition -m /path/to/model.rknn

# 启用 LIME 图像增强
./build/bin/lane_recognition -lime
```

### 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-d <device>` | 摄像头设备路径 | `/dev/video11` |
| `-w <width>` | 捕获宽度 | 1920 |
| `-h <height>` | 捕获高度 | 1080 |
| `-f <fps>` | 捕获帧率 | 30 |
| `-m <model>` | 模型文件路径 | `model/pp_liteseg.rknn` |
| `-lime` | 启用 LIME 增强 | 禁用 |
| `--help` | 显示帮助信息 | - |

### 交互控制

- 按 `q` 或 `ESC` 键退出程序
- 按 `Ctrl+C` 发送终止信号

## 配置说明

主要配置参数位于 [Config.h](app/include/Config.h)，包括：

### 摄像头配置

```cpp
CameraPath = "/dev/video11"    // 设备路径
CameraWidth = 1920             // 宽度
CameraHeight = 1080            // 高度
CameraFPS = 30                 // 帧率
```

### 推理配置

```cpp
ModelInputWidth = 224          // 模型输入宽度
ModelInputHeight = 224         // 模型输入高度
ModelPath = "model/pp_liteseg.rknn"
ConfidenceThreshold = 0.5f     // 置信度阈值
TargetInputFps = 10            // 推理目标帧率
```

### 流水线配置

```cpp
MaxQueueDepth = 10             // 队列最大深度
StatIntervalMs = 1000          // 统计输出间隔
DisplayLatestOnly = true       // 仅显示最新帧
```

## 性能优化建议

1. **调整推理帧率**: 通过 `TargetInputFps` 控制推理频率，降低 CPU/GPU 负载
2. **队列深度优化**: 根据内存情况调整 `MaxQueueDepth`
3. **LIME 增强**: 仅在低光照环境下启用，会增加计算开销
4. **分辨率调整**: 根据实际需求调整捕获和显示分辨率

## 日志系统

系统运行日志输出到 `build/bin/log.txt`，包含：
- 模块初始化状态
- 帧处理统计
- 错误和警告信息
- 性能指标

## 故障排除

### 常见问题

1. **摄像头打开失败**
   - 检查设备路径是否正确
   - 确认设备权限 (`/dev/video*`)
   - 检查设备是否被其他程序占用

2. **模型加载失败**
   - 确认模型文件路径正确
   - 检查模型格式是否为 RKNN 格式
   - 确认 RKNN 运行时库已正确安装

3. **显示窗口无法创建**
   - 确认系统支持 GUI 显示
   - 检查 OpenCV 是否编译了 GUI 支持

4. **性能不佳**
   - 降低捕获分辨率
   - 减少推理帧率
   - 关闭 LIME 增强

## 开发说明

### 代码规范

- C++17 标准
- 遵循 Google C++ 编码规范
- 使用 `constexpr` 定义常量
- 使用智能指针管理资源

### 扩展开发

1. **添加新的图像处理模块**
   - 继承现有接口
   - 在 PipeLine 中注册新线程

2. **更换检测模型**
   - 准备 RKNN 格式模型
   - 修改 `LaneDetector` 的预处理和后处理逻辑

## 许可证

本项目仅供学习和研究使用。

## 联系方式

如有问题或建议，请提交 Issue 或 Pull Request。
