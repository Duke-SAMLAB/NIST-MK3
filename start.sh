#! /bin/bash

# set up networking
# Note: DHCPD needs to already be configured and running on boot
sudo ifconfig wlan0 10.0.1.1
sudo ifconfig eth0 10.0.0.1

# make recordings directory if it doesn't exist
if [ ! -d /home/pi/recordings ]; then
	mkdir /home/pi/recordings
fi

record_function() {
	touch /home/pi/recordings/recordings.continue
	PATH="$PATH:/home/pi/mk3/mk3cap"
	local i=0
	# 20 seconds of recording is 80MB
	local recordtime=20
	local numrecords=10
	while [ -f /home/pi/recordings/recordings.continue ]
	do
		local recording=$(echo "/home/pi/recordings/$i.recording")
		local entry=$(echo "Recording to $recording at $(date) ")
		echo "$entry" >> /home/pi/recordings/recordings.log
		mk3cap -t "$recordtime" "$recording" >> /home/pi/recordings/recordings.log 2>/tmp/error
		if grep -irn -q error /tmp/error;
		then
			echo "Received error in recording, exitting" >> /home/pi/recordings/recordings.log
			rm -f /home/pi/recordings/recordings.continue
		fi
		i=$(((i+1) % numrecords))
	done
}

record_function &
