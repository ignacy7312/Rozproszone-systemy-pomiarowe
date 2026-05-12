export function formatNumber(value, digits = 2) {
    if (value === null || value === undefined || Number.isNaN(value))
        return '—';
    return new Intl.NumberFormat('pl-PL', { maximumFractionDigits: digits }).format(value);
}
export function formatTimestamp(tsMs) {
    if (!tsMs)
        return '—';
    const date = new Date(tsMs);
    if (Number.isNaN(date.getTime()))
        return String(tsMs);
    return new Intl.DateTimeFormat('pl-PL', {
        year: 'numeric', month: '2-digit', day: '2-digit',
        hour: '2-digit', minute: '2-digit', second: '2-digit',
    }).format(date);
}
export function shortId(value, head = 10, tail = 6) {
    if (!value)
        return '—';
    if (value.length <= head + tail + 3)
        return value;
    return `${value.slice(0, head)}…${value.slice(-tail)}`;
}
export function compactUrl(value) {
    return (value || '/api').replace(/^https?:\/\//, '').replace(/\/$/, '');
}
