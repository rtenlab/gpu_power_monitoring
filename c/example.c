#include "INA260.h"
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>

#include <time.h>
#include<signal.h>

u_int8_t user_interrupt = 0;
u_int8_t i2c_error_ind = 0;
u_int8_t measurement_timeout = 0;

__u8 SENSOR_ADDRS[] = {0x40, 0x41, 0x44, 0x45};
#define DEFAULT_SAMPLING_TIME 140
#define MAX_SIM_TIME 1800
#define MIN_SIM_TIME 0.1

#define INIT_RETRY_NUM 10 // Number of retries to initially configure a sensor
#define I2C_RETRY_NUM 100 // Number of retries to retry i2c bus during measurement

// unit conversion code, just to make the conversion more obvious and self-documenting
static long long SecondsToMicros(long long secs) {return secs*1000000;}
static long long NanosToMicros(long long nanos)  {return nanos/1000;}

// Returns the current absolute time, in microseconds, based on the appropriate high-resolution clock
static long long getCurrentTimeMicros()
{
   struct timespec ts;
   return (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) ? (SecondsToMicros(ts.tv_sec)+NanosToMicros(ts.tv_nsec)) : 0;
}

void usr_sig_handler(int signum){
    user_interrupt = 1;
}

void time_sig_handler(int signum){
    measurement_timeout = 1;
}

