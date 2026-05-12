# WSYST Dashboard MVP — dokumentacja techniczna

## 1. Cel aplikacji

Ten dashboard jest webowym odpowiednikiem panelu LabVIEW ze zrzutu ekranu. Ma pełnić rolę prostego, ale estetycznego interfejsu operatorskiego do REST API projektu „Rozproszone Systemy Pomiarowe”.

Najważniejsze założenia MVP:

- minimalny próg wejścia: `npm start`, bez instalowania zależności do samego uruchomienia,
- brak ciężkiego desktop GUI,
- brak dużych bibliotek frontendowych,
- praca w przeglądarce,
- zgodność z obecnym Flask API,
- architektura możliwa do rozwoju w stronę state machine albo producer/consumer,
- układ analogiczny do LabVIEW, ale wizualnie nowocześniejszy.

Aplikacja nie zapisuje danych i nie publikuje komunikatów MQTT. Jest tylko klientem odczytowym dla REST API.

## 2. Wybrany stack

### TypeScript

TypeScript daje typy dla danych z API. Dzięki temu szybciej widać, czy frontend oczekuje np. `ts_ms`, `device_id`, `value`, a nie przypadkowej nazwy pola.

### Zero-dependency Node server

Do uruchomienia nie potrzeba Expressa, Vite ani żadnego bundlera. `server.mjs` używa tylko wbudowanych modułów Node.js:

- `node:http`,
- `node:fs`,
- `node:path`,
- globalnego `fetch` dostępnego w Node 20+.

Serwer robi dwie rzeczy:

1. serwuje statyczne pliki z `public/`,
2. przekazuje `/api/...` do Flask API.

### Natywny DOM

MVP nie używa Reacta. To celowa decyzja: mniej zależności, prostsza instalacja, mniej rzeczy, które mogą się wysypać na cudzym komputerze. UI jest renderowany z TypeScriptu jako HTML string, a logika zdarzeń jest wiązana po każdym renderze.

### SVG

Wykres historii jest rysowany jako natywny SVG. Nie trzeba instalować Chart.js, Recharts ani D3.

## 3. Struktura katalogów

```txt
wsyst-dashboard-mvp/
  README.md
  docs.md
  package.json
  server.mjs
  tsconfig.json
  .env.example
  .gitignore
  public/
    index.html
    styles.css
    assets/
      main.js
      api/
        client.js
        types.js
      lib/
        format.js
      state/
        dashboardMachine.js
  src/
    main.ts
    api/
      client.ts
      types.ts
    lib/
      format.ts
    state/
      dashboardMachine.ts
```

## 4. Opis plików

### `package.json`

Definiuje skrypty projektu.

```bash
npm start
```

Uruchamia `server.mjs`. To jest główna, najprostsza ścieżka uruchomienia.

```bash
npm run dev
```

Alias do `npm start`.

```bash
npm run build
```

Kompiluje TypeScript z `src/` do `public/assets/`. Wymaga wcześniejszego `npm install`, bo TypeScript jest zależnością developerską.

```bash
npm run check
```

Sprawdza typy bez emitowania plików.

### `server.mjs`

Mały serwer HTTP bez zewnętrznych zależności.

Zachowanie:

- `GET /` zwraca `public/index.html`,
- `GET /styles.css` zwraca CSS,
- `GET /assets/...` zwraca skompilowany JS,
- `GET /api/...` przekazuje request do backendu.

Domyślne wartości:

```txt
PORT=5173
API_TARGET=http://localhost:5001
```

Zmiana backendu:

```bash
API_TARGET=http://localhost:5001 npm start
```

Windows PowerShell:

```powershell
$env:API_TARGET="http://localhost:5001"; npm start
```

### `public/index.html`

Minimalny HTML. Zawiera kontener:

```html
<main id="app"></main>
```

oraz import skompilowanej aplikacji:

```html
<script type="module" src="/assets/main.js"></script>
```

### `public/styles.css`

Wszystkie style aplikacji. Plik jest statyczny i nie wymaga buildu.

### `public/assets/`

Skompilowany JavaScript. Ten katalog jest gotowy do uruchomienia od razu po rozpakowaniu projektu.

### `src/main.ts`

Główny plik aplikacji. Zawiera:

- aktualny stan runtime,
- funkcję `dispatch`,
- render całego UI,
- funkcję rysowania wykresu SVG,
- obsługę kliknięć i zmian inputów,
- funkcje `testApi`, `fetchLatest`, `fetchHistory`, `refreshAll`,
- konfigurację auto-refresh.

