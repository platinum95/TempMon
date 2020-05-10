from sqlalchemy import create_engine
from sqlalchemy.orm import scoped_session, sessionmaker
from sqlalchemy.ext.declarative import declarative_base

Base = declarative_base()

def createSensorDatabaseSession( databaseUri, port=3306, debug=False, verbose=False ):
    echo=None
    if( debug ):
        echo = 'debug'
    
    engine = create_engine(
        databaseUri,
        connect_args = {
            'port': port
        },
        echo=echo,
        echo_pool=verbose
    )

    db_session = scoped_session(
        sessionmaker(
            bind=engine,
            autocommit=False,
            autoflush=False
        )
    )

    return db_session