// SPDX-License-Identifier: GPL-2.0
/*
 * imx415 driver
 *
 * Copyright (C) 2020 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X00 first version.
 * V0.0X01.0X01
 *  1. fix hdr ae ratio error,
 *     0x3260 should be set 0x01 in normal mode,
 *     should be 0x00 in hdr mode.
 *  2. rhs1 should be 4n+1 when set hdr ae.
 * V0.0X01.0X02
 *  1. shr0 should be greater than (rsh1 + 9).
 *  2. rhs1 should be ceil to 4n + 1.
 * V0.0X01.0X03
 *  1. support 12bit HDR DOL3
 *  2. support HDR/Linear quick switch
 * V0.0X01.0X04
 * 1. support enum format info by aiq
 * V0.0X01.0X05
 * 1. fixed 10bit hdr2/hdr3 frame rate issue
 * V0.0X01.0X06
 * 1. support DOL3 10bit 20fps 1485Mbps
 * 2. fixed linkfreq error
 * V0.0X01.0X07
 * 1. fix set_fmt & ioctl get mode unmatched issue.
 * 2. need to set default vblank when change format.
 * 3. enum all supported mode mbus_code, not just cur_mode.
 * V0.0X01.0X08
 * 1. add dcphy param for hdrx2 mode.
 */

#define DEBUG
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/rk-camera-module.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>
#include <linux/rk-preisp.h>
#include "rkisp_tb_helper.h"

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x08)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define MIPI_FREQ_891M			891000000
#define MIPI_FREQ_446M			446000000
#define MIPI_FREQ_743M			743000000
#define MIPI_FREQ_297M			297000000

#define IMX415_4LANES			4

#define IMX415_MAX_PIXEL_RATE		(MIPI_FREQ_891M / 10 * 2 * IMX415_4LANES)
#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"

#define IMX415_XVCLK_FREQ_37M		37125000

/* TODO: Get the real chip id from reg */
#define CHIP_ID				0xE0
#define IMX415_REG_CHIP_ID		0x311A

#define IMX415_REG_CTRL_MODE		0x3000
#define IMX415_MODE_SW_STANDBY		BIT(0)
#define IMX415_MODE_STREAMING		0x0

#define IMX415_LF_GAIN_REG_H		0x3091
#define IMX415_LF_GAIN_REG_L		0x3090

#define IMX415_SF1_GAIN_REG_H		0x3093
#define IMX415_SF1_GAIN_REG_L		0x3092

#define IMX415_SF2_GAIN_REG_H		0x3095
#define IMX415_SF2_GAIN_REG_L		0x3094

#define IMX415_LF_EXPO_REG_H		0x3052
#define IMX415_LF_EXPO_REG_M		0x3051
#define IMX415_LF_EXPO_REG_L		0x3050

#define IMX415_SF1_EXPO_REG_H		0x3056
#define IMX415_SF1_EXPO_REG_M		0x3055
#define IMX415_SF1_EXPO_REG_L		0x3054

#define IMX415_SF2_EXPO_REG_H		0x305A
#define IMX415_SF2_EXPO_REG_M		0x3059
#define IMX415_SF2_EXPO_REG_L		0x3058

#define IMX415_RHS1_REG_H		0x3062
#define IMX415_RHS1_REG_M		0x3061
#define IMX415_RHS1_REG_L		0x3060
#define IMX415_RHS1_DEFAULT		0x004D

#define IMX415_RHS2_REG_H		0x3066
#define IMX415_RHS2_REG_M		0x3065
#define IMX415_RHS2_REG_L		0x3064
#define IMX415_RHS2_DEFAULT		0x004D

#define	IMX415_EXPOSURE_MIN		4
#define	IMX415_EXPOSURE_STEP		1
#define IMX415_VTS_MAX			0x7fff

#define IMX415_GAIN_MIN			0x00
#define IMX415_GAIN_MAX			0xf0
#define IMX415_GAIN_STEP		1
#define IMX415_GAIN_DEFAULT		0x00
//#define IMX415_GAIN_DEFAULT		0x10 // 增大默认增益

#define IMX415_FETCH_GAIN_H(VAL)	(((VAL) >> 8) & 0x07)
#define IMX415_FETCH_GAIN_L(VAL)	((VAL) & 0xFF)

#define IMX415_FETCH_EXP_H(VAL)		(((VAL) >> 16) & 0x0F)
#define IMX415_FETCH_EXP_M(VAL)		(((VAL) >> 8) & 0xFF)
#define IMX415_FETCH_EXP_L(VAL)		((VAL) & 0xFF)

#define IMX415_FETCH_RHS1_H(VAL)	(((VAL) >> 16) & 0x0F)
#define IMX415_FETCH_RHS1_M(VAL)	(((VAL) >> 8) & 0xFF)
#define IMX415_FETCH_RHS1_L(VAL)	((VAL) & 0xFF)

#define IMX415_FETCH_VTS_H(VAL)		(((VAL) >> 16) & 0x0F)
#define IMX415_FETCH_VTS_M(VAL)		(((VAL) >> 8) & 0xFF)
#define IMX415_FETCH_VTS_L(VAL)		((VAL) & 0xFF)

#define IMX415_VTS_REG_L		0x3024
#define IMX415_VTS_REG_M		0x3025
#define IMX415_VTS_REG_H		0x3026

#define IMX415_MIRROR_BIT_MASK		BIT(0)
#define IMX415_FLIP_BIT_MASK		BIT(1)
#define IMX415_FLIP_REG			0x3030

#define REG_NULL			0xFFFF

#define IMX415_REG_VALUE_08BIT		1
#define IMX415_REG_VALUE_16BIT		2
#define IMX415_REG_VALUE_24BIT		3

#define IMX415_GROUP_HOLD_REG		0x3001
#define IMX415_GROUP_HOLD_START		0x01
#define IMX415_GROUP_HOLD_END		0x00

/* Basic Readout Lines. Number of necessary readout lines in sensor */
#define BRL_ALL				2228u
#define BRL_BINNING			1115u
/* Readout timing setting of SEF1(DOL2): RHS1 < 2 * BRL and should be 4n + 1 */
#define RHS1_MAX_X2(VAL)		(((VAL) * 2 - 1) / 4 * 4 + 1)
#define SHR1_MIN_X2			9u

/* Readout timing setting of SEF1(DOL3): RHS1 < 3 * BRL and should be 6n + 1 */
#define RHS1_MAX_X3(VAL)		(((VAL) * 3 - 1) / 6 * 6 + 1)
#define SHR1_MIN_X3			13u

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"sleep"

#define IMX415_NAME			"imx415"

/* 电源供应名称数组，用于定义摄像头模块所需的电源供应名称 */
static const char * const imx415_supply_names[] = {
	"dvdd",		/* Digital core power */
	"dovdd",	/* Digital I/O power */
	"avdd",		/* Analog power */
};

#define IMX415_NUM_SUPPLIES ARRAY_SIZE(imx415_supply_names)

/* 寄存器地址和值的结构体，用于存储摄像头寄存器的设置列表 */
struct regval {
	u16 addr;
	u8 val;
};

/* 模式结构体，包含了摄像头的各种模式参数和寄存器设置列表 */
struct imx415_mode {
	u32 bus_fmt; 							// 媒体总线格式
	u32 width;								// 图像宽度
	u32 height;								// 图像高度
	struct v4l2_fract max_fps;				// 最大帧率
	u32 hts_def;							// 水平时钟周期数
	u32 vts_def;							// 垂直时钟周期数
	u32 exp_def;							// 默认曝光时间
	u32 mipi_freq_idx;						// MIPI频率索引
	u32 bpp;								// 每像素位数
	const struct regval *global_reg_list;	// 全局寄存器设置列表
	const struct regval *reg_list;			// 模式寄存器设置列表
	u32 hdr_mode;							// HDR 模式标志
	u32 vc[PAD_MAX];						// 虚拟通道配置
};

/* 设备结构体，包含了摄像头设备的各种资源和状态信息 */
struct imx415 {
	struct i2c_client	*client;								// I2C客户端结构体指针
	struct clk		*xvclk;										// 外部时钟
	struct gpio_desc	*reset_gpio;							// 复位GPIO描述符
	// struct gpio_desc	*power_gpio; 							// 5 -plus没有用上
	struct regulator_bulk_data supplies[IMX415_NUM_SUPPLIES];	// 电源供应数据结构数组

	struct pinctrl		*pinctrl;								// Pin控制结构体指针
	struct pinctrl_state	*pins_default;						// Pin控制默认状态指针
	struct pinctrl_state	*pins_sleep;						// Pin控制睡眠状态指针

	struct v4l2_subdev	subdev;				// V4L2子设备结构体
	struct media_pad	pad;				// 媒体Pad结构体
	struct v4l2_ctrl_handler ctrl_handler;	// V4L2控制处理程序结构体
	struct v4l2_ctrl	*exposure;			// 曝光控制
	struct v4l2_ctrl	*anal_a_gain;		// 模拟增益控制
	struct v4l2_ctrl	*digi_gain;			// 数字增益控制
	struct v4l2_ctrl	*hblank;			// 水平空白控制
	struct v4l2_ctrl	*vblank;			// 垂直空白控制
	struct v4l2_ctrl	*pixel_rate;		// 像素速率控制
	struct v4l2_ctrl	*link_freq;			// 链路频率控制
	struct mutex		mutex;				// 互斥锁，用于保护设备数据结构
	bool			streaming;				// 流状态标志
	bool			power_on; 				// 电源状态标志
	bool			is_thunderboot;			// 快速启动标志
	bool			is_thunderboot_ng;		// 快速启动不成功标志
	bool			is_first_streamoff;		// 第一次流关闭标志
	const struct imx415_mode *cur_mode;		// 当前模式指针
	u32			module_index;				// 模块索引
	u32			cfg_num;					// 配置数量
	const char		*module_facing;			// 模块朝向字符串
	const char		*module_name;			// 模块名称字符串
	const char		*len_name;				// 长度名称字符串
	u32			cur_vts;					// 当前垂直时钟周期数
	bool			has_init_exp;			// 是否已初始化曝光标志
	struct preisp_hdrae_exp_s init_hdrae_exp; // 初始HDR AE曝光参数结构体
};

/* MIPI D-PHY参数结构体，包含了D-PHY的各种参数设置 */
static struct rkmodule_csi_dphy_param dcphy_param = {
	.vendor = PHY_VENDOR_SAMSUNG,		// D-PHY供应商
	.lp_vol_ref = 6,					// 低功耗电压参考值
	.lp_hys_sw = {3, 0, 0, 0},			// 低功耗滞后开关设置
	.lp_escclk_pol_sel = {1, 1, 1, 1},	// 低功耗逃生时钟极性选择
	.skew_data_cal_clk = {0, 3, 3, 3},	// 数据校准时钟偏移设置
	.clk_hs_term_sel = 2,				// 时钟高速终端选择
	.data_hs_term_sel = {2, 2, 2, 2},	// 数据高速终端选择
	.reserved = {0},					// 保留字段
};

#define to_imx415(sd) container_of(sd, struct imx415, subdev)

/*
 * Xclk 37.125Mhz
 */
