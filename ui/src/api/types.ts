export type ApiHealth = {
  status?: string;
  service?: string;
  endpoints?: string[];
};

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

export type HistoryParams = {
  deviceId?: string;
  sensor?: string;
  limit?: number;
};
