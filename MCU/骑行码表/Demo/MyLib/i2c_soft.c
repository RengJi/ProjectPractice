#include "i2c_soft.h"

static void soft_i2c_start(void);
static void soft_i2c_stop(void);
static void soft_i2c_ack(void);
static void soft_i2c_nack(void);
static uint8_t soft_i2c_read_ack(void);
static void soft_i2c_write_byte(uint8_t byte);
static uint8_t soft_i2c_read_byte(void);

/**
 * @brief  软件I2C初始化
 * @param  无
 * @return 无
 */ 
void soft_i2c_init(void)
{
	RCC_APB2PeriphClockCmd(SOFT_I2C_CLK, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_InitStructure.GPIO_Pin = SOFT_I2C_SCL_PIN;
	GPIO_Init(SOFT_I2C_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = SOFT_I2C_SDA_PIN;
	GPIO_Init(SOFT_I2C_PORT, &GPIO_InitStructure);
	
	SOFT_I2C_SDA_WRITE(1);
	SOFT_I2C_SCL_WRITE(1);
}

/**
 * @brief  实现软件I2C读取功能
 * @param  slave_addr 从机地址
 * @param  read_data 读的数据
 * @param  read_data 读的数据大小
 * @return 无
 */ 
void soft_i2c_read(uint8_t slave_addr, uint8_t *data, uint16_t data_size)
{
	soft_i2c_start();
	soft_i2c_write_byte(slave_addr | SOFT_I2C_READ_BIT);
	
	while (data_size--) {
		*data++ = soft_i2c_read_byte();
		if (data_size == 0) {
			soft_i2c_nack();
		} else {
			soft_i2c_ack();
		}
	}
	
	soft_i2c_stop();
}


/**
 * @brief  实现软件I2C写功能
 * @param  slave_addr 从机地址
 * @param  write_data 写的数据
 * @param  write_data_size 写的数据大小
 * @return 无
 */ 
void soft_i2c_write(uint8_t slave_addr, const uint8_t *data, uint16_t data_size)
{
	soft_i2c_start();
	soft_i2c_write_byte(slave_addr | SOFT_I2C_WRITE_BIT);
	
	while(data_size--) {
		soft_i2c_write_byte(*data++);
	}
	soft_i2c_stop();
}


/**
 *@brief  实现软件I2C读写功能，主要是用于对从机特定寄存器的读写 
 * @param  slave_addr 从机地址
 * @param  write_data 写的数据
 * @param  write_data_size 写的数据大小
 * @param  read_data 读的数据
 * @param  read_data 读的数据大小
 * @return 无
 */ 
void soft_i2c_write_read(uint8_t slave_addr, const uint8_t *write_data, uint16_t write_data_size,uint8_t *read_data, uint16_t read_data_size)
{
	soft_i2c_start();
	soft_i2c_write_byte(slave_addr | SOFT_I2C_WRITE_BIT);
	
	while(write_data_size--) {
		soft_i2c_write_byte(*write_data++);
	}
	
	soft_i2c_start();
	soft_i2c_write_byte(slave_addr | SOFT_I2C_READ_BIT);
	
	while (read_data_size--) {
		*read_data = soft_i2c_read_byte();
		read_data++;
		if (read_data_size == 0) {
			soft_i2c_nack();
		} else {
			soft_i2c_ack();
		}
	}
	
	soft_i2c_stop();
}

static void soft_i2c_start(void)
{
	SOFT_I2C_SDA_WRITE(1);
	SOFT_I2C_SCL_WRITE(1);
	delay_us(1);
	SOFT_I2C_SDA_WRITE(0);
	delay_us(1);
	SOFT_I2C_SCL_WRITE(0);
}

static void soft_i2c_stop(void)
{
	SOFT_I2C_SDA_WRITE(0);
	SOFT_I2C_SCL_WRITE(1);
	delay_us(1);
	SOFT_I2C_SDA_WRITE(1);
	delay_us(1);
}

static void soft_i2c_ack(void)
{
	SOFT_I2C_SDA_WRITE(0);
	SOFT_I2C_SCL_WRITE(1);
	delay_us(1);
	SOFT_I2C_SCL_WRITE(0);
}

static void soft_i2c_nack(void)
{
	SOFT_I2C_SDA_WRITE(1);
	SOFT_I2C_SCL_WRITE(1);
	delay_us(1);
	SOFT_I2C_SCL_WRITE(0);
}

static uint8_t soft_i2c_read_ack(void)
{
	SOFT_I2C_SDA_WRITE(1);//主机交出SDA线控制权
	SOFT_I2C_SCL_WRITE(1);
	delay_us(1);
	uint8_t ret_ack = 0;
	if (GPIO_ReadInputDataBit(SOFT_I2C_PORT, SOFT_I2C_SDA_PIN)) {
		ret_ack = 1;
	} else {
		ret_ack = 0;
	}
	SOFT_I2C_SCL_WRITE(0);
	return ret_ack;
}

static void soft_i2c_write_byte(uint8_t byte)
{
	for (uint8_t i = 0; i < 8; ++i) {
		if (byte & 0x80) {
			SOFT_I2C_SDA_WRITE(1);
		} else {
			SOFT_I2C_SDA_WRITE(0);
		}
		SOFT_I2C_SCL_WRITE(1);
		delay_us(1);
		SOFT_I2C_SCL_WRITE(0);
		delay_us(2);
		byte <<= 1;
	}
	
	while (soft_i2c_read_ack() != 0);//软件I2C需要等待应答位，否则会错乱
}

static uint8_t soft_i2c_read_byte(void)
{
	uint8_t rx_data = 0;
	
	SOFT_I2C_SDA_WRITE(1); //主机放出SDA线的控制权
	for (uint8_t i = 0; i < 8; ++i) {
		SOFT_I2C_SCL_WRITE(1);
		delay_us(1);
		rx_data <<= 1; //因为需要左移八次所以需要先移动不然会导致第一位溢出
		if (GPIO_ReadInputDataBit(SOFT_I2C_PORT, SOFT_I2C_SDA_PIN) == 1) {
			rx_data |= 0x01;
		}			
		SOFT_I2C_SCL_WRITE(0);
		delay_us(2);
	}
	return rx_data;
}
