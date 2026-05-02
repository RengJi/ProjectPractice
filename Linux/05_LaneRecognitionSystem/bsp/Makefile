#设置目标架构
export ARCH=arm64
#设置交叉编译工具和前缀
export CROSS_COMPILE=/home/chen/1.chip_sdk/orangepi_sdk/toolchains/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
#将目标文件编译成一个可加载的内核模块
obj-m += imx415.o
#搜索特定模块
EXTRA_CFLAGS += -I/home/chen/1.chip_sdk/orangepi_sdk/kernel/orange-pi-5.10-rk35xx/drivers/media/platform/rockchip/isp
#指定源码路径
KDIR:=/home/chen/1.chip_sdk/orangepi_sdk/kernel/orange-pi-5.10-rk35xx
#获取当前工作目录
PWD?=$(shell pwd)
#指定内核源码的目录下编译当前目录为源目录的内核模块
all:
	make -C $(KDIR) M=$(PWD) modules
#清楚编译过程中产生的文件
clean:
	make -C $(KDIR) M=$(PWD) clean