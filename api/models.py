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
