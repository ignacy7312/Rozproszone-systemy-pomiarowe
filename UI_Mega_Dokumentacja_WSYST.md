# Mega-dokumentacja UI dashboardu WSYST

Dokument opisuje, jak działa frontend/dashboard webowy w projekcie **Rozproszone Systemy Pomiarowe**, jak łączy się z backendem REST API, jakie endpointy są używane, jak przepływają dane od urządzenia do ekranu oraz co znajduje się w poszczególnych plikach UI.

Opis jest przygotowany tak, żeby dało się na jego podstawie opowiedzieć projekt na zajęciach / obronie / prezentacji kodu.

---

## 1. Najkrótszy opis całości

Projekt ma kilka warstw:

```txt
ESP32 / symulator pomiarów
        ↓
MQTT broker Mosquitto
        ↓
ingestor Python
        ↓
PostgreSQL
        ↓
Flask REST API
        ↓
Node proxy + statyczny frontend UI
        ↓
przeglądarka użytkownika
```

UI, czyli dashboard webowy, jest tylko warstwą prezentacji. Nie komunikuje się bezpośrednio z ESP32, brokerem MQTT ani bazą danych. UI pyta tylko REST API o dane pomiarowe.

W praktyce wygląda to tak:

1. Urządzenie / symulator publikuje dane pomiarowe przez MQTT.
2. `ingestor` odbiera wiadomości MQTT.
3. `ingestor` waliduje wiadomości i zapisuje poprawne pomiary do PostgreSQL.
4. `api` / `flask` odczytuje dane z PostgreSQL i wystawia je jako JSON.
5. `ui` wyświetla dane z API w estetycznym dashboardzie Y2K.

---

## 2. Gdzie jest frontend, a gdzie backend?

### Frontend / UI

Frontend znajduje się w katalogu:

```txt
/ui
```

Najważniejsze pliki frontendu:

```txt
ui/
  src/
    main.ts                    # główny kod aplikacji UI
    api/
      client.ts                # funkcje do odpytywania REST API
      types.ts                 # typy TypeScript opisujące odpowiedzi API
    lib/
      format.ts                # formatowanie liczb, dat, skracanie ID
    state/
      dashboardMachine.ts      # stan dashboardu i reducer/state machine
    styles.css                 # źródłowe style Y2K

  public/
    index.html                 # HTML startowy
    styles.css                 # style używane przez przeglądarkę
    assets/main.js             # skompilowany JS z TypeScriptu

  server.mjs                   # mały serwer Node: statyczny frontend + proxy /api
  package.json                 # skrypty npm i metadata projektu
  tsconfig.json                # konfiguracja TypeScript
  Dockerfile                   # obraz Dockera dla UI
  docker-compose.frontend.yml  # opcjonalny compose tylko dla UI
  README.md                    # szybki opis uruchomienia
  docs.md                      # wcześniejsza dokumentacja UI
```

### Backend REST API

Backend REST API znajduje się w katalogu:

```txt
/api
```

Najważniejsze pliki backendu:

```txt
api/
  app.py           # Flask API i endpointy HTTP
  db.py            # połączenie z PostgreSQL
  models.py        # mapowanie rekordu z bazy na JSON
  requirements.txt # zależności Pythona
  Dockerfile       # obraz Dockera dla API
  api.md           # opis endpointów API
```

### Ingestor

Ingestor, czyli serwis odbierający MQTT i zapisujący dane do bazy:

```txt
ingestor/
  ingestor.py      # logika MQTT, walidacja payloadu
  db.py            # tworzenie tabeli measurements i zapis pomiaru
  requirements.txt
  Dockerfile
```

### Baza danych

Baza danych to PostgreSQL uruchamiany jako kontener `database`.

Kod tworzący docelową tabelę pomiarów znajduje się głównie w:

```txt
ingestor/db.py
```

Tam funkcja `ensure_schema()` tworzy tabelę `measurements`, jeśli jej nie ma.

---

## 3. Jak działa komunikacja między UI a API?

W przeglądarce UI używa domyślnie adresu:

```txt
/api
```

To nie jest bezpośredni adres Flask API. To jest ścieżka obsługiwana przez `server.mjs` w kontenerze / procesie UI.

Czyli przeglądarka pyta:

```txt
http://localhost:5173/api/measurements/history?sensor=temperature&limit=20
```

A `server.mjs` przepisuje to zapytanie do backendu, np.:

```txt
http://flask:5001/measurements/history?sensor=temperature&limit=20
```

To rozwiązanie jest dobre, bo:

- przeglądarka zawsze rozmawia z tym samym hostem i portem UI,
- nie trzeba walczyć z CORS,
- backend może mieć inny adres w Dockerze i inny lokalnie,
- w UI wystarczy zostawić `REST API URL = /api`.

---

## 4. Bardzo ważne: nazwa API w Docker Compose

W aktualnym repo backendowa usługa Flask w `docker-compose.yml` jest nazwana:

```yaml
services:
  flask:
    build:
      context: ./api
      dockerfile: Dockerfile
    ports:
      - 5001:5001
    image: api:v1
    container_name: api
```

W Docker Compose najbezpieczniej odwoływać się do kontenerów po **nazwie usługi**, a nie po `container_name`.

Dlatego dla UI poprawna wartość powinna być:

```yaml
API_TARGET: http://flask:5001
```

A nie:

```yaml
API_TARGET: http://api:5001
```

Minimalnie poprawna definicja UI w głównym `docker-compose.yml` wygląda tak:

```yaml
  ui:
    build:
      context: ./ui
    ports:
      - "5173:5173"
    environment:
      API_TARGET: http://flask:5001
    depends_on:
      - flask
```

Jeżeli kiedyś zmienisz nazwę usługi z `flask` na `api`, wtedy `API_TARGET` też trzeba zmienić na `http://api:5001`.

---

## 5. Co robi `server.mjs`?

Plik:

```txt
ui/server.mjs
```

jest małym serwerem Node.js. Ma dwa zadania:

1. Serwuje frontend statyczny z katalogu `ui/public`.
2. Robi proxy dla zapytań zaczynających się od `/api`.

### 5.1. Serwowanie statycznych plików

Kiedy użytkownik wchodzi na:

```txt
http://localhost:5173/
```

serwer zwraca:

```txt
ui/public/index.html
```

`index.html` ładuje potem:

```html
<link rel="stylesheet" href="/styles.css" />
<script type="module" src="/assets/main.js"></script>
```

Czyli przeglądarka pobiera:

```txt
ui/public/styles.css
ui/public/assets/main.js
```

### 5.2. Proxy API

Jeżeli URL zaczyna się od `/api`, działa funkcja `proxyApi()`.

Przykład:

```txt
GET /api/health
```

zostaje zamienione na:

```txt
GET ${API_TARGET}/health
```

Jeśli `API_TARGET=http://flask:5001`, to finalnie idzie:

```txt
GET http://flask:5001/health
```

