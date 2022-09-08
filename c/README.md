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
./example -t SIMULATION_TIME_s -n NUMBER_OF_SENSORS
```
Default values for SIMULATION_TIME_s and NUMBER_OF_SENSORS are 0.001 and 1, respectively. For example to run the code for 3 sensors and simulation time of about 60 seconds:
```
./example -t 60 -n 3
```


