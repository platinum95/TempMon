#!/bin/bash

if [[ "$EUID" -ne 0 ]]
then
    echo "Script should be run as root. Failure may occur"
fi

BASE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# Create opt simlink to here
ln -sf ${BASE_DIR} /opt/TempMonPortal 

cp ${PWD}/tempmonportal.service /etc/systemd/system/tempmonportal.service
cp ${PWD}/tempmonreceiver.service /etc/systemd/system/tempmonreceiver.service

systemctl enable tempmonportal.service
systemctl start tempmonportal.service

systemctl enable tempmonreceiver.service
systemctl start tempmonreceiver.service