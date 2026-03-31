# Dokumentacja projektu ESP32 -> Wi-Fi -> MQTT

## 1. Cel projektu

Projekt uruchamia ESP32 jako węzeł pomiarowy publikujący dane do brokera MQTT.
Aktualna wersja firmware:
- łączy się z Wi-Fi,
- synchronizuje czas przez NTP,
- generuje trwały `device_id` jako UUIDv4 zapisany w NVS,
- publikuje wiele strumieni danych testowych generowanych jako sinus,
- publikuje także osobny komunikat statusowy,
- zachowuje zgodność z kontraktem wiadomości opisanym w `docs/message_contract_v2.md`.

## 2. Zakres funkcjonalny

Firmware publikuje dane na topicach:
- `lab/<group_id>/<device_id>/temperature`
- `lab/<group_id>/<device_id>/humidity`
- `lab/<group_id>/<device_id>/pressure`
- `lab/<group_id>/<device_id>/light`
- `lab/<group_id>/<device_id>/status`

Wiadomości pomiarowe zawierają pola:
- `schema_version`
- `group_id`
- `device_id`
- `sensor`
- `value`
- `unit`
- `ts_ms`
- `seq`

Wiadomość statusowa zawiera:
- `schema_version`
- `group_id`
- `device_id`
- `status`
- `ts_ms`

## 3. Pliki projektu

### `src/main.cc`
Główny plik firmware. Zawiera:
- konfigurację MQTT i Wi-Fi,
- generowanie/odczyt UUIDv4 z NVS,
- synchronizację czasu przez NTP,
- generator danych sinusoidalnych,
- publikację danych pomiarowych i statusowych,
- podstawową walidację topiców i payloadów.

### `include/secrets.h`
Lokalna konfiguracja środowiska:
- `WIFI_SSID`
- `WIFI_PASSWORD`
- `MQTT_HOST`
- `MQTT_PORT`
- `MQTT_GROUP`

Opcjonalnie można dodać:
- `MQTT_USERNAME`
- `MQTT_PASSWORD`
- `MQTT_USE_TLS`
- `MQTT_CA_CERT`

### `docs/message_contract_v2.md`
Opis kontraktu danych i topiców dla wiadomości MQTT.

### `include/secrets_tls_example.h`
Przykładowy plik pokazujący, jak wygląda konfiguracja opcjonalnego TLS i uwierzytelniania.

## 4. Identyfikacja urządzenia

Projekt używa UUIDv4 jako `device_id`.

Mechanizm działania:
1. przy starcie firmware sprawdza, czy w NVS istnieje zapisany UUID,
2. jeżeli tak, używa istniejącej wartości,
3. jeżeli nie, generuje nowy UUIDv4,
4. zapisuje UUID w NVS,
5. przy kolejnych restartach używa tego samego identyfikatora.

Dzięki temu jedno urządzenie zachowuje stałą tożsamość po restarcie i po kolejnych flashowaniach, o ile nie zostanie wyczyszczona pamięć NVS.

## 5. Synchronizacja czasu

Pole `ts_ms` jest generowane z czasu systemowego po synchronizacji NTP.

Przebieg:
1. ESP32 łączy się z Wi-Fi,
2. wykonuje synchronizację z serwerami NTP,
3. odczytuje aktualny czas przez `gettimeofday`,
4. przelicza go do milisekund od Unix epoch,
5. wpisuje wynik do pola `ts_ms`.

Dane nie są publikowane, dopóki synchronizacja czasu nie powiedzie się.

## 6. Generowanie danych testowych

Aktualna wersja projektu nie korzysta z fizycznych sensorów.
Zamiast tego generuje dane mock jako sinusoidy.

Każdy sensor ma osobną konfigurację:
- inną wartość bazową,
- inną amplitudę,
- inny okres,
- inne przesunięcie fazowe,
- własny dopuszczalny zakres.

Przykładowo:
- temperatura zmienia się wokół wartości około 24 C,
- wilgotność wokół 50%,
- ciśnienie wokół 1008 hPa,
- światło wokół 420 lx.

