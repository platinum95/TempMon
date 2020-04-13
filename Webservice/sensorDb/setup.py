from setuptools import setup, find_packages

setup(
    name='sensorDb',
    version='1.0',
    packages=[ 'sensorDb' ],
    install_requires=[ 'flask', 'flask-sqlalchemy' ],
)
