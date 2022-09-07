#include "INA260.h"
#include <stdio.h>
#include <unistd.h>

#include <time.h>

#define NUM_SAMPLES 20
#define NUM_SENSORS 4
__u8 SENSOR_ADDRS[] = {0x40, 0x41, 0x44, 0x45};
#define MEASUREMENT_DELAY_us 130

// unit conversion code, just to make the conversion more obvious and self-documenting
static unsigned long long SecondsToMicros(unsigned long secs) {return secs*1000000;}
static unsigned long long NanosToMicros(unsigned long nanos)  {return nanos/1000;}

// Returns the current absolute time, in microseconds, based on the appropriate high-resolution clock
static unsigned long long getCurrentTimeMicros()
{
   struct timespec ts;
   return (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) ? (SecondsToMicros(ts.tv_sec)+NanosToMicros(ts.tv_nsec)) : 0;
}


int main(int argc, char **argv)
{
    long long time_buffer[NUM_SAMPLES];
    __u16 current_buffer[NUM_SAMPLES*NUM_SENSORS];
    int fd[NUM_SENSORS];
    __u8 reachable[NUM_SENSORS];
    int current_values[NUM_SAMPLES];
    __u8 s=0;

    for (s=0; s<NUM_SENSORS; s++)
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

    unsigned long long nextExecTimeMicros = getCurrentTimeMicros() + MEASUREMENT_DELAY_us;
    long long microsToSleepFor;

    for (int i =0; i<NUM_SAMPLES; i++)
    {
        microsToSleepFor = nextExecTimeMicros - getCurrentTimeMicros();
        if (microsToSleepFor > 0) usleep(microsToSleepFor);
        time_buffer[i] = getCurrentTimeMicros();
        nextExecTimeMicros = time_buffer[i] + MEASUREMENT_DELAY_us;
        for (s=0; s<NUM_SENSORS; s++)
        {
            if (reachable[s]==1)
                current_buffer[i*NUM_SENSORS+s] =current_read(fd[s]);
        }

        
    }
    for (int i =0; i<(NUM_SAMPLES-1); i++)
    {
        int time_diff = time_buffer[i+1] - time_buffer[i];
        int current_value = reg_to_amp(current_buffer[i]);
        printf("Sampling Time: %d us\n", time_diff);
        for (s=0; s<NUM_SENSORS; s++)
        
        {
            if (reachable[s]==1)
            {
                printf("Sensor %d Current: %d uA \n", s, reg_to_amp(current_buffer[i*NUM_SENSORS+s]));
            }
        }

    }
    for (s=0; s<NUM_SENSORS; s++)
    {
        if (reachable[s]==1)
            close(fd[0]);
    }
    return 0;
}
