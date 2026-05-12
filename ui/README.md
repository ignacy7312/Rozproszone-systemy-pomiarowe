# WSYST Dashboard MVP

Webowy dashboard MVP dla projektu „Rozproszone Systemy Pomiarowe”. Aplikacja jest analogiczna do panelu LabVIEW: testuje REST API, pobiera najnowszy pomiar z uwzględnieniem filtrów, pobiera historię z filtrami, rysuje wykres i obsługuje auto-refresh.

Stack: **TypeScript + zero-dependency Node server + natywny DOM + SVG**. Celowo bez Reacta, Vite i bibliotek wykresów w ścieżce uruchomieniowej, żeby odpalenie było jak najmniej bolesne.

## Najszybsze uruchomienie

Wymagania:

- Node.js 20+ albo 22+,
- działające API projektu pod `http://localhost:5001`.

```bash
npm start
```

`npm install` nie jest potrzebne do samego uruchomienia. Repo zawiera już wygenerowane pliki JS w `public/assets/`.

Otwórz w przeglądarce:

```bash
http://localhost:5173
```

Domyślny adres API w UI to:

```txt
/api
```

To jest celowe. Wbudowany serwer Node robi proxy:

```txt
frontend /api/...  ->  http://localhost:5001/...
```

Dzięki temu nie trzeba od razu modyfikować Flask API pod CORS.

## Konfiguracja proxy API

Domyślnie proxy kieruje na `http://localhost:5001`. Jeśli backend działa gdzie indziej:

```bash
API_TARGET=http://localhost:5001 npm start
```

Windows PowerShell:

```powershell
$env:API_TARGET="http://localhost:5001"; npm start
```

## Przebudowanie TypeScriptu

Do uruchomienia wystarczy `npm start`. Jeśli zmienisz pliki w `src/`, doinstaluj TypeScript i przebuduj:

```bash
npm install
npm run build
npm start
```

`npm run build` kompiluje `src/**/*.ts` do `public/assets/`.

## Funkcje MVP

- test `/health`,
- pole REST API URL,
- status API: nie testowano / sprawdzanie / online / offline,
- pobieranie latest: globalnie przez `/measurements/latest`, a po ustawieniu Device/Sensor przez `/measurements/history?...&limit=1`, żeby karta latest zgadzała się z wybranym czujnikiem,
- pobieranie `/measurements/history?device_id=...&sensor=...&limit=...`,
- lista ostatnich rekordów `/measurements` używana do zbudowania listy urządzeń i sensorów,
- filtry Device, Sensor, Limit,
- wykres historii pomiarów jako SVG,
- auto-refresh z interwałem w ms,
- panel komunikatów,
- architektura z reducerem/state machine dla stanu GUI.

## Mapowanie na panel LabVIEW

| LabVIEW | Dashboard webowy |
|---|---|
| REST API URL | pole `REST API URL` w górnym panelu |
| Test API | przycisk `Test API` |
| Status API | status pill `online/offline/...` |
| Auto Refresh | checkbox `Auto Refresh` |
| Refresh Interval | input `Refresh Interval` |
| Device | select ze znanymi urządzeniami i opcją „wszystkie urządzenia” |
| Sensor | select ze znanymi sensorami i opcją „wszystkie sensory” |
| Limit | input liczbowy |
| Pobierz latest | przycisk `Pobierz latest` |
| Pobierz historię | przycisk `Pobierz historię` |
| Latest measurement | karta `Ostatni pomiar` |
| HistoryGraph | karta `Historia pomiarów` |
| Komunikaty | karta `Dziennik zdarzeń` |

## Endpointy używane przez frontend

```txt
GET /health
GET /measurements
GET /measurements/latest
GET /measurements/history?device_id=<id>&sensor=<name>&limit=<n>
```

Aplikacja zakłada format rekordu:

```ts
type Measurement = {
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

## Najczęstsze problemy

### `Nie udało się połączyć z API`

Sprawdź, czy API działa:

```bash
curl http://localhost:5001/health
```

Jeśli działa, zostaw w UI adres `/api`, a nie `http://localhost:5001`, bo `/api` korzysta z proxy wbudowanego w `server.mjs`.

