#! /bin/bash

# set up networking
# Note: DHCPD needs to already be configured and running on boot
sudo ifconfig wlan0 10.0.0.1

# make recordings directory if it doesn't exist
if [ ! -d /home/pi/recordings ]; then
	mkdir /home/pi/recordings
fi

record_function() {
	touch /home/pi/recordings/recordings.continue
	while [ -f /home/pi/recordings/recordings.continue ]
	do
		echo "We are recording" >> /home/pi/recordings/recordings.log
		sleep 10
	done
}

record_function &
