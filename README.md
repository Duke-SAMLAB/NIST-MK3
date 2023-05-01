# micarray-pi
Scripts and configuration files to setup a Raspberry Pi to interface with the NIST MK3 microphone array

Run install.sh on Raspberry Pi to install. Here is a brief description of each file:

`cma24b_to_intel32b` NIST program convert to binary
`dhcpd.conf` DHCP server configuration file
`install.sh` installation shell script
`interfaces` network interfaces calling network configuration
`isc-dhcp-server` DHCP server installation
`recording.service` NIST bootstrap recording service
`start.sh` depricated recording parameter setting and start shell script, now done with in jupyter notebook,
`stop.sh` depricated recording stop shell script, now done with in jupyter notebook,
`wpa_supplicant.conf` configure to use SAM wifi
