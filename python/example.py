import ina260
from datetime import datetime
import time
NUM_SAMPLES = 20
NUM_SENSORS = 4
SENSOR_ADDRS = [0x40, 0x41, 0x44, 0x45]

sensor = [None]*NUM_SENSORS
reachable = [0]*NUM_SENSORS
for s in range(NUM_SENSORS):    
    addr = SENSOR_ADDRS[s]
    sensor[s] = ina260.INA260(address=addr)
    res = sensor[s].config()
    if res !=0:
        print(f'Sensor {s} is unreachable.')
        reachable[s]=0
    else:
        print(f'Sensor {s} succesfully configured.')
        reachable[s]=1




time_buffer = [datetime.now()]*NUM_SAMPLES
current_buffer = [0]*(NUM_SENSORS*NUM_SAMPLES)
time.sleep(0.5)

for i in range(NUM_SAMPLES):
    time_buffer[i] = datetime.now()
    for s in range(NUM_SENSORS):
        if reachable[s]==1:
            current_buffer[NUM_SENSORS*i+s] = sensor[s].current_reg()

for i in range(NUM_SAMPLES-1):
    delta = time_buffer[i+1]-time_buffer[i]
    print(f' Sampling Time: {int(delta.total_seconds() * 1000000)}')
    for s in range(NUM_SENSORS):
        if reachable[s]==1:
            current_buffer[NUM_SENSORS*i+s] = sensor[s].current_reg()
            print(f' Sensor {s} Current: {sensor[s].reg_to_amp(current_buffer[NUM_SENSORS*i+s])} A')
    


