# Message contract MQTT v1.1

## 1. Struktura topiców

### Topic pomiarowy

```text
lab/<group_id>/<device_id>/<sensor>
```

Przykłady:

```text
lab/g03/550e8400-e29b-41d4-a716-446655440000/temperature
lab/g03/550e8400-e29b-41d4-a716-446655440000/humidity
lab/g03/550e8400-e29b-41d4-a716-446655440000/pressure
lab/g03/550e8400-e29b-41d4-a716-446655440000/light
```

### Topic statusowy

```text
lab/<group_id>/<device_id>/status
```

Przykład:

```text
lab/g03/550e8400-e29b-41d4-a716-446655440000/status
```

Zasady nazewnictwa:
- tylko małe litery, cyfry i `-`,
- bez spacji i polskich znaków,
- stała kolejność segmentów,
- topic opisuje klasę komunikatu, a nie pojedynczą próbkę.

## 2. Wiadomość pomiarowa JSON v1

Przykładowa wiadomość poprawna:

```json
{
  "schema_version": 1,
  "group_id": "g03",
  "device_id": "550e8400-e29b-41d4-a716-446655440000",
  "sensor": "temperature",
  "value": 24.57,
  "unit": "C",
  "ts_ms": 1742030400000,
  "seq": 15
}
```

## 3. Wiadomość statusowa JSON v1

Przykładowa wiadomość poprawna:

```json
{
  "schema_version": 1,
  "group_id": "g03",
  "device_id": "550e8400-e29b-41d4-a716-446655440000",
  "status": "online",
  "ts_ms": 1742030400000
}
```

## 4. Pola wymagane dla wiadomości pomiarowej

- `device_id`
- `sensor`
- `value`
- `ts_ms`

## 5. Pola zalecane / opcjonalne dla wiadomości pomiarowej

- `schema_version`
- `group_id`
- `unit`
- `seq`

## 6. Reguły walidacji

- `device_id` musi być niepustym napisem,
- `device_id` w implementacji firmware ma postać UUIDv4 zapisanego w NVS,
- `sensor` musi być niepustym napisem,
- `value` musi być liczbą,
- `ts_ms` musi być dodatnią liczbą całkowitą w milisekundach od Unix epoch,
- `unit`, jeśli występuje, musi odpowiadać typowi sensora,
- `seq`, jeśli występuje, musi być nieujemną liczbą całkowitą,
- dla topiców używamy wyłącznie małych liter, cyfr i `-`.

## 7. Obsługiwane sensory w tej implementacji testowej

- `temperature` → `C`
- `humidity` → `%`
- `pressure` → `hPa`
- `light` → `lx`

Wartości są generowane syntetycznie jako przebiegi sinusoidalne, żeby łatwo sprawdzać zmienność danych w MQTT Explorer.

## 8. Dodatkowe ustalenia implementacyjne

- `ts_ms` jest generowane po synchronizacji czasu przez NTP,
- `device_id` jest przechowywane w NVS i zachowywane po restarcie,
- `seq` zwiększa się o 1 po każdej udanej publikacji wiadomości pomiarowej,
- komunikaty statusowe są publikowane na osobnym topicu `status`.

## 9. Przykłady wiadomości błędnych

### Błędna wiadomość 1 — `value` jako tekst i brak `ts_ms`

```json
{
  "device_id": "550e8400-e29b-41d4-a716-446655440000",
  "sensor": "temperature",
  "value": "24.5",
  "unit": "C"
}
```

### Błędna wiadomość 2 — brak `device_id`

```json
{
  "schema_version": 1,
  "group_id": "g03",
  "sensor": "temperature",
  "value": 24.5,
  "unit": "C",
  "ts_ms": 1742030400000,
  "seq": 15
}
```

### Błędna wiadomość 3 — niepoprawna jednostka dla temperatury

```json
{
  "schema_version": 1,
  "group_id": "g03",
  "device_id": "550e8400-e29b-41d4-a716-446655440000",
  "sensor": "temperature",
  "value": 24.5,
  "unit": "%",
  "ts_ms": 1742030400000,
  "seq": 15
}
```

### Błędna wiadomość 4 — niepoprawny topic

```text
lab/G03/550e8400-e29b-41d4-a716-446655440000/Temperature
```

Błąd: użyto wielkich liter, a nazwa sensora nie jest zgodna z ustaloną konwencją.