static __maybe_unused const struct regval imx415_global_12bit_3864x2192_regs[] = {
	{0x3002, 0x00},
	{0x3008, 0x7F},
	{0x300A, 0x5B},
	{0x30C1, 0x00},
	{0x3031, 0x01},
	{0x3032, 0x01},
	{0x30D9, 0x06},
	{0x3116, 0x24},
	{0x3118, 0xC0},
	{0x311E, 0x24},
	{0x32D4, 0x21},
	{0x32EC, 0xA1},
	{0x3452, 0x7F},
	{0x3453, 0x03},
	{0x358A, 0x04},
	{0x35A1, 0x02},
	{0x36BC, 0x0C},
	{0x36CC, 0x53},
	{0x36CD, 0x00},
	{0x36CE, 0x3C},
	{0x36D0, 0x8C},
	{0x36D1, 0x00},
	{0x36D2, 0x71},
	{0x36D4, 0x3C},
	{0x36D6, 0x53},
	{0x36D7, 0x00},
	{0x36D8, 0x71},
	{0x36DA, 0x8C},
	{0x36DB, 0x00},
	{0x3701, 0x03},
	{0x3724, 0x02},
	{0x3726, 0x02},
	{0x3732, 0x02},
	{0x3734, 0x03},
	{0x3736, 0x03},
	{0x3742, 0x03},
	{0x3862, 0xE0},
	{0x38CC, 0x30},
	{0x38CD, 0x2F},
	{0x395C, 0x0C},
	{0x3A42, 0xD1},
	{0x3A4C, 0x77},
	{0x3AE0, 0x02},
	{0x3AEC, 0x0C},
	{0x3B00, 0x2E},
	{0x3B06, 0x29},
	{0x3B98, 0x25},
	{0x3B99, 0x21},
	{0x3B9B, 0x13},
	{0x3B9C, 0x13},
	{0x3B9D, 0x13},
	{0x3B9E, 0x13},
	{0x3BA1, 0x00},
	{0x3BA2, 0x06},
	{0x3BA3, 0x0B},
	{0x3BA4, 0x10},
	{0x3BA5, 0x14},
	{0x3BA6, 0x18},
	{0x3BA7, 0x1A},
	{0x3BA8, 0x1A},
	{0x3BA9, 0x1A},
	{0x3BAC, 0xED},
	{0x3BAD, 0x01},
	{0x3BAE, 0xF6},
	{0x3BAF, 0x02},
	{0x3BB0, 0xA2},
	{0x3BB1, 0x03},
	{0x3BB2, 0xE0},
	{0x3BB3, 0x03},
	{0x3BB4, 0xE0},
	{0x3BB5, 0x03},
	{0x3BB6, 0xE0},
	{0x3BB7, 0x03},
	{0x3BB8, 0xE0},
	{0x3BBA, 0xE0},
	{0x3BBC, 0xDA},
	{0x3BBE, 0x88},
	{0x3BC0, 0x44},
	{0x3BC2, 0x7B},
	{0x3BC4, 0xA2},
	{0x3BC8, 0xBD},
	{0x3BCA, 0xBD},
	{0x4004, 0x48},
	{0x4005, 0x09},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx415_linear_12bit_3864x2192_891M_regs[] = {
	{0x3020, 0x00},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3024, 0xCA},
	{0x3025, 0x08},
	{0x3028, 0x4C},
	{0x3029, 0x04},
	{0x302C, 0x00},
	{0x302D, 0x00},
	{0x3033, 0x05},
	{0x3050, 0x08},
	{0x3051, 0x00},
	{0x3054, 0x19},
	{0x3058, 0x3E},
	{0x3060, 0x25},
	{0x3064, 0x4A},
	{0x30CF, 0x00},
	{0x3260, 0x01},
	{0x400C, 0x00},
	{0x4018, 0x7F},
	{0x401A, 0x37},
	{0x401C, 0x37},
	{0x401E, 0xF7},
	{0x401F, 0x00},
	{0x4020, 0x3F},
	{0x4022, 0x6F},
	{0x4024, 0x3F},
	{0x4026, 0x5F},
	{0x4028, 0x2F},
	{0x4074, 0x01},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx415_hdr2_12bit_3864x2192_1782M_regs[] = {
	{0x3020, 0x00},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3024, 0xCA},
	{0x3025, 0x08},
	{0x3028, 0x26},
	{0x3029, 0x02},
	{0x302C, 0x01},
	{0x302D, 0x01},
	{0x3033, 0x04},
	{0x3050, 0x90},
	{0x3051, 0x0D},
	{0x3054, 0x09},
	{0x3058, 0x3E},
	{0x3060, 0x4D},
	{0x3064, 0x4A},
	{0x30CF, 0x01},
	{0x3260, 0x00},
	{0x400C, 0x01},
	{0x4018, 0xB7},
	{0x401A, 0x67},
	{0x401C, 0x6F},
	{0x401E, 0xDF},
	{0x401F, 0x01},
	{0x4020, 0x6F},
	{0x4022, 0xCF},
	{0x4024, 0x6F},
	{0x4026, 0xB7},
	{0x4028, 0x5F},
	{0x4074, 0x00},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx415_hdr3_12bit_3864x2192_1782M_regs[] = {
	{0x3020, 0x00},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3024, 0x96},
	{0x3025, 0x06},
	{0x3028, 0x26},
	{0x3029, 0x02},
	{0x302C, 0x01},
	{0x302D, 0x02},
	{0x3033, 0x04},
	{0x3050, 0x14},
	{0x3051, 0x01},
	{0x3054, 0x0D},
	{0x3058, 0x26},
	{0x3060, 0x19},
	{0x3064, 0x32},
	{0x30CF, 0x03},
	{0x3260, 0x00},
	{0x400C, 0x01},
	{0x4018, 0xB7},
	{0x401A, 0x67},
	{0x401C, 0x6F},
	{0x401E, 0xDF},
	{0x401F, 0x01},
	{0x4020, 0x6F},
	{0x4022, 0xCF},
	{0x4024, 0x6F},
	{0x4026, 0xB7},
	{0x4028, 0x5F},
	{0x4074, 0x00},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx415_global_10bit_3864x2192_regs[] = {
	{0x3002, 0x00},
	{0x3008, 0x7F},
	{0x300A, 0x5B},
	{0x3031, 0x00},
	{0x3032, 0x00},
	{0x30C1, 0x00},
	{0x30D9, 0x06},
	{0x3116, 0x24},
	{0x311E, 0x24},
	{0x32D4, 0x21},
	{0x32EC, 0xA1},
	{0x3452, 0x7F},
	{0x3453, 0x03},
	{0x358A, 0x04},
	{0x35A1, 0x02},
	{0x36BC, 0x0C},
	{0x36CC, 0x53},
	{0x36CD, 0x00},
	{0x36CE, 0x3C},
	{0x36D0, 0x8C},
	{0x36D1, 0x00},
	{0x36D2, 0x71},
	{0x36D4, 0x3C},
	{0x36D6, 0x53},
	{0x36D7, 0x00},
	{0x36D8, 0x71},
	{0x36DA, 0x8C},
	{0x36DB, 0x00},
	{0x3701, 0x00},
	{0x3724, 0x02},
	{0x3726, 0x02},
	{0x3732, 0x02},
	{0x3734, 0x03},
	{0x3736, 0x03},
	{0x3742, 0x03},
	{0x3862, 0xE0},
	{0x38CC, 0x30},
	{0x38CD, 0x2F},
	{0x395C, 0x0C},
	{0x3A42, 0xD1},
	{0x3A4C, 0x77},
	{0x3AE0, 0x02},
	{0x3AEC, 0x0C},
	{0x3B00, 0x2E},
	{0x3B06, 0x29},
	{0x3B98, 0x25},
	{0x3B99, 0x21},
	{0x3B9B, 0x13},
	{0x3B9C, 0x13},
	{0x3B9D, 0x13},
	{0x3B9E, 0x13},
	{0x3BA1, 0x00},
	{0x3BA2, 0x06},
	{0x3BA3, 0x0B},
	{0x3BA4, 0x10},
	{0x3BA5, 0x14},
	{0x3BA6, 0x18},
	{0x3BA7, 0x1A},
	{0x3BA8, 0x1A},
	{0x3BA9, 0x1A},
	{0x3BAC, 0xED},
	{0x3BAD, 0x01},
	{0x3BAE, 0xF6},
	{0x3BAF, 0x02},
	{0x3BB0, 0xA2},
	{0x3BB1, 0x03},
	{0x3BB2, 0xE0},
	{0x3BB3, 0x03},
	{0x3BB4, 0xE0},
	{0x3BB5, 0x03},
	{0x3BB6, 0xE0},
	{0x3BB7, 0x03},
	{0x3BB8, 0xE0},
	{0x3BBA, 0xE0},
	{0x3BBC, 0xDA},
	{0x3BBE, 0x88},
	{0x3BC0, 0x44},
	{0x3BC2, 0x7B},
	{0x3BC4, 0xA2},
	{0x3BC8, 0xBD},
	{0x3BCA, 0xBD},
	{0x4004, 0x48},
	{0x4005, 0x09},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx415_hdr3_10bit_3864x2192_1485M_regs[] = {
	{0x3020, 0x00},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3024, 0xBD},
	{0x3025, 0x06},
	{0x3028, 0x1A},
	{0x3029, 0x02},
	{0x302C, 0x01},
	{0x302D, 0x02},
	{0x3033, 0x08},
	{0x3050, 0x90},
	{0x3051, 0x15},
	{0x3054, 0x0D},
	{0x3058, 0xA4},
	{0x3060, 0x97},
	{0x3064, 0xB6},
	{0x30CF, 0x03},
	{0x3118, 0xA0},
	{0x3260, 0x00},
	{0x400C, 0x01},
	{0x4018, 0xA7},
	{0x401A, 0x57},
	{0x401C, 0x5F},
	{0x401E, 0x97},
	{0x401F, 0x01},
	{0x4020, 0x5F},
	{0x4022, 0xAF},
	{0x4024, 0x5F},
	{0x4026, 0x9F},
	{0x4028, 0x4F},
	{0x4074, 0x00},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx415_hdr3_10bit_3864x2192_1782M_regs[] = {
	{0x3020, 0x00},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3024, 0xEA},
	{0x3025, 0x07},
	{0x3028, 0xCA},
	{0x3029, 0x01},
	{0x302C, 0x01},
	{0x302D, 0x02},
	{0x3033, 0x04},
	{0x3050, 0x3E},
	{0x3051, 0x01},
	{0x3054, 0x0D},
	{0x3058, 0x9E},
	{0x3060, 0x91},
	{0x3064, 0xC2},
	{0x30CF, 0x03},
	{0x3118, 0xC0},
	{0x3260, 0x00},
	{0x400C, 0x01},
	{0x4018, 0xB7},
	{0x401A, 0x67},
	{0x401C, 0x6F},
	{0x401E, 0xDF},
	{0x401F, 0x01},
	{0x4020, 0x6F},
	{0x4022, 0xCF},
	{0x4024, 0x6F},
	{0x4026, 0xB7},
	{0x4028, 0x5F},
	{0x4074, 0x00},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx415_hdr2_10bit_3864x2192_1485M_regs[] = {
	{0x3020, 0x00},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3024, 0xFC},
	{0x3025, 0x08},
	{0x3028, 0x1A},
	{0x3029, 0x02},
	{0x302C, 0x01},
	{0x302D, 0x01},
	{0x3033, 0x08},
	{0x3050, 0xA8},
	{0x3051, 0x0D},
	{0x3054, 0x09},
	{0x3058, 0x3E},
	{0x3060, 0x4D},
	{0x3064, 0x4a},
	{0x30CF, 0x01},
	{0x3118, 0xA0},
	{0x3260, 0x00},
	{0x400C, 0x01},
	{0x4018, 0xA7},
	{0x401A, 0x57},
	{0x401C, 0x5F},
	{0x401E, 0x97},
	{0x401F, 0x01},
	{0x4020, 0x5F},
	{0x4022, 0xAF},
	{0x4024, 0x5F},
	{0x4026, 0x9F},
	{0x4028, 0x4F},
	{0x4074, 0x00},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx415_linear_10bit_3864x2192_891M_regs[] = {
	{0x3020, 0x00},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3024, 0xCA},
	{0x3025, 0x08},
	{0x3028, 0x4C},
	{0x3029, 0x04},
	{0x302C, 0x00},
	{0x302D, 0x00},
	{0x3033, 0x05},
	{0x3050, 0x08},
	{0x3051, 0x00},
	{0x3054, 0x19},
	{0x3058, 0x3E},
	{0x3060, 0x25},
	{0x3064, 0x4a},
	{0x30CF, 0x00},
	{0x3118, 0xC0},
	{0x3260, 0x01},
	{0x400C, 0x00},
	{0x4018, 0x7F},
	{0x401A, 0x37},
	{0x401C, 0x37},
	{0x401E, 0xF7},
	{0x401F, 0x00},
	{0x4020, 0x3F},
	{0x4022, 0x6F},
	{0x4024, 0x3F},
	{0x4026, 0x5F},
	{0x4028, 0x2F},
	{0x4074, 0x01},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx415_linear_12bit_1932x1096_594M_regs[] = {
	{0x3020, 0x01},
	{0x3021, 0x01},
	{0x3022, 0x01},
	{0x3024, 0x5D},
	{0x3025, 0x0C},
	{0x3028, 0x0E},
	{0x3029, 0x03},
	{0x302C, 0x00},
	{0x302D, 0x00},
	{0x3031, 0x00},
	{0x3033, 0x07},
	{0x3050, 0x08},
	{0x3051, 0x00},
	{0x3054, 0x19},
	{0x3058, 0x3E},
	{0x3060, 0x25},
	{0x3064, 0x4A},
	{0x30CF, 0x00},
	{0x30D9, 0x02},
	{0x30DA, 0x01},
	{0x3118, 0x80},
	{0x3260, 0x01},
	{0x3701, 0x00},
	{0x400C, 0x00},
	{0x4018, 0x67},
	{0x401A, 0x27},
	{0x401C, 0x27},
	{0x401E, 0xB7},
	{0x401F, 0x00},
	{0x4020, 0x2F},
	{0x4022, 0x4F},
	{0x4024, 0x2F},
	{0x4026, 0x47},
	{0x4028, 0x27},
	{0x4074, 0x01},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx415_hdr2_12bit_1932x1096_891M_regs[] = {
	{0x3020, 0x01},
	{0x3021, 0x01},
	{0x3022, 0x01},
	{0x3024, 0xFC},
	{0x3025, 0x08},
	{0x3028, 0x1A},
	{0x3029, 0x02},
	{0x302C, 0x01},
	{0x302D, 0x01},
	{0x3031, 0x00},
	{0x3033, 0x05},
	{0x3050, 0xB8},
	{0x3051, 0x00},
	{0x3054, 0x09},
	{0x3058, 0x3E},
	{0x3060, 0x25},
	{0x3064, 0x4A},
	{0x30CF, 0x01},
	{0x30D9, 0x02},
	{0x30DA, 0x01},
	{0x3118, 0xC0},
	{0x3260, 0x00},
	{0x3701, 0x00},
	{0x400C, 0x00},
	{0x4018, 0xA7},
	{0x401A, 0x57},
	{0x401C, 0x5F},
	{0x401E, 0x97},
	{0x401F, 0x01},
	{0x4020, 0x5F},
	{0x4022, 0xAF},
	{0x4024, 0x5F},
	{0x4026, 0x9F},
	{0x4028, 0x4F},
	{0x4074, 0x01},
	{REG_NULL, 0x00},
};

/*
 * The width and height must be configured to be
 * the same as the current output resolution of the sensor.
 * The input width of the isp needs to be 16 aligned.
 * The input height of the isp needs to be 8 aligned.
 * If the width or height does not meet the alignment rules,
 * you can configure the cropping parameters with the following function to
 * crop out the appropriate resolution.
 * struct v4l2_subdev_pad_ops {
 *	.get_selection
 * }
 */
static const struct imx415_mode supported_modes[] = {
	/*
	 * frame rate = 1 / (Vtt * 1H) = 1 / (VMAX * 1H)
	 * VMAX >= (PIX_VWIDTH / 2) + 46 = height + 46
	 */
	{
		.bus_fmt = MEDIA_BUS_FMT_SGBRG10_1X10,
		.width = 3864,
		.height = 2192,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		//.exp_def = 0x08ca - 0x08 + 0x200, // 增大默认曝光时间
		.exp_def = 0x08ca - 0x08,
		.hts_def = 0x044c * IMX415_4LANES * 2,
		.vts_def = 0x08ca,
		.global_reg_list = imx415_global_10bit_3864x2192_regs,
		.reg_list = imx415_linear_10bit_3864x2192_891M_regs,
		.hdr_mode = NO_HDR,
		.mipi_freq_idx = 1,
		.bpp = 10,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_SGBRG10_1X10,
		.width = 3864,
		.height = 2192,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x08fc * 2 - 0x0da8,
		.hts_def = 0x0226 * IMX415_4LANES * 2,
		/*
		 * IMX415 HDR mode T-line is half of Linear mode,
		 * make vts double to workaround.
		 */
		.vts_def = 0x08fc * 2,
		.global_reg_list = imx415_global_10bit_3864x2192_regs,
		.reg_list = imx415_hdr2_10bit_3864x2192_1485M_regs,
		.hdr_mode = HDR_X2,
		.mipi_freq_idx = 2,
		.bpp = 10,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD1] = V4L2_MBUS_CSI2_CHANNEL_0,//L->csi wr0
		.vc[PAD2] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD3] = V4L2_MBUS_CSI2_CHANNEL_1,//M->csi wr2
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_SGBRG10_1X10,
		.width = 3864,
		.height = 2192,
		.max_fps = {
			.numerator = 10000,
			.denominator = 200000,
		},
		.exp_def = 0x13e,
		.hts_def = 0x021A * IMX415_4LANES * 2,
		/*
		 * IMX415 HDR mode T-line is half of Linear mode,
		 * make vts double to workaround.
		 */
		.vts_def = 0x06BD * 4,
		.global_reg_list = imx415_global_10bit_3864x2192_regs,
		.reg_list = imx415_hdr3_10bit_3864x2192_1485M_regs,
		.hdr_mode = HDR_X3,
		.mipi_freq_idx = 2,
		.bpp = 10,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_2,
		.vc[PAD1] = V4L2_MBUS_CSI2_CHANNEL_1,//M->csi wr0
		.vc[PAD2] = V4L2_MBUS_CSI2_CHANNEL_0,//L->csi wr0
		.vc[PAD3] = V4L2_MBUS_CSI2_CHANNEL_2,//S->csi wr2
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_SGBRG10_1X10,
		.width = 3864,
		.height = 2192,
		.max_fps = {
			.numerator = 10000,
			.denominator = 200000,
		},
		.exp_def = 0x13e,
		.hts_def = 0x01ca * IMX415_4LANES * 2,
		/*
		 * IMX415 HDR mode T-line is half of Linear mode,
		 * make vts double to workaround.
		 */
		.vts_def = 0x07ea * 4,
		.global_reg_list = imx415_global_10bit_3864x2192_regs,
		.reg_list = imx415_hdr3_10bit_3864x2192_1782M_regs,
		.hdr_mode = HDR_X3,
		.mipi_freq_idx = 3,
		.bpp = 10,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_2,
		.vc[PAD1] = V4L2_MBUS_CSI2_CHANNEL_1,//M->csi wr0
		.vc[PAD2] = V4L2_MBUS_CSI2_CHANNEL_0,//L->csi wr0
		.vc[PAD3] = V4L2_MBUS_CSI2_CHANNEL_2,//S->csi wr2
	},
	{
		/* 1H period = (1100 clock) = (1100 * 1 / 74.25MHz) */
		.bus_fmt = MEDIA_BUS_FMT_SGBRG12_1X12,
		.width = 3864,
		.height = 2192,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x08ca - 0x08,
		.hts_def = 0x044c * IMX415_4LANES * 2,
		.vts_def = 0x08ca,
		.global_reg_list = imx415_global_12bit_3864x2192_regs,
		.reg_list = imx415_linear_12bit_3864x2192_891M_regs,
		.hdr_mode = NO_HDR,
		.mipi_freq_idx = 1,
		.bpp = 12,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_SGBRG12_1X12,
		.width = 3864,
		.height = 2192,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x08CA * 2 - 0x0d90,
		.hts_def = 0x0226 * IMX415_4LANES * 2,
		/*
		 * IMX415 HDR mode T-line is half of Linear mode,
		 * make vts double(that is FSC) to workaround.
		 */
		.vts_def = 0x08CA * 2,
		.global_reg_list = imx415_global_12bit_3864x2192_regs,
		.reg_list = imx415_hdr2_12bit_3864x2192_1782M_regs,
		.hdr_mode = HDR_X2,
		.mipi_freq_idx = 3,
		.bpp = 12,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD1] = V4L2_MBUS_CSI2_CHANNEL_0,//L->csi wr0
		.vc[PAD2] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD3] = V4L2_MBUS_CSI2_CHANNEL_1,//M->csi wr2
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_SGBRG12_1X12,
		.width = 3864,
		.height = 2192,
		.max_fps = {
			.numerator = 10000,
			.denominator = 200000,
		},
		.exp_def = 0x114,
		.hts_def = 0x0226 * IMX415_4LANES * 2,
		/*
		 * IMX415 HDR mode T-line is half of Linear mode,
		 * make vts double(that is FSC) to workaround.
		 */
		.vts_def = 0x0696 * 4,
		.global_reg_list = imx415_global_12bit_3864x2192_regs,
		.reg_list = imx415_hdr3_12bit_3864x2192_1782M_regs,
		.hdr_mode = HDR_X3,
		.mipi_freq_idx = 3,
		.bpp = 12,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_2,
		.vc[PAD1] = V4L2_MBUS_CSI2_CHANNEL_1,//M->csi wr0
		.vc[PAD2] = V4L2_MBUS_CSI2_CHANNEL_0,//L->csi wr0
		.vc[PAD3] = V4L2_MBUS_CSI2_CHANNEL_2,//S->csi wr2
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_SGBRG12_1X12,
		.width = 1944,
		.height = 1097,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x05dc - 0x08,
		.hts_def = 0x030e * 3,
		.vts_def = 0x0c5d,
		.global_reg_list = imx415_global_12bit_3864x2192_regs,
		.reg_list = imx415_linear_12bit_1932x1096_594M_regs,
		.hdr_mode = NO_HDR,
		.mipi_freq_idx = 0,
		.bpp = 12,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_SGBRG12_1X12,
		.width = 1944,
		.height = 1097,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x08FC / 4,
		.hts_def = 0x021A * 4,
		/*
		 * IMX415 HDR mode T-line is half of Linear mode,
		 * make vts double(that is FSC) to workaround.
		 */
		.vts_def = 0x08FC * 2,
		.global_reg_list = imx415_global_12bit_3864x2192_regs,
		.reg_list = imx415_hdr2_12bit_1932x1096_891M_regs,
		.hdr_mode = HDR_X2,
		.mipi_freq_idx = 1,
		.bpp = 12,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD1] = V4L2_MBUS_CSI2_CHANNEL_0,//L->csi wr0
		.vc[PAD2] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD3] = V4L2_MBUS_CSI2_CHANNEL_1,//M->csi wr2
	},
};

