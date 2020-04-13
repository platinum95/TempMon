from sqlalchemy import create_engine
from sqlalchemy.orm import scoped_session, sessionmaker
from sqlalchemy.ext.declarative import declarative_base
from config import TempMonConfig as Config

engine = create_engine(
    Config.DATABASE,
    connect_args = {
        'port': 3306
    },
    echo='debug',
    echo_pool=True
)

db_session = scoped_session(
    sessionmaker(
        bind=engine,
        autocommit=False,
        autoflush=False
    )
)

Base = declarative_base()

def init_db():
    Base.metadata.create_all( engine )

    print( "Initialised the database." )
    return db_session()