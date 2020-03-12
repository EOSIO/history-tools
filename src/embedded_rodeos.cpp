#include <eosio/history-tools/embedded_rodeos.h>

#include <eosio/history-tools/rodeos.hpp>

#include <memory>
#include <string>

namespace ht = eosio::history_tools;

struct rodeos_error_s {
   const char* msg = "no error";
   std::string buffer;

   bool set(const char* m) {
      try {
         buffer = m;
         msg    = buffer.c_str();
      } catch (...) { msg = "error storing error message"; }
      return false;
   }
};

struct rodeos_context_s {
   ht::rodeos_context obj;
};

struct rodeos_db_partition_s {
   ht::rodeos_db_partition obj;
};

struct rodeos_db_snapshot_s {
   ht::rodeos_db_snapshot obj;
};

extern "C" rodeos_error* rodeos_create_error() {
   try {
      return std::make_unique<rodeos_error>().release();
   } catch (...) { return nullptr; }
}

extern "C" void rodeos_destroy_error(rodeos_error* error) { std::unique_ptr<rodeos_error>{ error }; }

extern "C" const char* rodeos_get_error(rodeos_error* error) {
   if (!error)
      return "no error";
   return error->msg;
}

template <typename T, typename F>
auto handle_exceptions(rodeos_error* error, T errval, F f) noexcept -> decltype(f()) {
   if (!error)
      return errval;
   try {
      return f();
   } catch (std::exception& e) {
      error->set(e.what());
      return errval;
   } catch (...) {
      error->set("unknown exception");
      return errval;
   }
}

extern "C" rodeos_context* rodeos_create() {
   try {
      return std::make_unique<rodeos_context>().release();
   } catch (...) { return nullptr; }
}

extern "C" void rodeos_destroy(rodeos_context* context) { std::unique_ptr<rodeos_context>{ context }; }

extern "C" rodeos_bool rodeos_open_db(rodeos_error* error, rodeos_context* context, const char* path,
                                      rodeos_bool create_if_missing, int* threads, int* max_open_files) {
   return handle_exceptions(error, false, [&] {
      if (!context)
         return error->set("context is null");
      if (!path)
         return error->set("path is null");
      if (context->obj.db)
         return error->set("a database is already open on this context");
      context->obj.db = std::make_shared<chain_kv::database>(
            path, create_if_missing, threads ? std::make_optional(*threads) : std::nullopt,
            max_open_files ? std::make_optional(*max_open_files) : std::nullopt);
      return true;
   });
}

// Create or open a database partition. It is undefined behavior if more than 1 partition is opened for a given prefix,
// if any partitions have overlapping prefixes, or if any other threads are calling functions with the same context
// during this call. Returns NULL on failure.
extern "C" rodeos_db_partition* rodeos_create_partition(rodeos_error* error, rodeos_context* context,
                                                        const char* prefix, uint32_t prefix_size);

// Destroy a partition. It is undefined behavior if any other threads are calling functions with the same partition
// during this call. This is a no-op if partition == NULL.
extern "C" void rodeos_destroy_partition(rodeos_db_partition* partition);

// Create a database snapshot. Snapshots isolate changes from each other. All database reads and writes happen through
// snapshots. Snapshot objects may safely outlive partition objects.
//
// A single partition supports any number of simultaneous snapshots, but see the restrictions on ********
//
// Each snapshot may be used by a different thread, even if they're created from a single partition.
//
// It is undefined behavior if any other threads are calling functions with the same partition during this call. Returns
// NULL on failure.
extern "C" rodeos_db_snapshot* rodeos_create_snapshot(rodeos_db_partition* partition);

// Destroy a snapshot. It is undefined behavior if any other threads are calling functions with the same snapshot during
// this call. This is a no-op if snapshot == NULL.
extern "C" void rodeos_destroy_snapshot(rodeos_db_snapshot* snapshot);