Analogicznie:

```txt
GET /api/measurements/history?sensor=light&limit=20
```

idzie do:

```txt
GET http://flask:5001/measurements/history?sensor=light&limit=20
```

### 5.3. Zmienna środowiskowa `API_TARGET`

`server.mjs` czyta backend z:

```js
const apiTarget = new URL(process.env.API_TARGET || 'http://localhost:5001');
```

Czyli:

- lokalnie bez Dockera domyślnie celuje w `http://localhost:5001`,
- w Dockerze powinno się ustawić `API_TARGET=http://flask:5001`,
- w UI nadal wpisujemy `/api`.

---

## 6. Endpointy API używane przez UI

Frontend używa czterech endpointów:

```txt
GET /health
GET /measurements
GET /measurements/latest
GET /measurements/history?device_id=<id>&sensor=<name>&limit=<n>
```

W UI, ponieważ działa proxy, te endpointy zwykle są wołane jako:

```txt
GET /api/health
GET /api/measurements
GET /api/measurements/latest
GET /api/measurements/history?device_id=<id>&sensor=<name>&limit=<n>
```

### 6.1. `GET /health`

Cel: sprawdzenie, czy Flask API działa.

Backend w `api/app.py`:

```python
@app.route('/health', methods=['GET'])
def health():
    return jsonify({'status': 'ok'})
```

Odpowiedź:

```json
{
  "status": "ok"
}
```

W UI endpoint jest używany po kliknięciu przycisku:

```txt
Test API
```

Kod UI:

```ts
export function getHealth(baseUrl: string, signal?: AbortSignal): Promise<ApiHealth> {
  return requestJson<ApiHealth>(buildUrl(baseUrl, '/health'), signal);
}
```

Potem w `main.ts`:

```ts
async function testApi(): Promise<void> {
  dispatch({ type: 'CONNECTION_CHECKING' });
  await runBusy(async () => {
    try {
      await getHealth(state.apiUrl);
      dispatch({ type: 'CONNECTION_ONLINE' });
    } catch (error) {
      dispatch({ type: 'CONNECTION_OFFLINE' });
      dispatch({ type: 'ADD_MESSAGE', level: 'error', text: error instanceof Error ? error.message : String(error) });
    }
  });
}
```

Co widzi użytkownik:

- status API zmienia się na `sprawdzanie…`,
- po sukcesie na `online`,
- po błędzie na `offline`,
- w dzienniku zdarzeń pojawia się komunikat.

---

### 6.2. `GET /measurements`

Cel: pobranie 20 najnowszych pomiarów bez filtrowania.

Backend:

```python
@app.route('/measurements', methods=['GET'])
def get_measurements():
    rows = fetch_measurements(
        '''
        SELECT id, group_id, device_id, sensor, value, unit, ts_ms, seq, topic
        FROM measurements
        ORDER BY id DESC
        LIMIT 20
        '''
    )
    return jsonify([measurement_to_dict(row) for row in rows])
```

Czyli API:

- łączy się z PostgreSQL,
- wykonuje SQL na tabeli `measurements`,
- bierze 20 najnowszych rekordów według `id DESC`,
- zwraca tablicę JSON.

Przykładowa odpowiedź:

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

W UI endpoint jest używany głównie do podpowiadania dostępnych urządzeń i sensorów.

Kod:

```ts
export function getRecentMeasurements(baseUrl: string, signal?: AbortSignal): Promise<Measurement[]> {
  return requestJson<Measurement[]>(buildUrl(baseUrl, '/measurements'), signal);
}
```

W `main.ts`:

```ts
async function fetchRecent(): Promise<void> {
  try {
    recentMeasurements = await getRecentMeasurements(state.apiUrl);
  } catch {
    // To zapytanie służy tylko do podpowiedzi device/sensor, więc nie hałasujemy błędem przy starcie.
  }
}
```

Na starcie aplikacji jest wykonywane:

```ts
void fetchRecent().finally(render);
render();
```

Czyli UI próbuje pobrać ostatnie pomiary, żeby w listach `Device` i `Sensor` pojawiły się realne wartości z systemu.

---

### 6.3. `GET /measurements/latest`

Cel: pobranie globalnie najnowszego pomiaru.

Backend:

```python
@app.route('/measurements/latest', methods=['GET'])
def get_latest_measurement():
    row = fetch_measurements(
        '''
        SELECT id, group_id, device_id, sensor, value, unit, ts_ms, seq, topic
        FROM measurements
        ORDER BY id DESC
        LIMIT 1
        ''',
        fetch_one=True,
    )
    if row is None:
        return jsonify({'message': 'Brak danych'}), 404
    return jsonify(measurement_to_dict(row))
```

Jeśli tabela ma dane, odpowiedzią jest jeden obiekt JSON:

```json
{
  "id": 265,
  "group_id": "g05",
  "device_id": "00004813-ad00-4f8c-8c4f-00ad13480000",
  "sensor": "light",
  "value": 678.26,
  "unit": "lx",
  "ts_ms": 1774960185000,
  "seq": 81,
  "topic": "lab/g05/00004813-ad00-4f8c-8c4f-00ad13480000/light"
}
```

Jeśli tabela jest pusta, backend zwraca:

```json
{
  "message": "Brak danych"
}
```

ze statusem HTTP `404`.

W UI endpoint jest używany tylko wtedy, gdy użytkownik nie wybrał żadnego filtra `Device` ani `Sensor`.

Kod:

```ts
export function getLatestMeasurement(baseUrl: string, signal?: AbortSignal): Promise<Measurement> {
  return requestJson<Measurement>(buildUrl(baseUrl, '/measurements/latest'), signal);
}
```

Logika wyboru endpointu w `getLatestForCurrentFilters()`:

```ts
async function getLatestForCurrentFilters(): Promise<Measurement> {
  if (state.deviceId || state.sensor) {
    const rows = await getMeasurementHistory(state.apiUrl, { deviceId: state.deviceId, sensor: state.sensor, limit: 1 });
    if (rows.length === 0) {
      throw new Error('Brak pomiarów dla wybranego urządzenia/sensora.');
    }
    return rows[0];
  }

  return getLatestMeasurement(state.apiUrl);
}
```

To jest ważna poprawka: wcześniej `latest` mógł pokazywać np. światło `lx`, nawet jeśli w filtrze wybrano temperaturę. Teraz, jeżeli wybrany jest filtr, UI nie bierze globalnego latest, tylko robi historię z `limit=1` dla aktualnego filtra.

---

### 6.4. `GET /measurements/history`

Cel: pobranie historii pomiarów z opcjonalnym filtrowaniem.

Backend:

