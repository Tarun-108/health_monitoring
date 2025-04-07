from sqlalchemy import Column, Integer, Float, String, DateTime
from database import Base

class SensorConfig(Base):
    __tablename__ = "sensor_config"
    id = Column(Integer, primary_key=True, index=True)
    ssid = Column(String, nullable=False)
    password = Column(String, nullable=False)

class SensorData(Base):
    __tablename__ = "sensor_data"
    id = Column(Integer, primary_key=True, index=True)
    ds18b20_temp = Column(Float)
    dht11_temp = Column(Float)
    humidity = Column(Float)
    ir = Column(Integer)
    bpm = Column(Float)
    bpm_avg = Column(Float)
    timestamp = Column(DateTime)