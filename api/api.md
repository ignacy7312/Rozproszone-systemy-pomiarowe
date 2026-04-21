# REST API - pomiary z PostgreSQL

## Cel
API udostepnia odczyt danych zapisanych w tabeli `measurements` przez serwis `ingestor`.

## Endpointy

### GET /health
Sprawdza, czy serwis Flask dziala.

Przyklad:
```bash
curl http://localhost:5001/health
```

Przykladowa odpowiedz:
```json
{"status":"ok"}
```

### GET /measurements
Zwraca liste 20 najnowszych rekordow.

Przyklad:
```bash
curl http://localhost:5001/measurements
```

### GET /measurements/latest
Zwraca najnowszy rekord z tabeli `measurements`.

Przyklad:
```bash
curl http://localhost:5001/measurements/latest
```

### GET /measurements/history
Zwraca historie pomiarow z podstawowym filtrowaniem.

Obslugiwane parametry:
- `device_id`
- `sensor`
- `limit`

Przyklad:
```bash
curl "http://localhost:5001/measurements/history?device_id=00004813-ad00-4f8c-8c4f-00ad13480000&sensor=temperature&limit=5"
```

Przykladowa odpowiedz:
```json
[
  {
    "id": 264,
    "group_id": "g05",
    "device_id": "00004813-ad00-4f8c-8c4f-00ad13480000",
    "sensor": "temperature",
    "value": 23.18,
    "unit": "C",
    "ts_ms": 1774960083528,
    "seq": 80,
    "topic": "lab/g05/00004813-ad00-4f8c-8c4f-00ad13480000/temperature"
  }
]
```

## Uruchomienie
```bash
docker compose up -d --build
```

## Logi API
```bash
docker compose logs -f flask
```

## Test w przegladarce
- http://localhost:5001/health
- http://localhost:5001/measurements
- http://localhost:5001/measurements/latest
- http://localhost:5001/measurements/history?device_id=00004813-ad00-4f8c-8c4f-00ad13480000&sensor=temperature&limit=5
