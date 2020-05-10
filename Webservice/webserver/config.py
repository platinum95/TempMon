
class TempMonConfig: 
    SQLDbHost = "localhost"
    SQLDbUser = "TempMonUser"
    SQLDbPassword = "<Sql PW here>"
    SQLDbName = "TempMonDB"
    SQLDbSensorTable = "sensorData"

    logFilepath = "./log.txt"
    DATABASE = "mysql://" + SQLDbUser + ":" + SQLDbPassword + "@localhost:3308/" + SQLDbName
    username = "webUsername"
    userPassword = "webPassword"
    SECRET_KEY = "<secret_key_here>"

