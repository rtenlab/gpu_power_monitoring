# GPU Power Monitoring
This project aims to implement fast GPU power monitoring system using INA260 sensors connected to a Raspberry Pi 3

## Changeing Raspberry i2c Speed

In order to achieve higher i2c speed, it is required to change the default i2c speed of Raspberry Pi from 100Mbp to 2Mbps
1. Open /boot/config.txt file
```
sudo nano /boot/config.txt
```

2. Find the line Find the line containing **/boot/config.txt** and add **,i2c_arm_baudrate=2000000** to the end of the line. The line should become like the following
```
dtparam=i2c_arm=on,i2c_arm_baudrate=2000000
```

3. reboot the Raspberry Pi
```
sudo reboot
```
4. Verify if the changed are applied
```
bash i2cspeed.sh
```

## installing requirements
```
sudo pip3 install -r requirements.txt
```
## Usage
To run the exmple codes:
```
python3 example.py
```

