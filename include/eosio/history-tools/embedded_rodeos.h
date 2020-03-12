#pragma once

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rodeos_error_s        rodeos_error;
typedef struct rodeos_context_s      rodeos_context;
typedef struct rodeos_db_partition_s rodeos_db_partition;
typedef struct rodeos_db_snapshot_s  rodeos_db_snapshot;
typedef int                          rodeos_bool;

// Create an error object. A single error object may be used by only 1 thread at a time. Returns NULL on failure.
rodeos_error* rodeos_create_error();

// Destroy an error object. This is a no-op if error == NULL.
void rodeos_destroy_error(rodeos_error* error);

// Get last error in this object. Never returns NULL. The error object owns the returned string.
const char* rodeos_get_error(rodeos_error* error);

// Create a context. Returns NULL on failure.
rodeos_context* rodeos_create();

// Destroy a context. It is undefined behavior if any other threads are calling functions with the same context during
// this call. This is a no-op if context == NULL.
void rodeos_destroy(rodeos_context* context);

// Open database. It is undefined behavior if any other threads are calling functions with the same context
// during this call. If threads is non-null, then it specifies the target number of rocksdb background threads.
// If max_open_files is non-null, then it specifies the max number of rocksdb files; either null or -1 is unlimited.
//
// Returns false on error.
rodeos_bool rodeos_open_db(rodeos_error* error, rodeos_context* context, const char* path,
                           rodeos_bool create_if_missing, int* threads, int* max_open_files);

// Create or open a database partition. It is undefined behavior if more than 1 partition is opened for a given prefix,
// if any partitions have overlapping prefixes, or if any other threads are calling functions with the same context
// during this call. Returns NULL on failure.
rodeos_db_partition* rodeos_create_partition(rodeos_error* error, rodeos_context* context, const char* prefix,
                                             uint32_t prefix_size);

// Destroy a partition. It is undefined behavior if any other threads are calling functions with the same partition
// during this call. This is a no-op if partition == NULL.
void rodeos_destroy_partition(rodeos_db_partition* partition);

// Create a database snapshot. Snapshots isolate changes from each other. All database reads and writes happen through
// snapshots. Snapshot objects may safely outlive partition objects.
//
// A single partition supports any number of simultaneous snapshots, but see the restrictions on ********
//
// Each snapshot may be used by a different thread, even if they're created from a single partition.
//
// It is undefined behavior if any other threads are calling functions with the same partition during this call. Returns
// NULL on failure.
rodeos_db_snapshot* rodeos_create_snapshot(rodeos_db_partition* partition);

// Destroy a snapshot. It is undefined behavior if any other threads are calling functions with the same snapshot during
// this call. This is a no-op if snapshot == NULL.
void rodeos_destroy_snapshot(rodeos_db_snapshot* snapshot);

#ifdef __cplusplus
}
#endif
