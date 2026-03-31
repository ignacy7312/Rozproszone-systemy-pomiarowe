import os
import time
import psycopg2

DB_HOST = os.getenv("DB_HOST", "database")
DB_NAME = os.getenv("DB_NAME", "postgres")
DB_USER = os.getenv("DB_USER", "postgres")
DB_PASSWORD = os.getenv("DB_PASSWORD", "postgres")
DB_PORT = int(os.getenv("DB_PORT", "5432"))


def get_connection():
    return psycopg2.connect(
        host=DB_HOST,
        dbname=DB_NAME,
        user=DB_USER,
        password=DB_PASSWORD,
        port=DB_PORT,
    )


def wait_for_db(max_attempts: int = 30, delay_s: float = 2.0) -> None:
    last_error = None
    for attempt in range(1, max_attempts + 1):
        try:
            conn = get_connection()
            conn.close()
            print(f"[db] Connected to PostgreSQL on attempt {attempt}.")
            return
        except Exception as exc:
            last_error = exc
            print(f"[db] Waiting for PostgreSQL ({attempt}/{max_attempts}): {exc}")
            time.sleep(delay_s)
    raise RuntimeError(f"Could not connect to PostgreSQL: {last_error}")


def ensure_schema() -> None:
    sql = '''
    CREATE TABLE IF NOT EXISTS measurements (
        id SERIAL PRIMARY KEY,
        group_id TEXT,
        device_id TEXT NOT NULL,
        sensor TEXT NOT NULL,
        value DOUBLE PRECISION NOT NULL,
        unit TEXT,
        ts_ms BIGINT NOT NULL,
        seq INTEGER,
        topic TEXT,
        received_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
    );
    '''
    conn = get_connection()
    try:
        cur = conn.cursor()
        cur.execute(sql)
        conn.commit()
        cur.close()
    finally:
        conn.close()


def save_measurement(topic: str, data: dict) -> None:
    conn = get_connection()
    try:
        cur = conn.cursor()
        cur.execute(
            '''
            INSERT INTO measurements
            (group_id, device_id, sensor, value, unit, ts_ms, seq, topic)
            VALUES (%s, %s, %s, %s, %s, %s, %s, %s)
            ''',
            (
                data.get("group_id"),
                data["device_id"],
                data["sensor"],
                data["value"],
                data.get("unit"),
                data["ts_ms"],
                data.get("seq"),
                topic,
            ),
        )
        conn.commit()
        cur.close()
    finally:
        conn.close()