/* MIPI lane 传输速率，单位为赫兹 */
static const s64 link_freq_items[] = {
	MIPI_FREQ_297M,
	MIPI_FREQ_446M,
	MIPI_FREQ_743M,
	MIPI_FREQ_891M,
};

/* 写寄存器，寄存器地址为16位，寄存器值最大为32位 */
static int imx415_write_reg(struct i2c_client *client, u16 reg,
			    u32 len, u32 val)
{
	u32 buf_i, val_i;
	u8 buf[6];
	u8 *val_p;
	__be32 val_be;

	if (len > 4)
		return -EINVAL;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	val_be = cpu_to_be32(val);
	val_p = (u8 *)&val_be;
	buf_i = 2;
	val_i = 4 - len;

	while (val_i < 4)
		buf[buf_i++] = val_p[val_i++];

	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

/* 写寄存器列表，寄存器地址为16位，寄存器值最大为32位，列表以REG_NULL结尾 */
static int imx415_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++) {
		ret = imx415_write_reg(client, regs[i].addr,
				       IMX415_REG_VALUE_08BIT, regs[i].val);
	}
	return ret;
}

/* 读寄存器，寄存器地址为16位，寄存器值最大为32位 */
static int imx415_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
			   u32 *val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	__be16 reg_addr_be = cpu_to_be16(reg);
	int ret;

	if (len > 4 || !len)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;
	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = (u8 *)&reg_addr_be;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_be_p[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = be32_to_cpu(data_be);

	return 0;
}

