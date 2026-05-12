import { getHealth, getLatestMeasurement, getMeasurementHistory, getRecentMeasurements } from './api/client.js';
import type { Measurement } from './api/types.js';
import { formatNumber, formatTimestamp, shortId, compactUrl } from './lib/format.js';
import { initialState, reduceDashboard, type DashboardAction, type DashboardState } from './state/dashboardMachine.js';

const app = document.querySelector<HTMLDivElement>('#app');
if (app === null) throw new Error('Missing #app element');
const appRoot = app;

let state: DashboardState = initialState;
let latestMeasurement: Measurement | null = null;
let historyMeasurements: Measurement[] = [];
let recentMeasurements: Measurement[] = [];
let autoRefreshTimer: number | null = null;
let busy = false;
let latestError = '';
let historyError = '';

function dispatch(action: DashboardAction): void {
  state = reduceDashboard(state, action);
  if (action.type === 'SET_AUTO_REFRESH' || action.type === 'SET_REFRESH_INTERVAL') configureAutoRefresh();
  render();
}

function uniqueSorted(values: Array<string | null | undefined>): string[] {
  return [...new Set(values.map((value) => value?.trim()).filter((value): value is string => Boolean(value)))].sort((a, b) => a.localeCompare(b));
}

function sensorUnit(sensor: string, fallback?: string | null): string {
  const units: Record<string, string> = {
    temperature: '°C',
    humidity: '%',
    pressure: 'hPa',
    light: 'lx',
  };
  return units[sensor] ?? fallback ?? '';
}

function optionHtml(value: string, selectedValue: string): string {
  return `<option value="${escapeHtml(value)}" ${value === selectedValue ? 'selected' : ''}>${escapeHtml(value)}</option>`;
}

function allKnownMeasurements(): Measurement[] {
  return [...recentMeasurements, ...historyMeasurements, ...(latestMeasurement ? [latestMeasurement] : [])];
}

function connectionLabel(): string {
  return ({ idle: 'nie testowano', checking: 'sprawdzanie…', online: 'online', offline: 'offline' } as const)[state.connectionState];
}

function escapeHtml(value: string): string {
  return value
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#039;');
}

function renderChart(measurements: Measurement[]): string {
  const data = [...measurements].sort((a, b) => a.ts_ms - b.ts_ms);
  if (!data.length) return '<div class="empty-state">Brak historii dla podanych filtrów.</div>';

  const width = 900;
  const height = 320;
  const pad = { left: 58, right: 22, top: 20, bottom: 46 };
  const values = data.map((item) => item.value);
  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = max - min || 1;
  const innerWidth = width - pad.left - pad.right;
  const innerHeight = height - pad.top - pad.bottom;

  const points = data.map((item, index) => {
    const x = pad.left + (data.length === 1 ? innerWidth / 2 : (index / (data.length - 1)) * innerWidth);
    const y = pad.top + innerHeight - ((item.value - min) / range) * innerHeight;
    return { x, y, item, index };
  });

  const yTicks = Array.from({ length: 5 }, (_, index) => min + (range * index) / 4);
  const polyline = points.map((point) => `${point.x},${point.y}`).join(' ');
  const unit = sensorUnit(data[0].sensor, data.find((item) => item.unit)?.unit);

  return `
    <div class="chart-wrap">
      <svg class="chart" viewBox="0 0 ${width} ${height}" role="img" aria-label="Wykres historii pomiarów">
        ${yTicks.map((tick) => {
          const y = pad.top + innerHeight - ((tick - min) / range) * innerHeight;
          return `
            <line class="chart-grid" x1="${pad.left}" x2="${width - pad.right}" y1="${y}" y2="${y}" />
            <text class="chart-tick" x="${pad.left - 12}" y="${y + 4}" text-anchor="end">${formatNumber(tick)}</text>
          `;
        }).join('')}
        <line class="chart-axis" x1="${pad.left}" x2="${width - pad.right}" y1="${height - pad.bottom}" y2="${height - pad.bottom}" />
        <line class="chart-axis" x1="${pad.left}" x2="${pad.left}" y1="${pad.top}" y2="${height - pad.bottom}" />
        <polyline class="chart-line" points="${polyline}" fill="none" />
        ${points.map((point) => `
          <circle class="chart-dot" cx="${point.x}" cy="${point.y}" r="4">
            <title>${formatTimestamp(point.item.ts_ms)} • ${point.item.sensor}: ${formatNumber(point.item.value)} ${sensorUnit(point.item.sensor, point.item.unit)}</title>
          </circle>
        `).join('')}
        <text class="chart-label" x="${width / 2}" y="${height - 10}" text-anchor="middle">próbka</text>
        <text class="chart-label" x="18" y="${height / 2}" text-anchor="middle" transform="rotate(-90 18 ${height / 2})">${escapeHtml(unit || data[0].sensor)}</text>
      </svg>
    </div>`;
}

