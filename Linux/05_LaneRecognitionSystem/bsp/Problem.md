# sensor 驱动过程中的问题点
## 驱动移植阶段
1. 板卡的内核 deb 包替换后不会自动替换正在使用的 DTC 文件(手动替换)
2. 板卡使用了 overlay 机制，所以 sensor 节点默认关闭(手动开启)
3. 板卡并未设置 power pin，而是使用恒定 v3.3 电源(删除了源码中power pin 相关的程序)
4. sensor 电源配置也未寻找到(设备树中提供了伪配置)

## 画面调试阶段
1. 画面暗淡无光，isp没有正常工作(没有寻找到对应的rkaiq配置文件，寻找然后放置到指定位置/etc/iqfile )

## 图像数据链路
```c
IMX415 Sensor
   ↓ (MIPI CSI-2)
rockchip-csi2-dphy
   ↓
rkcif (CIF)
   ↓ (RAW Bayer)
rkisp (ISP 硬件)
   ↓ (AIQ 算法参与)
rkisp_mainpath (/dev/video11)
   ↓ (NV12)
GStreamer / v4l2-ctl
   ↓
显示画面
```