/* 计算当前模式与用户设置的分辨率之间的距离，距离越小越匹配 */
static int imx415_get_reso_dist(const struct imx415_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

/* 根据用户设置的分辨率，从支持的模式列表中找到最匹配的模式，匹配规则如下：
 * 1. 模式的分辨率必须大于或等于用户设置的分辨率。
 * 2. 模式的格式必须与用户设置的格式相同。
 * 3. 在满足以上条件的模式中，选择分辨率与用户设置的分辨率距离最小的模式。
 */
static const struct imx415_mode *
imx415_find_best_fit(struct imx415 *imx415, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < imx415->cfg_num; i++) {
		dist = imx415_get_reso_dist(&supported_modes[i], framefmt);
		if ((cur_best_fit_dist == -1 || dist < cur_best_fit_dist) &&
			supported_modes[i].bus_fmt == framefmt->code) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}
	dev_info(&imx415->client->dev, "%s: cur_best_fit(%d)",
		 __func__, cur_best_fit);

	return &supported_modes[cur_best_fit];
}

/* 开启电源，配置寄存器使能时钟和复位 */
static int __imx415_power_on(struct imx415 *imx415);

/* 切换模式，切换模式前需要先开启电源，如果当前处于雷电引导且雷电引导状态为NG，则先关闭雷电引导再开启电源 */
static void imx415_change_mode(struct imx415 *imx415, const struct imx415_mode *mode)
{
	if (imx415->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
		imx415->is_thunderboot = false;
		imx415->is_thunderboot_ng = true;
		__imx415_power_on(imx415);
	}
	imx415->cur_mode = mode;
	imx415->cur_vts = imx415->cur_mode->vts_def;
	dev_dbg(&imx415->client->dev, "set fmt: cur_mode: %dx%d, hdr: %d\n",
		mode->width, mode->height, mode->hdr_mode);
}

/* 设置格式，设置格式时会切换模式，切换模式时会修改当前模式的垂直总线时间和水平总线时间的默认值，并将它们设置为当前模式的默认值 */
static int imx415_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct imx415 *imx415 = to_imx415(sd);
	const struct imx415_mode *mode;
	s64 h_blank, vblank_def, vblank_min;
	u64 pixel_rate = 0;

	mutex_lock(&imx415->mutex);

	mode = imx415_find_best_fit(imx415, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, cfg, fmt->pad) = fmt->format;
#else
		mutex_unlock(&imx415->mutex);
		return -ENOTTY;
#endif
	} else {
		imx415_change_mode(imx415, mode);
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(imx415->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		/* VMAX >= (PIX_VWIDTH / 2) + 46 = height + 46 */
		vblank_min = (mode->height + 46) - mode->height;
		__v4l2_ctrl_modify_range(imx415->vblank, vblank_min,
					 IMX415_VTS_MAX - mode->height,
					 1, vblank_def);
		__v4l2_ctrl_s_ctrl(imx415->vblank, vblank_def);
		__v4l2_ctrl_s_ctrl(imx415->link_freq, mode->mipi_freq_idx);
		pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] / mode->bpp * 2 * IMX415_4LANES;
		__v4l2_ctrl_s_ctrl_int64(imx415->pixel_rate,
					 pixel_rate);
	}
	dev_info(&imx415->client->dev, "%s: mode->mipi_freq_idx(%d)",
		 __func__, mode->mipi_freq_idx);

	mutex_unlock(&imx415->mutex);

	return 0;
}

/* 获取格式，获取格式时会返回当前模式的分辨率和格式，如果当前模式是HDR模式，还会返回当前模式的虚拟通道号 */
static int imx415_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct imx415 *imx415 = to_imx415(sd);
	const struct imx415_mode *mode = imx415->cur_mode;

	mutex_lock(&imx415->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
#else
		mutex_unlock(&imx415->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
		if (fmt->pad < PAD_MAX && mode->hdr_mode != NO_HDR)
			fmt->reserved[0] = mode->vc[fmt->pad];
		else
			fmt->reserved[0] = mode->vc[PAD0];
	}
	mutex_unlock(&imx415->mutex);

	return 0;
}

/* 枚举格式，枚举格式时会返回支持的格式列表，如果当前索引超过了支持的格式数量，则返回错误码 */
static int imx415_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx415 *imx415 = to_imx415(sd);

	if (code->index >= imx415->cfg_num)
		return -EINVAL;

	code->code = supported_modes[code->index].bus_fmt;

	return 0;
}

/* 枚举分辨率，枚举分辨率时会返回支持的分辨率列表，如果当前索引超过了支持的分辨率数量，或者当前格式与支持的格式不匹配，则返回错误码 */
static int imx415_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct imx415 *imx415 = to_imx415(sd);

	if (fse->index >= imx415->cfg_num)
		return -EINVAL;

	if (fse->code != supported_modes[fse->index].bus_fmt)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

/* 枚举帧率，枚举帧率时会返回支持的帧率列表，如果当前索引超过了支持的帧率数量，或者当前格式与支持的格式不匹配，则返回错误码 */
static int imx415_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct imx415 *imx415 = to_imx415(sd);
	const struct imx415_mode *mode = imx415->cur_mode;

	fi->interval = mode->max_fps;

	return 0;
}

/* 获取媒体总线配置，获取媒体总线配置时会返回当前模式的媒体总线配置，如果当前模式是HDR模式，还会返回当前模式的虚拟通道号 */
static int imx415_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	struct imx415 *imx415 = to_imx415(sd);
	const struct imx415_mode *mode = imx415->cur_mode;
	u32 val = 0;

	val = 1 << (IMX415_4LANES - 1) |
	      V4L2_MBUS_CSI2_CHANNEL_0 |
	      V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
	if (mode->hdr_mode != NO_HDR)
		val |= V4L2_MBUS_CSI2_CHANNEL_1;
	if (mode->hdr_mode == HDR_X3)
		val |= V4L2_MBUS_CSI2_CHANNEL_2;
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->flags = val;

	return 0;
}

/* 获取模块信息，获取模块信息时会返回当前传感器的名称、模块的名称和镜头的名称 */
static void imx415_get_module_inf(struct imx415 *imx415,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strlcpy(inf->base.sensor, IMX415_NAME, sizeof(inf->base.sensor));
	strlcpy(inf->base.module, imx415->module_name,
		sizeof(inf->base.module));
	strlcpy(inf->base.lens, imx415->len_name, sizeof(inf->base.lens));
}

/* 设置HDR AE，设置HDR AE时会根据用户设置的曝光时间和增益，计算出SHR0、SHR1、SHR2、RHS1和RHS2的值，并将它们写入寄存器 */
static int imx415_set_hdrae_3frame(struct imx415 *imx415,
				   struct preisp_hdrae_exp_s *ae)
{
	struct i2c_client *client = imx415->client;
	u32 l_exp_time, m_exp_time, s_exp_time;
	u32 l_a_gain, m_a_gain, s_a_gain;
	int shr2, shr1, shr0, rhs2, rhs1 = 0;
	int rhs1_change_limit, rhs2_change_limit = 0;
	static int rhs1_old = IMX415_RHS1_DEFAULT;
	static int rhs2_old = IMX415_RHS2_DEFAULT;
	int ret = 0;
	u32 fsc;
	int rhs1_max = 0;
	int shr2_min = 0;

	if (!imx415->has_init_exp && !imx415->streaming) {
		imx415->init_hdrae_exp = *ae;
		imx415->has_init_exp = true;
		dev_dbg(&imx415->client->dev, "imx415 is not streaming, save hdr ae!\n");
		return ret;
	}
	l_exp_time = ae->long_exp_reg;
	m_exp_time = ae->middle_exp_reg;
	s_exp_time = ae->short_exp_reg;
	l_a_gain = ae->long_gain_reg;
	m_a_gain = ae->middle_gain_reg;
	s_a_gain = ae->short_gain_reg;
	dev_dbg(&client->dev,
		"rev exp req: L_exp: 0x%x, 0x%x, M_exp: 0x%x, 0x%x S_exp: 0x%x, 0x%x\n",
		l_exp_time, m_exp_time, s_exp_time,
		l_a_gain, m_a_gain, s_a_gain);

	ret = imx415_write_reg(client, IMX415_GROUP_HOLD_REG,
		IMX415_REG_VALUE_08BIT, IMX415_GROUP_HOLD_START);
	/* gain effect n+1 */
	ret |= imx415_write_reg(client, IMX415_LF_GAIN_REG_H,
		IMX415_REG_VALUE_08BIT, IMX415_FETCH_GAIN_H(l_a_gain));
	ret |= imx415_write_reg(client, IMX415_LF_GAIN_REG_L,
		IMX415_REG_VALUE_08BIT, IMX415_FETCH_GAIN_L(l_a_gain));
	ret |= imx415_write_reg(client, IMX415_SF1_GAIN_REG_H,
		IMX415_REG_VALUE_08BIT, IMX415_FETCH_GAIN_H(m_a_gain));
	ret |= imx415_write_reg(client, IMX415_SF1_GAIN_REG_L,
		IMX415_REG_VALUE_08BIT, IMX415_FETCH_GAIN_L(m_a_gain));
	ret |= imx415_write_reg(client, IMX415_SF2_GAIN_REG_H,
		IMX415_REG_VALUE_08BIT, IMX415_FETCH_GAIN_H(s_a_gain));
	ret |= imx415_write_reg(client, IMX415_SF2_GAIN_REG_L,
		IMX415_REG_VALUE_08BIT, IMX415_FETCH_GAIN_L(s_a_gain));

	/* Restrictions
	 *   FSC = 4 * VMAX and FSC should be 6n;
	 *   exp_l = FSC - SHR0 + Toffset;
	 *
	 *   SHR0 = FSC - exp_l + Toffset;
	 *   SHR0 <= (FSC -12);
	 *   SHR0 >= RHS2 + 13;
	 *   SHR0 should be 3n;
	 *
	 *   exp_m = RHS1 - SHR1 + Toffset;
	 *
	 *   RHS1 < BRL * 3;
	 *   RHS1 <= SHR2 - 13;
	 *   RHS1 >= SHR1 + 12;
	 *   SHR1 >= 13;
	 *   SHR1 <= RHS1 - 12;
	 *   RHS1(n+1) >= RHS1(n) + BRL * 3 -FSC + 3;
	 *
	 *   SHR1 should be 3n+1 and RHS1 should be 6n+1;
	 *
	 *   exp_s = RHS2 - SHR2 + Toffset;
	 *
	 *   RHS2 < BRL * 3 + RHS1;
	 *   RHS2 <= SHR0 - 13;
	 *   RHS2 >= SHR2 + 12;
	 *   SHR2 >= RHS1 + 13;
	 *   SHR2 <= RHS2 - 12;
	 *   RHS1(n+1) >= RHS1(n) + BRL * 3 -FSC + 3;
	 *
	 *   SHR2 should be 3n+2 and RHS2 should be 6n+2;
	 */

