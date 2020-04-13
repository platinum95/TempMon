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
from LogWatcher import LogWatcher
from config import TempMonConfig as Config
from sensorDb.database import db_session
from sensorDb.models import SystemStatusPoint
from threading import Lock
import os
import eventlet
import subprocess
import time

print( "Hello Servo" )

mcLogPath = Config.logFilepath
mcLogDir = os.path.dirname( mcLogPath )
thread = None
thread_lock = Lock()

# Patch STD lib threading
eventlet.monkey_patch()

app = Flask( __name__ )
# Config file (seperate) contains secret keys and such
app.config.from_object( Config )

# Using eventlet for the sockets
socketio = SocketIO( app )#, async_mode="eventlet" )


if __name__ == '__main__':
    socketio.run(app)

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
    
    sensorData = db_session.query( SystemStatusPoint ).filter( SystemStatusPoint.timestamp > rTime ).all()

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



# Callback function from the logfile watcher
# Delivers file updates to all clients connected to 
# mclog namespace
def logCallback( filename, lines ):
    if filename != mcLogPath:
        return False
    print( "Sending log data for file %s:" % filename )
    print( lines )
    lines = [ line.decode( "utf-8" ) for line in lines ]
    lines = ''.join( lines )
    emit( 'log_data', { 'log': lines }, 
           namespace="/mclog", broadcast=True )


def userQuery():
    # Get MC service status
    serviceStatus = getServiceStatus()
    userList = []
    if serviceStatus == 1:
        userListCmd = "mcrcon -H localhost -p %s '/list'" % Config.mcrconPassword
        rInfo = subprocess.run( userListCmd,
                                shell=True,
                                universal_newlines=True,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE )
        mcrconStdout = rInfo.stdout
        
        if not mcrconStdout:
            print( "No users received from RCON" )
            emit( 'user_list', { 'user_list' : [] }, namespace="/mclog" )
            return True

        ansi_escape = re.compile( r'\x1B\[[0-?]*[ -/]*[@-~]' )
        mcrconStdout = ansi_escape.sub( '', mcrconStdout )
        mcrconStdout = mcrconStdout.strip( '\n' )
        #print( "UserQuery: RCON returned %s" % mcrconStdout )
        mcrconStdout = mcrconStdout.split( ':' )
        print( mcrconStdout )
        if len( mcrconStdout ) <= 1 or len( mcrconStdout[ 1 ] ) <= 2 :
            # no one logged on/server borked
            print( "No users online: %s" % mcrconStdout )
            emit( 'user_list', { 'user_list' : [] }, namespace="/mclog" )
            return True

        userList = mcrconStdout[ 1 ]
        # Remove whitespace
        userList = userList.replace( ' ', '' )
        userList = userList.strip()
        ansi_escape = re.compile( r'\x1B\[[0-?]*[ -/]*[@-~]' )
        userList = ansi_escape.sub( '', userList )
        # Convert seperator to newlines
        userList = userList.split( ',' )
    #print( "Sending user list %s" % str( userList ) )
    emit( 'user_list', { 'user_list' : userList }, namespace="/mclog" )

def getServiceStatus():
    statusCmd = "/bin/systemctl is-active minecraft_resurrection"
    subprocReturn = subprocess.run( statusCmd, shell=True )
    serviceRunning = subprocReturn.returncode
    if( serviceRunning == 0 ):
        return 1
    else:
        isFailedCmd = "/bin/systemctl is-failed minecraft_resurrection"
        isFailedProc = subprocess.run( isFailedCmd, shell=True )
        isFailed = isFailedProc.returncode
        if( isFailed == 0 ):
            return 2
        else:
            return 0
    return 3

# Tear down database session
@app.teardown_appcontext
def shutdown_dbsession( exception=None ):
    db_session.remove()
