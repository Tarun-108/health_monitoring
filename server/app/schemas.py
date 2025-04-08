from pydantic import BaseModel
from datetime import datetime
from typing import Optional

class SensorConfigSchema(BaseModel):
    ssid: str
    password: str

    class Config:
        orm_mode = True

class SensorDataSchema(BaseModel):
    ds18b20_temp: float
    dht11_temp: float
    humidity: float
    ir: int
    bpm: float
    bpm_avg: float

    class Config:
        orm_mode = True

class SensorDataResponse(SensorDataSchema):
    timestamp: datetime

    class Config:
        from_attributes = True

class PaginatedSensorHistory(BaseModel):
    data: list[SensorDataResponse]
    total: int
    page: int
    page_size: int
    
    class Config:
        orm_mode = True