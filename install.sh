sudo apt-get install isc-dhcp-server git curl

curl -O https://www.nist.gov/sites/default/files/documents/itl/iad/mig/smartspace/mk3.zip
unzip mk3.zip
sudo echo "export PATH=$PATH:/home/pi/mk3/mk3cap" >> /home/pi/.bashrc
sudo source /home/pi/.bashrc

cd /home/pi/micarray-pi
echo "Configuring DHCPD service to interface with array"
sudo cp dhcpd.confg /etc/dhcp/dhcpd.conf
sudo cp isc-dhcp-server /etc/default/isc-dhcp-server
sudo service isc-dhcp-server restart

echo "Adding recording service as a daemon service"

#sudo cp recording.service /lib/systemd/system/
sudo chmod +x /home/pi/micarray-pi/start.sh
sudo chmod +x /home/pi/micarray-pi/stop.sh
#sudo systemctl daemon-reload
#sudo systemctl enable recording.service
