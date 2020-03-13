#include <eosio/history-tools/embedded_rodeos.h>
#include <eosio/history-tools/rodeos.hpp>

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

struct rodeos_context_s : eosio::history_tools::rodeos_context {};

struct rodeos_db_partition_s : eosio::history_tools::rodeos_db_partition {
   using rodeos_db_partition::rodeos_db_partition;
};

struct rodeos_db_snapshot_s : eosio::history_tools::rodeos_db_snapshot {
   using rodeos_db_snapshot::rodeos_db_snapshot;
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
                                      rodeos_bool create_if_missing, int num_threads, int max_open_files) {
   return handle_exceptions(error, false, [&] {
      if (!context)
         return error->set("context is null");
      if (!path)
         return error->set("path is null");
      if (context->db)
         return error->set("a database is already open on this context");
      context->db = std::make_shared<chain_kv::database>(
            path, create_if_missing, num_threads ? std::make_optional(num_threads) : std::nullopt,
            max_open_files ? std::make_optional(max_open_files) : std::nullopt);
      return true;
   });
}

extern "C" rodeos_db_partition* rodeos_create_partition(rodeos_error* error, rodeos_context* context,
                                                        const char* prefix, uint32_t prefix_size) {
   return handle_exceptions(error, nullptr, [&]() -> rodeos_db_partition* {
      if (!context)
         return error->set("context is null"), nullptr;
      if (!prefix)
         return error->set("prefix is null"), nullptr;
      if (!context->db)
         return error->set("database wasn't opened"), nullptr;
      return std::make_unique<rodeos_db_partition>(context->db, std::vector<char>{ prefix, prefix + prefix_size })
            .release();
   });
}

extern "C" void rodeos_destroy_partition(rodeos_db_partition* partition) {
   std::unique_ptr<rodeos_db_partition>{ partition };
}

extern "C" rodeos_db_snapshot* rodeos_create_snapshot(rodeos_error* error, rodeos_db_partition* partition,
                                                      rodeos_bool persistent) {
   return handle_exceptions(error, nullptr, [&]() -> rodeos_db_snapshot* {
      if (!partition)
         return error->set("partition is null"), nullptr;
      return std::make_unique<rodeos_db_snapshot>(*partition, persistent).release();
   });
}

extern "C" void rodeos_destroy_snapshot(rodeos_db_snapshot* snapshot) {
   std::unique_ptr<rodeos_db_snapshot>{ snapshot };
}

extern "C" rodeos_bool refresh_snapshot(rodeos_error* error, rodeos_db_snapshot* snapshot) {
   return handle_exceptions(error, false, [&]() {
      if (!snapshot)
         return error->set("snapshot is null");
      snapshot->refresh();
      return true;
   });
}

template <typename F>
void with_result(const char* data, uint64_t size, F f) {
   eosio::input_stream          bin{ data, data + size };
   eosio::ship_protocol::result result;
   eosio::check_discard(from_bin(result, bin));
   auto* result_v0 = std::get_if<eosio::ship_protocol::get_blocks_result_v0>(&result);
   if (!result_v0)
      throw std::runtime_error("expected a get_blocks_result_v0");
   f(*result_v0);
}

extern "C" rodeos_bool start_block(rodeos_error* error, rodeos_db_snapshot* snapshot, const char* data, uint64_t size) {
   return handle_exceptions(error, false, [&]() {
      if (!snapshot)
         return error->set("snapshot is null");
      with_result(data, size, [&](auto& result) { snapshot->start_block(result); });
      return true;
   });
}

extern "C" rodeos_bool end_block(rodeos_error* error, rodeos_db_snapshot* snapshot, const char* data, uint64_t size,
                                 bool force_write) {
   return handle_exceptions(error, false, [&]() {
      if (!snapshot)
         return error->set("snapshot is null");
      with_result(data, size, [&](auto& result) { snapshot->end_block(result, force_write); });
      return true;
   });
}

extern "C" rodeos_bool write_deltas(rodeos_error* error, rodeos_db_snapshot* snapshot, const char* data, uint64_t size,
                                    rodeos_bool (*shutdown)(void*), void* shutdown_arg) {
   return handle_exceptions(error, false, [&]() {
      if (!snapshot)
         return error->set("snapshot is null");
      with_result(data, size, [&](auto& result) {
         snapshot->write_deltas(result, [=]() -> bool {
            if (shutdown)
               return shutdown(shutdown_arg);
            else
               return false;
         });
      });
      return true;
   });
}
