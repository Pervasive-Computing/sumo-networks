CREATE TABLE IF NOT EXISTS streetlamps (
    id TEXT PRIMARY KEY,
    lon REAL,
    lat REAL
);

CREATE TABLE IF NOT EXISTS measurements (
    id INTEGER PRIMARY KEY,
    streetlamp_id TEXT,
    timestamp INTEGER,
    light_level REAL,
    FOREIGN KEY (streetlamp_id) REFERENCES streetlamps(id)
);