	/* The HDR mode vts is double by default to workaround T-line */
	fsc = imx415->cur_vts;
	fsc = fsc / 6 * 6;
	shr0 = fsc - l_exp_time;
	dev_dbg(&client->dev,
		"line(%d) shr0 %d, l_exp_time %d, fsc %d\n",
		__LINE__, shr0, l_exp_time, fsc);

	rhs1 = (SHR1_MIN_X3 + m_exp_time + 5) / 6 * 6 + 1;
	if (imx415->cur_mode->height == 2192)
		rhs1_max = RHS1_MAX_X3(BRL_ALL);
	else
		rhs1_max = RHS1_MAX_X3(BRL_BINNING);
	if (rhs1 < 25)
		rhs1 = 25;
	else if (rhs1 > rhs1_max)
		rhs1 = rhs1_max;
	dev_dbg(&client->dev,
		"line(%d) rhs1 %d, m_exp_time %d rhs1_old %d\n",
		__LINE__, rhs1, m_exp_time, rhs1_old);

	//Dynamic adjustment rhs2 must meet the following conditions
	if (imx415->cur_mode->height == 2192)
		rhs1_change_limit = rhs1_old + 3 * BRL_ALL - fsc + 3;
	else
		rhs1_change_limit = rhs1_old + 3 * BRL_BINNING - fsc + 3;
	rhs1_change_limit = (rhs1_change_limit < 25) ? 25 : rhs1_change_limit;
	rhs1_change_limit = (rhs1_change_limit + 5) / 6 * 6 + 1;
	if (rhs1_max < rhs1_change_limit) {
		dev_err(&client->dev,
			"The total exposure limit makes rhs1 max is %d,but old rhs1 limit makes rhs1 min is %d\n",
			rhs1_max, rhs1_change_limit);
		return -EINVAL;
	}
	if (rhs1 < rhs1_change_limit)
		rhs1 = rhs1_change_limit;

	dev_dbg(&client->dev,
		"line(%d) m_exp_time %d rhs1_old %d, rhs1_new %d\n",
		__LINE__, m_exp_time, rhs1_old, rhs1);

	rhs1_old = rhs1;

	/* shr1 = rhs1 - s_exp_time */
	if (rhs1 - m_exp_time <= SHR1_MIN_X3) {
		shr1 = SHR1_MIN_X3;
		m_exp_time = rhs1 - shr1;
	} else {
		shr1 = rhs1 - m_exp_time;
	}

	shr2_min = rhs1 + 13;
	rhs2 = (shr2_min + s_exp_time + 5) / 6 * 6 + 2;
	if (rhs2 > (shr0 - 13))
		rhs2 = shr0 - 13;
	else if (rhs2 < 50)
		rhs2 = 50;
	dev_dbg(&client->dev,
		"line(%d) rhs2 %d, s_exp_time %d, rhs2_old %d\n",
		__LINE__, rhs2, s_exp_time, rhs2_old);

	//Dynamic adjustment rhs2 must meet the following conditions
	if (imx415->cur_mode->height == 2192)
		rhs2_change_limit = rhs2_old + 3 * BRL_ALL - fsc + 3;
	else
		rhs2_change_limit = rhs2_old + 3 * BRL_BINNING - fsc + 3;
	rhs2_change_limit = (rhs2_change_limit < 50) ?  50 : rhs2_change_limit;
	rhs2_change_limit = (rhs2_change_limit + 5) / 6 * 6 + 2;
	if ((shr0 - 13) < rhs2_change_limit) {
		dev_err(&client->dev,
			"The total exposure limit makes rhs2 max is %d,but old rhs1 limit makes rhs2 min is %d\n",
			shr0 - 13, rhs2_change_limit);
		return -EINVAL;
	}
	if (rhs2 < rhs2_change_limit)
		rhs2 = rhs2_change_limit;

	rhs2_old = rhs2;

	/* shr2 = rhs2 - s_exp_time */
	if (rhs2 - s_exp_time <= shr2_min) {
		shr2 = shr2_min;
		s_exp_time = rhs2 - shr2;
	} else {
		shr2 = rhs2 - s_exp_time;
	}
	dev_dbg(&client->dev,
		"line(%d) rhs2_new %d, s_exp_time %d shr2 %d, rhs2_change_limit %d\n",
		__LINE__, rhs2, s_exp_time, shr2, rhs2_change_limit);

	if (shr0 < rhs2 + 13)
		shr0 = rhs2 + 13;
	else if (shr0 > fsc - 12)
		shr0 = fsc - 12;

	dev_dbg(&client->dev,
		"long exposure: l_exp_time=%d, fsc=%d, shr0=%d, l_a_gain=%d\n",
		l_exp_time, fsc, shr0, l_a_gain);
	dev_dbg(&client->dev,
		"middle exposure(SEF1): m_exp_time=%d, rhs1=%d, shr1=%d, m_a_gain=%d\n",
		m_exp_time, rhs1, shr1, m_a_gain);
	dev_dbg(&client->dev,
		"short exposure(SEF2): s_exp_time=%d, rhs2=%d, shr2=%d, s_a_gain=%d\n",
		s_exp_time, rhs2, shr2, s_a_gain);
	/* time effect n+1 */
	/* write SEF2 exposure RHS2 regs*/
	ret |= imx415_write_reg(client,
		IMX415_RHS2_REG_L,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_RHS1_L(rhs2));
	ret |= imx415_write_reg(client,
		IMX415_RHS2_REG_M,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_RHS1_M(rhs2));
	ret |= imx415_write_reg(client,
		IMX415_RHS2_REG_H,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_RHS1_H(rhs2));
	/* write SEF2 exposure SHR2 regs*/
	ret |= imx415_write_reg(client,
		IMX415_SF2_EXPO_REG_L,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_EXP_L(shr2));
	ret |= imx415_write_reg(client,
		IMX415_SF2_EXPO_REG_M,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_EXP_M(shr2));
	ret |= imx415_write_reg(client,
		IMX415_SF2_EXPO_REG_H,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_EXP_H(shr2));
	/* write SEF1 exposure RHS1 regs*/
	ret |= imx415_write_reg(client,
		IMX415_RHS1_REG_L,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_RHS1_L(rhs1));
	ret |= imx415_write_reg(client,
		IMX415_RHS1_REG_M,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_RHS1_M(rhs1));
	ret |= imx415_write_reg(client,
		IMX415_RHS1_REG_H,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_RHS1_H(rhs1));
	/* write SEF1 exposure SHR1 regs*/
	ret |= imx415_write_reg(client,
		IMX415_SF1_EXPO_REG_L,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_EXP_L(shr1));
	ret |= imx415_write_reg(client,
		IMX415_SF1_EXPO_REG_M,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_EXP_M(shr1));
	ret |= imx415_write_reg(client,
		IMX415_SF1_EXPO_REG_H,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_EXP_H(shr1));
	/* write LF exposure SHR0 regs*/
	ret |= imx415_write_reg(client,
		IMX415_LF_EXPO_REG_L,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_EXP_L(shr0));
	ret |= imx415_write_reg(client,
		IMX415_LF_EXPO_REG_M,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_EXP_M(shr0));
	ret |= imx415_write_reg(client,
		IMX415_LF_EXPO_REG_H,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_EXP_H(shr0));

	ret |= imx415_write_reg(client, IMX415_GROUP_HOLD_REG,
		IMX415_REG_VALUE_08BIT, IMX415_GROUP_HOLD_END);
	return ret;
}

/* 设置HDR AE，设置HDR AE时会根据用户设置的曝光时间和增益，计算出SHR0、SHR1、RHS1的值，并将它们写入寄存器 */
static int imx415_set_hdrae(struct imx415 *imx415,
			    struct preisp_hdrae_exp_s *ae)
{
	struct i2c_client *client = imx415->client;
	u32 l_exp_time, m_exp_time, s_exp_time;
	u32 l_a_gain, m_a_gain, s_a_gain;
	int shr1, shr0, rhs1, rhs1_max, rhs1_min;
	static int rhs1_old = IMX415_RHS1_DEFAULT;
	int ret = 0;
	u32 fsc;

	if (!imx415->has_init_exp && !imx415->streaming) {
		imx415->init_hdrae_exp = *ae;
		imx415->has_init_exp = true;
		dev_dbg(&imx415->client->dev, "imx415 is not streaming, save hdr ae!\n");
		return ret;
	}
	l_exp_time = ae->long_exp_reg;
	m_exp_time = ae->middle_exp_reg;
	s_exp_time = ae->short_exp_reg;
	l_a_gain = ae->long_gain_reg;
	m_a_gain = ae->middle_gain_reg;
	s_a_gain = ae->short_gain_reg;
	dev_dbg(&client->dev,
		"rev exp req: L_exp: 0x%x, 0x%x, M_exp: 0x%x, 0x%x S_exp: 0x%x, 0x%x\n",
		l_exp_time, m_exp_time, s_exp_time,
		l_a_gain, m_a_gain, s_a_gain);

	if (imx415->cur_mode->hdr_mode == HDR_X2) {
		l_a_gain = m_a_gain;
		l_exp_time = m_exp_time;
	}

	ret = imx415_write_reg(client, IMX415_GROUP_HOLD_REG,
		IMX415_REG_VALUE_08BIT, IMX415_GROUP_HOLD_START);
	/* gain effect n+1 */
	ret |= imx415_write_reg(client, IMX415_LF_GAIN_REG_H,
		IMX415_REG_VALUE_08BIT, IMX415_FETCH_GAIN_H(l_a_gain));
	ret |= imx415_write_reg(client, IMX415_LF_GAIN_REG_L,
		IMX415_REG_VALUE_08BIT, IMX415_FETCH_GAIN_L(l_a_gain));
	ret |= imx415_write_reg(client, IMX415_SF1_GAIN_REG_H,
		IMX415_REG_VALUE_08BIT, IMX415_FETCH_GAIN_H(s_a_gain));
	ret |= imx415_write_reg(client, IMX415_SF1_GAIN_REG_L,
		IMX415_REG_VALUE_08BIT, IMX415_FETCH_GAIN_L(s_a_gain));

	/* Restrictions
	 *   FSC = 2 * VMAX and FSC should be 4n;
	 *   exp_l = FSC - SHR0 + Toffset;
	 *   exp_l should be even value;
	 *
	 *   SHR0 = FSC - exp_l + Toffset;
	 *   SHR0 <= (FSC -8);
	 *   SHR0 >= RHS1 + 9;
	 *   SHR0 should be 2n;
	 *
	 *   exp_s = RHS1 - SHR1 + Toffset;
	 *   exp_s should be even value;
	 *
	 *   RHS1 < BRL * 2;
	 *   RHS1 <= SHR0 - 9;
	 *   RHS1 >= SHR1 + 8;
	 *   SHR1 >= 9;
	 *   RHS1(n+1) >= RHS1(n) + BRL * 2 -FSC + 2;
	 *
	 *   SHR1 should be 2n+1 and RHS1 should be 4n+1;
	 */

	/* The HDR mode vts is double by default to workaround T-line */
	fsc = imx415->cur_vts;
	shr0 = fsc - l_exp_time;

