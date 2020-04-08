#!/bin/bash

if [[ "$EUID" -ne 0 ]]
then
    echo "Script should be run as root to access MySQL. Failure may occur"
fi


if [[ $# -ne 1 ]]
then
    read -sp "New user password: " UserPw
    echo
    read -sp "Enter new password again: " UserPw2
    echo
    if [[ "${UserPw}" != "${UserPw2}" ]] 
    then
        echo "Passwords did not match"
        exit 1
    fi
else
    UserPw=$1
fi

read -sp "MySQL root password: " dbRootPassword
echo
mysql -uroot -p{dbRootPassword} -e "SOURCE ./dbSchema.sql;"
mysql -uroot -p{dbRootPassword} -e "CREATE USER IF NOT EXISTS TempMonUser@localhost IDENTIFIED BY '${UserPw}';"
mysql -uroot -p{dbRootPassword} -e "GRANT ALL PRIVILEGES ON TempMonDB.* to 'TempMonUser'@'localhost';"
mysql -uroot -p{dbRootPassword} -e "FLUSH PRIVILEGES"

