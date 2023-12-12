CREATE TABLE IF NOT EXISTS streetlamps (
    id INTEGER PRIMARY KEY,
    lon REAL,
    lat REAL
);

CREATE TABLE IF NOT EXISTS measurements (
    id INTEGER PRIMARY KEY,
    streetlamp_id INTEGER,
    timestamp INTEGER,
    value REAL,
    FOREIGN KEY (streetlamp_id) REFERENCES streetlamps(id)
);