	if (imx415->cur_mode->height == 2192) {
		rhs1_max = min(RHS1_MAX_X2(BRL_ALL), ((shr0 - 9u) / 4 * 4 + 1));
		rhs1_min = max(SHR1_MIN_X2 + 8u, rhs1_old + 2 * BRL_ALL - fsc + 2);
	} else {
		rhs1_max = min(RHS1_MAX_X2(BRL_BINNING), ((shr0 - 9u) / 4 * 4 + 1));
		rhs1_min = max(SHR1_MIN_X2 + 8u, rhs1_old + 2 * BRL_BINNING - fsc + 2);
	}
	rhs1_min = (rhs1_min + 3) / 4 * 4 + 1;
	rhs1 = (SHR1_MIN_X2 + s_exp_time + 3) / 4 * 4 + 1;/* shall be 4n + 1 */
	dev_dbg(&client->dev,
		"line(%d) rhs1 %d, rhs1 min %d rhs1 max %d\n",
		__LINE__, rhs1, rhs1_min, rhs1_max);
	if (rhs1_max < rhs1_min) {
		dev_err(&client->dev,
			"The total exposure limit makes rhs1 max is %d,but old rhs1 limit makes rhs1 min is %d\n",
			rhs1_max, rhs1_min);
		return -EINVAL;
	}
	rhs1 = clamp(rhs1, rhs1_min, rhs1_max);
	dev_dbg(&client->dev,
		"line(%d) rhs1 %d, short time %d rhs1_old %d, rhs1_new %d\n",
		__LINE__, rhs1, s_exp_time, rhs1_old, rhs1);

	rhs1_old = rhs1;

	/* shr1 = rhs1 - s_exp_time */
	if (rhs1 - s_exp_time <= SHR1_MIN_X2) {
		shr1 = SHR1_MIN_X2;
		s_exp_time = rhs1 - shr1;
	} else {
		shr1 = rhs1 - s_exp_time;
	}

	if (shr0 < rhs1 + 9)
		shr0 = rhs1 + 9;
	else if (shr0 > fsc - 8)
		shr0 = fsc - 8;

	dev_dbg(&client->dev,
		"fsc=%d,RHS1_MAX=%d,SHR1_MIN=%d,rhs1_max=%d\n",
		fsc, RHS1_MAX_X2(BRL_ALL), SHR1_MIN_X2, rhs1_max);
	dev_dbg(&client->dev,
		"l_exp_time=%d,s_exp_time=%d,shr0=%d,shr1=%d,rhs1=%d,l_a_gain=%d,s_a_gain=%d\n",
		l_exp_time, s_exp_time, shr0, shr1, rhs1, l_a_gain, s_a_gain);
	/* time effect n+2 */
	ret |= imx415_write_reg(client,
		IMX415_RHS1_REG_L,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_RHS1_L(rhs1));
	ret |= imx415_write_reg(client,
		IMX415_RHS1_REG_M,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_RHS1_M(rhs1));
	ret |= imx415_write_reg(client,
		IMX415_RHS1_REG_H,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_RHS1_H(rhs1));

	ret |= imx415_write_reg(client,
		IMX415_SF1_EXPO_REG_L,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_EXP_L(shr1));
	ret |= imx415_write_reg(client,
		IMX415_SF1_EXPO_REG_M,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_EXP_M(shr1));
	ret |= imx415_write_reg(client,
		IMX415_SF1_EXPO_REG_H,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_EXP_H(shr1));
	ret |= imx415_write_reg(client,
		IMX415_LF_EXPO_REG_L,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_EXP_L(shr0));
	ret |= imx415_write_reg(client,
		IMX415_LF_EXPO_REG_M,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_EXP_M(shr0));
	ret |= imx415_write_reg(client,
		IMX415_LF_EXPO_REG_H,
		IMX415_REG_VALUE_08BIT,
		IMX415_FETCH_EXP_H(shr0));

	ret |= imx415_write_reg(client, IMX415_GROUP_HOLD_REG,
		IMX415_REG_VALUE_08BIT, IMX415_GROUP_HOLD_END);
	return ret;
}

/* 获取通道信息，获取通道信息时会返回当前模式的虚拟通道号、分辨率和格式，如果当前索引超过了支持的通道数量，或者当前模式不支持HDR模式，则返回错误码 */
static int imx415_get_channel_info(struct imx415 *imx415, struct rkmodule_channel_info *ch_info)
{
	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;
	ch_info->vc = imx415->cur_mode->vc[ch_info->index];
	ch_info->width = imx415->cur_mode->width;
	ch_info->height = imx415->cur_mode->height;
	ch_info->bus_fmt = imx415->cur_mode->bus_fmt;
	return 0;
}

