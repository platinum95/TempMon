#!/bin/bash
if [[ $# -ne 1 ]]
then
    echo "No user password specified. Usage: dbSetup.sh <user password>"
    exit 1
fi

UserPw=$1

echo "Enter MySQL root Password"
read -sp dbRootPassword
echo "Creating DB user TempMonUser@localhost with password " ${UserPw}
mysql -uroot -p{dbRootPassword} -e "SOURCE ./dbSchema.sql;"
mysql -uroot -p{dbRootPassword} -e "CREATE USER IF NOT EXISTS TempMonUser@localhost IDENTIFIED BY '${UserPw}';"
mysql -uroot -p{dbRootPassword} -e "GRANT ALL PRIVILEGES ON TempMonDB.* to 'TempMonUser'@'localhost';"
mysql -uroot -p{dbRootPassword} -e "FLUSH PRIVILEGES"

