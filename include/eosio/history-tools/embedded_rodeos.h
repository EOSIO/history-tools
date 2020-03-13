#pragma once

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rodeos_error_s        rodeos_error;
typedef struct rodeos_context_s      rodeos_context;
typedef struct rodeos_db_partition_s rodeos_db_partition;
typedef struct rodeos_db_snapshot_s  rodeos_db_snapshot;
typedef struct rodeos_filter_s       rodeos_filter;
typedef int                          rodeos_bool;

// Create an error object. If multiple threads use an error object, then they must synchronize it. Returns NULL on
// failure.
rodeos_error* rodeos_create_error();

// Destroy an error object. This is a no-op if error == NULL.
void rodeos_destroy_error(rodeos_error* error);

// Get last error in this object. Never returns NULL. The error object owns the returned string.
const char* rodeos_get_error(rodeos_error* error);

// Create a context. Returns NULL on failure.
rodeos_context* rodeos_create();

// Destroy a context. It is undefined behavior if the context is used between threads without synchronization, or if any
// partition objects or snapshot objects currently exist for this context. This is a no-op if context == NULL.
void rodeos_destroy(rodeos_context* context);

// Open database. num_threads is the target number of rocksdb background threads; use 0 for default. max_open_files is
// the max number of open rocksdb files; 0 to make this unlimited.
//
// It is undefined behavior if the context is used between threads without synchronization. Returns false on error.
rodeos_bool rodeos_open_db(rodeos_error* error, rodeos_context* context, const char* path,
                           rodeos_bool create_if_missing, int num_threads, int max_open_files);

// Create or open a database partition. It is undefined behavior if more than 1 partition is opened for a given prefix,
// if any partitions have overlapping prefixes, or if the context is used between threads without synchronization.
// Returns NULL on failure.
rodeos_db_partition* rodeos_create_partition(rodeos_error* error, rodeos_context* context, const char* prefix,
                                             uint32_t prefix_size);

// Destroy a partition. It is undefined behavior if the partition is used between threads without synchronization. This
// is a no-op if partition == NULL.
void rodeos_destroy_partition(rodeos_db_partition* partition);

// Create a database snapshot. Snapshots isolate changes from each other. All database reads and writes happen through
// snapshots. Snapshot objects may safely outlive partition objects.
//
// A single partition supports any number of simultaneous non-persistent snapshots, but only a single persistent
// snapshot at any time. persistent and non-persistent may coexist. Only persistent snapshots make permanent changes to
// the database.
//
// Each snapshot may be used by a different thread, even if they're created from a single partition.
//
// It is undefined behavior if more than 1 persistent snapshot exists on a partition, or if the partition is used
// between threads without synchronization. Returns NULL on failure.
rodeos_db_snapshot* rodeos_create_snapshot(rodeos_error* error, rodeos_db_partition* partition, rodeos_bool persistent);

// Destroy a snapshot. It is undefined behavior if the snapshot is used between threads without synchronization. This is
// a no-op if snapshot == NULL.
void rodeos_destroy_snapshot(rodeos_db_snapshot* snapshot);

// Refresh a snapshot so it may read recently-written database changes. This operation is invalid if the snapshot is
// persistent. It is undefined behavior if the snapshot is used between threads without synchronization.
rodeos_bool refresh_snapshot(rodeos_error* error, rodeos_db_snapshot* snapshot);

// Start writing a block. data must be the serialized `result` type defined by the state-history plugin's ABI. Currently
// only supports `get_blocks_result_v0`. It is undefined behavior if the snapshot is used between threads without
// synchronization.
rodeos_bool start_block(rodeos_error* error, rodeos_db_snapshot* snapshot, const char* data, uint64_t size);

// Finish writing a block. data must be the serialized `result` type defined by the state-history plugin's ABI.
// Currently only supports `get_blocks_result_v0`. If `force_write` is true, then the data will become immediately
// available to newly-created or newly-refreshed snapshots to read. If `force_write` is false, then the write may be
// delayed until a future end_block call. It is undefined behavior if the snapshot is used between threads without
// synchronization.
rodeos_bool end_block(rodeos_error* error, rodeos_db_snapshot* snapshot, const char* data, uint64_t size,
                      bool force_write);

// Write state-history deltas to a block. data must be the serialized `result` type defined by the state-history
// plugin's ABI. Currently only supports `get_blocks_result_v0`. If `shutdown` isn't null, then `write_deltas` may call
// it during long operations. If `shutdown` returns true, then `write_deltas` abandons the writes. If `write_deltas`
// returns false, the snapshot will be in an inconsistent state; call `start_block` to abandon the current write and
// start another. It is undefined behavior if the snapshot is used between threads without synchronization.
rodeos_bool write_deltas(rodeos_error* error, rodeos_db_snapshot* snapshot, const char* data, uint64_t size,
                         rodeos_bool (*shutdown)(void*), void* shutdown_arg);

// Create a filter. Returns NULL on failure.
rodeos_filter* rodeos_create_filter(rodeos_error* error, const char* wasm_filename);

// Destroy a filter. It is undefined behavior if the filter is used between threads without synchronization. This
// is a no-op if filter == NULL.
void rodeos_destroy_filter(rodeos_filter* filter);

#ifdef __cplusplus
}
#endif
