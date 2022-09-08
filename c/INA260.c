#include "INA260.h"
#include "smbus.h"
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <math.h>
#define VERBOSE 0



int i2c_init(__u8 dev_addr)
{
	// Returns a file id
	int fd = 0;
	char *fileName = "/dev/i2c-1";
	
	// Open port for reading and writing
	if ((fd = open(fileName, O_RDWR)) < 0)
	{
		if (VERBOSE) printf("Error! Cannot opend the port\n");
	}
	
	// Set the port options and set the address of the device
	if (ioctl(fd, I2C_SLAVE, dev_addr) < 0) 
	{					
		close(fd);
		if (VERBOSE) printf("Device with device address  %02x is not reachable\n",dev_addr);
	}

	return fd;
}

__u16 bitset(__u16 number, __u8 i)
{
	 /*
	Sets i-th bit of "number" to 1. i
	Parameters
		number: the input data
		i: contains the location of the bit that needs to be set

	Returns the updated data
	*/
	__u16 output = number;
	output = output | (0x0001 << i);
	return output;
}

__u16 bitclear(__u16 number, __u8 i)
{
	/*
	Sets i-th bit of "number" to 0. i 
	Parameters
		number: the input data
		i: contains the location of bit that needs to be cleared

	Returns the updated data
	 */	
	__u16 output = number;
	output = output & (~(0x0001 << i));
	return output;
	
}


__s8 ina260_config(int fd)
{
	/*
	Configures the INA260 registers

	Returns 0 if the configuration is succesfull.
	*/
	// Reset the device
	__s8 status = 0;
	__u8 wr_addr = REG_CONFIG;
	__u16 wr_data = 0x0000;
	wr_data = bitset(wr_data, RST);
	write_reg(fd, wr_addr, wr_data);

	sleep(0.1);

	// Check if device register reading is working
	__u16 man_id = manufacturer_id(fd);
	if (man_id != MAN_ID)
	{
		if (VERBOSE) printf("Device is not reachable. Wrong manufacturer ID\n");
		status = status | (1<<1);
	}

	__u16 die_id_reg = die_id(fd);
	if (die_id_reg != DIE_ID)
	{
		if (VERBOSE) printf("Device is not reachable. Wrong Die ID\n");
		status = status | (1<<2);
	}
		

	// Set Confugation Register
	wr_addr = REG_CONFIG;
	wr_data = 0x0000;
	// Check Datasheet of INA260 for each of the bits
	// Make sure to set the read-only bits!!! (CONF_ROX)
	wr_data = bitset(wr_data, CONF_RO2);
	wr_data = bitset(wr_data, CONF_RO1);
	wr_data = bitset(wr_data, MODE0);
	write_reg(fd, wr_addr, wr_data);
	sleep(0.1);
	__u16 rd_data = read_reg(fd, wr_addr);
	if (rd_data != wr_data)
	{
		if (VERBOSE) printf("Device configuration failed\n");
		status = status | (1<<3);
	}
	
	return status;

}

__u16 read_reg(int fd, __u8 address)
{
	/*
		Reads a word from the device
	
	Parameters:
		address: register address

	Returns the register value (16 bits)
	*/
	__s32 res = i2c_smbus_read_word_data(fd, address);
	if (res < 0) 
	{
		if (VERBOSE) printf("Error in reading!\n");
		close(fd);
	}

	// Convert result to 16 bits and swap bytes
	res = ((res<<8) & 0xFF00) | ((res>>8) & 0xFF);

	return res;
}

void write_reg(int fd, __u8 address, __u16 data)
{
	/*
        Writes a word to the device
        
        Parameters:
            address: register address
            data: resgister data (16 bits)
	*/	
	__s32 res = i2c_smbus_write_word_data(fd, address, ((data<<8) & 0xFF00) | ((data>>8) & 0xFF));
	if (res < 0) 
	{
		if (VERBOSE) printf("Error in writing!\n");
		close(fd);
	}
}

__u16 voltage_read(int fd)
{
	// Returns the voltage register of INA260 (Register 0x02)
	return read_reg(fd, REG_BUS_VOLTAGE);
}

__s16 reg_to_volt(__u16 reg_voltage_raw)
{
	/*
	Converts the voltage register raw value to Millivolts
	Parameters:
		reg_voltage_raw: raw value read from voltage register of INA260

	Returns the voltage in millivolts
	*/
	return round(((__s16)reg_voltage_raw)*1.25);  //   1.25mv/bit
}

__u16 current_read(int fd)
{
	// Returns the current register of INA260 (Register 0x01)
	return read_reg(fd, REG_CURRENT);
}

__s16 reg_to_amp(__u16 reg_current_raw)
{
	/*
	Converts the current register raw value to Miillimpers
	
	Parameters:
		reg_current_raw: raw value read from current register of INA260

	Returns the Current in milliampers
	*/
	__s16 current;
	if (reg_current_raw & (1 << 15)) //Two's complement
		current = reg_current_raw - 65535;
	else
		current = reg_current_raw;

	return round((current*1.25));  //   1.25mA/bit
}

__u16 power_read(int fd)
{
	// Returns the power register of INA260 (Register 0x03)
	return read_reg(fd, REG_POWER);
}

__u16 reg_to_watt(__u16 reg_power_raw)
{
	/*
	Converts the power register raw value to Milliwatts
	Parameters:
		reg_power_raw: raw value read from power register of INA260

	Returns the Power in milliwatts
	*/
	return reg_power_raw*10;  //   10mW/bit
}


__u16 manufacturer_id(int fd)
{
	/*
    Returns the manufacturer ID - it should always be 0x5449
	*/
	return read_reg(fd, REG_MANUFACTURER_ID);
}

__u16 die_id(int fd)
{
	/*
        Returns the die ID register - it should be 0x2270.
	*/
	return read_reg(fd, REG_DIE_ID);
}