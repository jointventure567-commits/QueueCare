import sqlite3
import json
from contextlib import closing
from contextlib import asynccontextmanager
from pathlib import Path
from threading import Lock
import re
import subprocess

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field, field_validator
from fastapi.responses import FileResponse


ROOT = Path(__file__).resolve().parent.parent
ENGINE_PATH = ROOT / "cpp" / "engine.exe"

engine = None
engine_lock = Lock()


# Start C++ once when the server starts.
# Stop it when the server closes.

DB_PATH = ROOT / "queuecare.db"


def load_saved_actions():
    # Create the database/table on the first launch.
    with closing(sqlite3.connect(DB_PATH)) as db:
        with db:
            db.execute("""
                CREATE TABLE IF NOT EXISTS actions (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    command_json TEXT NOT NULL,
                    response_json TEXT NOT NULL,
                    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
                )
            """)

        return db.execute(
            "SELECT command_json, response_json FROM actions ORDER BY id"
        ).fetchall()


def restore_engine():
    actions = load_saved_actions()

    for command_json, response_json in actions:
        command = json.loads(command_json)
        expected = json.loads(response_json)
        actual = send_command(*command)

        if actual != expected:
            raise RuntimeError(
                "Saved history does not match this C++ engine. "
                "Database was left unchanged."
            )

    print(f"SQLite ready: restored {len(actions)} saved actions.")


def save_command(*lines):
    # The route already holds engine_lock.
    # C++ performs the action first.
    response = send_command(*lines)

    # Failed C++ actions raise an exception above and are not saved.
    try:
        with closing(sqlite3.connect(DB_PATH)) as db:
            with db:
                db.execute(
                    "INSERT INTO actions "
                    "(command_json, response_json) VALUES (?, ?)",
                    (
                        json.dumps(lines, ensure_ascii=False),
                        json.dumps(response, ensure_ascii=False),
                    ),
                )

    except sqlite3.Error as error:
        # Stop accepting changes if saving fails.
        # Restart will restore the committed database state.
        if engine.poll() is None:
            engine.kill()
            engine.wait()

        raise HTTPException(
            503,
            "Database save failed. Restart the backend to restore saved state."
        ) from error

    return response


@asynccontextmanager
async def lifespan(app: FastAPI):
    global engine

    engine = subprocess.Popen(
        [str(ENGINE_PATH), "--api"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        bufsize=1,
        cwd=str(ROOT),
    )

    try:
        restore_engine()
        yield
    finally:
        if engine.poll() is None:
            try:
                engine.stdin.write("EXIT\n")
                engine.stdin.flush()
                engine.wait(timeout=3)
            except (OSError, subprocess.TimeoutExpired):
                engine.kill()
                engine.wait()

        engine.stdin.close()
        engine.stdout.close()


app = FastAPI(title="QueueCare API", lifespan=lifespan)


# Call only while holding engine_lock.
def send_command(*lines):
    if engine is None or engine.poll() is not None:
        raise HTTPException(503, "C++ engine is not running.")

    # Each argument must occupy exactly one protocol line.
    if any("\n" in line or "\r" in line for line in lines):
        raise HTTPException(422, "Line breaks are not allowed.")

    try:
        engine.stdin.write("\n".join(lines) + "\n")
        engine.stdin.flush()

        response = []

        while True:
            line = engine.stdout.readline()

            if line == "":
                raise HTTPException(503, "C++ engine disconnected.")

            line = line.rstrip("\r\n")

            if line == "END":
                break

            response.append(line)

    except OSError as error:
        raise HTTPException(503, "Cannot communicate with C++.") from error

    if response and response[0].startswith("ERROR "):
        raise HTTPException(400, response[0][6:])

    return response


# Convert C++ patient output into JSON-friendly records.
def parse_patients(lines):
    patients = []

    for line in lines:
        match = re.fullmatch(
            r"Token: (\d+) \| Name: (.*) \| Age: (\d+)",
            line,
        )

        if match:
            patients.append({
                "id": int(match.group(1)),
                "name": match.group(2),
                "age": int(match.group(3)),
            })

    return patients


class PatientInput(BaseModel):
    name: str = Field(min_length=1, max_length=80)
    age: int = Field(ge=0, le=120, strict=True)

    @field_validator("name")
    @classmethod
    def validate_name(cls, value):
        if any(ord(character) < 32 for character in value):
            raise ValueError("Name cannot contain control characters.")

        value = value.strip()

        if not value:
            raise ValueError("Name is required.")

        return value


@app.get("/api/state")
def get_state():
    # Keep both reads together so they describe the same state.
    with engine_lock:
        patients = parse_patients(send_command("LIST"))
        queue_output = send_command("WAITING")

    waiting_ids = []

    for line in queue_output:
        if line.startswith("Waiting tokens:"):
            waiting_ids = [
                int(token)
                for token in line.split(":", 1)[1].split()
            ]

    patient_by_id = {patient["id"]: patient for patient in patients}

    for patient in patients:
        patient["status"] = (
            "Waiting" if patient["id"] in waiting_ids else "Called"
        )

    return {
        "patients": patients,
        "queue": [patient_by_id[token] for token in waiting_ids],
        "stats": {
            "registered": len(patients),
            "waiting": len(waiting_ids),
            "called": len(patients) - len(waiting_ids),
        },
    }


@app.post("/api/patients")
def register_patient(patient: PatientInput):
    with engine_lock:
        response = save_command("ADD", patient.name, str(patient.age))

    return {"message": response[0]}


@app.post("/api/call")
def call_next():
    with engine_lock:
        response = save_command("CALL")

    patients = parse_patients(response)

    return {
        "message": response[0],
        "patient": patients[0] if patients else None,
    }


@app.post("/api/undo")
def undo_call():
    with engine_lock:
        response = save_command("UNDO")

    return {"message": response[0]}


@app.get("/api/search")
def search_patient(name: str):
    with engine_lock:
        response = send_command("SEARCH", name)

    return {"patients": parse_patients(response)}
@app.get("/")
def dashboard():
    return FileResponse(ROOT / "frontend" / "index.html")

@app.get("/favicon.svg")
def favicon():
    return FileResponse(
        ROOT / "frontend" / "favicon.svg",
        media_type="image/svg+xml"
    )