To jest odpowiednik głównego diagramu aplikacji z LabVIEW.

### `src/api/types.ts`

Typy danych zwracanych przez backend.

Najważniejszy typ:

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

Ten typ odpowiada temu, co zwraca funkcja `measurement_to_dict` w Flask API.

### `src/api/client.ts`

Warstwa komunikacji z API. `main.ts` nie składa endpointów ręcznie, tylko używa funkcji z tego pliku.

Dostępne funkcje:

```ts
getHealth(baseUrl)
getLatestMeasurement(baseUrl)
getRecentMeasurements(baseUrl)
getMeasurementHistory(baseUrl, params)
```

Plik zawiera też klasę `ApiError`, która przechowuje komunikat błędu, status HTTP i ewentualne szczegóły odpowiedzi.

### `src/state/dashboardMachine.ts`

To jest najbliższy odpowiednik podejścia state machine z LabVIEW.

Stan aplikacji:

```ts
type DashboardState = {
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

Najważniejsze przejścia statusu API:

```txt
idle -> checking -> online
idle -> checking -> offline
online -> checking -> online/offline
offline -> checking -> online/offline
```

Oprócz tego reducer obsługuje zmianę filtrów, interwału, trybu auto-refresh i dziennika komunikatów.

### `src/lib/format.ts`

Pomocnicze funkcje formatowania:

- `formatNumber`,
- `formatTimestamp`,
- `shortId`,
- `compactUrl`.

## 5. Jak działa renderowanie

Aplikacja jest małą SPA bez frameworka.

Główna pętla wygląda koncepcyjnie tak:

```txt
akcja użytkownika
  -> dispatch(action)
  -> reduceDashboard(state, action)
  -> render()
  -> bindEvents()
```

`render()` generuje HTML na podstawie aktualnego stanu oraz ostatnio pobranych danych:

- `latestMeasurement`,
- `historyMeasurements`,
- `recentMeasurements`,
- `latestError`,
- `historyError`,
- `busy`.

Po każdym renderze wywoływane jest `bindEvents()`, ponieważ ustawienie `innerHTML` tworzy nowe elementy DOM.

## 6. Jak działa pobieranie danych

### Test API

Przycisk `Test API` wykonuje:

```txt
GET /health
```

Sekwencja:

```txt
CONNECTION_CHECKING
  -> sukces: CONNECTION_ONLINE
  -> błąd: CONNECTION_OFFLINE + ADD_MESSAGE error
```

### Pobierz latest

Przycisk `Pobierz latest` działa w dwóch trybach.

Bez filtrów wykonuje:

```txt
GET /measurements/latest
```

Po wybraniu Device albo Sensor wykonuje:

```txt
GET /measurements/history?device_id=<...>&sensor=<...>&limit=1
```

To obejście jest potrzebne, bo obecny endpoint `/measurements/latest` nie przyjmuje filtrów. Dzięki temu karta `Ostatni pomiar` pokazuje najnowszy rekord zgodny z wyborem użytkownika, np. `temperature` w `°C`, a nie globalnie najnowszy rekord `light` w `lx`. Wynik trafia do zmiennej `latestMeasurement` i jest renderowany w karcie `Ostatni pomiar`.

### Pobierz historię

Przycisk `Pobierz historię` wykonuje:

```txt
GET /measurements/history?device_id=<...>&sensor=<...>&limit=<...>
```

Jeśli Device albo Sensor są puste, frontend nie wysyła tych parametrów. Dzięki temu można pobrać ostatnie rekordy bez filtrowania.

### Pobierz recent

Na starcie aplikacja próbuje wykonać:

```txt
GET /measurements
```

Te dane służą głównie do zbudowania list Device/Sensor. Jeśli request się nie uda, aplikacja nie hałasuje błędem na start, bo backend może jeszcze nie być włączony.

## 7. Auto-refresh

Auto-refresh jest realizowany przez `window.setInterval`.

Gdy użytkownik zaznacza checkbox:

```txt
SET_AUTO_REFRESH true
  -> configureAutoRefresh()
  -> setInterval(refreshAll, refreshIntervalMs)
