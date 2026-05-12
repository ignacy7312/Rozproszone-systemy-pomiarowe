export class ApiError extends Error {
    status;
    details;
    constructor(message, status, details) {
        super(message);
        this.name = 'ApiError';
        this.status = status;
        this.details = details;
    }
}
function normalizeBaseUrl(baseUrl) {
    const trimmed = baseUrl.trim();
    return (trimmed || '/api').replace(/\/$/, '');
}
function buildUrl(baseUrl, path, params) {
    const root = normalizeBaseUrl(baseUrl);
    const normalizedPath = path.startsWith('/') ? path : `/${path}`;
    const search = new URLSearchParams();
    Object.entries(params ?? {}).forEach(([key, value]) => {
        if (value !== undefined && value !== '')
            search.set(key, String(value));
    });
    const query = search.toString();
    return `${root}${normalizedPath}${query ? `?${query}` : ''}`;
}
async function requestJson(url, signal) {
    let response;
    try {
        response = await fetch(url, { headers: { Accept: 'application/json' }, signal });
    }
    catch (error) {
        throw new ApiError('Nie udało się połączyć z API. Sprawdź backend, URL, proxy Vite albo CORS.', undefined, error);
    }
    const contentType = response.headers.get('content-type') ?? '';
    const body = contentType.includes('application/json') ? await response.json().catch(() => null) : await response.text();
    if (!response.ok) {
        const message = typeof body === 'object' && body && 'message' in body
            ? String(body.message)
            : `API zwróciło błąd HTTP ${response.status}`;
        throw new ApiError(message, response.status, body);
    }
    return body;
}
export function getHealth(baseUrl, signal) {
    return requestJson(buildUrl(baseUrl, '/health'), signal);
}
export function getLatestMeasurement(baseUrl, signal) {
    return requestJson(buildUrl(baseUrl, '/measurements/latest'), signal);
}
export function getRecentMeasurements(baseUrl, signal) {
    return requestJson(buildUrl(baseUrl, '/measurements'), signal);
}
export function getMeasurementHistory(baseUrl, params, signal) {
    return requestJson(buildUrl(baseUrl, '/measurements/history', {
        device_id: params.deviceId,
        sensor: params.sensor,
        limit: params.limit ?? 20,
    }), signal);
}
