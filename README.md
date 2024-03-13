# NIST-MK3 Overview
This repository contains software to put on a Raspberry Pi to run the NIST-MK3 microphone array as well as basic scripts to import the data and plot a spectrogram.
The Raspberry Pi interfaces with the NIST over an Ethernet interface with a static IP address. To talk to the Raspberry Pi then requires a connection over a separate interface, i.e. the WLAN.
The software interface to the Raspberry Pi is done using a Jupyter Notebook. This contains the necessary code to start data collects, play back audio, and do basic plots to sanity check the data.
For small data transfers, it is recommended to `scp` the data from Pi to your local computer.
To do bulk transfers of data from the Pi to a separate computer, it is recommended to remove the SD card from the Pi, plug it into your computer and transfer data this way.

## Repo Contents
`cma24b_to_intel32b` NIST program convert to binary <br>
`dhcpd.conf` DHCP server configuration file <br>
`install.sh` installation shell script <br>
`interfaces` network interfaces calling network configuration <br>
`isc-dhcp-server` DHCP server installation <br>
`recording.service` NIST bootstrap recording service <br>
`start.sh` depricated recording parameter setting and start shell script, now done in jupyter notebook, <br>
`stop.sh` depricated recording stop shell script, now done in jupyter notebook, <br>
`wpa_supplicant.conf` configure to use SAM wifi <br>

## Pi Setup and Installation
### OS Image
Currently we are running on a Raspberry Pi 3B+ which is kind of old. As for the OS image, use a Debian 11 (bullseye) image, NOT a Debian 12 (bookworm). Links below:
https://raspi.debian.net/tested-images/
Currently we are using the 2023.01.02	release TODO: check that this works!

Download the Rpi imaging tool and follow these instructions:
https://www.tomshardware.com/reviews/raspberry-pi-headless-setup-how-to,6028.html
Be sure to enable SSH access as well.

### Old method of imaging
Once you have downloaded the image, on your local machine, insert a micro SD card, and image it with the downloaded .img file. On a linux machine, you will need to unpack the image first:

`xz -d MYIMAGE `

Then, once you have the extracted image, you can image your SD card. First, verify where your SD card is mounted to with:

`sudo fdisk -l`

Once you've found the mount point, which should be something like `/dev/sdX` where `X` could be 'a' or 'b' or something else. Verify that the size of this is what you expect the SD card size to be! Now, you are ready to image with:

`sudo dd bs=4M if=/path/to/image of=/dev/sdX`

where again, replace `X` with whatever you found with `fdisk -l`. This may take a minute or two. Once it is done, safely eject the SD card and reinsert it. You should see a new folder pop up titled 'bootfs' or something like that. To enable SSH, create a file called `ssh` in that home directory:

`touch /path/to/bootfs/ssh`

Finally, copy the contents of this repo to the home folder:

## Install NIST MK3 Software on Pi
Once you have a fresh Pi install, you have a few options to get the NIST software working:

1. Clone this repo and simply run `install.sh`. The downside is you will have to provide your github credentials on the Pi.
2. Directly write this repo to the SD card using `cd /path/to/NIST-MK3 && cp ./* /path/to/rootfs/home/pi`

The `install.sh` script will set up the network interfaces required for the Pi to interface with the NIST-MK3 as well  as your local computer over the SAMLAB wifi network.
Parts of this script may not run automatically, it may need some babysitting.

Once this has run, verify that you can launch the jupyter notebook server. First use:
`jupyter notebook password`
to set the password.
Then run:
`systemctl restart jupyter
## Running Data Collection
1. Begin by turning on the Pi, connecting it via Ethernet to the NIST MK3 FPGA board.
2. SSH into the NIST MK3 and run `jupyter notebook --ip 0.0.0.0`
3. Connect to the Jupyter Notebook from your local computer with your browser at `https://PIHOSTNAME:8888`
4. Open the notebook `NIST_MK3.ipynb`
