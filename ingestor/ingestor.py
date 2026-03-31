import json
import os
import ssl
from typing import Any, Dict, Tuple

import paho.mqtt.client as mqtt

from db import ensure_schema, save_measurement, wait_for_db

MQTT_HOST = os.getenv("MQTT_HOST", "broker")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_TOPIC = os.getenv("MQTT_TOPIC", "lab/+/+/+")
MQTT_USERNAME = os.getenv("MQTT_USERNAME", "")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "")
MQTT_USE_TLS = os.getenv("MQTT_USE_TLS", "0") == "1"
MQTT_CA_CERT = os.getenv("MQTT_CA_CERT", "")
MQTT_CLIENT_ID = os.getenv("MQTT_CLIENT_ID", "ingestor")

REQUIRED_FIELDS = ["device_id", "sensor", "value", "ts_ms"]
ALLOWED_UNITS = {
    "temperature": "C",
    "humidity": "%",
    "pressure": "hPa",
    "light": "lx",
}


def parse_topic(topic: str) -> Tuple[str, str, str] | None:
    parts = topic.split("/")
    if len(parts) != 4:
        return None
    root, group_id, device_id, sensor = parts
    if root != "lab":
        return None
    return group_id, device_id, sensor


def is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def validate_payload(topic: str, data: Dict[str, Any]) -> Tuple[bool, str]:
    topic_parts = parse_topic(topic)
    if topic_parts is None:
        return False, "Invalid topic format"

    topic_group_id, topic_device_id, topic_sensor = topic_parts

    for field in REQUIRED_FIELDS:
        if field not in data:
            return False, f"Missing required field: {field}"

    if not isinstance(data["device_id"], str) or not data["device_id"]:
        return False, "device_id must be a non-empty string"

    if not isinstance(data["sensor"], str) or not data["sensor"]:
        return False, "sensor must be a non-empty string"

    if not is_number(data["value"]):
        return False, "value must be numeric"

    if not isinstance(data["ts_ms"], int) or data["ts_ms"] <= 0:
        return False, "ts_ms must be a positive integer"

    if "seq" in data and data["seq"] is not None and not isinstance(data["seq"], int):
        return False, "seq must be an integer"

    if data["device_id"] != topic_device_id:
        return False, "device_id does not match topic"

    if data["sensor"] != topic_sensor:
        return False, "sensor does not match topic"

    if "group_id" in data and data["group_id"] is not None and data["group_id"] != topic_group_id:
        return False, "group_id does not match topic"

    unit = data.get("unit")
    expected_unit = ALLOWED_UNITS.get(data["sensor"])
    if expected_unit and unit not in (None, expected_unit):
        return False, f"unit must be {expected_unit!r} for sensor {data['sensor']!r}"

    return True, "OK"


def on_connect(client, userdata, flags, rc):
    print(f"[mqtt] Connected with result code {rc}")
    client.subscribe(MQTT_TOPIC)
    print(f"[mqtt] Subscribed to {MQTT_TOPIC}")


def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode("utf-8")
        data = json.loads(payload)
    except UnicodeDecodeError as exc:
        print(f"[ingestor] Invalid UTF-8 on topic {msg.topic}: {exc}")
        return
    except json.JSONDecodeError as exc:
        print(f"[ingestor] Invalid JSON on topic {msg.topic}: {exc}")
        return

    valid, reason = validate_payload(msg.topic, data)
    if not valid:
        print(f"[ingestor] Rejected message from topic {msg.topic}: {reason}; payload={data}")
        return

    try:
        save_measurement(msg.topic, data)
        print(f"[ingestor] Saved message from topic: {msg.topic}")
    except Exception as exc:
        print(f"[ingestor] Database error while saving message from {msg.topic}: {exc}")


def build_client() -> mqtt.Client:
    client = mqtt.Client(client_id=MQTT_CLIENT_ID)
    client.on_connect = on_connect
    client.on_message = on_message

    if MQTT_USERNAME:
        client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)

    if MQTT_USE_TLS:
        if MQTT_CA_CERT:
            client.tls_set(ca_certs=MQTT_CA_CERT, tls_version=ssl.PROTOCOL_TLS_CLIENT)
        else:
            client.tls_set(tls_version=ssl.PROTOCOL_TLS_CLIENT)

    return client


def main():
    wait_for_db()
    ensure_schema()

    client = build_client()
    print(f"[mqtt] Connecting to {MQTT_HOST}:{MQTT_PORT} ...")
    client.connect(MQTT_HOST, MQTT_PORT, 60)
    client.loop_forever()


if __name__ == "__main__":
    main()
