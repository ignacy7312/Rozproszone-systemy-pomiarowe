export const initialState = {
    apiUrl: '/api',
    deviceId: '',
    sensor: '',
    limit: 20,
    autoRefresh: false,
    refreshIntervalMs: 5000,
    connectionState: 'idle',
    messages: [
        {
            id: crypto.randomUUID(),
            level: 'info',
            text: 'Dashboard gotowy. Domyślnie używa /api, czyli proxy Vite do http://localhost:5001.',
            createdAt: Date.now(),
        },
    ],
};
function withMessage(state, level, text) {
    return {
        ...state,
        messages: [{ id: crypto.randomUUID(), level, text, createdAt: Date.now() }, ...state.messages].slice(0, 12),
    };
}
export function reduceDashboard(state, action) {
    switch (action.type) {
        case 'SET_API_URL': return { ...state, apiUrl: action.value };
        case 'SET_DEVICE_ID': return { ...state, deviceId: action.value };
        case 'SET_SENSOR': return { ...state, sensor: action.value };
        case 'SET_LIMIT': return { ...state, limit: Math.max(1, Math.min(500, Math.trunc(action.value || 1))) };
        case 'SET_AUTO_REFRESH': return withMessage({ ...state, autoRefresh: action.value }, 'info', action.value ? 'Auto-refresh włączony.' : 'Auto-refresh wyłączony.');
        case 'SET_REFRESH_INTERVAL': return { ...state, refreshIntervalMs: Math.max(1000, Math.trunc(action.value || 1000)) };
        case 'CONNECTION_CHECKING': return { ...state, connectionState: 'checking' };
        case 'CONNECTION_ONLINE': return withMessage({ ...state, connectionState: 'online' }, 'success', 'API działa poprawnie.');
        case 'CONNECTION_OFFLINE': return withMessage({ ...state, connectionState: 'offline' }, 'error', 'API jest niedostępne albo zwróciło błąd.');
        case 'ADD_MESSAGE': return withMessage(state, action.level, action.text);
        case 'CLEAR_MESSAGES': return { ...state, messages: [] };
    }
}
