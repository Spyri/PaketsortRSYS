while read line; do
	echo $line
	# do something here, like e.g. echo to RTAI FIFO
done < <(cat /dev/ttyUSB0)

