use std::collections::BTreeMap;
use std::path::Path;

use anyhow::Result;
use env_logger::fmt::Timestamp;
use serde::{Deserialize, Serialize};
// use zmq;

// {
//     timestamp: 1234567890,
//     changes: {
//         1: 0.5,
//         2: 0.6,
//         3: 0.7,
//     }
// }

#[derive(Debug, Deserialize, Serialize)]
struct Message {
    timestamp: i64,
    changes: BTreeMap<i64, f32>,
}

fn main() -> Result<()> {
    env_logger::init();

    let context = zmq::Context::new();
    let subscriber_socket = context
        .socket(zmq::SUB)
        .expect("Failed to create ZeroMQ SUB socket");
    let host = "localhost";
    let port: u16 = 12333;
    let url = format!("tcp://{}:{}", host, port);
    let topic = b"light_level";
    subscriber_socket
        .connect(&url)
        .expect("Failed to connect to ZeroMQ SUB socket");
    subscriber_socket
        .set_subscribe(topic)
        .expect("Failed to set subscription on ZeroMQ SUB socket");
    log::info!("Connected to {}", url);

    let db_file_path = Path::new("streetlamps.sqlite3");
    if !db_file_path.exists() {
        return Err(anyhow::anyhow!(
            "Database file {} does not exist",
            db_file_path.display()
        ));
    }
    let connection = rusqlite::Connection::open(db_file_path)
        .unwrap_or_else(|_| panic!("Failed to open {}", db_file_path.display()));

    let query = "INSERT INTO measurements (streetlamp_id, timestamp, light_level) VALUES (:streetlamp_id, :timestamp, :light_level)";
    // let insert_statement = connection
    //     .prepare(query)
    //     .expect("Failed to prepare SQL statement");

    log::info!("Connected to {}", db_file_path.display());

    loop {
        let message = subscriber_socket.recv_bytes(0).unwrap();
        let payload = &message[topic.len()..]; // Remove the topic from the message
        let message: Message =
            serde_cbor::from_slice(payload).expect("Failed to decode CBOR message");
        log::info!(
            "Received message with timestamp {} (datetime: {}) and {} changes",
            message.timestamp,
            chrono::NaiveDateTime::from_timestamp_opt(message.timestamp, 0).unwrap(),
            message.changes.len()
        );

        // TODO: figure out how to use a prepared statement here with transaction
        let mut insert_statement = connection.prepare_cached(query)?;
        // let mut transaction = connection.transaction()?;
        connection.execute_batch("BEGIN")?;
        for (streetlamp_id, light_level) in message.changes {
            insert_statement
                .execute(rusqlite::named_params! {
                ":streetlamp_id": streetlamp_id.to_string(),
                ":timestamp": message.timestamp,
                ":light_level": light_level,})
                .expect("Failed to execute SQL statement");
        }
        connection.execute_batch("COMMIT")?;
        // transaction.commit()?;
    }

    Ok(())
}
