## Data Description

The `talk` contract has this action:

```
void post(uint64_t id, uint64_t reply_to, eosio::name user, const std::string& content);
```

This action populates the `message` table:

```
struct [[eosio::table("message"), eosio::contract("talk")]] message {
    uint64_t    id       = {}; // Non-0
    uint64_t    reply_to = {}; // Non-0 if this is a reply
    eosio::name user     = {};
    talk_string content  = {};

    uint64_t primary_key() const { return id; }
    uint64_t get_reply_to() const { return reply_to; }
};
```

## Ordering issue

Each table row is a single message. Assume that `id` indicates creation order.

| id | reply_to | user | content |
| -- | -------- | ---- | ------- |
| 01 | 00       | Bob  | First Post |
| 02 | 00       | Jane | My First Post |
| 03 | 01       | Sue  | Reply to Bob's Post |
| 04 | 02       | Bob  | Reply to Jane's Post |
| 05 | 03       | John | Reply to Sue's Reply to Bob's Post |

The threaded order of this data is:
```
id: 01, reply_to: 00, user: Bob,   content: First Post
id: 03, reply_to: 01, user: Sue,   content: Reply to Bob's Post
id: 05, reply_to: 03, user: John,  content: Reply to Sue's Reply to Bob's Post
id: 02, reply_to: 00, user: Jane,  content: My First Post
id: 04, reply_to: 02, user: Bob,   content: Reply to Jane's Post
```

If a database index sorts the rows by `id` or by `reply_to`, the result won't be in threaded order.
If the client reorders the data, it may need to either fetch much more data than fits on screen,
or use many round-trips to the server, which then needs round trips to the database.

If the server reorders the data, then it can make round trips to the database, but doesn't need extra
round trips to the client. This reduces latency.

`talk-server.wasm`, which runs in `combo-lmdb`, reorders the data on demand by clients.
