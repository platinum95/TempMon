#!/bin/bash

python3 -m venv $(pwd)/venv
source ./venv/bin/activate

apt install mariadb-server mosquitto
pip3 install SQLAlchemy flask gunicorn eventlet flask_socketio mysqlclient mysql-connector paho-mqtt
python3 sensorDb/setup.py install
pip3 install sensorDb/.