int main(int argc, char **argv)
{
    signal(SIGUSR1,usr_sig_handler); // Registering signal handler
    signal(SIGALRM,time_sig_handler); // Registering signal handler

    int c;
    float meas_time = 1.0;
    u_int8_t num_sensors = 1;
    char *filename = "measurements.csv";
    u_int8_t current_enable = 0;
    u_int8_t voltage_enable = 0;
    int usr_sampling_time = DEFAULT_SAMPLING_TIME;
    // Parsing the input arguments
    while ((c = getopt (argc, argv, "hn:t:f:cvs:")) != -1)
    {
        switch (c)
            {
            case 'h':
                printf("-h             Display this help and exit\n");
                printf("-t             Set entire measurement time (between %.2f and %.2f seconds\n",(float)MIN_SIM_TIME,(float)MAX_SIM_TIME);
                printf("-c             Enable the current consumption measurement\n");
                printf("-v             Enable the voltage measurement\n");
                printf("-s             Set INA260 sampling time (valid values: 140, 204, 332,\n");
                printf("               588, 1100, 2116, 4156, 8244 microseconds)\n");
                printf("-n             Set number of sensors (between 1 and %d) \n", sizeof(SENSOR_ADDRS)/sizeof(SENSOR_ADDRS[0]));
                printf("-f             Set file name to store measurements\n");
                return 0;
            case 't':
                meas_time = atof(optarg); // Measurement time in seconds (by default it is set to 0.1 seconds)
                if (meas_time > MAX_SIM_TIME)
                {
                    printf("Simulation time is set for too long\n");
                    return 1;
                }
                if (meas_time < MIN_SIM_TIME)
                {
                    printf("Simulation time is set for too short\n");
                    return 1;
                }
                break;
            case 'c':
                current_enable = 1;
                break;
            case 'v':
                voltage_enable = 1;
                break;

            case 's':
                usr_sampling_time = atoi(optarg);
                if (usr_sampling_time != 140  && usr_sampling_time != 204  && usr_sampling_time != 332  && \
                    usr_sampling_time != 588  && usr_sampling_time != 1100 && usr_sampling_time != 2116 && \
                    usr_sampling_time != 4156 && usr_sampling_time != 8244 )
                {
                    usr_sampling_time = 140;
                    printf("\033[0;33mSampling time input cannot be set. The default value of %d us is applied. \033[0m\n",usr_sampling_time);
                }
                break;

            case 'n':
                num_sensors = round(atoi(optarg)); // Number of sensors used for measurements (by default it is set to 1)
                if (num_sensors > sizeof(SENSOR_ADDRS)/sizeof(SENSOR_ADDRS[0]))
                {
                    printf("Addresses are not defined in the code properly.\n");
                    return 1;
                }
                if (num_sensors < 1)
                {
                    printf("\033[31mInvalid number of sensors.\033[0m\n");
                    return 1;
                }
                break;
            case 'f':
                filename = optarg;
                break;
            case '?': 
                if (optopt == 't' || optopt == 'n')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr,
                            "Unknown option character `\\x%x'.\n",
                            optopt);
                return 1;
            default:
                abort();
        }

    }
    if (voltage_enable==0 && current_enable==0)
        current_enable = 1;

    printf("Measurement time is set to %.2f seconds. \n",meas_time);
    
    // Reporting start time and approximate finish time of the program
    struct timespec st_date_time; 
    clock_gettime(CLOCK_REALTIME, &st_date_time);
    struct tm *st_time = localtime(&st_date_time.tv_sec);
    printf("Starting time:           %02d:%02d:%02d \n",st_time->tm_hour,st_time->tm_min,st_time->tm_sec);
    st_date_time.tv_sec = st_date_time.tv_sec + round(meas_time);
    struct tm *end_time = localtime(&st_date_time.tv_sec);
    printf("Approximate finish time: %02d:%02d:%02d \n",end_time->tm_hour,end_time->tm_min,end_time->tm_sec);

    printf("Number of active sensors is set to %d. \n", num_sensors);
    if (voltage_enable==1)
        printf("Voltage measurement is enabled.\n");
    else
        printf("\033[0;33mVoltage measurement is disabled. \033[0m \n");

    if (current_enable==1)
        printf("Current measurement is enabled.\n");
    else
        printf("\033[0;33mCurrent measurement is disabled. \033[0m \n");

    
    printf("Sampling time is set to %d microseconds. \n",usr_sampling_time);

    // Number of samples required for the measurements.
    long measurement_time_us = usr_sampling_time;
    long num_samples = round((meas_time*1000000)/measurement_time_us);

    // Definining the array that contains time took to measure each sample in microseconds
    // with reference to starting time of entire measeasurement  
    __u32 *time_offset_buffer;
    time_offset_buffer = (__u32*) malloc(num_samples * sizeof(__u32));
    if (time_offset_buffer==NULL)
        {
            printf("Could not allocate memory for time_offset_buffer\n");
            return 1;
        }

    // Definining the array that contains measured voltage current samples (register values)  
    __u16 *current_buffer;
    __u16 *voltage_buffer;
    if (current_enable == 1)
    {
        current_buffer = (__u16*) malloc(num_samples * ((long)num_sensors) *sizeof(__u16));
        if (current_buffer==NULL)
            {
                printf("Could not allocate memory for current_buffer.\n");
                return 1;
            }
    }

    if (voltage_enable == 1)
    {
        voltage_buffer = (__u16*) malloc(num_samples * ((long)num_sensors) *sizeof(__u16));
        if (voltage_buffer==NULL)
            {
                printf("Could not allocate memory for voltage_buffer.\n");
                return 1;
            }
    }


    // File ID of each sensor
    int *fd;
    fd = (int*) malloc(num_sensors *sizeof(int));

    // The flag that shows if the sensor is reachable or not
    __u8 *reachable;
    reachable = (__u8*) malloc(num_sensors *sizeof(__u8));

    __u8 s=0;

    for (s=0; s<num_sensors; s++)
    {
        reachable[s] = 0;
        long long time_before_init = getCurrentTimeMicros();

        for (int r=0; r<INIT_RETRY_NUM; r++)
        {
            fd[s] = i2c_init(SENSOR_ADDRS[s]);
            if (ina260_config(fd[s], current_enable, voltage_enable, usr_sampling_time)==0)
            {
                printf("\033[0;32mSensor %d succesfully configured. \033[0m \n", s);
                reachable[s]=1; 
                long long time_after_init = getCurrentTimeMicros();
                printf("Elapsed time for first initialization of Sensor %d: %lld us\n",s ,time_after_init-time_before_init);
                break;
            }
            usleep(30000);
        }

        if (reachable[s]==0)
        {
            printf("\033[31mSensor %d is unreachable.  \033[0m\n", s);
        }

    }
    long long nextExecTimeMicros; // Holds the next time to execute measurements
    long long microsToSleepFor;
    long long meas_starting_timestamp; // Starting time of the measurement (using high presicion clock)

    meas_starting_timestamp = getCurrentTimeMicros();
    nextExecTimeMicros = meas_starting_timestamp + measurement_time_us;
    printf("Measruement started. Please wait...\n");
    alarm(meas_time+1);
    long captured_samples = num_samples;
    struct timespec starting_date_time; // Starting time of the measurement in wall-clock which includes year, month, day, ... (lower precision)
    clock_gettime(CLOCK_REALTIME, &starting_date_time);
    int i2c_retry_cnt = 0;
   for (long i =0; i<num_samples; i++)
    {
        // Making sure there is at least measurement_time_us microseconds between measurments
        microsToSleepFor = nextExecTimeMicros - getCurrentTimeMicros();
        while(microsToSleepFor>0 && i!=0) // Busy waiting sleep works more precise than using usleep!
        {
            microsToSleepFor = nextExecTimeMicros - getCurrentTimeMicros();
        }

        // Calculating the next time to do the measurements 
        nextExecTimeMicros = getCurrentTimeMicros() + measurement_time_us;

        // Calculating the time elapsed to perform one measurement from all the sensor since the starting timestamp
        time_offset_buffer[i] = getCurrentTimeMicros() - meas_starting_timestamp;

        // Performing one measurement for each of the available sensor
        for (s=0; s<num_sensors; s++)
        {
            if (reachable[s]==1)
            {
                int Err = 0;
                do
                {
                    if (Err != 0)
                    {
                        fd[s] = i2c_init(SENSOR_ADDRS[s]);
                        Err = ina260_config(fd[s], current_enable, voltage_enable, usr_sampling_time);
                        printf("\033[31mI2C Error! \033[0m \n");
                        i2c_retry_cnt++;
                    }
                    if (current_enable == 1)
                    {
                        current_buffer[i*((long)num_sensors)+(long)s] = current_read(fd[s]);
                        if (current_buffer[i*((long)num_sensors)+(long)s]==0x7fff)
                            Err = 1;
                    }

                    if (voltage_enable == 1)
                    {
                        voltage_buffer[i*((long)num_sensors)+(long)s] = voltage_read(fd[s]);
                        if (voltage_buffer[i*((long)num_sensors)+(long)s]==0x7fff)
                            Err = 1;
                    }
                    if (i2c_retry_cnt>=I2C_RETRY_NUM)
                        i2c_error_ind = 1;

                } while(Err != 0 && i2c_error_ind == 0);
            }
        }
        if (user_interrupt==1)
        {
            printf("Program was interrupted by user.\n");
            captured_samples = i;
            break;
        }
        if (measurement_timeout==1)
        {
            printf("Measurement time has been reached.\n");
            captured_samples = i;
            break;
        }
        if (i2c_error_ind==1)
        {
            printf("\033[31mI2C bus error caused the program to stop. %lld \033[0m \n");
            captured_samples = i;
            break;
        }
    }
    for (s=0; s<num_sensors; s++)
    {
        if (reachable[s]==1)
        {
            close(fd[s]);
        }
    }

    // Writing Data to file
    printf("Measruement is done. Writing to file...\n");
    struct timespec w_st, w_et;// Writing to file starting and ending time
    clock_gettime(CLOCK_REALTIME, &w_st);
    long long min_meas_time = 1000000000;
    long long max_meas_time = 0;
    long long avg_meas_time = 0;
    long long sum_meas_time = 0;

    FILE *fpt;
    fpt = fopen(filename, "w+");
    fprintf(fpt,"Date,Time of the day (us)");
    for (s=0; s<num_sensors; s++)

        if (reachable[s]==1)
            {
                if (current_enable == 1)
                    fprintf(fpt,",Sensor %#02X current (mA)",SENSOR_ADDRS[s]);

                if (voltage_enable == 1)
                    fprintf(fpt,",Sensor %#02X voltage (mV)",SENSOR_ADDRS[s]);
            }
    
    fprintf(fpt,"\n");
    long long time_of_day_us;
    struct timespec timestamp;
    struct tm *st = localtime(&starting_date_time.tv_sec);
    long long starting_time_of_day_us = 0;
    starting_time_of_day_us = (((long long)(st->tm_hour))*3600 + ((long long)(st->tm_min))*60 + ((long long)(st->tm_sec)))*1000000 + (long long) starting_date_time.tv_nsec/1000;
    signed short max_current = 0;
    signed short min_current = 0x7FFF;
    signed short max_voltage = 0;
    signed short min_voltage = 0x7FFF;
    signed short current_ma = 0;
    signed short voltage_mv = 0;

    for (long i =0; i<captured_samples-1; i++)
    {
        timestamp.tv_sec = starting_date_time.tv_sec + (time_offset_buffer[i] + starting_date_time.tv_nsec/1000)/1000000;
        struct tm *ct = localtime(&timestamp.tv_sec);

        // Writing date (year, month, and day)
        fprintf(fpt,"%02d/%02d/%04d,",(ct->tm_mon+1), ct->tm_mday, (ct->tm_year+1900));
        
        // Writing time of the day in microseconds (number of microseconds past since 12:00AM)
        time_of_day_us = starting_time_of_day_us + (long long)time_offset_buffer[i];
        fprintf(fpt,"%lld",time_of_day_us);

        // Writing Sensor Data
        for (s=0; s<num_sensors; s++)
        {
            if (reachable[s]==1)
            {
                if (current_enable == 1)
                {
                    current_ma = reg_to_amp(current_buffer[i*((long)num_sensors)+((long)s)]);
                    fprintf(fpt,",%d",current_ma);
                    if (current_ma > max_current)
                        max_current = current_ma;
                    if (current_ma < min_current)
                        min_current = current_ma;
                }
                if (voltage_enable == 1)
                {
                    voltage_mv = reg_to_volt(voltage_buffer[i*((long)num_sensors)+((long)s)]);
                    fprintf(fpt,",%d",voltage_mv);
                    if (voltage_mv > max_voltage)
                        max_voltage = voltage_mv;
                    if (voltage_mv < min_voltage)
                        min_voltage = voltage_mv;
                }
            }
        }
        long long time_diff = time_offset_buffer[i+1]-time_offset_buffer[i];
        sum_meas_time = sum_meas_time + time_diff;
        if (time_diff>max_meas_time)
            max_meas_time = time_diff;

        if (time_diff<min_meas_time)
            min_meas_time = time_diff;


        fprintf(fpt,"\n");
    }    
    fclose(fpt);

    clock_gettime(CLOCK_REALTIME, &w_et);
    printf("Writing measruements to file is succesfully finished!\n");
    printf("It took %ld seconds to write %ld samples to file.\n",(w_et.tv_sec-w_st.tv_sec),captured_samples-1);

    avg_meas_time = sum_meas_time/(captured_samples-1);
    printf("Maximum measurement time: %lld us\n", max_meas_time);
    printf("Minimum measurement time: %lld us\n", min_meas_time);
    printf("Average measurement time: %lld us\n", avg_meas_time);

    free(time_offset_buffer);
    if (current_enable==1)
    {
        printf("Maximum Current recorded: %d mA\n",max_current);
        printf("Minimum Current recorded: %d mA\n",min_current);
        free(current_buffer);
    }
    if (voltage_enable==1)
    {
        printf("Maximum Voltage recorded: %d mV\n",max_voltage);
        printf("Minimum Voltage recorded: %d mV\n",min_voltage);
        free(voltage_buffer);
    }
    free(fd);
    free(reachable);

    return 0;
}
