/*
Circuit detail:

	VIN     - 	3.3V (Raspberry Pi pin 17)
	GND		-	GND  (Raspberry Pi pin 30)
	SCL 	-	SCL  (Raspberry Pi pin 5)
	SDA     - 	SDA  (Raspberry Pi pin 3)
*/


#include <linux/types.h>

#ifndef _INA260_H_
#define _INA260_H_

#define PCA_AUTOINCREMENT_OFF 0x00
#define PCA_AUTOINCREMENT_ALL 0x80
#define PCA_AUTOINCREMENT_INDIVIDUAL 0xA0
#define PCA_AUTOINCREMENT_CONTROL 0xC0
#define PCA_AUTOINCREMENT_CONTROL_GLOBAL 0xE0

#define REG_CONFIG 0x00
#define REG_CURRENT 0x01
#define REG_BUS_VOLTAGE 0x02
#define REG_POWER 0x03
#define REG_MASK_ENABLE 0x06
#define REG_ALERT 0x07
#define REG_MANUFACTURER_ID 0xFE
#define REG_DIE_ID 0xFF

#define MAN_ID 0x5449
#define DIE_ID 0x2270

#define RST 15
#define CONF_RO2 14
#define CONF_RO1 13
#define CONF_RO0 12
#define AVG2 11
#define AVG1 10
#define AVG0 9
#define VBUSCT2 8
#define VBUSCT1 7
#define VBUSCT0 6
#define ISHCT2 5
#define ISHCT1 4
#define ISHCT0 3
#define MODE2 2
#define MODE1 1
#define MODE0 0

#define OCL 15
#define UCL 14
#define BOL 13
#define BUL 12
#define POL 11
#define CNVR 10
#define AFF 4
#define CVRF 3
#define OVF 2
#define APOL 1
#define LEN 0

#define INA260_I2C_ADDRESS 0x40



int i2c_init(__u8 address);
__u16 read_reg(int fd, __u8 address);
void write_reg(int fd, __u8 address, __u16 data);
__u16 manufacturer_id(int fd);
__u16 die_id(int fd);
__u16 bitset(__u16 number, __u8 i);
__u16 bitclear(__u16 number, __u8 i);
__s8 ina260_config(int fd);
__u16 voltage_read(int fd);
__s16 reg_to_volt(__u16 reg_voltage_raw);
__u16 current_read(int fd);
__s16 reg_to_amp(__u16 reg_current_raw);
__u16 power_read(int fd);
__u16 reg_to_watt(__u16 reg_power_raw);




#endif
