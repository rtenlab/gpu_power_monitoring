#include "INA260.h"
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>

#include <time.h>
#include<signal.h>

u_int8_t finish_reading = 0;
__u8 SENSOR_ADDRS[] = {0x40, 0x41, 0x44, 0x45};
#define MEASUREMENT_DELAY_us 130 // To insure there is at least MEASUREMENT_DELAY_us of time between two measurements
#define MEASUREMENT_TIME_us 150 // approximate time between measurements in reality (used to calculate number of samples)

// unit conversion code, just to make the conversion more obvious and self-documenting
static long long SecondsToMicros(long secs) {return secs*1000000;}
static long long NanosToMicros(long nanos)  {return nanos/1000;}

// Returns the current absolute time, in microseconds, based on the appropriate high-resolution clock
static long long getCurrentTimeMicros()
{
   struct timespec ts;
   return (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) ? (SecondsToMicros(ts.tv_sec)+NanosToMicros(ts.tv_nsec)) : 0;
}

void sig_handler(int signum){
    finish_reading = 1;
}

int main(int argc, char **argv)
{
    signal(SIGUSR1,sig_handler); // Registering signal handler

    int c;
    float meas_time = 0.001;
    u_int8_t num_sensors = 1;
    char *filename = "current_measurements.csv";


    // Parsing the input arguments
    while ((c = getopt (argc, argv, "n:t:f:")) != -1)
    {
        switch (c)
            {
            case 't':
                meas_time = atof(optarg); // Measurement time in seconds (by default it is set to 0.1 seconds)
                if (meas_time > 1800)
                {
                    printf("Simulation time is set for too long");
                    return 1;
                }
                break;
            case 'n':
                num_sensors = round(atoi(optarg)); // Number of sensors used for measurements (by default it is set to 1)
                if (num_sensors > sizeof(SENSOR_ADDRS)/sizeof(SENSOR_ADDRS[0]))
                {
                    printf("Addresses are not defined in the code properly");
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
    printf("Measurement time is set to %f seconds \n",meas_time);
    printf("Number of active sensors is set to %d \n", num_sensors);

    // Number of samples required for the measurements.
    long num_samples = round((meas_time*1000000)/MEASUREMENT_TIME_us);

    // Definining the array that contains time took to measure each sample in microseconds
    // with reference to starting time of entire measeasurement  
    __u32 *time_offset_buffer;
    time_offset_buffer = (__u32*) malloc(num_samples * sizeof(__u32));

    // Definining the array that contains measured electricity current samples (register values)  
    __u16 *current_buffer;
    current_buffer = (__u16*) malloc(num_samples * num_sensors *sizeof(__u16));

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
        if (ina260_config(fd[s])==0)
        {
            printf("Sensor %d succesfully configured.\n", s);
            reachable[s]=1;
        }
        else
        {
            printf("Sensor %d is unreachable.\n", s);
            reachable[s]=0;
        }

    }

    long long nextExecTimeMicros; // Holds the next time to execute measurements
    long long microsToSleepFor;
    long long meas_starting_timestamp; // Starting time of the measurement (using high presicion clock)
    struct timespec starting_date_time; // Starting time of the measurement in wall-clock which includes year, month, day, ... (lower precision)
    clock_gettime(CLOCK_REALTIME, &starting_date_time);

    meas_starting_timestamp = getCurrentTimeMicros();
    nextExecTimeMicros = meas_starting_timestamp + MEASUREMENT_DELAY_us;
    printf("Measruement started. Please wait...\n");
    long captured_samples = num_samples;
    for (long i =0; i<num_samples; i++)
    {
        // Making sure there is at least MEASUREMENT_DELAY_us microseconds between measurments
        microsToSleepFor = nextExecTimeMicros - getCurrentTimeMicros();
        while(microsToSleepFor>0 && i!=0) // Busy waiting sleep works more precise than using usleep!
        {
            microsToSleepFor = nextExecTimeMicros - getCurrentTimeMicros();
        }

        // Calculating the next time to do the measurements 
        nextExecTimeMicros = getCurrentTimeMicros() + MEASUREMENT_DELAY_us;

        // Calculating the time elapsed to perform one measurement from all the sensor since the starting timestamp
        time_offset_buffer[i] = getCurrentTimeMicros() - meas_starting_timestamp;

        // Performing one measurement for each of the available sensor
        for (s=0; s<num_sensors; s++)
        {
            if (reachable[s]==1)
                current_buffer[i*num_sensors+s] =current_read(fd[s]);
        }
        if (finish_reading==1)
        {
            printf("Program was interrupted by user\n");
            captured_samples = i;
            break;
        }
    }
    // Printing out measurements as well as time takes for each measurement
    /*
    for (int i =0; i<num_samples; i++)
    {
        int time_diff;
        if (i==0)
            time_diff = time_offset_buffer[i];
        else
            time_diff = time_offset_buffer[i]-time_offset_buffer[i-1];

        int current_value = reg_to_amp(current_buffer[i]);
        printf("Sampling Time: %d us\n", time_diff);
        for (s=0; s<num_sensors; s++)
        {
            if (reachable[s]==1)
            {
                printf("Sensor %d Current: %d mA \n", s, reg_to_amp(current_buffer[i*num_sensors+s]));
            }
        }

    }
    */
    for (s=0; s<num_sensors; s++)
    {
        if (reachable[s]==1)
            close(fd[s]);
    }

    // Writing Data to file
    printf("Measruement is done. Writing to file... Please wait!\n");

    FILE *fpt;
    struct tm *st = localtime(&starting_date_time.tv_sec);

    fpt = fopen(filename, "w+");
    fprintf(fpt,"Sample,");
    for (s=0; s<num_sensors; s++)
        if (reachable[s]==1)
            fprintf(fpt,"Sensor %#02X current (mA),",SENSOR_ADDRS[s]);
    fprintf(fpt,"Sampling Time Offset (us),");
    fprintf(fpt,"Starting Time: %02d/%02d/%02d %02d:%02d:%02d.%ld,", (st->tm_mon + 1), st->tm_mday, (1900+st->tm_year), st->tm_hour, st->tm_min, st->tm_sec, starting_date_time.tv_nsec/1000);
    fprintf(fpt,"%ld, %ld\n", starting_date_time.tv_sec, starting_date_time.tv_nsec);
    

     for (long i =0; i<captured_samples; i++)
    {
        fprintf(fpt,"%ld,",i);
        for (s=0; s<num_sensors; s++)
        {
            if (reachable[s]==1)
            {
                fprintf(fpt,"%d,",reg_to_amp(current_buffer[i*num_sensors+s]));
            }
        }
        fprintf(fpt,"%u\n",time_offset_buffer[i]);
    }
   
    printf("Measruement to file succesfully finished!\n");
    fclose(fpt);



    free(time_offset_buffer);
    free(current_buffer);
    free(fd);
    free(reachable);

    return 0;
}
