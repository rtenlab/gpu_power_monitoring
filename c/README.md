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
./example -t SIMULATION_TIME_s -n NUMBER_OF_SENSORS -f FILE_NAME
```

Default values for ```SIMULATION_TIME_s``` , ```NUMBER_OF_SENSORS```, and ```FILE_NAME``` are 0.001 and 1, and current_measurements.csv respectively. For example to run the code for 3 sensors and simulation time of about 60 seconds and save in test.csv file:

```
./example -t 60 -n 3 -f test.csv
```

The user can interrupt the sensing by raising the ```SIGUSR1``` signal. In this case, the program stops the sensing and writes all the received data to the file. An example command in terminal to safely stop the program while the program is running

```
kill -s SIGUSR1 PID_OF_THE_PROGRAM
```

