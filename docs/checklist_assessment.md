# Assessment checklist względem obu laboratoriów

## 1. Laboratorium 3 — ESP32, PlatformIO, Wi-Fi i publikacja MQTT

### Checklista z PDF

| Punkt | Status | Ocena |
|---|---|---|
| Utworzono projekt w PlatformIO | niezweryfikowane | Nie mam wgrądu w cały katalog projektu ani historię utworzenia projektu. |
| Skonfigurowano plik `platformio.ini` | niezweryfikowane | Plik nie został dostarczony do analizy. |
| Dodano biblioteki wymagane do Wi-Fi, MQTT i JSON | częściowo potwierdzone | Kod używa `WiFi.h`, `PubSubClient.h`, `ArduinoJson.h`. Nie mogę jednak potwierdzić wpisów w `platformio.ini`. |
| Przygotowano plik `secrets.h` z konfiguracją sieci i brokera | częściowo potwierdzone | Kod zawiera `#include "secrets.h"`, więc plik istnieje w lokalnym projekcie, ale jego treści nie dostałem do analizy. |
| ESP32 łączy się z siecią Wi-Fi laboratorium | potwierdzone | Na podstawie działania po flashowaniu i publikacji do MQTT. |
| Program generuje identyfikator urządzenia | potwierdzone | Aktualna wersja generuje lub odczytuje UUIDv4 z NVS. |
| ESP32 łączy się z brokerem MQTT | potwierdzone | Na podstawie działania i widocznych publikacji w MQTT Explorer. |
| Dane są publikowane na topicu `lab/gXX/<device_id>/temperature` | potwierdzone z rozszerzeniem | Jest to spełnione, a dodatkowo publikowane są kolejne sensory i `status`. |
| Wiadomość ma poprawny format JSON | potwierdzone | Payload ma spójną strukturę JSON i jest widoczny w MQTT Explorer. |
| Publikacja została potwierdzona w MQTT Explorer | potwierdzone | Pokazałeś zrzuty ekranu z widocznymi wiadomościami. |

### Podsumowanie lab 3

- Potwierdzone: **6/10**
- Częściowo potwierdzone: **2/10**
- Niezweryfikowane formalnie: **2/10**

Najbardziej prawdopodobne braki formalne:
- brak dostarczonego `platformio.ini` do oceny,
- brak dostarczonego `include/secrets.h` do oceny.

## 2. Laboratorium 4 — kontrakt danych i topicy MQTT

### Checklista z PDF

| Punkt | Status | Ocena |
|---|---|---|
| Przeanalizowano strukturę topicu używaną w poprzednim laboratorium | zrobione | Aktualny projekt wychodzi od schematu `lab/gXX/<device_id>/temperature` i go rozwija. |
| Zaprojektowano docelową strukturę topiców MQTT | zrobione | Jest baza `lab/<group_id>/<device_id>/<sensor>` oraz osobny `status`. |
| Przygotowano wiadomość pomiarową JSON w wersji v1 | zrobione | Aktualny payload zawiera pełny kontrakt v1. |
| Określono pola wymagane i opcjonalne | zrobione | Są opisane w `docs/message_contract_v2.md`. |
| Określono podstawowe reguły walidacji | zrobione | Są opisane w dokumentacji i częściowo wymuszone przez firmware. |
| Sprawdzono, w jaki sposób urządzenie ESP generuje pole `ts_ms` | zrobione | Używane jest NTP + `gettimeofday`. |
| Przygotowano przykład wiadomości poprawnej | zrobione | Jest w `docs/message_contract_v2.md`. |
| Przygotowano co najmniej dwa przykłady wiadomości błędnych | zrobione | W dokumencie są cztery przykłady błędne. |
| Wykonano test publikacji i subskrypcji MQTT | częściowo potwierdzone | Publikacja jest potwierdzona, subskrypcja CLI nie została pokazana. |
| Zweryfikowano działanie wiadomości także z użyciem MQTT Explorer | zrobione | Zrzuty z MQTT Explorer to potwierdzają. |
| Przygotowano plik `docs/message_contract.md` | zrobione | Powstał plik `docs/message_contract.md`, a później rozwinięta wersja `docs/message_contract_v2.md`. |

### Dodatkowo opcjonalnie z lab 4

| Punkt opcjonalny | Status | Ocena |
|---|---|---|
| Dodać pole `type` rozróżniające pomiar i status | brak | Nie ma jeszcze pola `type`. |
| Dodać pole `meta` na informacje diagnostyczne | brak | Nie ma jeszcze pola `meta`. |
| Porównać `ts_ms` i ISO 8601 | brak | Nie ma jeszcze drugiego zapisu czasu. |
| Zaproponować walidację po stronie ingestora | brak | Nie zostało jeszcze opisane. |
| Przygotować własny wariant kontraktu v2 i opisać różnice | częściowo zrobione | Jest `message_contract_v2.md`, ale można jeszcze dopisać sekcję „różnice v1 vs v2”. |

### Podsumowanie lab 4

- Rdzeń checklisty: **10/11 zrobione**, **1/11 częściowo potwierdzone**
- Opcjonalne: **0/5 w pełni zrobione**, **1/5 częściowo zrobione**

## 3. Najważniejsze rzeczy, których jeszcze może brakować

### Braki obowiązkowe / formalne

1. **Pełna weryfikacja `platformio.ini`**
   - nie mam pliku, więc nie potwierdzę zależności i konfiguracji środowiska.

2. **Pełna weryfikacja `include/secrets.h`**
   - nie widzę faktycznej konfiguracji hosta, portu i grupy.

3. **Jawny test subskrypcji CLI**
   - dobrze byłoby wykonać i zapisać np. test `mosquitto_sub` + publikacja wiadomości.

### Braki nieobowiązkowe, ale sensowne

1. Dodać pole `type` do payloadu.
2. Dodać pole `meta`.
3. Dopisać do dokumentacji różnice między kontraktem v1 i v2.
4. Dodać sekcję „test cases” z poprawną i błędną publikacją ręczną.
5. Dodać prawdziwy sensor albo tryb przełączania `mock/real`.
6. Dodać jawny opis, że TLS jest opcjonalne i nie jest wymagane przez aktualną checklistę laboratoriów.

## 4. Ocena końcowa projektu

### Stan praktyczny
Projekt wygląda na **działający i bliski kompletnego oddania**.
Najważniejsze elementy funkcjonalne są już na miejscu:
- Wi-Fi działa,
- MQTT działa,
- JSON działa,
- topici są spójne,
- UUIDv4 jest trwały,
- `ts_ms` jest poprawne,
- MQTT Explorer pokazuje poprawne dane,
- dokument kontraktu istnieje.

### Stan formalny
Do pełnego, „bezdyskusyjnego” domknięcia projektu warto jeszcze:
- dołączyć `platformio.ini`,
- dołączyć finalne `include/secrets.h` bez haseł albo z placeholderami,
- dopisać dokumentację projektu,
- ewentualnie wykonać jeden zrzut lub log z `mosquitto_sub`.
