import logging
from flask import Flask, jsonify, request

from db import get_connection
from models import measurement_to_dict

app = Flask(__name__)
logging.basicConfig(level=logging.INFO, format='[api] %(message)s')
logger = logging.getLogger(__name__)


def fetch_measurements(query, params=None, fetch_one=False):
    params = params or []
    conn = get_connection()
    try:
        cur = conn.cursor()
        cur.execute(query, params)
        result = cur.fetchone() if fetch_one else cur.fetchall()
        cur.close()
        return result
    finally:
        conn.close()


@app.route('/', methods=['GET'])
def index():
    return jsonify({
        'service': 'measurements-api',
        'status': 'ok',
        'endpoints': [
            '/health',
            '/measurements',
            '/measurements/latest',
            '/measurements/history?device_id=<id>&sensor=<name>&limit=<n>',
        ]
    })


@app.route('/health', methods=['GET'])
def health():
    return jsonify({'status': 'ok'})


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


if __name__ == '__main__':
    logger.info('Starting Flask API on 0.0.0.0:5001')
    app.run(debug=True, host='0.0.0.0', port=5001)
