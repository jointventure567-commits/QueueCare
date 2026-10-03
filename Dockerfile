FROM python:3.14-slim

WORKDIR /app

RUN apt-get update && apt-get install -y --no-install-recommends g++ \
    && rm -rf /var/lib/apt/lists/*

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY cpp/engine.cpp cpp/engine.cpp
RUN g++ -std=c++17 -O2 cpp/engine.cpp -o cpp/engine

COPY backend/main.py backend/main.py
COPY frontend/ frontend/

ENV PYTHONUNBUFFERED=1
ENV SQLITE_PATH=/data/queuecare.db

CMD ["sh", "-c", "exec python -m uvicorn backend.main:app --host 0.0.0.0 --port ${PORT:-8000} --workers 1"]