```python
@app.route('/measurements/history', methods=['GET'])
def get_measurement_history():
    device_id = request.args.get('device_id')
    sensor = request.args.get('sensor')
    limit = request.args.get('limit', default=20, type=int)

    if limit is None or limit <= 0:
        return jsonify({'message': 'Parametr limit musi byc dodatnia liczba calkowita'}), 400

    query = '''
    SELECT id, group_id, device_id, sensor, value, unit, ts_ms, seq, topic
    FROM measurements
    WHERE 1=1
    '''
    params = []

    if device_id:
        query += ' AND device_id = %s'
        params.append(device_id)
    if sensor:
        query += ' AND sensor = %s'
        params.append(sensor)

    query += ' ORDER BY id DESC LIMIT %s'
    params.append(limit)

    rows = fetch_measurements(query, params=params)
    return jsonify([measurement_to_dict(row) for row in rows])
```

Obsługiwane parametry query string:

| Parametr | Wymagany? | Znaczenie | Przykład |
|---|---:|---|---|
| `device_id` | nie | filtruje po konkretnym urządzeniu | `6932dd04-08fd-4145-80cf-1988beb33679` |
| `sensor` | nie | filtruje po typie sensora | `temperature`, `humidity`, `pressure`, `light` |
| `limit` | nie | maksymalna liczba rekordów | `20` |

Przykłady:

```txt
/api/measurements/history?limit=20
/api/measurements/history?sensor=temperature&limit=20
/api/measurements/history?device_id=6932dd04-08fd-4145-80cf-1988beb33679&sensor=light&limit=20
```

W UI endpoint jest używany przez:

- przycisk `Pobierz historię`,
- wykres historii,
- `Pobierz latest`, jeśli są aktywne filtry,
- auto-refresh.

Kod klienta:

```ts
export function getMeasurementHistory(baseUrl: string, params: HistoryParams, signal?: AbortSignal): Promise<Measurement[]> {
  return requestJson<Measurement[]>(buildUrl(baseUrl, '/measurements/history', {
    device_id: params.deviceId,
    sensor: params.sensor,
    limit: params.limit ?? 20,
  }), signal);
}
```

Warto zwrócić uwagę, że `buildUrl()` nie dodaje pustych wartości:

```ts
if (value !== undefined && value !== '') search.set(key, String(value));
```

Czyli jeśli `deviceId` jest pustym stringiem, parametr `device_id` nie trafia do URL-a.

---

## 7. Model danych pomiaru

Typ w UI znajduje się w:

```txt
ui/src/api/types.ts
```

```ts
export type Measurement = {
  id: number;
  group_id: string | null;
  device_id: string;
  sensor: string;
  value: number;
  unit: string | null;
  ts_ms: number;
  seq: number | null;
  topic: string | null;
};
```

To dokładnie odpowiada temu, co backend zwraca przez `measurement_to_dict()` w `api/models.py`:

```python
def measurement_to_dict(row):
    return {
        'id': row[0],
        'group_id': row[1],
        'device_id': row[2],
        'sensor': row[3],
        'value': row[4],
        'unit': row[5],
        'ts_ms': row[6],
        'seq': row[7],
        'topic': row[8],
    }
```

Znaczenie pól:

| Pole | Typ | Znaczenie |
|---|---|---|
| `id` | number | ID rekordu w PostgreSQL, rośnie automatycznie |
| `group_id` | string/null | grupa / lokalizacja / namespace z tematu MQTT |
| `device_id` | string | identyfikator urządzenia |
| `sensor` | string | nazwa sensora, np. `temperature`, `humidity`, `pressure`, `light` |
| `value` | number | wartość pomiaru |
| `unit` | string/null | jednostka przesłana przez urządzenie / ingestor |
| `ts_ms` | number | timestamp w milisekundach od epoki Unix |
| `seq` | number/null | opcjonalny numer sekwencyjny pomiaru |
| `topic` | string/null | oryginalny topic MQTT |

---

## 8. Skąd dane trafiają do tabeli `measurements`?

Dane do tabeli `measurements` zapisuje `ingestor`.

Tabela jest tworzona w `ingestor/db.py`:

```python
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
```

Czyli API nie tworzy tabeli. API tylko czyta z tabeli. Tabelę tworzy ingestor przy starcie.

Wiadomość MQTT musi przejść walidację w `ingestor/ingestor.py`.

Topic musi mieć format:

```txt
lab/<group_id>/<device_id>/<sensor>
```

Przykład:

```txt
lab/g05/6932dd04-08fd-4145-80cf-1988beb33679/light
```

Payload JSON powinien zawierać przynajmniej:

```json
{
  "device_id": "6932dd04-08fd-4145-80cf-1988beb33679",
  "sensor": "light",
  "value": 678.26,
  "unit": "lx",
  "ts_ms": 1774960185000,
  "seq": 81,
  "group_id": "g05"
}
```

Wymagane pola w ingestorze:

```python
REQUIRED_FIELDS = ["device_id", "sensor", "value", "ts_ms"]
```

Dozwolone jednostki według sensora:

```python
ALLOWED_UNITS = {
    "temperature": "C",
    "humidity": "%",
    "pressure": "hPa",
    "light": "lx",
}
```

Ważne: w UI temperatura jest wyświetlana jako `°C`, ale ingestor oczekuje jednostki `C`. To jest kosmetyczna różnica prezentacyjna po stronie UI.

---

## 9. Struktura frontendu w TypeScript

Frontend nie używa Reacta. To jest bardzo lekka aplikacja TypeScript + DOM API.

Dlaczego tak:

- mniej zależności,
- łatwiejszy Docker,
- proste uruchomienie,
- cała logika jest jawna,
- aplikacja jest mała, więc framework nie jest konieczny.

Główny przepływ:

```txt
index.html
  ↓ ładuje
/assets/main.js
  ↓ kod wygenerowany z
src/main.ts
  ↓ korzysta z
src/api/client.ts
src/state/dashboardMachine.ts
src/lib/format.ts
```

---

## 10. `ui/public/index.html`

Plik startowy:

```html
<!doctype html>
<html lang="pl">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>WSYST Dashboard MVP</title>
    <link rel="stylesheet" href="/styles.css" />
  </head>
  <body>
    <main id="app"></main>
    <script type="module" src="/assets/main.js"></script>
  </body>
</html>
```

Najważniejszy element to:

```html
<main id="app"></main>
```

Całe UI jest renderowane dynamicznie do tego elementu przez `main.ts`.

---

## 11. `ui/src/api/client.ts` — klient API

Ten plik izoluje komunikację HTTP. Reszta UI nie musi znać szczegółów budowania URL-i ani obsługi błędów.

### 11.1. `ApiError`

```ts
export class ApiError extends Error {
  status?: number;
  details?: unknown;

  constructor(message: string, status?: number, details?: unknown) {
    super(message);
    this.name = 'ApiError';
    this.status = status;
    this.details = details;
  }
}
```

To własna klasa błędu. Przechowuje:

- `message` — czytelny komunikat,
- `status` — kod HTTP, np. `404`, `500`,
- `details` — oryginalne ciało odpowiedzi albo obiekt błędu.

### 11.2. `normalizeBaseUrl()`