function renderLatest(): string {
  if (latestError) return `<div class="empty-state empty-state--error">${escapeHtml(latestError)}</div>`;
  if (!latestMeasurement) return '<div class="empty-state">Brak danych. Użyj „Pobierz latest” albo włącz auto-refresh.</div>';

  const item = latestMeasurement;
  return `
    <div class="measurement-grid">
      <div class="metric metric--primary"><span>Value</span><strong>${formatNumber(item.value)} ${escapeHtml(sensorUnit(item.sensor, item.unit))}</strong></div>
      <div class="metric"><span>Device ID</span><strong title="${escapeHtml(item.device_id)}">${escapeHtml(shortId(item.device_id))}</strong></div>
      <div class="metric"><span>Sensor</span><strong>${escapeHtml(item.sensor)}</strong></div>
      <div class="metric"><span>Timestamp</span><strong>${formatTimestamp(item.ts_ms)}</strong></div>
      <div class="metric"><span>Group</span><strong>${escapeHtml(item.group_id ?? '—')}</strong></div>
      <div class="metric"><span>Seq</span><strong>${item.seq ?? '—'}</strong></div>
    </div>`;
}

function render(): void {
  const known = allKnownMeasurements();
  const deviceOptions = uniqueSorted([...known.map((item) => item.device_id), state.deviceId]);
  const sensorOptions = uniqueSorted(['temperature', 'humidity', 'pressure', 'light', ...known.map((item) => item.sensor), state.sensor]);

  appRoot.innerHTML = `
    <section class="panel hero-panel portal-header">
      <div class="hero-copy">
        <p class="eyebrow">Data stream</p>
        <h1>Measurement Portal</h1>
        <p class="hero-description">Rozproszone Systemy Pomiarowe // REST API monitor // Y2K telemetry interface.</p>
      </div>
      <div class="toolbar-grid">
        <label class="field field--wide">
          <span>REST API URL</span>
          <input id="apiUrl" value="${escapeHtml(state.apiUrl)}" placeholder="/api albo http://localhost:5001" spellcheck="false" />
          <small>Aktualnie: ${escapeHtml(compactUrl(state.apiUrl || '/api'))}</small>
        </label>
        <div class="toolbar-actions">
          <button class="button button--secondary" id="testApi" ${busy ? 'disabled' : ''}>Test API</button>
          <button class="button" id="refreshAll" ${busy ? 'disabled' : ''}>Refresh</button>
        </div>
        <div class="status-row"><span>Status API</span><span class="status-pill status-pill--${state.connectionState}">${connectionLabel()}</span></div>
        <label class="toggle-row"><input type="checkbox" id="autoRefresh" ${state.autoRefresh ? 'checked' : ''} /><span>Auto Refresh</span></label>
        <label class="field field--compact"><span>Refresh Interval</span><input id="refreshInterval" type="number" min="1000" step="500" value="${state.refreshIntervalMs}" /><small>ms</small></label>
      </div>
    </section>

    <div class="dashboard-grid">
      <div class="left-column">
        <section class="panel form-panel">
          <div class="panel-heading"><div><p class="eyebrow">Konfiguracja i filtry</p><h2>Zakres odczytu</h2></div></div>
          <div class="form-stack">
            <label class="field"><span>Device</span><select id="deviceId"><option value="">wszystkie urządzenia</option>${deviceOptions.map((value) => optionHtml(value, state.deviceId)).join('')}</select></label>
            <label class="field"><span>Sensor</span><select id="sensor"><option value="">wszystkie sensory</option>${sensorOptions.map((value) => optionHtml(value, state.sensor)).join('')}</select></label>
            <label class="field field--number"><span>Limit</span><input id="limit" type="number" min="1" max="500" value="${state.limit}" /></label>
          </div>
          <div class="button-row">
            <button class="button button--secondary" id="fetchLatest" ${busy ? 'disabled' : ''}>Pobierz latest</button>
            <button class="button" id="fetchHistory" ${busy ? 'disabled' : ''}>Pobierz historię</button>
          </div>
        </section>

        <section class="panel latest-panel">
          <div class="panel-heading"><div><p class="eyebrow">Latest measurement</p><h2>Ostatni pomiar</h2></div>${busy ? '<span class="mini-loader">odczyt…</span>' : ''}</div>
          ${renderLatest()}
        </section>
      </div>

      <div class="right-column">
        <section class="panel chart-panel">
          <div class="panel-heading"><div><p class="eyebrow">HistoryGraph</p><h2>Historia pomiarów</h2></div><div class="chart-badge">${historyMeasurements.length} pkt</div></div>
          ${historyError ? `<div class="empty-state empty-state--error">${escapeHtml(historyError)}</div>` : renderChart(historyMeasurements)}
        </section>
        <section class="panel messages-panel">
          <div class="panel-heading"><div><p class="eyebrow">Komunikaty</p><h2>Dziennik zdarzeń</h2></div><button class="button button--ghost" id="clearMessages">Wyczyść</button></div>
          ${state.messages.length ? `<ul class="message-list">${state.messages.map((msg) => `<li class="message message--${msg.level}"><span>${formatTimestamp(msg.createdAt)}</span><p>${escapeHtml(msg.text)}</p></li>`).join('')}</ul>` : '<div class="empty-state">Brak komunikatów.</div>'}
        </section>
      </div>
    </div>

    <footer class="portal-footer" aria-label="Status stopka">
      <div><span>DATA STREAM</span><i></i></div>
      <div><span>SECURE LINK</span><b>▣</b></div>
      <div><span>v0.1.2-y2k</span></div>
    </footer>`;

  bindEvents();
}

