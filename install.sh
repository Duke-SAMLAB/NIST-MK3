sudo apt-get update

echo "export LC_ALL=en_US.UTF-8" >> ~/.bashrc
echo "export LANG=en_US.UTF-8" >> ~/.bashrc
echo "export LANGUAGE=en_US.UTF-8" >> ~/.bashrc
source ~/.bashrc

sudo apt-get upgrade
sudo apt-get install isc-dhcp-server vim curl

wget http://repo.continuum.io/miniconda/Miniconda3-latest-Linux-armv7l.sh
/bin/bash Miniconda3-latest-Linux-armv7l.sh
~/miniconda3/bin/conda init bash
pip install jupyter

curl -O https://www.nist.gov/document/mk3zip
unzip mk3.zip
sudo echo "export PATH=$PATH:/home/pi/mk3/mk3cap" >> /home/pi/.bashrc
sudo bash /home/pi/.bashrc

echo "Configuring DHCPD service to interface with array"
sudo cp dhcpd.conf /etc/dhcp/dhcpd.conf
sudo cp isc-dhcp-server /etc/default/isc-dhcp-server
sudo cp wpa_supplicant.conf /etc/wpa_supplicant/wpa_supplicant.conf
sudo cp jupyter.service /usr/lib/systemd/system/jupyter.service
sudo /etc/init.d/networking reload
sudo systemctl daemon-reload
sudo /etc/init.d/networking restart
sudo service isc-dhcp-server restart

echo "Adding recording service as a daemon service"

#sudo cp recording.service /lib/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable jupyter
#sudo systemctl enable recording.service