/* IOCTL操作，IOCTL操作时会根据用户传入的命令和参数，执行相应的操作，如果命令不支持，则返回错误码 */
static long imx415_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct imx415 *imx415 = to_imx415(sd);
	struct rkmodule_hdr_cfg *hdr;
	struct rkmodule_channel_info *ch_info;
	u32 i, h, w, stream;
	long ret = 0;
	const struct imx415_mode *mode;
	u64 pixel_rate = 0;
	struct rkmodule_csi_dphy_param *dphy_param;

	switch (cmd) {
	case PREISP_CMD_SET_HDRAE_EXP:
		if (imx415->cur_mode->hdr_mode == HDR_X2)
			ret = imx415_set_hdrae(imx415, arg);
		else if (imx415->cur_mode->hdr_mode == HDR_X3)
			ret = imx415_set_hdrae_3frame(imx415, arg);
		break;
	case RKMODULE_GET_MODULE_INFO:
		imx415_get_module_inf(imx415, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = imx415->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		w = imx415->cur_mode->width;
		h = imx415->cur_mode->height;
		for (i = 0; i < imx415->cfg_num; i++) {
			if (w == supported_modes[i].width &&
			    h == supported_modes[i].height &&
			    supported_modes[i].hdr_mode == hdr->hdr_mode) {
				imx415_change_mode(imx415, &supported_modes[i]);
				break;
			}
		}
		if (i == imx415->cfg_num) {
			dev_err(&imx415->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			mode = imx415->cur_mode;
			if (imx415->streaming) {
				ret = imx415_write_reg(imx415->client, IMX415_GROUP_HOLD_REG,
					IMX415_REG_VALUE_08BIT, IMX415_GROUP_HOLD_START);

				ret |= imx415_write_array(imx415->client, imx415->cur_mode->reg_list);

				ret |= imx415_write_reg(imx415->client, IMX415_GROUP_HOLD_REG,
					IMX415_REG_VALUE_08BIT, IMX415_GROUP_HOLD_END);
				if (ret)
					return ret;
			}
			w = mode->hts_def - imx415->cur_mode->width;
			h = mode->vts_def - mode->height;
			mutex_lock(&imx415->mutex);
			__v4l2_ctrl_modify_range(imx415->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(imx415->vblank, h,
				IMX415_VTS_MAX - mode->height,
				1, h);
			__v4l2_ctrl_s_ctrl(imx415->link_freq, mode->mipi_freq_idx);
			pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] / mode->bpp * 2 * IMX415_4LANES;
			__v4l2_ctrl_s_ctrl_int64(imx415->pixel_rate,
						 pixel_rate);
			mutex_unlock(&imx415->mutex);
		}
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream)
			ret = imx415_write_reg(imx415->client, IMX415_REG_CTRL_MODE,
				IMX415_REG_VALUE_08BIT, IMX415_MODE_STREAMING);
		else
			ret = imx415_write_reg(imx415->client, IMX415_REG_CTRL_MODE,
				IMX415_REG_VALUE_08BIT, IMX415_MODE_SW_STANDBY);
		break;
	case RKMODULE_GET_SONY_BRL:
		if (imx415->cur_mode->width == 3864 && imx415->cur_mode->height == 2192)
			*((u32 *)arg) = BRL_ALL;
		else
			*((u32 *)arg) = BRL_BINNING;
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = imx415_get_channel_info(imx415, ch_info);
		break;
	case RKMODULE_GET_CSI_DPHY_PARAM:
		if (imx415->cur_mode->hdr_mode == HDR_X2) {
			dphy_param = (struct rkmodule_csi_dphy_param *)arg;
			*dphy_param = dcphy_param;
			dev_info(&imx415->client->dev,
				 "get sensor dphy param\n");
		} else
			ret = -EINVAL;
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
/* 32位兼容IOCTL操作，32位兼容IOCTL操作时会根据用户传入的命令和参数，执行相应的操作，如果命令不支持，则返回错误码 */
static long imx415_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *cfg;
	struct rkmodule_hdr_cfg *hdr;
	struct preisp_hdrae_exp_s *hdrae;
	struct rkmodule_channel_info *ch_info;
	long ret;
	u32  stream;
	u32 brl = 0;
	struct rkmodule_csi_dphy_param *dphy_param;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx415_ioctl(sd, cmd, inf);
		if (!ret) {
			if (copy_to_user(up, inf, sizeof(*inf))) {
				kfree(inf);
				return -EFAULT;
			}
		}
		kfree(inf);
		break;
	case RKMODULE_AWB_CFG:
		cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
		if (!cfg) {
			ret = -ENOMEM;
			return ret;
		}

		if (copy_from_user(cfg, up, sizeof(*cfg))) {
			kfree(cfg);
			return -EFAULT;
		}
		ret = imx415_ioctl(sd, cmd, cfg);
		kfree(cfg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx415_ioctl(sd, cmd, hdr);
		if (!ret) {
			if (copy_to_user(up, hdr, sizeof(*hdr))) {
				kfree(hdr);
				return -EFAULT;
			}
		}
		kfree(hdr);
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		if (copy_from_user(hdr, up, sizeof(*hdr))) {
			kfree(hdr);
			return -EFAULT;
		}
		ret = imx415_ioctl(sd, cmd, hdr);
		kfree(hdr);
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		hdrae = kzalloc(sizeof(*hdrae), GFP_KERNEL);
		if (!hdrae) {
			ret = -ENOMEM;
			return ret;
		}

		if (copy_from_user(hdrae, up, sizeof(*hdrae))) {
			kfree(hdrae);
			return -EFAULT;
		}
		ret = imx415_ioctl(sd, cmd, hdrae);
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		if (copy_from_user(&stream, up, sizeof(u32)))
			return -EFAULT;
		ret = imx415_ioctl(sd, cmd, &stream);
		break;
	case RKMODULE_GET_SONY_BRL:
		ret = imx415_ioctl(sd, cmd, &brl);
		if (!ret) {
			if (copy_to_user(up, &brl, sizeof(u32)))
				return -EFAULT;
		}
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx415_ioctl(sd, cmd, ch_info);
		if (!ret) {
			ret = copy_to_user(up, ch_info, sizeof(*ch_info));
			if (ret)
				ret = -EFAULT;
		}
		kfree(ch_info);
		break;
	case RKMODULE_GET_CSI_DPHY_PARAM:
		dphy_param = kzalloc(sizeof(*dphy_param), GFP_KERNEL);
		if (!dphy_param) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx415_ioctl(sd, cmd, dphy_param);
		if (!ret) {
			ret = copy_to_user(up, dphy_param, sizeof(*dphy_param));
			if (ret)
				ret = -EFAULT;
		}
		kfree(dphy_param);
		break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

/* 开始和停止流，开始流时会根据当前模式的寄存器列表，写入寄存器来配置传感器，并将流状态设置为true；停止流时会将流状态设置为false，并将传感器置于待机模式 */
static int __imx415_start_stream(struct imx415 *imx415)
{
	int ret;

	if (!imx415->is_thunderboot) {
		ret = imx415_write_array(imx415->client, imx415->cur_mode->global_reg_list);
		if (ret)
			return ret;
		ret = imx415_write_array(imx415->client, imx415->cur_mode->reg_list);
		if (ret)
			return ret;
	}

	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&imx415->ctrl_handler);
	if (ret)
		return ret;
	if (imx415->has_init_exp && imx415->cur_mode->hdr_mode != NO_HDR) {
		ret = imx415_ioctl(&imx415->subdev, PREISP_CMD_SET_HDRAE_EXP,
			&imx415->init_hdrae_exp);
		if (ret) {
			dev_err(&imx415->client->dev,
				"init exp fail in hdr mode\n");
			return ret;
		}
	}
	return imx415_write_reg(imx415->client, IMX415_REG_CTRL_MODE,
				IMX415_REG_VALUE_08BIT, 0);
}

static int __imx415_stop_stream(struct imx415 *imx415)
{
	imx415->has_init_exp = false;
	if (imx415->is_thunderboot)
		imx415->is_first_streamoff = true;
	return imx415_write_reg(imx415->client, IMX415_REG_CTRL_MODE,
				IMX415_REG_VALUE_08BIT, 1);
}

/* 设置流状态，设置流状态时会根据用户传入的状态，调用开始流或停止流的函数来配置传感器，并更新流状态 */
static int imx415_s_stream(struct v4l2_subdev *sd, int on)
{
	struct imx415 *imx415 = to_imx415(sd);
	struct i2c_client *client = imx415->client;
	int ret = 0;

	dev_info(&imx415->client->dev, "s_stream: %d. %dx%d, hdr: %d, bpp: %d\n",
	       on, imx415->cur_mode->width, imx415->cur_mode->height,
	       imx415->cur_mode->hdr_mode, imx415->cur_mode->bpp);

	mutex_lock(&imx415->mutex);
	on = !!on;
	if (on == imx415->streaming)
		goto unlock_and_return;

	if (on) {
		if (imx415->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
			imx415->is_thunderboot = false;
			__imx415_power_on(imx415);
		}
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __imx415_start_stream(imx415);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__imx415_stop_stream(imx415);
		pm_runtime_put(&client->dev);
	}

	imx415->streaming = on;

unlock_and_return:
	mutex_unlock(&imx415->mutex);

	return ret;
}

/* 设置电源状态，设置电源状态时会根据用户传入的状态，调用电源开或关的函数来配置传感器，并更新电源状态 */
static int imx415_s_power(struct v4l2_subdev *sd, int on)
{
	struct imx415 *imx415 = to_imx415(sd);
	struct i2c_client *client = imx415->client;
	int ret = 0;

	mutex_lock(&imx415->mutex);

	if (imx415->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}
		imx415->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		imx415->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&imx415->mutex);

	return ret;
}

/* 电源管理函数，电源管理函数会根据当前的电源状态，调用相应的函数来配置传感器的电源，并处理错误情况 */
int __imx415_power_on(struct imx415 *imx415)
{
	int ret;
	struct device *dev = &imx415->client->dev;

	if (imx415->is_thunderboot)
		return 0;

	if (!IS_ERR_OR_NULL(imx415->pins_default)) {
		ret = pinctrl_select_state(imx415->pinctrl,
					   imx415->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}

	ret = regulator_bulk_enable(IMX415_NUM_SUPPLIES, imx415->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto err_pinctrl;
	}

	/*
	if (!IS_ERR(imx415->power_gpio))
		gpiod_direction_output(imx415->power_gpio, 1);
	*/

	/* At least 500ns between power raising and XCLR */
	/* fix power on timing if insmod this ko */
	usleep_range(10 * 1000, 20 * 1000);
	if (!IS_ERR(imx415->reset_gpio))
		gpiod_direction_output(imx415->reset_gpio, 0);

	/* At least 1us between XCLR and clk */
	/* fix power on timing if insmod this ko */
	usleep_range(10 * 1000, 20 * 1000);
	ret = clk_set_rate(imx415->xvclk, IMX415_XVCLK_FREQ_37M);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate\n");
	if (clk_get_rate(imx415->xvclk) != IMX415_XVCLK_FREQ_37M)
		dev_warn(dev, "xvclk mismatched\n");
	ret = clk_prepare_enable(imx415->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		goto err_clk;
	}

	/* At least 20us between XCLR and I2C communication */
	usleep_range(20*1000, 30*1000);

	return 0;

err_clk:
	if (!IS_ERR(imx415->reset_gpio))
		gpiod_direction_output(imx415->reset_gpio, 1);
	regulator_bulk_disable(IMX415_NUM_SUPPLIES, imx415->supplies);

err_pinctrl:
	if (!IS_ERR_OR_NULL(imx415->pins_sleep))
		pinctrl_select_state(imx415->pinctrl, imx415->pins_sleep);

	return ret;
}

/* 关闭电源函数，关闭电源函数会根据当前的电源状态，调用相应的函数来配置传感器的电源，并处理错误情况 */
static void __imx415_power_off(struct imx415 *imx415)
{
	int ret;
	struct device *dev = &imx415->client->dev;

	if (imx415->is_thunderboot) {
		if (imx415->is_first_streamoff) {
			imx415->is_thunderboot = false;
			imx415->is_first_streamoff = false;
		} else {
			return;
		}
	}

	if (!IS_ERR(imx415->reset_gpio))
		gpiod_direction_output(imx415->reset_gpio, 1);
	clk_disable_unprepare(imx415->xvclk);
	if (!IS_ERR_OR_NULL(imx415->pins_sleep)) {
		ret = pinctrl_select_state(imx415->pinctrl,
					   imx415->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	
	/*
	if (!IS_ERR(imx415->power_gpio))
		gpiod_direction_output(imx415->power_gpio, 0);
	*/

	regulator_bulk_disable(IMX415_NUM_SUPPLIES, imx415->supplies);
}

/* 运行时电源管理函数，运行时电源管理函数会根据当前的电源状态，调用相应的函数来配置传感器的电源，并处理错误情况 */
static int __maybe_unused imx415_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx415 *imx415 = to_imx415(sd);

	return __imx415_power_on(imx415);
}

/* 运行时电源管理函数，运行时电源管理函数会根据当前的电源状态，调用相应的函数来配置传感器的电源，并处理错误情况 */
static int __maybe_unused imx415_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx415 *imx415 = to_imx415(sd);

	__imx415_power_off(imx415);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
/* 打开子设备，打开子设备时会初始化尝试格式为默认模式的分辨率、格式和字段，并返回成功 */
static int imx415_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct imx415 *imx415 = to_imx415(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->pad, 0);
	const struct imx415_mode *def_mode = &supported_modes[0];

	mutex_lock(&imx415->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&imx415->mutex);
	/* No crop or compose */

	return 0;
}
#endif

/* 枚举帧间隔，枚举帧间隔时会根据用户传入的索引，返回当前模式的分辨率、格式、帧间隔和HDR模式，如果当前索引超过了支持的模式数量，则返回错误码 */
static int imx415_enum_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	struct imx415 *imx415 = to_imx415(sd);

	if (fie->index >= imx415->cfg_num)
		return -EINVAL;

	fie->code = supported_modes[fie->index].bus_fmt;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	fie->reserved[0] = supported_modes[fie->index].hdr_mode;
	return 0;
}

#define CROP_START(SRC, DST) (((SRC) - (DST)) / 2 / 4 * 4)
#define DST_WIDTH_3840 3840
#define DST_HEIGHT_2160 2160
#define DST_WIDTH_1920 1920
#define DST_HEIGHT_1080 1080

/*
 * The resolution of the driver configuration needs to be exactly
 * the same as the current output resolution of the sensor,
 * the input width of the isp needs to be 16 aligned,
 * the input height of the isp needs to be 8 aligned.
 * Can be cropped to standard resolution by this function,
 * otherwise it will crop out strange resolution according
 * to the alignment rules.
 */
 /* 获取选择，获取选择时会根据用户传入的目标，返回当前模式的裁剪边界，如果当前模式的分辨率为3864x2192，
  * 则裁剪边界为3840x2160；如果当前模式的分辨率为1944x1092，则裁剪边界为1920x1080；否则裁剪边界为当前模式的分辨率 
  */
static int imx415_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_selection *sel)
{
	struct imx415 *imx415 = to_imx415(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		if (imx415->cur_mode->width == 3864) {
			sel->r.left = CROP_START(imx415->cur_mode->width, DST_WIDTH_3840);
			sel->r.width = DST_WIDTH_3840;
			sel->r.top = CROP_START(imx415->cur_mode->height, DST_HEIGHT_2160);
			sel->r.height = DST_HEIGHT_2160;
		} else if (imx415->cur_mode->width == 1944) {
			sel->r.left = CROP_START(imx415->cur_mode->width, DST_WIDTH_1920);
			sel->r.width = DST_WIDTH_1920;
			sel->r.top = CROP_START(imx415->cur_mode->height, DST_HEIGHT_1080);
			sel->r.height = DST_HEIGHT_1080;
		} else {
			sel->r.left = CROP_START(imx415->cur_mode->width, imx415->cur_mode->width);
			sel->r.width = imx415->cur_mode->width;
			sel->r.top = CROP_START(imx415->cur_mode->height, imx415->cur_mode->height);
			sel->r.height = imx415->cur_mode->height;
		}
		return 0;
	}
	return -EINVAL;
}

