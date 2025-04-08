from fastapi import FastAPI, Depends, HTTPException, Query
from fastapi.middleware.cors import CORSMiddleware
from sqlalchemy.orm import Session
from datetime import datetime
from typing import Optional

from database import SessionLocal, engine, Base
from models import SensorConfig, SensorData
from schemas import SensorConfigSchema, SensorDataSchema, SensorDataResponse, PaginatedSensorHistory

# Initialize DB
Base.metadata.create_all(bind=engine)

# Create FastAPI app
app = FastAPI(title="Sensor API")

# CORS setup (allow all origins)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Dependency for DB session
def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

# ================== ROUTES ==================

@app.get("/")
def root():
    return {"message": "Sensor API is live!"}

# --- Config Endpoints ---

@app.get("/sensor/configure", response_model=SensorConfigSchema)
def get_config(db: Session = Depends(get_db)):
    config = db.query(SensorConfig).first()
    if not config:
        raise HTTPException(status_code=404, detail="Configuration not set")
    return config

@app.post("/sensor/configure", response_model=SensorConfigSchema)
def set_config(config: SensorConfigSchema, db: Session = Depends(get_db)):
    existing = db.query(SensorConfig).first()
    if existing:
        existing.ssid = config.ssid
        existing.password = config.password
    else:
        existing = SensorConfig(**config.dict())
        db.add(existing)
    db.commit()
    db.refresh(existing)
    return existing

# --- Sensor Data Endpoints ---

@app.post("/sensor/data", response_model=SensorDataResponse)
def store_data(data: SensorDataSchema, db: Session = Depends(get_db)):
    entry = SensorData(**data.dict(), timestamp=datetime.now())
    db.add(entry)
    db.commit()
    db.refresh(entry)
    return entry

@app.get("/sensor/data", response_model=SensorDataResponse)
def get_latest_data(db: Session = Depends(get_db)):
    latest = db.query(SensorData).order_by(SensorData.timestamp.desc()).first()
    if not latest:
        raise HTTPException(status_code=404, detail="No data available")
    return latest

@app.get("/sensor/history", response_model=PaginatedSensorHistory)
def get_history(
    db: Session = Depends(get_db),
    page: int = Query(1, ge=1),
    page_size: int = Query(10, gt=0, le=100),
    start_date: Optional[datetime] = Query(None),
    end_date: Optional[datetime] = Query(None)
):
    try:
        query = db.query(SensorData)

        if start_date:
            query = query.filter(SensorData.timestamp >= start_date)
        if end_date:
            query = query.filter(SensorData.timestamp <= end_date)

        total = query.count()
        skip = (page - 1) * page_size

        records = (
            query.order_by(SensorData.timestamp.desc())
            .offset(skip)
            .limit(page_size)
            .all()
        )

        return PaginatedSensorHistory(
            data=[SensorDataResponse.from_orm(record) for record in records],
            total=total,
            page=page,
            page_size=page_size
        )
    except Exception as e:
        print(f"Error fetching history: {e}")   
        raise HTTPException(status_code=500, detail=str(e))