```ts
function normalizeBaseUrl(baseUrl: string): string {
  const trimmed = baseUrl.trim();
  return (trimmed || '/api').replace(/\/$/, '');
}
```

Funkcja:

- ucina spacje,
- jeśli pole jest puste, używa `/api`,
- usuwa końcowy slash.

Przykłady:

```txt
"/api/"      -> "/api"
" /api "     -> "/api"
""           -> "/api"
"http://x/"  -> "http://x"
```

### 11.3. `buildUrl()`

```ts
function buildUrl(baseUrl: string, path: string, params?: Record<string, string | number | undefined>): string
```

Buduje pełny URL dla zapytania.

Przykład:

```ts
buildUrl('/api', '/measurements/history', {
  device_id: 'abc',
  sensor: 'light',
  limit: 20,
})
```

wynik:

```txt
/api/measurements/history?device_id=abc&sensor=light&limit=20
```

Puste parametry nie są dodawane.

### 11.4. `requestJson<T>()`

To główna funkcja HTTP.

Robi:

1. `fetch(url)` z nagłówkiem `Accept: application/json`.
2. Próbuje odczytać JSON.
3. Jeśli HTTP nie jest `2xx`, rzuca `ApiError`.
4. Jeśli wszystko jest OK, zwraca dane jako typ `T`.

Dzięki temu pozostałe funkcje są krótkie:

```ts
export function getHealth(baseUrl: string): Promise<ApiHealth> { ... }
export function getLatestMeasurement(baseUrl: string): Promise<Measurement> { ... }
export function getRecentMeasurements(baseUrl: string): Promise<Measurement[]> { ... }
export function getMeasurementHistory(baseUrl: string, params: HistoryParams): Promise<Measurement[]> { ... }
```

---

## 12. `ui/src/state/dashboardMachine.ts` — state machine / reducer

Ten plik opisuje stan aplikacji oraz akcje, które mogą go zmienić.

To jest uproszczony wzorzec state machine / reducer, podobny koncepcyjnie do architektury event-driven z LabVIEW albo Redux.

### 12.1. Stan połączenia

```ts
export type ConnectionState = 'idle' | 'checking' | 'online' | 'offline';
```

Znaczenie:

| Stan | Znaczenie |
|---|---|
| `idle` | API jeszcze nie testowano |
| `checking` | trwa test `/health` |
| `online` | API odpowiedziało poprawnie |
| `offline` | API nie odpowiedziało albo zwróciło błąd |

### 12.2. Poziomy komunikatów

```ts
export type MessageLevel = 'info' | 'success' | 'warning' | 'error';
```

Komunikaty trafiają do panelu `Dziennik zdarzeń`.

### 12.3. `DashboardState`

```ts
export type DashboardState = {
  apiUrl: string;
  deviceId: string;
  sensor: string;
  limit: number;
  autoRefresh: boolean;
  refreshIntervalMs: number;
  connectionState: ConnectionState;
  messages: DashboardMessage[];
};
```

Znaczenie pól:

| Pole | Znaczenie |
|---|---|
| `apiUrl` | baza API wpisana w UI, domyślnie `/api` |
| `deviceId` | aktualnie wybrany filtr urządzenia |
| `sensor` | aktualnie wybrany filtr sensora |
| `limit` | ile rekordów pobierać do historii |
| `autoRefresh` | czy automatyczne odświeżanie jest włączone |
| `refreshIntervalMs` | interwał auto-refresh w milisekundach |
| `connectionState` | stan połączenia z API |
| `messages` | lista komunikatów do dziennika zdarzeń |

### 12.4. `initialState`

Stan startowy:

```ts
export const initialState: DashboardState = {
  apiUrl: '/api',
  deviceId: '',
  sensor: '',
  limit: 20,
  autoRefresh: false,
  refreshIntervalMs: 5000,
  connectionState: 'idle',
  messages: [ ... ],
};
```

Czyli po uruchomieniu:

- UI używa proxy `/api`,
- nie ma wybranego urządzenia,
- nie ma wybranego sensora,
- limit historii to 20,
- auto-refresh jest wyłączony,
- status API to `nie testowano`.

### 12.5. Akcje

Przykładowe akcje:

```ts
{ type: 'SET_SENSOR'; value: 'light' }
{ type: 'SET_LIMIT'; value: 20 }
{ type: 'CONNECTION_ONLINE' }
{ type: 'ADD_MESSAGE'; level: 'success'; text: 'Pobrano historię: 20 rekordów.' }
```

### 12.6. `reduceDashboard()`

Reducer przyjmuje stary stan i akcję, a zwraca nowy stan.

Przykład:

```ts
case 'SET_SENSOR': return { ...state, sensor: action.value };
```

Przy zmianie limitu jest walidacja:

```ts
case 'SET_LIMIT': return { ...state, limit: Math.max(1, Math.min(500, Math.trunc(action.value || 1))) };
```

Czyli limit jest obcięty do zakresu od 1 do 500.

Przy dodawaniu komunikatów:

```ts
messages: [{ id: crypto.randomUUID(), level, text, createdAt: Date.now() }, ...state.messages].slice(0, 12)
```

Czyli:

- najnowszy komunikat idzie na górę,
- trzymamy maksymalnie 12 komunikatów.

---

## 13. `ui/src/main.ts` — główna aplikacja

To najważniejszy plik UI. Zawiera:

- globalne zmienne z danymi,
- renderowanie HTML,
- podpinanie event listenerów,
- obsługę przycisków,
- pobieranie danych,
- rysowanie wykresu SVG,
- auto-refresh.

### 13.1. Główne zmienne

```ts
let state: DashboardState = initialState;
let latestMeasurement: Measurement | null = null;
let historyMeasurements: Measurement[] = [];
let recentMeasurements: Measurement[] = [];
let autoRefreshTimer: number | null = null;
let busy = false;
let latestError = '';
let historyError = '';
```

Znaczenie:

| Zmienna | Znaczenie |
|---|---|
| `state` | stan formularza, statusu i komunikatów |
| `latestMeasurement` | ostatni pomiar pokazywany w panelu `Ostatni pomiar` |
| `historyMeasurements` | dane do wykresu historii |
| `recentMeasurements` | 20 ostatnich rekordów używane do list wyboru |
| `autoRefreshTimer` | identyfikator timera `setInterval` |
| `busy` | czy trwa operacja HTTP, blokuje przyciski |
| `latestError` | błąd panelu latest |
| `historyError` | błąd panelu historii |

### 13.2. `dispatch()`

```ts
function dispatch(action: DashboardAction): void {
  state = reduceDashboard(state, action);
  if (action.type === 'SET_AUTO_REFRESH' || action.type === 'SET_REFRESH_INTERVAL') configureAutoRefresh();
  render();
}
```

To centralny punkt zmiany stanu.

Robi trzy rzeczy:

1. Przepuszcza akcję przez reducer.
2. Jeśli zmieniono auto-refresh lub interwał, rekonfiguruje timer.
3. Renderuje UI od nowa.

