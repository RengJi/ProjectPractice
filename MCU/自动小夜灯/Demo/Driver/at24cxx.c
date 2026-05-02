#include "at24cxx.h"


static void print_hex_array(const char* prefix, uint8_t* array, int size) 
{
    int i;
    printf("%s\r\n", prefix);
    for (i = 0; i < size; i++) {
        printf("0x%02X ", array[i]);
        if(i % 16 == 15) {
            printf("\r\n");
        }
    }
}
/**@brief  读取当前地址指针的地址内容
  *@param  data 读取到的内容
  *@retval 无
  */
void at24cxx_read_current(uint8_t *data)
{
	soft_i2c_read(AT24CXX_ADDR, data, 1);
}

/**@brief  顺序读取指定个数当前地址指针所以的内容
  *@param  data 读取到的内容组成的数组
  *@param  data_size 读取到的内容个数
  *@retval 无
  */
void at24cxx_read_seq(uint8_t *data, uint16_t data_size)
{
	soft_i2c_read(AT24CXX_ADDR, data, data_size);
}

/**@brief  顺序读取指定位置指定个数的内容
  *@param  addr 指定读取的地址
  *@param  data 读取的内容组成的数组  
  *@param  data_size 读取到的内容个数
  *@retval 无
  */
void at24cxx_read_bytes(uint16_t addr, uint8_t *data, uint16_t data_size)
{
	uint8_t addr_arr[2] = {((addr >> 8) & 0xff), (addr & 0xff)};
	soft_i2c_write_read(AT24CXX_ADDR, addr_arr, 2, data, data_size);
}

/**@brief  对EPROM某个地址写入
  *@param  addr 需要写入的地址
  *@param  data 写入的数据
  *@retval 无
  */
void at24cxx_write_byte(uint16_t addr, uint8_t *data)
{
	uint8_t wr_arr[3] = {((addr >> 8) & 0xff), (addr & 0xff), *data};
	soft_i2c_write(AT24CXX_ADDR, wr_arr, 3);
	delay_ms(5); //手册说写入周期不超过5ms
}

/**@brief  对EPROM特定地址开始写入多个字节
  *@param  addr 需要写入的地址
  *@param  data 写入的数据
  *@param  data_size 写入数据的个数
  *@retval 无
  */
void at24cxx_write_bytes(uint16_t addr, uint8_t *data, uint16_t data_size)
{
	uint8_t wr_data[2 + AT24C64_PAGE_SIZE];
	uint8_t write_len = 0;
	
	write_len = AT24C64_PAGE_SIZE - (data_size % AT24C64_PAGE_SIZE); //超出整页的部分
	if (write_len > data_size) {  //未占满第一页
		write_len = data_size;
	}
	
	while (write_len > 0) {
		data_size -= write_len;
		
		wr_data[0] = (addr >> 8) & 0xff;
		wr_data[1] = addr & 0xff;
		
		memcpy(&wr_data[2], data, write_len);
		soft_i2c_write(AT24CXX_ADDR, wr_data, write_len + 2);
		
		addr += write_len;
		data += write_len;
		
		write_len = (data_size < AT24C64_PAGE_SIZE ? data_size : AT24C64_PAGE_SIZE);//判断后序页是否能写满
		delay_ms(5);
	}
	
}

/* 功能测试程序 */
void at24cxx_test(void)
{
	#define AT24CXX_TEST_LEN 256
	uint8_t write_buf[AT24CXX_TEST_LEN];
	uint8_t read_buf[AT24CXX_TEST_LEN];
	
	printf("------------I2C AT24c64 read and write test-----------\r\n");
	/*先按顺序读取，如果第一次上电，会从0地址读取*/
	memset(read_buf, 0, sizeof(read_buf));
	at24cxx_read_seq(read_buf, 128);
	
	print_hex_array("read sequential data:", read_buf, 128);
	
	printf("-----------------------------------------------------------\r\n");
	
	for (uint16_t i = 0; i < AT24CXX_TEST_LEN; i++) {
		write_buf[i] = i;
	}
	
	/*从θ地址开始，写入256字节递增数据*/
	print_hex_array("write data:", write_buf, AT24CXX_TEST_LEN);
	at24cxx_write_bytes(0x00, write_buf, AT24CXX_TEST_LEN);
	
	printf("-----------------------------------------------------------\r\n");
	
	/*从θ地址开始，读取256字节数据*/
	memset(read_buf, 0, sizeof(read_buf));
	at24cxx_read_bytes(0x00 , read_buf, AT24CXX_TEST_LEN);
	print_hex_array("read data", read_buf, AT24CXX_TEST_LEN);
	
	printf("-----------------------------------------------------------\r\n");
	/*比较写入的和读出的数据*/
	if (memcmp(write_buf, read_buf, AT24CXX_TEST_LEN)!= 0) {
		printf("write failed\r\n");
		return;
	} else {
		printf("write succeed!\r\n");
	}
	
	for(uint16_t i = 0; i<AT24CXX_TEST_LEN; i++) {
		write_buf[i]=0x88;
	}
	/*从30地址开始，写入36字节0x88*/
	printf("write 36 bytes \"0x88\" data to address 30:\r\n");
	at24cxx_write_bytes(30,write_buf, 36);
	
	/*从0地址开始，读取256字节数据*/
	at24cxx_read_bytes(0x00, read_buf, AT24CXX_TEST_LEN);
	print_hex_array("read data", read_buf, AT24CXX_TEST_LEN);
	printf("-------------test over--------------------\r\n");
}
