sudo apt-get install isc-dhcp-server

cd /home/pi/micarray-pi
echo "Configuring DHCPD service to interface with array"
sudo cp dhcpd.confg /etc/dhcp/dhcpd.conf
sudo cp interfaces /etc/network/interfaces
sudo cp isc-dhcp-server /etc/default/isc-dhcp-server
sudo service isc-dhcp-server restart

echo "Adding recording service as a daemon service"

sudo cp recording.service /lib/systemd/system/
sudo cp start.sh /home/pi/
sudo cp stop.sh /home/pi/
sudo chmod +x /home/pi/start.sh
sudo chmod +x /home/pi/stop.sh
sudo systemctl daemon-reload
sudo systemctl enable recording.service
