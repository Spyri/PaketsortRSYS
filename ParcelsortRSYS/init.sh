insmod /usr/realtime/modules/rtai_hal.ko
insmod /usr/realtime/modules/rtai_sched.ko
insmod /usr/realtime/modules/rtai_fifos.ko
insmod /usr/realtime/modules/rtai_sem.ko

echo 500000 > /proc/sys/kernel/sched_rt_runtime_us

sysctl -p
