#include "INA260.h"
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>

#include <time.h>
#include<signal.h>

u_int8_t user_interrupt = 0;
u_int8_t measurement_timeout = 0;
__u8 SENSOR_ADDRS[] = {0x40, 0x41, 0x44, 0x45};
// #define MEASUREMENT_DELAY_us 140 // To insure there is at least MEASUREMENT_DELAY_us of time between two measurements
// #define MEASUREMENT_TIME_us 140 // approximate time between measurements in reality (used to calculate number of samples)
#define RETRY_NUM 5 // Number of retries to configure a sensor

// unit conversion code, just to make the conversion more obvious and self-documenting
static long long SecondsToMicros(long secs) {return secs*1000000;}
static long long NanosToMicros(long nanos)  {return nanos/1000;}

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
    float meas_time = 0.001;
    u_int8_t num_sensors = 1;
    char *filename = "current_measurements.csv";
    u_int8_t current_enable = 0;
    u_int8_t voltage_enable = 0;
    u_int8_t sampling_time = CONVERSION_TIME_140us;
    int usr_sampling_time = 140;

    // Parsing the input arguments
    while ((c = getopt (argc, argv, "n:t:f:cvs:")) != -1)
    {
        switch (c)
            {
            case 't':
                meas_time = atof(optarg); // Measurement time in seconds (by default it is set to 0.1 seconds)
                if (meas_time > 1800)
                {
                    printf("Simulation time is set for too long\n");
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
                switch (usr_sampling_time)
                {
                    case 140:
                        sampling_time = CONVERSION_TIME_140us;
                        break;
                    case 204:
                        sampling_time = CONVERSION_TIME_204us;
                        break;
                    case 332:
                        sampling_time = CONVERSION_TIME_332us;
                        break;
                    case 588:
                        sampling_time = CONVERSION_TIME_588us;
                        break;
                    case 1100:
                        sampling_time = CONVERSION_TIME_1100us;
                        break;
                    case 2116:
                        sampling_time = CONVERSION_TIME_2116us;
                        break;
                    case 4156:
                        sampling_time = CONVERSION_TIME_4156us;
                        break;
                    case 8244:
                        sampling_time = CONVERSION_TIME_8244us;
                        break;
                    default:
                        usr_sampling_time = 140;
                        printf("\033[0;33mSampling time input cannot be set. The default value of %d us is applied. \033[0m\n",usr_sampling_time);
                        break;
                }
                break;

            case 'n':
                num_sensors = round(atoi(optarg)); // Number of sensors used for measurements (by default it is set to 1)
                if (num_sensors > sizeof(SENSOR_ADDRS)/sizeof(SENSOR_ADDRS[0]))
                {
                    printf("Addresses are not defined in the code properly.\n");
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

    printf("Measurement time is set to %f seconds. \n",meas_time);
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
        fd[s] = i2c_init(SENSOR_ADDRS[s]);
        reachable[s] = 0;
        for (int r=0; r<RETRY_NUM; r++)
        {
            if (ina260_config(fd[s], current_enable, voltage_enable, sampling_time)==0)
            {
                printf("\033[0;32mSensor %d succesfully configured. \033[0m \n", s);
                reachable[s]=1;
                break;
            }
            usleep(10000);
        }

        if (reachable[s]==0)
        {
            printf("\033[31mSensor %d is unreachable.  \033[0m\n", s);
        }

    }
    // fd[1]=fd[0];
    // fd[2]=fd[0];
    // reachable[1]=reachable[0];
    // reachable[2]=reachable[0];

    long long nextExecTimeMicros; // Holds the next time to execute measurements
    long long microsToSleepFor;
    long long meas_starting_timestamp; // Starting time of the measurement (using high presicion clock)
    struct timespec starting_date_time; // Starting time of the measurement in wall-clock which includes year, month, day, ... (lower precision)
    clock_gettime(CLOCK_REALTIME, &starting_date_time);

    meas_starting_timestamp = getCurrentTimeMicros();
    nextExecTimeMicros = meas_starting_timestamp + measurement_time_us;
    printf("Measruement started. Please wait...\n");
    alarm(meas_time+1);
    long captured_samples = num_samples;
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
                if (current_enable == 1)
                    current_buffer[i*((long)num_sensors)+(long)s] = current_read(fd[s]);

                if (voltage_enable == 1)
                    voltage_buffer[i*((long)num_sensors)+(long)s] = voltage_read(fd[s]);
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

    }
    // Printing out measurements as well as time takes for each measurement
    /*
    for (int i =0; i<captured_samples; i++)
    {
        int time_diff;
        if (i==0)
            time_diff = time_offset_buffer[i];
        else
            time_diff = time_offset_buffer[i]-time_offset_buffer[i-1];

        if (i%256 == 0)
            {
            printf("Sampling Time: %d us\n", time_diff);

            for (s=0; s<num_sensors; s++)
            {
                if (reachable[s]==1)
                {

                    printf("Sensor %d Current: %d mA \n", s, reg_to_amp(current_buffer[i*((long)num_sensors)+((long)s)]));
                }
            }

        }

    }
    */
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

    for (long i =0; i<captured_samples; i++)
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
                    fprintf(fpt,",%d",reg_to_amp(current_buffer[i*((long)num_sensors)+((long)s)]));                
                if (voltage_enable == 1)
                    fprintf(fpt,",%d",reg_to_volt(voltage_buffer[i*((long)num_sensors)+((long)s)]));                
            }
        }
        fprintf(fpt,"\n");
    }    
    fclose(fpt);

    clock_gettime(CLOCK_REALTIME, &w_et);
    printf("Writing measruements to file is succesfully finished!\n");
    printf("It took %ld seconds to write to file.\n",(w_et.tv_sec-w_st.tv_sec));

    free(time_offset_buffer);
    if (current_enable==1)
        free(current_buffer);
    if (voltage_enable==1)
        free(voltage_buffer);
    free(fd);
    free(reachable);

    return 0;
}
