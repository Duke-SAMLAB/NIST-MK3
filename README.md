# micarray-pi
Scripts and configuration files to setup a Raspberry Pi to interface with the NIST MK3 microphone array

Run install.sh on Raspberry Pi to install. Here is a brief description of each file:

`cma24b_to_intel32b` NIST program convert to binary <br>
`dhcpd.conf` DHCP server configuration file <br>
`install.sh` installation shell script <br>
`interfaces` network interfaces calling network configuration <br>
`isc-dhcp-server` DHCP server installation <br>
`recording.service` NIST bootstrap recording service <br>
`start.sh` depricated recording parameter setting and start shell script, now done in jupyter notebook, <br>
`stop.sh` depricated recording stop shell script, now done in jupyter notebook, <br>
`wpa_supplicant.conf` configure to use SAM wifi <br>
