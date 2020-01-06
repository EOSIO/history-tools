# History Tools

The history tools repo has these components:

* Database fillers connect to the nodeos state-history plugin and populate databases
* wasm-ql servers answer incoming queries by running server WASMs, which have read-only access to the databases
* The wasm-ql library, when combined with the CDT library, provides utilities that server WASMs and client WASMs need
* A set of example server WASMs and client WASMs

| App               | Fills RocksDB | wasm-ql with RocksDB       | Fills PostgreSQL | wasm-ql with PostgreSQL |
| ----------------- | ------------- | -------------------------- | ---------------- | ---------------- |
| `fill-rocksdb`    | Yes           |                            |                  |                  |        
| `wasm-ql-rocksdb` |               | Yes                        |                  |                  |            
| `combo-rocksdb`   | Yes           | Yes                        |                  |                  |            
| `fill-pg`         |               |                            | Yes              |                  |        
| `wasm-ql-pg`      |               |                            |                  | Yes              |            
| `history-tools`   | Yes*          | Yes*                       | Yes*             | Yes*             |            

Note: by default, `history-tools` does nothing; use the `--plugin` option to select plugins.