To działa podobnie do prostego state machine:

```txt
akcja użytkownika
  ↓
dispatch(action)
  ↓
reducer zmienia state
  ↓
render() przerysowuje dashboard
```

### 13.3. `render()`

Funkcja `render()` generuje cały HTML aplikacji i wstawia go do:

```ts
appRoot.innerHTML = `...`;
```

Potem wywołuje:

```ts
bindEvents();
```

Ponieważ cały HTML jest tworzony od nowa, event listenery też muszą być podpinane od nowa po każdym renderze.

### 13.4. Dlaczego to działa bez Reacta?

React robi podobną rzecz koncepcyjnie, ale przez Virtual DOM. Tutaj aplikacja jest mała, więc wystarczy ręcznie:

1. Trzymać stan w zmiennych.
2. Wygenerować HTML stringiem.
3. Podpiąć eventy.
4. Po zmianie stanu wyrenderować ponownie.

---

## 14. Komponenty UI — opis panel po panelu

W kodzie nie ma osobnych plików komponentów React, ale logicznie UI składa się z kilku komponentów/paneli.

### 14.1. Header / `portal-header`

Fragment w `render()`:

```html
<section class="panel hero-panel portal-header">
```

Zawiera:

- tytuł `Measurement Portal`,
- opis `Rozproszone Systemy Pomiarowe // REST API monitor // Y2K telemetry interface`,
- pole `REST API URL`,
- przycisk `Test API`,
- przycisk `Refresh`,
- status API,
- checkbox `Auto Refresh`,
- pole `Refresh Interval`.

Funkcje:

- `Test API` wywołuje `testApi()` i sprawdza `/health`,
- `Refresh` wywołuje `refreshAll()`,
- `Auto Refresh` ustawia timer,
- `Refresh Interval` ustawia interwał timera.

### 14.2. Panel `Konfiguracja i filtry`

HTML:

```html
<section class="panel form-panel">
```

Zawiera:

- `Device` — select z urządzeniami,
- `Sensor` — select z sensorami,
- `Limit` — input liczbowy,
- `Pobierz latest`,
- `Pobierz historię`.

Opcje w selectach są budowane z danych, które UI już zna:

```ts
const known = allKnownMeasurements();
const deviceOptions = uniqueSorted([...known.map((item) => item.device_id), state.deviceId]);
const sensorOptions = uniqueSorted(['temperature', 'humidity', 'pressure', 'light', ...known.map((item) => item.sensor), state.sensor]);
```

Czyli:

- urządzenia są brane z ostatnich / historycznych / latest pomiarów,
- sensory mają zawsze bazowe opcje `temperature`, `humidity`, `pressure`, `light`,
- dodatkowo pojawiają się sensory, które faktycznie przyjdą z API.

To rozwiązuje problem, że po wybraniu jednego sensora nie dało się łatwo wybrać drugiego — teraz to normalny `<select>`.

### 14.3. Panel `Latest measurement` / `Ostatni pomiar`

HTML:

```html
<section class="panel latest-panel">
```

Zawartość generuje funkcja:

```ts
function renderLatest(): string
```

Jeśli nie ma danych, pokazuje:

```txt
Brak danych. Użyj „Pobierz latest” albo włącz auto-refresh.
```

Jeśli jest błąd, pokazuje błąd.

Jeśli jest pomiar, pokazuje:

- dużą wartość `Value`,
- `Device ID`,
- `Sensor`,
- `Timestamp`,
- `Group`,
- `Seq`.

Kod jednostki:

```ts
<strong>${formatNumber(item.value)} ${escapeHtml(sensorUnit(item.sensor, item.unit))}</strong>
```

Czyli jednostka jest dobrana przez funkcję `sensorUnit()`.

### 14.4. Panel `HistoryGraph` / `Historia pomiarów`

HTML:

```html
<section class="panel chart-panel">
```

Zawiera:

- tytuł,
- badge z liczbą punktów, np. `20 pkt`,
- wykres SVG.

Wykres generuje funkcja:

```ts
function renderChart(measurements: Measurement[]): string
```

Jeśli nie ma danych, pokazuje:

```txt
Brak historii dla podanych filtrów.
```

Jeśli są dane:

1. Sortuje pomiary po czasie `ts_ms` rosnąco.
2. Liczy minimum i maksimum wartości.
3. Skaluje punkty do wymiaru SVG.
4. Rysuje linie siatki.
5. Rysuje osie.
6. Rysuje polyline jako wykres liniowy.
7. Rysuje kropki pomiarowe.
8. Dodaje tooltipy w `<title>` dla punktów.

Ważny detal: API zwraca historię `ORDER BY id DESC`, czyli od najnowszych do najstarszych. UI przed rysowaniem sortuje dane po `ts_ms`, żeby wykres był naturalnie od lewej do prawej.

```ts
const data = [...measurements].sort((a, b) => a.ts_ms - b.ts_ms);
```

### 14.5. Panel `Komunikaty` / `Dziennik zdarzeń`

HTML:

```html
<section class="panel messages-panel">
```

Zawiera:

- tytuł,
- przycisk `Wyczyść`,
- listę komunikatów.

Komunikaty są w `state.messages`.

Przykładowe komunikaty:

- `Dashboard gotowy...`,
- `API działa poprawnie.`,
- `Pobrano najnowszy pomiar.`,
- `Pobrano historię: 20 rekordów.`,
- `Refresh zakończony.`,
- komunikaty błędów z API.

Przycisk `Wyczyść` robi:

```ts
dispatch({ type: 'CLEAR_MESSAGES' })
```

---

## 15. Funkcje pomocnicze w `main.ts`

### 15.1. `uniqueSorted()`

```ts
function uniqueSorted(values: Array<string | null | undefined>): string[] {
  return [...new Set(values.map((value) => value?.trim()).filter((value): value is string => Boolean(value)))].sort((a, b) => a.localeCompare(b));
}
```

Robi listę unikalnych, niepustych stringów i sortuje alfabetycznie.

Używane do budowania listy urządzeń i sensorów.

### 15.2. `sensorUnit()`

```ts
function sensorUnit(sensor: string, fallback?: string | null): string {
  const units: Record<string, string> = {
    temperature: '°C',
    humidity: '%',
    pressure: 'hPa',
    light: 'lx',
  };
  return units[sensor] ?? fallback ?? '';
}
```

Mapuje sensor na jednostkę prezentowaną w UI.

Dlaczego to istnieje:

- backend zwraca `unit`, ale dla bezpieczeństwa UI zna domyślne jednostki,
- jeśli backend albo stary rekord ma brak jednostki, UI nadal potrafi ją pokazać,
- eliminuje problem pokazywania `lx` dla wszystkiego.

### 15.3. `escapeHtml()`

```ts
function escapeHtml(value: string): string
```

Zabezpiecza przed wstrzyknięciem HTML w danych pochodzących z API.

