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

## Installation
To do a fresh installation on a new Pi, clone this repo and simply run `install.sh`. This script will set up the network interfaces required for the Pi to interface with the NIST-MK3 as well  as your local computer over the SAMLAB wifi network.

## Running Data Collection
1. Begin by `ssh`ing into the Pi and launching a jupyter notebook