```

Gdy użytkownik wyłącza checkbox albo zmienia interwał, poprzedni timer jest czyszczony:

```ts
window.clearInterval(autoRefreshTimer)
```

`refreshAll()` próbuje równolegle pobrać:

- latest,
- historię,
- recent measurements.

## 8. Jak działa wykres SVG

Wykres jest rysowany w `renderChart()`.

Kroki:

1. Dane są sortowane po `ts_ms` rosnąco.
2. Wyliczane są `min`, `max` i zakres wartości.
3. Każdy pomiar jest mapowany na punkt `(x, y)`.
4. Punkty są łączone jako `polyline`.
5. Dodawane są grid lines, osie, etykiety i kropki.
6. Każda kropka ma `<title>`, więc po najechaniu myszką przeglądarka pokazuje podstawowy tooltip.

Dlaczego sortowanie po czasie jest ważne: backend zwraca historię malejąco po `id`, czyli najnowsze rekordy są pierwsze. Dla wykresu czytelniejszy jest układ od starszych do nowszych.

## 9. Zgodność z obecnym backendem

Frontend jest dopasowany do API z pliku `api/app.py`:

```txt
GET /
GET /health
GET /measurements
GET /measurements/latest
GET /measurements/history
```

Dla historii używane są parametry:

```txt
device_id
sensor
limit
```

Typ rekordu jest zgodny z `measurement_to_dict` z backendu.

## 10. CORS i proxy

Frontend działa np. pod:

```txt
http://localhost:5173
```

Backend działa pod:

```txt
http://localhost:5001
```

Dla przeglądarki to różne originy. Jeśli Flask nie ma CORS, request może zostać zablokowany. Dlatego dashboard domyślnie wywołuje:

```txt
/api/health
```

A `server.mjs` przekazuje to do:

```txt
http://localhost:5001/health
```

Dla przeglądarki request nadal idzie do `localhost:5173`, więc CORS nie przeszkadza.

## 11. Jak uruchomić razem z backendem

Najpierw uruchom backend projektu, np. według dokumentacji API:

```bash
docker compose up -d --build
```

Sprawdź:

```bash
curl http://localhost:5001/health
```

Potem uruchom dashboard:

```bash
npm start
```

W UI zostaw `REST API URL` jako:

```txt
/api
```

Kliknij `Test API`.

## 12. Ograniczenia MVP

### Brak endpointu listy urządzeń

Obecny backend nie ma endpointu `/devices`. Dlatego lista Device jest budowana z rekordów, które frontend już widział. Gdy lista nie zawiera konkretnego urządzenia, można wybrać „wszystkie urządzenia” i filtrować tylko po sensorze.

### Brak endpointu listy sensorów

Analogicznie, nie ma `/sensors`. Frontend dodaje jednak domyślne sensory zgodne z projektem: `temperature`, `humidity`, `pressure`, `light`, a dodatkowo dokleja sensory znalezione w danych z API.

### Brak zapisu konfiguracji

Po odświeżeniu strony stan wraca do domyślnego. Można później dodać localStorage.

### Brak autoryzacji

MVP zakłada lokalne środowisko labowe. Nie ma loginu, tokenów ani ról.

### Jeden wykres, jedna seria

Aktualnie wykres pokazuje jedną serię `value`. Jeśli historia obejmie wiele sensorów, wartości zostaną pokazane na jednej linii. Najlepiej filtrować po jednym sensorze.

## 13. Proponowane rozszerzenia backendu

Najbardziej przydatne endpointy:

```txt
GET /devices
GET /sensors
GET /sensors?device_id=<id>
```

Przykładowa odpowiedź dla `/devices`:

```json
[
  "esp32-abcd01",
  "00004813-ad00-4f8c-8c4f-00ad13480000"
]
```

Przykładowa odpowiedź dla `/sensors`:

```json
[
  "temperature",
  "humidity",
  "pressure",
  "light"
]
```

Dzięki temu listy `select` będą kompletne od startu, zamiast być uzupełniane dopiero po pobraniu rekordów.

## 14. Proponowane rozszerzenia frontendu

### LocalStorage

Zapamiętywanie API URL, ostatniego device, sensora, limitu i interwału auto-refresh.

### Eksport CSV

Dodanie przycisku `Export CSV` dla danych historii.

### Tabela historii

Pod wykresem można dodać tabelę rekordów z kolumnami:

```txt
id, group_id, device_id, sensor, value, unit, ts_ms, seq, topic
```

### Multi-series chart

Jeśli użytkownik nie wybierze sensora, można rysować osobną linię dla każdego sensora.

### Tryb kiosk

Widok pełnoekranowy bez formularzy, tylko status, latest i wykres.

### WebSocket/SSE

Zamiast odpytywania REST co kilka sekund można dodać Server-Sent Events albo WebSocket. Wtedy dashboard reaguje natychmiast na nowe pomiary.

### Migracja na React

Jeśli dashboard zacznie rosnąć, można później przenieść renderowanie do Reacta. Obecna separacja `api/`, `lib/` i `state/` ułatwia taką migrację.

## 15. Styl i UX

Aplikacja celowo nie kopiuje topornego wyglądu LabVIEW. Zachowuje analogiczny układ i funkcje, ale używa nowoczesnego UI:

- duży nagłówek,
- karty z zaokrągleniami,
- czytelne pola,
- status API jako pill,
- metryki w kafelkach,
- panel komunikatów jako log zdarzeń,
- responsywny układ dla mniejszych ekranów.

## 16. Bezpieczeństwo

MVP nie powinien być wystawiany publicznie bez dodatkowych zabezpieczeń. Przed wdrożeniem poza lokalne środowisko warto dodać:

- autoryzację,
- HTTPS,
- ograniczenie CORS,
- rate limiting na API,
- walidację i limity parametrów po stronie backendu,
- reverse proxy.

## 17. Diagnostyka

### Sprawdzenie API

```bash
curl http://localhost:5001/health
curl http://localhost:5001/measurements/latest
curl "http://localhost:5001/measurements/history?limit=5"
```

### Sprawdzenie frontendu

```bash
npm start
```

### Przebudowanie TypeScriptu

```bash
npm install
npm run build
```

### Typowe błędy

#### Backend wyłączony

Objaw w UI:

```txt
Nie udało się połączyć z API
```

Rozwiązanie:

```bash
docker compose up -d --build
curl http://localhost:5001/health
```

#### Pusty wykres

Możliwe powody:

- brak danych w bazie,
- filtr Device nie pasuje do danych,
- filtr Sensor nie pasuje do danych,
- limit jest zbyt mały,
- backend zwraca pustą tablicę.

## 18. Główna ścieżka użytkownika

1. Uruchom backend.
2. Uruchom dashboard przez `npm start`.
3. Zostaw API URL jako `/api`.
4. Kliknij `Test API`.
5. Kliknij `Pobierz latest`.
6. Wybierz Device i Sensor z list.
7. Ustaw Limit.
8. Kliknij `Pobierz historię`.
9. Włącz `Auto Refresh`, jeśli dashboard ma odświeżać się sam.

## 19. Decyzje projektowe

### Dlaczego web, a nie Python GUI

Dla tego projektu GUI jest klientem REST API. Web daje łatwiejszy deployment, lepszy wygląd, prostsze wykresy, naturalny auto-refresh i wygodną komunikację HTTP.

Python GUI miałby sens, jeśli dashboard musiałby bezpośrednio komunikować się ze sprzętem albo działać jako aplikacja desktopowa offline.

### Dlaczego bez Reacta

MVP ma być maksymalnie painless. Brak Reacta oznacza mniej paczek, szybsze uruchamianie, prostszy debug i mniej abstrakcji dla małego panelu.

### Dlaczego reducer/state machine

Reducer dobrze modeluje stan operatorski GUI. Zmiany stanu są jawne, a akcje mają nazwy. To jest bliskie mentalnemu modelowi state machine z LabVIEW.

## 20. Szybki słownik pojęć

- **API URL** — bazowy adres REST API. W dev najlepiej `/api`.
- **Proxy** — mechanizm przekierowania `/api` do Flask API przez `server.mjs`.
- **Latest measurement** — najnowszy rekord z tabeli `measurements`.
- **History** — lista rekordów filtrowana przez `device_id`, `sensor`, `limit`.
- **Auto-refresh** — cykliczne odpytywanie API.
- **Reducer** — funkcja zmieniająca stan GUI na podstawie akcji.
- **SVG** — natywny format grafiki wektorowej użyty do wykresu.


## Motyw graficzny Y2K w wersji 0.1.2

Ta wersja przebudowuje samą prezentację GUI. Zachowana została ta sama struktura aplikacji i ten sam przepływ danych, ale arkusz `src/styles.css` nadaje interfejsowi styl retro-web/Y2K: ciemne panele, chromowane obramowania, neonowe akcenty, zieloną siatkę w tle, niebieski świecący wykres oraz dziennik zdarzeń w formie status-logu.

Zmiany są celowo ograniczone do frontendu. Nie trzeba zmieniać backendu, konfiguracji REST API ani sposobu uruchamiania. Jeśli chcesz wrócić do jaśniejszego motywu, wystarczy podmienić `src/styles.css` oraz `public/styles.css` na poprzednią wersję.

# Docker

Frontend jest przygotowany jako osobna usługa Dockerowa. Ma to sens w tym projekcie, bo pozostałe elementy systemu też są usługami: broker MQTT, baza danych, ingestor i REST API. Dashboard jest wtedy kolejnym kontenerem, który komunikuje się z API po wewnętrznej sieci Dockera.

## Pliki Dockerowe

### `Dockerfile`

Dockerfile jest dwuetapowy:

1. `build` na bazie `node:22-alpine`:
   - kopiuje `package.json` i `package-lock.json`,
   - uruchamia `npm ci`,
   - kopiuje `src/`, `public/`, `tsconfig.json`,
   - uruchamia `npm run build`, czyli kompiluje TypeScript do `public/assets/`.

2. `runtime` na bazie `node:22-alpine`:
   - zawiera tylko `public/`, `server.mjs` i `package.json`,
   - działa jako użytkownik `node`, nie jako `root`,
   - wystawia port `5173`,
   - ma `HEALTHCHECK`, który odpytuje stronę główną,
   - uruchamia `node server.mjs`.

Dzięki temu obraz runtime jest prosty i nie zawiera `src/`, `node_modules` ani narzędzi developerskich.

### `.dockerignore`

Wyklucza z kontekstu budowania między innymi:

- `node_modules`,
- logi npm,
- `.git`,
- pliki zip,
- cache i coverage.

To przyspiesza build i ogranicza przypadkowe kopiowanie śmieci do obrazu.

### `docker-compose.frontend.yml`

To minimalny compose tylko dla frontendu. Zakłada, że backend API w tej samej sieci Dockera nazywa się `api` i nasłuchuje na porcie `5001`.

```yaml
services:
  dashboard:
    build:
      context: .
      dockerfile: Dockerfile
    image: wsyst-dashboard-mvp:0.1.3-y2k
    container_name: wsyst-dashboard
    environment:
      PORT: "5173"
      API_TARGET: "http://api:5001"
    ports:
      - "5173:5173"
    restart: unless-stopped
