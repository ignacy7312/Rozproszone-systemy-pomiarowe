# Message contract MQTT v1

## 1. Struktura topiców

Format topicu pomiarowego:

```text
lab/<group_id>/<device_id>/<sensor>
```

Przykład:

```text
lab/g03/esp32-ab12cd34/temperature
```

Zasady nazewnictwa:
- tylko małe litery, cyfry i `-`,
- bez spacji i polskich znaków,
- stała kolejność segmentów,
- topic opisuje klasę komunikatu, a nie pojedynczą próbkę.

## 2. Wiadomość JSON v1

Przykładowa wiadomość poprawna:

```json
{
  "schema_version": 1,
  "group_id": "g03",
  "device_id": "esp32-ab12cd34",
  "sensor": "temperature",
  "value": 24.5,
  "unit": "C",
  "ts_ms": 1742030400000,
  "seq": 15
}
```

## 3. Pola wymagane

- `device_id`
- `sensor`
- `value`
- `ts_ms`

## 4. Pola opcjonalne / zalecane

- `schema_version`
- `group_id`
- `unit`
- `seq`

## 5. Reguły walidacji

- `device_id` musi być niepustym napisem,
- `sensor` musi być niepustym napisem,
- `value` musi być liczbą,
- `ts_ms` musi być dodatnią liczbą całkowitą w milisekundach od Unix epoch,
- `unit`, jeśli występuje, musi odpowiadać typowi sensora,
- `seq`, jeśli występuje, musi być nieujemną liczbą całkowitą.

## 6. Dodatkowe ustalenia implementacyjne

- `ts_ms` jest generowane po synchronizacji czasu przez NTP,
- `device_id` jest generowane z eFuse układu ESP32,
- `seq` zwiększa się o 1 po każdej udanej publikacji,
- dla sensora `temperature` używana jest jednostka `C`.

## 7. Przykłady wiadomości błędnych

### Błędna wiadomość 1 — `value` jako tekst i brak `ts_ms`

```json
{
  "device_id": "esp32-ab12cd34",
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
  "device_id": "esp32-ab12cd34",
  "sensor": "temperature",
  "value": 24.5,
  "unit": "%",
  "ts_ms": 1742030400000,
  "seq": 15
}
```
