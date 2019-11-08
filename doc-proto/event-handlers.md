

# Event Handlers

## Basic Event Hander

![Event Handler](event-handler.svg)

An event handler receives input events, may optionally read and write to a database, and may optionally create additional events. An event handler may look like the following:

```c++
class token_event_handler {
   public:
      [[eosio::action]]
      void open(
         names  action_senders,
         name   account,
         symbol token_type) {
            // open an account
      }

      [[eosio::action]]
      void transfer(
         names  action_senders,
         name   from,
         name   to,
         asset  amount,
         string memo) {
            // transfer tokens
      }
};
```

Event handlers don't have to be classes. The following also defines an event handler:

```c++
[[eosio::action]]
void open(
   names  action_senders,
   name   account,
   symbol token_type) {
      // open an account
}

[[eosio::action]]
void transfer(
   names  action_senders,
   name   from,
   name   to,
   asset  amount,
   string memo) {
      // transfer tokens
}
```

## Actions and Transactions

![Transaction](transaction.svg)

An `action` is an event which is part of a `transaction`. A transaction is atomic; either all actions succeed or the entire transaction fails. If a transaction fails, then the system rolls back any database changes the action handlers made.

Action handlers may produce additional actions during their execution:

```c++
[[eosio::handle_action]]
void foo(
   names  action_senders) {
      send_action(get_self(), "bar"_n, "arg0", 1, 2.1);
}

[[eosio::handle_action]]
void bar(
   names    action_senders,
   string   arg0,
   uint32_t arg1,
   double   arg2) {
      send_action(get_self(), "baz"_n);
}

[[eosio::handle_action]]
void baz(
   names    action_senders) {
}
```

These additional actions become part of the transaction. When this happens, the actions form a tree:

![Transaction Action Tree](transaction-action-tree.svg)

In this example, Action 0's handler produced Action 2 and Action 2's handler produced Action 3.

## Queries