### Brak urządzenia na liście

Backend nie ma jeszcze endpointu `/devices`. MVP buduje listę urządzeń z danych zwróconych przez `/measurements`, `/measurements/latest` i `/measurements/history`. Najpierw pobierz kilka rekordów albo użyj opcji „wszystkie urządzenia”. Sensory mają dodatkowo domyślne wartości z projektu: `temperature`, `humidity`, `pressure`, `light`.

### Wykres jest pusty

Kliknij `Pobierz historię`. Jeśli dalej jest pusty, sprawdź filtry Device/Sensor albo zwiększ Limit.

## Zmiany w wersji 0.1.2

- `Pobierz latest` respektuje wybrane filtry Device/Sensor. Jeśli filtr jest ustawiony, frontend pobiera najnowszy pasujący rekord przez `/measurements/history` z `limit=1`; dzięki temu np. `temperature` nie pokazuje już przypadkowego najnowszego pomiaru `light` w `lx`.
- Device i Sensor są teraz normalnymi listami `select`, więc można przełączać sensor bez ręcznego czyszczenia pola.
- Jednostka jest mapowana także po nazwie sensora: `temperature -> °C`, `humidity -> %`, `pressure -> hPa`, `light -> lx`.

## Proponowany kolejny krok

Najbardziej praktyczne rozszerzenie backendu to dodanie endpointów:

```txt
GET /devices
GET /sensors
GET /sensors?device_id=<id>
```

Wtedy frontend może mieć prawdziwe selecty zamiast inputów z podpowiedziami.


## Motyw Y2K / retro web

Wersja 0.1.2 zmienia tylko warstwę frontendową: CSS i drobny układ nagłówka/stopki. Backend, endpointy, proxy `/api`, logika pobierania latest/historii oraz state machine pozostają zgodne z poprzednią wersją.

Styl jest inspirowany późnymi latami 90./Y2K: ciemne tło, neonowa siatka, chromowane ramki, świecące wykresy, terminalowy dziennik zdarzeń i przyciski w stylu retro-web.

## Docker

Frontend ma osobny `Dockerfile`, analogicznie do pozostałych usług projektu. Obraz uruchamia ten sam serwer `server.mjs`, który serwuje pliki z `public/` i robi proxy `/api` do backendu.

### Build

```bash
docker build -t wsyst-dashboard-mvp:0.1.3-y2k .
```

### Uruchomienie w docker compose razem z usługą `api`

Jeśli backend REST API działa w tej samej sieci Dockera jako usługa `api`, użyj domyślnego ustawienia:

```bash
docker compose -f docker-compose.frontend.yml up --build
```

Otwórz:

```txt
http://localhost:5173
```

W UI zostaw `REST API URL` jako:

```txt
/api
```

Wtedy przepływ jest taki:

```txt
przeglądarka -> dashboard container /api -> http://api:5001
```

### Uruchomienie kontenera, gdy backend działa na hoście

Docker Desktop, macOS/Windows:

```bash
docker run --rm -p 5173:5173 \
  -e API_TARGET=http://host.docker.internal:5001 \
  wsyst-dashboard-mvp:0.1.3-y2k
```

Linux:

```bash
docker run --rm -p 5173:5173 \
  --add-host=host.docker.internal:host-gateway \
  -e API_TARGET=http://host.docker.internal:5001 \
  wsyst-dashboard-mvp:0.1.3-y2k
```

### Zmienne środowiskowe

| Zmienna | Domyślnie w Dockerze | Znaczenie |
|---|---:|---|
| `PORT` | `5173` | port serwera frontendu w kontenerze |
| `API_TARGET` | `http://api:5001` | adres backendu REST API widziany z kontenera |

`API_TARGET` nie jest adresem wpisywanym w przeglądarce. W UI nadal najlepiej zostawić `/api`, a `API_TARGET` ustawia tylko wewnętrzne proxy Node.
