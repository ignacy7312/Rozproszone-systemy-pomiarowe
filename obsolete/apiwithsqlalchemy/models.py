def measurement_to_dict(row):
    if hasattr(row, 'keys'):
        return {
            'id': row['id'],
            'group_id': row['group_id'],
            'device_id': row['device_id'],
            'sensor': row['sensor'],
            'value': row['value'],
            'unit': row['unit'],
            'ts_ms': row['ts_ms'],
            'seq': row['seq'],
            'topic': row['topic'],
        }

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
