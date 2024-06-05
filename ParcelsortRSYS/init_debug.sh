#!/bin/sh
echo 1000000 > /proc/sys/kernel/sched_rt_period_us
echo 950000 > /proc/sys/kernel/sched_rt_runtime_us