To ułatwia testowanie w MQTT Explorer, bo przebiegi są płynne i różnią się między sobą.

## 7. Publikacja MQTT

### Pomiary
Firmware publikuje wszystkie pomiary cyklicznie, domyślnie co 5 sekund.

Dla każdego sensora tworzona jest osobna wiadomość JSON.
Po każdej udanej publikacji zwiększany jest licznik `seq`.

### Status
Topic `status` publikuje stan urządzenia:
- po udanym połączeniu z brokerem MQTT,
- potem cyklicznie co 30 sekund.

Status jest wysyłany jako retained, aby ostatni stan był widoczny dla nowych subskrybentów.

## 8. Walidacja

Firmware wykonuje podstawową walidację przed publikacją:
- `group_id` i segmenty topicu muszą mieć poprawny format,
- `device_id` musi być poprawnym UUIDv4,
- `sensor` nie może być pusty,
- `value` musi być liczbą skończoną,
- `ts_ms` musi być dodatni,
- `unit` musi pasować do sensora.

Mapowanie jednostek:
- `temperature` -> `C`
- `humidity` -> `%`
- `pressure` -> `hPa`
- `light` -> `lx`

## 9. Konfiguracja opcjonalna: uwierzytelnianie i TLS

Materiały laboratoryjne pokazują podstawowy wariant z brokerem na porcie `1883`.
Aktualny firmware ma jednak przygotowany opcjonalny szkielet dla:
- logowania MQTT (`MQTT_USERNAME`, `MQTT_PASSWORD`),
- TLS (`MQTT_USE_TLS`),
- weryfikacji certyfikatu CA (`MQTT_CA_CERT`).

Domyślnie TLS jest wyłączony.
Należy go włączyć tylko wtedy, gdy broker faktycznie obsługuje TLS, np. na porcie `8883`.

## 10. Jak uruchomić projekt

1. Uzupełnij `include/secrets.h`.
2. Upewnij się, że `MQTT_GROUP` jest zapisane małymi literami, np. `g05`.
3. Zbuduj projekt w PlatformIO.
4. Wgraj firmware do ESP32.
5. Otwórz Serial Monitor.
6. Sprawdź:
   - połączenie z Wi-Fi,
   - synchronizację NTP,
   - wygenerowany `device_id`,
   - topic statusowy,
   - publikacje MQTT.
7. Otwórz MQTT Explorer i sprawdź strukturę topiców.

## 11. Oczekiwany widok w MQTT Explorer

Dla przykładowej grupy `g05` i przykładowego UUID powinny być widoczne topicy:

```text
lab/g05/0000f88d-ab00-4f8c-8c4f-00ab8df80000/status
lab/g05/0000f88d-ab00-4f8c-8c4f-00ab8df80000/temperature
lab/g05/0000f88d-ab00-4f8c-8c4f-00ab8df80000/humidity
lab/g05/0000f88d-ab00-4f8c-8c4f-00ab8df80000/pressure
lab/g05/0000f88d-ab00-4f8c-8c4f-00ab8df80000/light
```

## 12. Znane ograniczenia

- Kod nie subskrybuje żadnych topiców sterujących.
- Kod nie zapisuje lokalnego logu błędów poza Serial Monitor.
- TLS jest tylko opcjonalnym szkieletem konfiguracyjnym i wymaga poprawnego brokera.
- W tej wersji dane są syntetyczne; nie ma jeszcze integracji z realnymi sensorami.
- Brak pola `type` oraz `meta` w payloadzie.
- Brak drugiego formatu czasu, np. ISO 8601.

## 13. Rekomendowane dalsze kroki

1. Dodać `platformio.ini` do pełnej dokumentacji projektu.
2. Dodać test ręcznej publikacji poprawnej i błędnej wiadomości w MQTT Explorer.
3. Dodać pole `type` (`measurement` / `status`) jako rozszerzenie kontraktu.
4. Dodać pole `meta` z informacjami diagnostycznymi.
5. Dodać możliwość przełączenia między mock danymi a realnymi sensorami.
6. Dodać retry/backoff dla NTP i bardziej szczegółową diagnostykę błędów sieciowych.
