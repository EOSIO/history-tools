# Event Handlers

## Basic Event Hander

![Event Handler](event-handler.svg)

An event handler receives input events, may optionally read and write to a database, and may optionally create additional events. An event handler may look like the following:

```c++
[[eosio::action]]
void open(
   context& c,
   name     owner,
   symbol   token_type) {
      // create a token balance record
}

[[eosio::action]]
void transfer(
   context& c,
   name     from,
   name     to,
   asset    amount,
   string   memo) {
      // modify token balances
}
```

Event handlers may optionally live in classes or structs:

```c++
struct token_contract {
   [[eosio::action]]
   void open(
      context& c,
      name     owner,
      symbol   token_type) {
         // create a token balance record
   }

   [[eosio::action]]
   void transfer(
      context& c,
      name     from,
      name     to,
      asset    amount,
      string   memo) {
         // modify token balances
   }
};
```

## Context Objects

Context objects describe the environment an event handler is executing in.

```c++
struct context {
   name self;
   bool is_in_contract;
   bool is_in_filter;
   bool is_in_query;
   bool is_action;
   bool database_is_writable;
   // ...
   bool has_auth(name account);
   name sender;
   // ...
};
```

## Actions and Transactions

![Transaction](transaction.svg)

An `action` is an event which is part of a `transaction`. A transaction is atomic; either all actions succeed or the entire transaction fails. If a transaction fails, then the system rolls back any database changes the action handlers made.

Action handlers may produce additional actions during their execution:

```c++
[[eosio::action]]
void foo(context& c) {
      send_action(c.self, "bar"_n, "arg0", 1, 2.1);
}

[[eosio::action]]
void bar(
   context& c,
   string   arg0,
   uint32_t arg1,
   double   arg2) {
      send_action(c.self, "baz"_n);
}

[[eosio::action]]
void baz(context& c) {
}
```

These additional actions become part of the transaction. When this happens, the actions form a tree:

![Transaction Action Tree](transaction-action-tree.svg)

In this example, Action 0's handler produced Action 2 and Action 2's handler produced Action 3.

## Queries

![Query Handler](query-handler.svg)

Query handlers receive query requests from an RPC API, query a database, and send results back to the API user. Unlike action handlers and filter handlers, query handlers may not modify databases.

```c++
[[eosio::query]]
vector<asset> get_balances(
   name   owner) {
      // fetch data from database and return result
}
```

## Filters

![Filter Handler](filter-handler.svg)

Filter handlers monitor actions and record additional data to their off-chain database. This
additional database isn't available to action handlers; off-chain databases may be created,
modified, and dropped without impacting chain operation. Unlike action handlers, filters may
not create additional events.

```c++
[[eosio::filter]]
void transfer(
   context& c,
   name   from,
   name   to,
   asset  amount,
   string memo) {
      // record transfer history
}
```
