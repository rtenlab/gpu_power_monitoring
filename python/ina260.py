from smbus2 import SMBus
import time

PCA_AUTOINCREMENT_OFF = 0x00
PCA_AUTOINCREMENT_ALL = 0x80
PCA_AUTOINCREMENT_INDIVIDUAL = 0xA0
PCA_AUTOINCREMENT_CONTROL = 0xC0
PCA_AUTOINCREMENT_CONTROL_GLOBAL = 0xE0

REG_CONFIG = 0x00
REG_CURRENT = 0x01
REG_BUS_VOLTAGE = 0x02
REG_POWER = 0x03
REG_MASK_ENABLE = 0x06
REG_ALERT = 0x07
REG_MANUFACTURER_ID = 0xFE
REG_DIE_ID = 0xFF

MAN_ID = 0x5449
DIE_ID = 0x2270

RST = 15
CONF_RO2 = 14
CONF_RO1 = 13
CONF_RO0 = 12
AVG2 = 11
AVG1 = 10
AVG0 = 9
VBUSCT2 = 8
VBUSCT1 = 7
VBUSCT0 = 6
ISHCT2 = 5
ISHCT1 = 4
ISHCT0 = 3
MODE2 = 2
MODE1 = 1
MODE0 = 0

OCL = 15
UCL = 14
BOL = 13
BUL = 12
POL = 11
CNVR = 10
AFF = 4
CVRF = 3
OVF = 2
APOL = 1
LEN = 0

class INA260:
       
    def __init__(self, address=0x40, channel=1):
        self.i2c_channel = channel
        self.bus = SMBus(self.i2c_channel)
        self.address = address

    def byte(self, number, i):
        """
        Returns the i-th byte of "number"
        Parameters
            number: the input data
            i: contains the byte location to be retrieved

        Returns the i-th byte of "number"
        """
        return (number & (0xff << (i * 8))) >> (i * 8)

    def bitset(self, number, i):
        """
        Sets i-th bit of "number" to 1. i can be a list 
        Parameters
            number: the input data
            i: contains the location of all the bits that need to be set

        Returns the updated data
        """
        output_number = number
        if isinstance(i, list):
            for num in i:
                output_number = output_number | (1 << num)
        else:
            output_number = output_number | (1 << i)
        return output_number
        
    def bitclear(self, number, i):
        """
        Sets i-th bit of "number" to 0. i can be a list 
        Parameters
            number: the input data
            i: contains the location of all the bits that need to be cleared

        Returns the updated data
        """
        output_number = number
        if isinstance(i, list):
            for num in i:
                output_number = output_number & ~(1 << num)
        else:
            output_number = output_number & ~(1 << i)
        return output_number
    def config(self):
        """
        Configures the INA260 registers

        Returns 0 if the configuration is succesfull.
        """
        status = 0
        # Reset the device
        wr_addr = REG_CONFIG
        wr_data = 0x0000
        wr_data = self.bitset(wr_data, RST)
        try:
            self.write_reg(wr_addr, wr_data)
        except:
            status |= (1<<0)
            print("Unable to connect to the device")
            return status
        
        time.sleep(0.1)

        # Check if device register reading is working 
        man_id = self.manufacturer_id()
        if man_id != MAN_ID:
            print("Device is not reachable. Wrong manufacturer ID")
            status |= (1<<1)
            return status
        die_id = self.die_id()
        if die_id != DIE_ID:
            print("Device is not reachable. Wrong Die ID")
            status |= (1<<2)
            return status
        
        # Set Confugation Register
        wr_addr = REG_CONFIG
        wr_data = 0x0000
        # Check Datasheet of INA260 for each of the bits
        # Make sure to set the read-only bits!!! (CONF_ROX)
        wr_data = self.bitset(wr_data, [CONF_RO2, CONF_RO1, MODE2, MODE0]) 
        # wr_data = self.bitclear(wr_data, [CONF_RO0, AVG2, AVG1, AVG0, VBUSCT2, VBUSCT1, VBUSCT0, ISHCT2, ISHCT1, ISHCT0, MODE1])
        self.write_reg(wr_addr, wr_data)
        time.sleep(0.1)
        rd_data = self.read_reg(wr_addr)
        if rd_data != wr_data:
            print("Device configuration failed")
            status |= (1<<3)
        return status



    def read_reg(self, reg):
        """
        Reads a word from the device
        
        Parameters:
            reg: register address

        Returns the register value (16 bits)
        """
        reg_val = self.bus.read_i2c_block_data(self.address, reg, 2)
        return reg_val[1] | (reg_val[0]<<8)

    def write_reg(self, reg, data):
        """
        Writes a word to the device
        
        Parameters:
            reg: register address
            data: resgister data (16 bits)
        """
        write_data = [self.byte(data,1), self.byte(data,0)]
        res = self.bus.write_i2c_block_data(self.address, reg, write_data)

    def voltage_reg(self):
        """
        Returns the voltage register of INA260 (Register 0x02)
        """
        reg_voltage_raw = self.read_reg(REG_BUS_VOLTAGE)
        return reg_voltage_raw

    def reg_to_volt(self, reg_voltage_raw):
        """
        Converts the voltage register raw value to Volt
        Parameters:
            reg_voltage_raw: raw value read from voltage register of INA260

        Returns the voltage in Volt
        """
        voltage = reg_voltage_raw*0.00125 # 1.25mv/bit
        return voltage

    def current_reg(self):
        """
        Returns the current register of INA260 (Register 0x01)
        """
        reg_current_raw = self.read_reg(REG_CURRENT)
        return reg_current_raw

    def reg_to_amp(self, reg_current_raw):
        """
        Converts the current register raw value to Ampers
        
        Parameters:
            reg_current_raw: raw value read from current register of INA260

        Returns the Current in Ampers
        """
        if reg_current_raw & (1 << 15): #Two's complement
            current = reg_current_raw - 65535
        else:
            current = reg_current_raw
            
        current *=0.00125 # 1.25mA/bit
        return current
    def power_reg(self):
        reg_power_raw = self.read_reg(REG_POWER)
        return reg_power_raw

    def reg_to_watt(self, reg_power_raw):
        """
        Converts the power register raw value to Watts
        Parameters:
            reg_power_raw: raw value read from power register of INA260

        Returns the Power in Watts
        """
        power = reg_power_raw* 0.01 # 10mW/bit
        return power

    def manufacturer_id(self):
        """
        Returns the manufacturer ID - it should always be 0x5449
        """
        man_id = self.read_reg(REG_MANUFACTURER_ID)
        return man_id

    def die_id(self):
        """
        Returns the die ID register - it should be 0x2270.
        """
        die_id = self.read_reg(REG_DIE_ID)
        return die_id

    def __del__(self):
        self.bus.close()
