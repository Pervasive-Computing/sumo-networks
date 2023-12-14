use std::path::Path;

use axum::{
    http::StatusCode,
    routing::{get, post},
    Json, Router,
};
use serde::{Deserialize, Serialize};

// use tokio_rusqlite::{params, Connection, Result};

use toml;

use anyhow::Result;

#[derive(Clone)]
// struct AppState<'a> {
struct AppState {
    // db_connection: std::sync::Arc<rusqlite::Connection>,
    // db_connection: std::sync::RwLock<rusqlite::Connection>,
    db_connection: tokio_rusqlite::Connection,
    // db_connection: &'a rusqlite::Connection,
    // db_connection: &'a rusqlite::Connection,
}

#[derive(Deserialize, Debug)]
struct JsonRpcRequest {
    jsonrpc: String,
    method: String,
    params: Params,
    id: u64,
}

#[derive(Deserialize, Debug)]
struct Params {
    streetlamp: String,
    reducer: String,
    per: String,
    from: String,
    to: String,
}

#[derive(Serialize, Debug)]
struct JsonRpcResponse {
    jsonrpc: String,
    result: Option<serde_json::Value>,
    error: Option<JsonRpcError>,
    id: u64,
}

#[derive(Serialize, Debug)]
struct JsonRpcError {
    code: i32,
    message: String,
}



#[derive(Debug)]
struct LightLevelMeasurement {
    light_level: f64,
    timestamp: i64,
}

fn get_light_levels_between(
    conn: &tokio_rusqlite::Connection,
    streetlamp_id: &str,
    start_timestamp: i64,
    end_timestamp: i64,
) -> Result<Vec<LightLevelMeasurement>> {
    let mut stmt = conn.prepare(
        "SELECT light_level, timestamp FROM measurements
        WHERE streetlamp_id = ?1 AND timestamp BETWEEN ?2 AND ?3
        ORDER BY timestamp ASC",
    )?;

    let measurements = stmt.query_map(
        tokio_rusqlite::params![streetlamp_id, start_timestamp, end_timestamp],
        |row| {
            Ok(LightLevelMeasurement {
                light_level: row.get(0)?,
                timestamp: row.get(1)?,
            })
        },
    )?
    .collect::<Result<Vec<_>, _>>()?;

    Ok(measurements)
}

trait Reducer {
    fn reduce(&self, values: &[f64]) -> f64;
}

struct MeanReducer {}

impl Reducer for MeanReducer {
    fn reduce(&self, values: &[f64]) -> f64 {
        mean(values)
    }
}

struct MedianReducer {}

impl Reducer for MedianReducer {
    fn reduce(&self, values: &[f64]) -> f64 {
        median(values)
    }
}

fn mean(values: &[f64]) -> f64 {
    values.iter().sum::<f64>() / values.len() as f64
}

fn median(values: &[f64]) -> f64 {
    let mut sorted_values = values.to_vec();
    sorted_values.sort_by(|a, b| a.partial_cmp(b).unwrap());
    let mid = sorted_values.len() / 2;
    if sorted_values.len() % 2 == 0 {
        (sorted_values[mid] + sorted_values[mid - 1]) / 2.0
    } else {
        sorted_values[mid]
    }
}


#[tokio::main]
async fn main() -> Result<()> {
    let config_file_path = std::path::Path::new("config.toml");
    if !config_file_path.exists() {
        eprintln!("Config file '{}' not found", config_file_path.display());
        std::process::exit(1);
    }

    let config: toml::Value = toml::from_str(&std::fs::read_to_string(config_file_path)?)?;

    let port = config["port"].as_integer().unwrap_or(3000);
    let host = "localhost";
    let addr = dbg!(format!("{}:{}", host, port));

    let db_path = "./streetlamps.sqlite3";
    let db_connection = rusqlite::Connection::open(db_path).expect(format!("Failed to open database {}", db_path).as_str());
    // let shared_db_connection = std::sync::Arc::new(db_connection);
    let shared_db_connection = std::sync::RwLock::new(db_connection);

    // let measurements = get_light_levels_between(&shared_db_conn, "8918593277", 0, 1000000000000).unwrap();
    // for measurement in measurements {
    //     println!("Measurement: {:?}", measurement);
    // }

    // const auto	  sql_get_light_levels_between = R"(
    //     SELECT light_level, timestamp FROM measurements
    //     WHERE streetlamp_id = ? AND timestamp BETWEEN ? AND ?
    //     ORDER BY timestamp ASC;
    // )";

    let state = AppState {
        db_connection: shared_db_connection,
    };

    // build our application with a route
    let app = Router::new()
        .route("/streetlamps", get(streetlamps))
        .with_state(state);

    let listener = tokio::net::TcpListener::bind(addr).await.unwrap();
    axum::serve(listener, app).await.unwrap();

    Ok(())
}

// async fn streetlamps() -> &'static str {
//     println!("Streetlamps!");
//     "Hello, Streetlamps!"
// }

// async fn streetlamps(Json(payload): Json<JsonRpcRequest>) -> String {
//     // Process the `payload` here
//     println!("Received: {:?}", payload);

//     "Received your request".to_string()
// }

async fn streetlamps(Json(request): Json<JsonRpcRequest>) -> Json<JsonRpcResponse> {

}
