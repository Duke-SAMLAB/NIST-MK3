echo "Adding recording service as a daemon service"

cd ~/micarray-pi
sudo cp recording.service /lib/systemd/system/
sudo cp micarray-pi/start.sh /home/pi/
sudo cp micarray-pi/stop.sh /home/pi/
sudo chmod +x ~/start.sh
sudo chmod +x ~/stop.sh
sudo systemctl daemon-reload
sudo systemctl enable recording.service