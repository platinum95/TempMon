#!/usr/bin/env python3

import paho.mqtt.client as mqtt
import json
import mysql.connector
from config import TempMonConfig as CONFIG
import time

def on_connect( client, userdata, flags, rc ):
    global tempMonDb
    print( "We did it! rc:" + str( rc ) )
    print( "Creating DB connection" )
    tempMonDb = mysql.connector.connect(
            host=CONFIG.SQLDbHost,
            user=CONFIG.SQLDbUser,
            database=CONFIG.SQLDbName,
            passwd=CONFIG.SQLDbPassword )
    print( "DB: " + str( tempMonDb ) )
    
def on_message( client, userdata, msg ):
    global tempMonDb
    if( msg.topic == "test/message" ):
        y = json.loads( msg.payload )
        temp = float( y[ "temp" ] )
        hum = float( y[ "hum" ] )
        eTime = time.time()
        print( "Temperature: " + str( temp ) + ", humidity: " + str( hum ) )
        sql = "INSERT INTO SensorData (temp, humidity, timestamp) VALUES(%s, %s, %s)"
        vals = ( temp, hum, eTime )
        if ( not tempMonDb.is_connected() ):
            tempMonDb.reconnect()
            if ( not tempMonDb.is_connected() ):
                print ( "Lost connection to MySQL, can't store data" )
                return
        cursor = tempMonDb.cursor()
        cursor.execute( sql, vals )
        tempMonDb.commit()

    else:
        print( "Received: " + str( msg.payload ) )


def on_subscribe(mqttc, obj, mid, granted_qos):
    print( "Subscribed: " + str( mid ) + " " + str( granted_qos ) )

def on_log(mqttc, obj, level, string):
    print(string)

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.on_subscribe = on_subscribe
client.on_log = on_log

client.connect( "localhost", 1883, 60 )

client.subscribe( "test/message" )

client.loop_forever()
