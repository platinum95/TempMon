'''
----Sensor Portal----
A Sensor Portal page 
implemented using WebSockets/Socket.io
-----------------
'''

from flask import Flask, render_template, session, request, \
    copy_current_request_context, redirect, url_for
from flask_socketio import SocketIO, emit, disconnect
from flask_sqlalchemy import SQLAlchemy
import re
from config import TempMonConfig as Config
import sensorDb
from sensorDb.models import SensorDataPoint
from threading import Lock
import os
import eventlet
import subprocess
import time

print( "Hello Servo" )

thread = None
thread_lock = Lock()

# Patch STD lib threading
eventlet.monkey_patch()

app = Flask( __name__ )
# Config file (seperate) contains secret keys and such
app.config.from_object( Config )

# Using eventlet for the sockets
socketio = SocketIO( app )#, async_mode="eventlet" )

# Set up the database connection
dbSession = sensorDb.database.createSensorDatabaseSession( Config.DATABASE, debug=False )

if __name__ == '__main__':
    print( "Running app" )
    socketio.run( app )

@app.route( '/' )
def index():
    if not session.get( 'logged_in' ):
        return render_template( 'login.html' )
    else:
        return render_template( 'status.html' )

@app.route( '/login', methods=[ 'POST' ] )
def do_login():
    if request.form[ 'password' ] == Config.userPassword and \
       request.form[ 'username' ] == Config.username:
        session[ 'logged_in' ] = True            
    else:
        flash( 'incorrect password' )
    return redirect( url_for( "index" ) )

@app.route( '/logout' )
def do_logout():
    session[ 'logged_in' ] = False
    return redirect( url_for( "index" ) )

# Connection request to the system_monitor namespace
@socketio.on( 'connect', namespace="/system_monitor" )
def sysMonConnect():
    global thread
    if not session.get( 'logged_in' ):
        return False
    print( "Client connected to monitor" )


@socketio.on( "sensor_request", namespace="/system_monitor" )
def sysMonRequest( payload ):
    if not session.get( "logged_in" ):
        return False
    tDiff = payload.get( "timeDiff" )
    print( "Received monitor request for %i seconds" % tDiff )


    cTime = int( time.time() )
    # Get last hour of data...
    rTime = cTime - tDiff
    if tDiff == 0:
        rTime = 0
    
    print( "Getting data from %i to %i" % ( rTime, cTime ) )
    
    sensorData = dbSession.query( SensorDataPoint ).filter( SensorDataPoint.timestamp > rTime ).all()

    temps = [ dataPoint.temp for dataPoint in sensorData ]
    hums = [ dataPoint.humidity for dataPoint in sensorData ]
    
    timestamps = [ dataPoint.timestamp for dataPoint in sensorData ]
    sendData = { "timestamps" : timestamps,
                    "temperatures" : temps,
                    "humidities" : hums,
                  }
    

    emit( 'sensor_data', sendData, namespace='/system_monitor' )

# Disconnect request for the system_monitor namespace
@socketio.on( 'disconnect_request', namespace='/system_monitor' )
def sysMonDisconnectRequest():
    if not session.get( 'logged_in' ):
        return False

    @copy_current_request_context
    def can_disconnect():
        disconnect()

    # for this emit we use a callback function
    # when the callback function is invoked we know 
    # that the message has been received and it is safe to disconnect
    emit( 'my_response',
          { 'data': 'Disconnected!' },
          callback=can_disconnect, namespace="/system_monitor" )


# Tear down database session
@app.teardown_appcontext
def shutdown_dbsession( exception=None ):
    dbSession.remove()