function bindEvents(): void {
  byId<HTMLInputElement>('apiUrl').addEventListener('change', (event) => dispatch({ type: 'SET_API_URL', value: (event.target as HTMLInputElement).value }));
  byId<HTMLSelectElement>('deviceId').addEventListener('change', (event) => dispatch({ type: 'SET_DEVICE_ID', value: (event.target as HTMLSelectElement).value }));
  byId<HTMLSelectElement>('sensor').addEventListener('change', (event) => dispatch({ type: 'SET_SENSOR', value: (event.target as HTMLSelectElement).value }));
  byId<HTMLInputElement>('limit').addEventListener('change', (event) => dispatch({ type: 'SET_LIMIT', value: Number((event.target as HTMLInputElement).value) }));
  byId<HTMLInputElement>('refreshInterval').addEventListener('change', (event) => dispatch({ type: 'SET_REFRESH_INTERVAL', value: Number((event.target as HTMLInputElement).value) }));
  byId<HTMLInputElement>('autoRefresh').addEventListener('change', (event) => dispatch({ type: 'SET_AUTO_REFRESH', value: (event.target as HTMLInputElement).checked }));
  byId<HTMLButtonElement>('testApi').addEventListener('click', testApi);
  byId<HTMLButtonElement>('refreshAll').addEventListener('click', refreshAll);
  byId<HTMLButtonElement>('fetchLatest').addEventListener('click', fetchLatest);
  byId<HTMLButtonElement>('fetchHistory').addEventListener('click', fetchHistory);
  byId<HTMLButtonElement>('clearMessages').addEventListener('click', () => dispatch({ type: 'CLEAR_MESSAGES' }));
}

function byId<T extends HTMLElement>(id: string): T {
  const element = document.getElementById(id);
  if (!element) throw new Error(`Missing element #${id}`);
  return element as T;
}

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

async function fetchLatest(): Promise<void> {
  await runBusy(async () => {
    try {
      latestMeasurement = await getLatestForCurrentFilters();
      latestError = '';
      dispatch({ type: 'ADD_MESSAGE', level: 'success', text: 'Pobrano najnowszy pomiar.' });
    } catch (error) {
      latestError = error instanceof Error ? error.message : String(error);
      dispatch({ type: 'ADD_MESSAGE', level: 'error', text: latestError });
    }
  });
}

async function fetchRecent(): Promise<void> {
  try {
    recentMeasurements = await getRecentMeasurements(state.apiUrl);
  } catch {
    // To zapytanie służy tylko do podpowiedzi device/sensor, więc nie hałasujemy błędem przy starcie.
  }
}

async function fetchHistory(): Promise<void> {
  await runBusy(async () => {
    try {
      historyMeasurements = await getMeasurementHistory(state.apiUrl, { deviceId: state.deviceId, sensor: state.sensor, limit: state.limit });
      historyError = '';
      dispatch({ type: 'ADD_MESSAGE', level: 'success', text: `Pobrano historię: ${historyMeasurements.length} rekordów.` });
    } catch (error) {
      historyError = error instanceof Error ? error.message : String(error);
      dispatch({ type: 'ADD_MESSAGE', level: 'error', text: historyError });
    }
  });
}

async function refreshAll(): Promise<void> {
  await runBusy(async () => {
    await Promise.allSettled([
      getLatestForCurrentFilters().then((result) => { latestMeasurement = result; latestError = ''; }),
      getMeasurementHistory(state.apiUrl, { deviceId: state.deviceId, sensor: state.sensor, limit: state.limit }).then((result) => { historyMeasurements = result; historyError = ''; }),
      getRecentMeasurements(state.apiUrl).then((result) => { recentMeasurements = result; }),
    ]);
    dispatch({ type: 'ADD_MESSAGE', level: 'info', text: 'Refresh zakończony.' });
  });
}

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

void fetchRecent().finally(render);
render();