```

## Jak działa proxy w Dockerze

Przeglądarka nie powinna bezpośrednio uderzać w `http://api:5001`, bo nazwa `api` istnieje tylko wewnątrz sieci Dockera. Dlatego w UI zostaje:

```txt
/api
```

Serwer frontendu dostaje request:

```txt
GET /api/measurements/history?...
```

Następnie `server.mjs` usuwa prefiks `/api` i wysyła request do:

```txt
http://api:5001/measurements/history?...
```

Z punktu widzenia przeglądarki cały frontend i API są pod jednym originem `http://localhost:5173`, więc nie trzeba rozwiązywać CORS po stronie backendu.

## Scenariusze uruchomienia

### 1. Frontend i API w jednym docker compose

Najwygodniejszy docelowy wariant:

```bash
docker compose -f docker-compose.frontend.yml up --build
```

W większym compose projektu sekcja `dashboard` może zostać po prostu doklejona obok usług `api`, `ingestor`, `broker` i `database`.

### 2. Frontend w Dockerze, backend na hoście

Wtedy `localhost` wewnątrz kontenera oznacza sam kontener, nie komputer hosta. Trzeba użyć `host.docker.internal`.

Docker Desktop:

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

### 3. Frontend bez Dockera

Nadal działa tak jak wcześniej:

```bash
npm start
```

W trybie lokalnym domyślny `API_TARGET` w `server.mjs` to `http://localhost:5001`.

## Najważniejsze zmienne

### `PORT`

Port, na którym działa serwer Node w kontenerze. Domyślnie:

```txt
5173
```

### `API_TARGET`

Adres backendu widziany przez kontener frontendu. Dla compose:

```txt
http://api:5001
```

Dla backendu uruchomionego lokalnie na hoście:

```txt
http://host.docker.internal:5001
```

Ważne: `API_TARGET` to konfiguracja serwera proxy. W polu `REST API URL` w aplikacji nadal najlepiej zostawiać `/api`.