Przykład:

```txt
<abc>
```

zamienia się na:

```html
&lt;abc&gt;
```

To ważne, bo wartości takie jak `device_id`, `sensor`, `group_id`, `topic` pochodzą z zewnętrznych danych.

### 15.4. `allKnownMeasurements()`

```ts
function allKnownMeasurements(): Measurement[] {
  return [...recentMeasurements, ...historyMeasurements, ...(latestMeasurement ? [latestMeasurement] : [])];
}
```

Łączy wszystkie znane UI pomiary w jedną listę. Dzięki temu selecty mogą korzystać z danych z różnych źródeł.

---

## 16. Obsługa akcji użytkownika

Event listenery są podpinane w:

```ts
function bindEvents(): void
```

Najważniejsze mapowania:

| Element UI | Event | Funkcja / akcja |
|---|---|---|
| `REST API URL` | `change` | `SET_API_URL` |
| `Device` | `change` | `SET_DEVICE_ID` |
| `Sensor` | `change` | `SET_SENSOR` |
| `Limit` | `change` | `SET_LIMIT` |
| `Refresh Interval` | `change` | `SET_REFRESH_INTERVAL` |
| `Auto Refresh` | `change` | `SET_AUTO_REFRESH` |
| `Test API` | `click` | `testApi()` |
| `Refresh` | `click` | `refreshAll()` |
| `Pobierz latest` | `click` | `fetchLatest()` |
| `Pobierz historię` | `click` | `fetchHistory()` |
| `Wyczyść` | `click` | `CLEAR_MESSAGES` |

---

## 17. Przepływy danych w UI

### 17.1. Start aplikacji

Na końcu `main.ts`:

```ts
void fetchRecent().finally(render);
render();
```

Kolejność:

1. UI renderuje się natychmiast z pustymi danymi.
2. Równolegle próbuje pobrać `/measurements`.
3. Po pobraniu ostatnich pomiarów renderuje się drugi raz.
4. Selecty `Device` i `Sensor` mogą już mieć realne opcje.

### 17.2. Kliknięcie `Test API`

```txt
klik Test API
  ↓
testApi()
  ↓
dispatch(CONNECTION_CHECKING)
  ↓
GET /api/health
  ↓
sukces: CONNECTION_ONLINE + komunikat
błąd: CONNECTION_OFFLINE + komunikat błędu
```

### 17.3. Kliknięcie `Pobierz latest`

```txt
klik Pobierz latest
  ↓
fetchLatest()
  ↓
getLatestForCurrentFilters()
  ↓
jeśli nie ma filtrów:
    GET /api/measurements/latest
jeśli jest device albo sensor:
    GET /api/measurements/history?...&limit=1
  ↓
latestMeasurement = wynik
  ↓
renderLatest()
```

To zachowanie jest specjalnie tak zrobione, żeby `latest` respektował wybrane filtry.

### 17.4. Kliknięcie `Pobierz historię`

```txt
klik Pobierz historię
  ↓
fetchHistory()
  ↓
GET /api/measurements/history?device_id=...&sensor=...&limit=...
  ↓
historyMeasurements = wynik
  ↓
renderChart(historyMeasurements)
```

### 17.5. Kliknięcie `Refresh`

```txt
klik Refresh
  ↓
refreshAll()
  ↓
Promise.allSettled([
  latest dla aktualnych filtrów,
  historia dla aktualnych filtrów,
  recent measurements dla selectów
])
  ↓
komunikat "Refresh zakończony."
  ↓
render()
```

`Promise.allSettled()` oznacza, że UI czeka na wszystkie operacje, ale pojedynczy błąd nie przerywa całego refresha tak agresywnie jak `Promise.all()`.

### 17.6. Auto-refresh

Auto-refresh działa przez `setInterval`.

```ts
function configureAutoRefresh(): void {
  if (autoRefreshTimer !== null) {
    window.clearInterval(autoRefreshTimer);
    autoRefreshTimer = null;
  }

  if (state.autoRefresh) {
    autoRefreshTimer = window.setInterval(() => {
      void refreshAll();
    }, state.refreshIntervalMs);
  }
}
```

Zachowanie:

- po włączeniu auto-refresh tworzy timer,
- po wyłączeniu usuwa timer,
- po zmianie interwału usuwa stary timer i tworzy nowy,
- co interwał wykonuje `refreshAll()`.

---

## 18. `runBusy()` — blokowanie UI podczas odczytu

```ts
async function runBusy(task: () => Promise<void>): Promise<void> {
  busy = true;
  render();
  try {
    await task();
  } finally {
    busy = false;
    render();
  }
}
```

To wrapper dla operacji asynchronicznych.

Dzięki temu:

- przyciski mają `disabled`, gdy trwa request,
- panel latest pokazuje `odczyt…`,
- po zakończeniu UI zawsze wraca do normalnego stanu, nawet gdy wystąpi błąd.

---

## 19. Formatowanie danych

Plik:

```txt
ui/src/lib/format.ts
```

### 19.1. `formatNumber()`

```ts
export function formatNumber(value: number | null | undefined, digits = 2): string
```

Formatuje liczby po polsku.

Przykład:

```txt
678.26 -> 678,26
```

Używa:

```ts
new Intl.NumberFormat('pl-PL', { maximumFractionDigits: digits })
```

### 19.2. `formatTimestamp()`

```ts
export function formatTimestamp(tsMs: number | null | undefined): string
```

Zamienia timestamp w milisekundach na czytelną datę.

Przykład:

```txt
1774960185000 -> 05.05.2026, 16:09:45
```

Używa `Intl.DateTimeFormat('pl-PL')`.

### 19.3. `shortId()`

Skraca długie identyfikatory urządzeń.

Przykład:

```txt
6932dd04-08fd-4145-80cf-1988beb33679
```

może być pokazane jako:

```txt
6932dd04-0…b33679
```

Pełna wartość nadal jest w atrybucie `title`, więc po najechaniu można ją zobaczyć.

### 19.4. `compactUrl()`

Skraca URL do pokazania w UI, np. usuwa `http://` i końcowy slash.

---

## 20. Style Y2K / retro-web

Style są w:

```txt
ui/src/styles.css
ui/public/styles.css
```

`src/styles.css` to źródło, a `public/styles.css` to plik używany przez przeglądarkę.

Aplikacja ma styl inspirowany stronami z końca lat 90. i początku 2000:

- ciemne tło,
- neonowa zieleń,
- elektryczny niebieski,
- chromowane ramki,
- gradienty,
- pseudo-skanlinie,
- siatka w tle,
- błyszczące przyciski,
- panele jak z retro interfejsu sci-fi.

### 20.1. Zmienne CSS

Na początku pliku są zmienne:

```css
:root {
  --bg: #020407;
  --green: #8cff29;
  --blue: #1478ff;
  --cyan: #36e5ff;
  ...
}
```

Dzięki temu można łatwo zmieniać motyw.

