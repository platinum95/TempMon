from sqlalchemy import Column, Integer, String, Float
from sensorDb.database import Base
import time

class SystemStatusPoint( Base ):
    __tablename__ = 'SystemStatus'

    id = Column( Integer, primary_key=True )
    temp = Column( Float )
    humidity = Column( Float )
    timestamp = Column( Integer )

    def __init__(self, _temp=None, _humidty=None, _cTime=None ):
        if _cTime == None:
            # Get current epoch
            _cTime = int( time.time() )

        if _temp == None:
            # Invalid temp
            _temp = 255
        if _humidty == None:
            # Invalid humidity
            _humidty = 255

        self.temp = _temp
        self.humidity = _humidty
        self.timestamp = _cTime
        
          

#    def __repr__(self):
#        return "<User(id=%d, username='%s', password='%s')>" % (self.id, self.username, self.password)

    def to_dict(self):
        return {
            'id': self.id,
            'timestamp': self.timestamp,
            'temp': self.temp,
            'humidity' : self.humidity
        }
