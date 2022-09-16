# GPU Power Monitoring C package
This package uses smbus C library to configure and read current from INA260 sensors
## Usage
To make the codes:
```
make
```
To clean the code:
```
make clean
```
To run the example code after making:
```
./example
```

Paremeters for the example code:
```
-h             Display help and exit
-t             Set entire measurement time (between 0.10 and 1800.00 seconds). Default: 1
-c             Enables the current consumption measurement. Default: Enabled if neither of -c nor -v are selected 
-v             Enables the voltage measurement. Default: Disabled
-s             Set INA260 sampling time (valid values 140, 204, 332,
               588, 1100, 2116, 4156, 8244 microseconds). Default: 140
-n             Set number of sensors (between 1 and 4): Default: 1
-f             Set file name to store measurements: Default: measurements.csv
```

For example, to run the code to measure current and voltage for 3 sensors with sampling rate of 1100 microseconds and entire measurement time of 60 seconds and save in test.csv file:

```
./example -n 3 -c -v -f test.csv -s 1100 -t 60
```

The user can interrupt the sensing by raising the ```SIGUSR1``` signal. In this case, the program stops the sensing and writes all the received data to the file. An example command in terminal to safely stop the program while the program is running

```
pkill -SIGUSR1 example
```