### 20.2. `.panel`

Klasa `.panel` odpowiada za większość wyglądu kafli:

- chromowane ramki,
- ciemne wnętrze,
- zaokrąglenia,
- cień,
- poświata,
- dekoracyjne elementy w `::before` i `::after`.

Wszystkie główne sekcje używają `.panel`.

### 20.3. Layout

Główne klasy layoutu:

```css
.portal-header
.dashboard-grid
.left-column
.right-column
.portal-footer
```

Układ logicznie odpowiada LabVIEW:

```txt
header z API

lewa kolumna: konfiguracja + latest
prawa kolumna: wykres + komunikaty

footer statusowy
```

### 20.4. Wykres

Wykres jest zwykłym SVG generowanym przez TypeScript, a wygląd linii i kropek kontroluje CSS:

```css
.chart-line
.chart-dot
.chart-grid
.chart-axis
.chart-tick
.chart-label
```

Dlatego nie potrzeba biblioteki typu Chart.js czy Recharts.

---

## 21. Docker i uruchomienie

### 21.1. Główny `docker-compose.yml`

Projekt uruchamia kilka usług:

```yaml
services:
  flask:
    ...
  broker:
    ...
  database:
    ...
  ingestor:
    ...
  ui:
    ...
```

Znaczenie:

| Usługa | Rola |
|---|---|
| `flask` | REST API na porcie 5001 |
| `broker` | Mosquitto MQTT na porcie 1883 |
| `database` | PostgreSQL na porcie 5432 |
| `ingestor` | odbiera MQTT i zapisuje do PostgreSQL |
| `ui` | dashboard webowy na porcie 5173 |

### 21.2. Najważniejsze porty

| Port hosta | Serwis | Znaczenie |
|---:|---|---|
| `5173` | UI | dashboard w przeglądarce |
| `5001` | Flask API | REST API |
| `1883` | broker | MQTT |
| `5432` | database | PostgreSQL |

### 21.3. Uruchomienie całego projektu

```bash
docker compose up -d --build
```

Po starcie:

```txt
http://localhost:5173
```

W UI pole `REST API URL` powinno zostać:

```txt
/api
```

### 21.4. Logi

UI:

```bash
docker compose logs -f ui
```

API:

```bash
docker compose logs -f flask
```

Ingestor:

```bash
docker compose logs -f ingestor
```

Baza:

```bash
docker compose logs -f database
```

### 21.5. Test API z hosta

```bash
curl http://localhost:5001/health
curl http://localhost:5001/measurements
curl "http://localhost:5001/measurements/history?sensor=light&limit=5"
```

### 21.6. Test API przez proxy UI

```bash
curl http://localhost:5173/api/health
curl http://localhost:5173/api/measurements
curl "http://localhost:5173/api/measurements/history?sensor=light&limit=5"
```

Jeśli przez port 5001 działa, a przez 5173 nie działa, problem jest w UI proxy / `API_TARGET`.

---

## 22. Uwaga o Dockerfile UI

W repo może być Dockerfile z build stage:

```dockerfile
FROM node:22-alpine AS build
COPY package.json package-lock.json ./
RUN npm ci
...
RUN npm run build
```

To jest klasyczne podejście: kontener sam instaluje zależności i kompiluje TypeScript.

Ale w tym konkretnym projekcie frontend ma już skompilowane pliki w:

```txt
ui/public/assets/main.js
```

Dlatego można też używać prostszego Dockerfile bez `npm ci`, co bywa wygodniejsze, jeśli Docker ma problem z pobieraniem paczek npm.

Wersja runtime-only:

```dockerfile
FROM node:22-alpine

WORKDIR /app

ENV NODE_ENV=production
ENV PORT=5173
ENV API_TARGET=http://flask:5001

COPY public ./public
COPY server.mjs ./server.mjs
COPY package.json ./package.json

USER node

EXPOSE 5173

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD node -e "fetch('http://127.0.0.1:' + (process.env.PORT || 5173) + '/').then(r => process.exit(r.ok ? 0 : 1)).catch(() => process.exit(1))"

CMD ["node", "server.mjs"]
```

Minus tej wersji: jeśli zmienisz `ui/src/*.ts`, musisz lokalnie skompilować TypeScript:

```bash
cd ui
npm install
npm run build
```

albo wrócić do Dockerfile z `npm ci`.

---

## 23. Jak opowiedzieć ten UI na prezentacji

Możesz powiedzieć mniej więcej tak:

> Dashboard jest osobnym modułem w katalogu `/ui`. Jest napisany w TypeScript jako lekka aplikacja webowa bez ciężkiego frameworka. Frontend jest serwowany przez mały serwer Node.js, który jednocześnie działa jako proxy do REST API. Dzięki temu przeglądarka komunikuje się z `/api`, a Node przekazuje zapytania do kontenera Flask po wewnętrznej sieci Dockera.

Dalej:

> UI ma prostą architekturę state machine / reducer. Stan dashboardu jest opisany w `dashboardMachine.ts`, a każda akcja użytkownika, np. wybór sensora, zmiana limitu, test API czy włączenie auto-refresh, przechodzi przez `dispatch()`. Po zmianie stanu aplikacja renderuje interfejs ponownie.

Dalej:

> Dane pobierane są przez moduł `api/client.ts`, który opakowuje `fetch()` i wystawia funkcje `getHealth`, `getLatestMeasurement`, `getRecentMeasurements` i `getMeasurementHistory`. Dzięki temu logika UI nie musi bezpośrednio budować zapytań HTTP.

Dalej:

> Backend Flask udostępnia endpointy `/health`, `/measurements`, `/measurements/latest` oraz `/measurements/history`. UI używa `/measurements/history` z parametrami `device_id`, `sensor` i `limit` do wykresu oraz do pobierania ostatniego pomiaru dla aktywnych filtrów.

Dalej:

> Wizualnie interfejs jest stylizowany na retro Y2K / cyber web dashboard, ale funkcjonalnie odpowiada panelowi LabVIEW: ma konfigurację filtrów, panel ostatniego pomiaru, wykres historii, status API, auto-refresh i dziennik zdarzeń.

---

## 24. Najważniejsze pliki, które warto znać

### `ui/src/main.ts`

Główna logika aplikacji:

- renderowanie UI,
- przyciski,
- wykres,
- pobieranie danych,
- auto-refresh.

### `ui/src/api/client.ts`

Komunikacja z REST API:

- budowanie URL-i,
- `fetch`,
- obsługa JSON,
- obsługa błędów.

### `ui/src/api/types.ts`

Typy danych z backendu:

- `Measurement`,
- `ApiHealth`,
- `HistoryParams`.

### `ui/src/state/dashboardMachine.ts`

Stan i akcje UI:

- `DashboardState`,
- `DashboardAction`,
- `reduceDashboard()`.

### `ui/src/lib/format.ts`

Formatowanie:

- liczb,
- timestampów,
- krótkich ID,
- URL.