/* 电源管理操作结构体，包含运行时电源管理的挂钩函数 */
static const struct dev_pm_ops imx415_pm_ops = {  
	SET_RUNTIME_PM_OPS(imx415_runtime_suspend,
			   imx415_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
/* 内部操作结构体，包含子设备打开时的挂钩函数 */
static const struct v4l2_subdev_internal_ops imx415_internal_ops = {
	.open = imx415_open,
};
#endif

/* 核心操作结构体，包含电源控制和ioctl处理函数 */
static const struct v4l2_subdev_core_ops imx415_core_ops = {
	.s_power = imx415_s_power,
	.ioctl = imx415_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = imx415_compat_ioctl32,
#endif
};

/* 视频操作结构体，包含流控制和帧间隔获取函数 */
static const struct v4l2_subdev_video_ops imx415_video_ops = {
	.s_stream = imx415_s_stream,
	.g_frame_interval = imx415_g_frame_interval,
};

/* Pad操作结构体，包含媒体总线格式枚举、帧大小枚举、帧间隔枚举、格式获取和设置、选择获取和媒体总线配置获取函数 */
static const struct v4l2_subdev_pad_ops imx415_pad_ops = {
	.enum_mbus_code = imx415_enum_mbus_code,
	.enum_frame_size = imx415_enum_frame_sizes,
	.enum_frame_interval = imx415_enum_frame_interval,
	.get_fmt = imx415_get_fmt,
	.set_fmt = imx415_set_fmt,
	.get_selection = imx415_get_selection,
	.get_mbus_config = imx415_g_mbus_config,
};

/* 子设备操作结构体，包含核心、视频和Pad操作结构体 */
static const struct v4l2_subdev_ops imx415_subdev_ops = {
	.core	= &imx415_core_ops,
	.video	= &imx415_video_ops,
	.pad	= &imx415_pad_ops,
};

/* 子设备内部操作结构体，包含核心操作结构体和内部操作结构体 */
static int imx415_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx415 *imx415 = container_of(ctrl->handler,
					     struct imx415, ctrl_handler);
	struct i2c_client *client = imx415->client;
	s64 max;
	u32 vts = 0, val;
	int ret = 0;
	u32 shr0 = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		if (imx415->cur_mode->hdr_mode == NO_HDR) {
			/* Update max exposure while meeting expected vblanking */
			max = imx415->cur_mode->height + ctrl->val - 8;
			__v4l2_ctrl_modify_range(imx415->exposure,
					 imx415->exposure->minimum, max,
					 imx415->exposure->step,
					 imx415->exposure->default_value);
		}
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		if (imx415->cur_mode->hdr_mode != NO_HDR)
			goto ctrl_end;
		shr0 = imx415->cur_vts - ctrl->val;
		ret = imx415_write_reg(imx415->client, IMX415_LF_EXPO_REG_L,
				       IMX415_REG_VALUE_08BIT,
				       IMX415_FETCH_EXP_L(shr0));
		ret |= imx415_write_reg(imx415->client, IMX415_LF_EXPO_REG_M,
				       IMX415_REG_VALUE_08BIT,
				       IMX415_FETCH_EXP_M(shr0));
		ret |= imx415_write_reg(imx415->client, IMX415_LF_EXPO_REG_H,
				       IMX415_REG_VALUE_08BIT,
				       IMX415_FETCH_EXP_H(shr0));
		dev_dbg(&client->dev, "set exposure(shr0) %d = cur_vts(%d) - val(%d)\n",
			shr0, imx415->cur_vts, ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		if (imx415->cur_mode->hdr_mode != NO_HDR)
			goto ctrl_end;
		ret = imx415_write_reg(imx415->client, IMX415_LF_GAIN_REG_H,
				       IMX415_REG_VALUE_08BIT,
				       IMX415_FETCH_GAIN_H(ctrl->val));
		ret |= imx415_write_reg(imx415->client, IMX415_LF_GAIN_REG_L,
				       IMX415_REG_VALUE_08BIT,
				       IMX415_FETCH_GAIN_L(ctrl->val));
		dev_dbg(&client->dev, "set analog gain 0x%x\n",
			ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		vts = ctrl->val + imx415->cur_mode->height;
		/*
		 * vts of hdr mode is double to correct T-line calculation.
		 * Restore before write to reg.
		 */
		if (imx415->cur_mode->hdr_mode == HDR_X2) {
			vts = (vts + 3) / 4 * 4;
			imx415->cur_vts = vts;
			vts /= 2;
		} else if (imx415->cur_mode->hdr_mode == HDR_X3) {
			vts = (vts + 11) / 12 * 12;
			imx415->cur_vts = vts;
			vts /= 4;
		} else {
			imx415->cur_vts = vts;
		}
		ret = imx415_write_reg(imx415->client, IMX415_VTS_REG_L,
				       IMX415_REG_VALUE_08BIT,
				       IMX415_FETCH_VTS_L(vts));
		ret |= imx415_write_reg(imx415->client, IMX415_VTS_REG_M,
				       IMX415_REG_VALUE_08BIT,
				       IMX415_FETCH_VTS_M(vts));
		ret |= imx415_write_reg(imx415->client, IMX415_VTS_REG_H,
				       IMX415_REG_VALUE_08BIT,
				       IMX415_FETCH_VTS_H(vts));
		dev_dbg(&client->dev, "set vblank 0x%x vts %d\n",
			ctrl->val, vts);
		break;
	case V4L2_CID_HFLIP:
		ret = imx415_read_reg(imx415->client, IMX415_FLIP_REG,
				      IMX415_REG_VALUE_08BIT, &val);
		if (ret)
			break;
		if (ctrl->val)
			val |= IMX415_MIRROR_BIT_MASK;
		else
			val &= ~IMX415_MIRROR_BIT_MASK;
		ret = imx415_write_reg(imx415->client, IMX415_FLIP_REG,
				       IMX415_REG_VALUE_08BIT, val);
		break;
	case V4L2_CID_VFLIP:
		ret = imx415_read_reg(imx415->client, IMX415_FLIP_REG,
				      IMX415_REG_VALUE_08BIT, &val);
		if (ret)
			break;
		if (ctrl->val)
			val |= IMX415_FLIP_BIT_MASK;
		else
			val &= ~IMX415_FLIP_BIT_MASK;
		ret = imx415_write_reg(imx415->client, IMX415_FLIP_REG,
				       IMX415_REG_VALUE_08BIT, val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

ctrl_end:
	pm_runtime_put(&client->dev);

	return ret;
}

/* 控制操作结构体，包含控制处理函数 */
static const struct v4l2_ctrl_ops imx415_ctrl_ops = {
	.s_ctrl = imx415_set_ctrl,
};

/* 初始化控制，设置摄像头的各种参数和控制项，包括曝光、增益、翻转等，并将这些控制项与驱动的控制处理函数关联起来 */
static int imx415_initialize_controls(struct imx415 *imx415)
{
	const struct imx415_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u64 pixel_rate;
	u32 h_blank;
	int ret;

	handler = &imx415->ctrl_handler;
	mode = imx415->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &imx415->mutex;

	imx415->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
				V4L2_CID_LINK_FREQ,
				ARRAY_SIZE(link_freq_items) - 1, 0,
				link_freq_items);
	v4l2_ctrl_s_ctrl(imx415->link_freq, mode->mipi_freq_idx);

	/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
	pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] / mode->bpp * 2 * IMX415_4LANES;
	imx415->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
		V4L2_CID_PIXEL_RATE, 0, IMX415_MAX_PIXEL_RATE,
		1, pixel_rate);

	h_blank = mode->hts_def - mode->width;
	imx415->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
				h_blank, h_blank, 1, h_blank);
	if (imx415->hblank)
		imx415->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	imx415->vblank = v4l2_ctrl_new_std(handler, &imx415_ctrl_ops,
				V4L2_CID_VBLANK, vblank_def,
				IMX415_VTS_MAX - mode->height,
				1, vblank_def);
	imx415->cur_vts = mode->vts_def;

	exposure_max = mode->vts_def - 8;
	imx415->exposure = v4l2_ctrl_new_std(handler, &imx415_ctrl_ops,
				V4L2_CID_EXPOSURE, IMX415_EXPOSURE_MIN,
				exposure_max, IMX415_EXPOSURE_STEP,
				mode->exp_def);

	imx415->anal_a_gain = v4l2_ctrl_new_std(handler, &imx415_ctrl_ops,
				V4L2_CID_ANALOGUE_GAIN, IMX415_GAIN_MIN,
				IMX415_GAIN_MAX, IMX415_GAIN_STEP,
				IMX415_GAIN_DEFAULT);

	v4l2_ctrl_new_std(handler, &imx415_ctrl_ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, &imx415_ctrl_ops, V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (handler->error) {
		ret = handler->error;
		dev_err(&imx415->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	imx415->subdev.ctrl_handler = handler;
	imx415->has_init_exp = false;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

/* 检查传感器 ID 是否正确，确保连接的设备是预期的 IMX415 摄像头 */
static int imx415_check_sensor_id(struct imx415 *imx415,
				  struct i2c_client *client)
{
	struct device *dev = &imx415->client->dev;
	u32 id = 0;
	int ret;

	if (imx415->is_thunderboot) {
		dev_info(dev, "Enable thunderboot mode, skip sensor id check\n");
		return 0;
	}

	ret = imx415_read_reg(client, IMX415_REG_CHIP_ID,
			      IMX415_REG_VALUE_08BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	dev_info(dev, "Detected imx415 id %06x\n", CHIP_ID);

	return 0;
}

/* 配置电源调节器，获取摄像头模块所需的电源资源 */
static int imx415_configure_regulators(struct imx415 *imx415)
{
	unsigned int i;

	for (i = 0; i < IMX415_NUM_SUPPLIES; i++)
		imx415->supplies[i].supply = imx415_supply_names[i];

	return devm_regulator_bulk_get(&imx415->client->dev,
				       IMX415_NUM_SUPPLIES,
				       imx415->supplies);
}

/* 驱动的 probe 函数，在设备被检测到时调用，负责初始化设备、读取设备树信息、配置资源和注册子设备 */
static int imx415_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct imx415 *imx415;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	u32 i, hdr_mode = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	/* 为 IMX415 结构体分配内存空间 */
	imx415 = devm_kzalloc(dev, sizeof(*imx415), GFP_KERNEL);
	if (!imx415)
		return -ENOMEM;

	/* 从设备树中读取摄像头模块的信息，包括模块索引、朝向、名称和镜头名称 */
	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &imx415->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &imx415->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &imx415->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &imx415->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	/* 查看是否配置了 HDR 模式 */
	ret = of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	if (ret) {
		hdr_mode = NO_HDR;
		dev_warn(dev, " Get hdr mode failed! no hdr default\n");
	}
	imx415->client = client;
	imx415->cfg_num = ARRAY_SIZE(supported_modes);
	for (i = 0; i < imx415->cfg_num; i++) {
		if (hdr_mode == supported_modes[i].hdr_mode) {
			imx415->cur_mode = &supported_modes[i];
			break;
		}
	}

	/* 检查是否启用了 Thunderboot 模式 */
	imx415->is_thunderboot = IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP);

	/* 获取 XVCLK 时钟资源 */
	imx415->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(imx415->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	/* 获取复位 GPIO 资源 */
	imx415->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(imx415->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	/*
	imx415->power_gpio = devm_gpiod_get(dev, "power", GPIOD_ASIS);
	if (IS_ERR(imx415->power_gpio))
		dev_warn(dev, "Failed to get power-gpios\n");
	*/

	/* 获取 pinctrl 资源，并查找默认和睡眠状态的 pinctrl 状态 */
	imx415->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(imx415->pinctrl)) {
		imx415->pins_default =
			pinctrl_lookup_state(imx415->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(imx415->pins_default))
			dev_info(dev, "could not get default pinstate\n");

		imx415->pins_sleep =
			pinctrl_lookup_state(imx415->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(imx415->pins_sleep))
			dev_info(dev, "could not get sleep pinstate\n");
	} else {
		dev_info(dev, "no pinctrl\n");
	}

	/* 配置电源调节器 */
	ret = imx415_configure_regulators(imx415);	
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	/* 初始化互斥锁 */
	mutex_init(&imx415->mutex);

	/* 初始化 V4L2 子设备，并设置子设备的操作函数 */
	sd = &imx415->subdev;
	v4l2_i2c_subdev_init(sd, client, &imx415_subdev_ops);
	ret = imx415_initialize_controls(imx415);
	if (ret)
		goto err_destroy_mutex;

	/* 上电并检查传感器 ID */
	ret = __imx415_power_on(imx415);
	if (ret)
		goto err_free_handler;

	ret = imx415_check_sensor_id(imx415, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	/* 设置子设备的内部操作函数和标志位 */
	sd->internal_ops = &imx415_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	/* 初始化媒体实体，并设置为传感器类型 */
	imx415->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &imx415->pad);
	if (ret < 0)
		goto err_power_off;
#endif
	
	/* 设置子设备的名称，格式为 "mXX_facing_imx415 dev_name" */
	memset(facing, 0, sizeof(facing));
	if (strcmp(imx415->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 imx415->module_index, facing,
		 IMX415_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor_common(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	/* 开启运行时电源管理，并将设备设置为活动状态 */
	pm_runtime_set_active(dev);	
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__imx415_power_off(imx415);
err_free_handler:
	v4l2_ctrl_handler_free(&imx415->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&imx415->mutex);

	return ret;
}

static int imx415_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx415 *imx415 = to_imx415(sd);

	v4l2_async_unregister_subdev(sd);  // 注销V4L2子设备
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity); // 清理媒体实体
#endif
	v4l2_ctrl_handler_free(&imx415->ctrl_handler); // 释放控制处理程序
	mutex_destroy(&imx415->mutex);	// 销毁互斥锁

	pm_runtime_disable(&client->dev); // 禁用运行时电源管理
	if (!pm_runtime_status_suspended(&client->dev))
		__imx415_power_off(imx415);
	pm_runtime_set_suspended(&client->dev); // 设置设备为挂起状态

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id imx415_of_match[] = {
	{ .compatible = "sony,imx415" },
	{},
};
MODULE_DEVICE_TABLE(of, imx415_of_match);
#endif

static const struct i2c_device_id imx415_match_id[] = {
	{ "sony,imx415", 0 },
	{ },
};

static struct i2c_driver imx415_i2c_driver = {
	.driver = {
		.name = IMX415_NAME,
		.pm = &imx415_pm_ops,
		.of_match_table = of_match_ptr(imx415_of_match),
	},
	.probe		= &imx415_probe,
	.remove		= &imx415_remove,
	.id_table	= imx415_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&imx415_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&imx415_i2c_driver);
}

//device_initcall_sync(sensor_mod_init);
module_init(sensor_mod_init); // 模块加载函数
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("Sony imx415 sensor driver");
MODULE_LICENSE("GPL v2");