### `ui/src/styles.css` / `ui/public/styles.css`

Wygląd Y2K.

### `ui/server.mjs`

Serwer:

- statyczne pliki,
- proxy `/api`.

### `api/app.py`

REST API:

- endpointy,
- SQL SELECT,
- JSON responses.

### `ingestor/ingestor.py`

Wejście danych:

- MQTT,
- walidacja wiadomości,
- zapis przez `save_measurement()`.

### `ingestor/db.py`

Baza:

- tworzenie tabeli `measurements`,
- zapis pomiarów.

---

## 25. Typowe problemy i diagnoza

### 25.1. UI działa, ale API offline

Sprawdź:

```bash
docker compose ps
```

Potem:

```bash
curl http://localhost:5001/health
```

Jeśli nie działa, problem jest po stronie Flask API.

Jeśli działa, sprawdź proxy:

```bash
curl http://localhost:5173/api/health
```

Jeśli to nie działa, problemem jest `API_TARGET` w UI.

Dla obecnego compose powinno być:

```yaml
API_TARGET: http://flask:5001
```

### 25.2. Docker Compose mówi: `services.services additional properties 'ui' not allowed`

To oznacza, że `ui` zostało wklejone pod drugim `services:`.

Źle:

```yaml
services:
  flask:
    ...
  services:
    ui:
      ...
```

Dobrze:

```yaml
services:
  flask:
    ...
  ui:
    ...
```

### 25.3. Docker stoi na `RUN npm ci`

Możliwe, że Docker nie ma dostępu do npm albo sieć jest wolna. Można użyć runtime-only Dockerfile opisanego wyżej, bo skompilowany frontend jest już w `public/assets/main.js`.

### 25.4. Wykres pusty

Możliwe przyczyny:

- brak danych w tabeli `measurements`,
- zbyt wąski filtr `device_id` / `sensor`,
- API nie działa,
- ingestor nie zapisuje danych.

Test:

```bash
curl http://localhost:5001/measurements
curl "http://localhost:5001/measurements/history?limit=20"
```

### 25.5. Latest pokazuje brak danych dla filtra

Jeśli wybrano sensor albo device, UI robi:

```txt
/measurements/history?...&limit=1
```

Jeśli API zwróci pustą tablicę, UI pokazuje:

```txt
Brak pomiarów dla wybranego urządzenia/sensora.
```

To znaczy, że w bazie nie ma rekordu pasującego do filtra.

---

## 26. Co jest najważniejsze do zapamiętania

1. UI jest w `/ui`.
2. Backend REST API jest w `/api`.
3. UI nie rozmawia z bazą bezpośrednio.
4. UI woła `/api/...`, a `server.mjs` przekazuje to do Flask API.
5. Flask API czyta dane z PostgreSQL.
6. Dane do PostgreSQL zapisuje `ingestor` po odebraniu MQTT.
7. Stan UI jest w `dashboardMachine.ts`.
8. Komunikacja HTTP jest w `api/client.ts`.
9. Główny render i obsługa przycisków są w `main.ts`.
10. Wykres jest generowany jako SVG, bez zewnętrznej biblioteki.
11. Auto-refresh działa przez `setInterval` i wywołuje `refreshAll()`.
12. W Docker Compose dla aktualnej nazwy usługi backendu poprawny target to `http://flask:5001`.

---

## 27. Minimalna ścieżka od kliknięcia do danych na ekranie

Przykład: użytkownik wybiera `Sensor = light` i klika `Pobierz historię`.

```txt
<select id="sensor"> zmienia wartość na light
  ↓
bindEvents() obsługuje change
  ↓
dispatch({ type: 'SET_SENSOR', value: 'light' })
  ↓
reduceDashboard() zapisuje sensor w state
  ↓
render()
  ↓
użytkownik klika Pobierz historię
  ↓
fetchHistory()
  ↓
getMeasurementHistory('/api', { sensor: 'light', limit: 20 })
  ↓
buildUrl() tworzy /api/measurements/history?sensor=light&limit=20
  ↓
fetch()
  ↓
server.mjs widzi /api i robi proxy
  ↓
Flask dostaje /measurements/history?sensor=light&limit=20
  ↓
Flask wykonuje SELECT z WHERE sensor = 'light'
  ↓
PostgreSQL zwraca rekordy
  ↓
Flask mapuje rekordy przez measurement_to_dict()
  ↓
UI dostaje JSON
  ↓
historyMeasurements = wynik
  ↓
renderChart(historyMeasurements)
  ↓
na ekranie pojawia się wykres
```

To jest najważniejszy flow całego dashboardu.

---

## 28. Minimalna ścieżka danych od MQTT do wykresu

```txt
ESP32 publikuje MQTT:
  topic: lab/g05/device123/light
  payload: { device_id, sensor, value, unit, ts_ms, seq, group_id }

broker Mosquitto odbiera wiadomość
  ↓
ingestor subskrybuje lab/+/+/+
  ↓
ingestor waliduje topic i payload
  ↓
ingestor zapisuje do PostgreSQL tabeli measurements
  ↓
Flask API robi SELECT z measurements
  ↓
UI pobiera JSON przez /api/measurements/history
  ↓
UI renderuje wykres SVG
```

---

## 29. Sugestie dalszych usprawnień

To nie jest wymagane do działania, ale można rozwijać projekt tak:

1. Dodać endpoint `/devices` i `/sensors`, żeby selecty nie musiały bazować na ostatnich 20 pomiarach.
2. Dodać filtrowanie po czasie: `from_ts`, `to_ts`.
3. Dodać backendowy endpoint `latest` z filtrami, np. `/measurements/latest?device_id=...&sensor=...`.
4. Dodać WebSocket albo Server-Sent Events zamiast odpytywania co kilka sekund.
5. Dodać eksport historii do CSV.
6. Dodać informację o jakości połączenia / czasie ostatniego pomiaru.
7. Dodać testy jednostkowe dla `api/client.ts` i `dashboardMachine.ts`.
8. Dodać prosty e2e test sprawdzający, czy `/api/health` działa przez UI proxy.

---

## 30. Podsumowanie jednym akapitem

UI jest lekkim webowym dashboardem napisanym w TypeScript, osadzonym w katalogu `/ui`. Aplikacja renderuje interfejs w przeglądarce, trzyma stan w prostym reducerze, pobiera dane z REST API przez moduł `api/client.ts`, pokazuje ostatni pomiar, historię na wykresie SVG oraz dziennik zdarzeń. Serwer `server.mjs` serwuje statyczne pliki UI i robi proxy `/api` do backendu Flask, dzięki czemu całość dobrze działa w Docker Compose. Backend Flask pobiera dane z PostgreSQL, a dane do bazy zapisuje ingestor po odebraniu poprawnych wiadomości MQTT. Cały dashboard jest funkcjonalnym odpowiednikiem panelu LabVIEW, ale w estetyce retro Y2K i z webową architekturą.
