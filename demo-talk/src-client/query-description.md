## Query Description

To retrieve the first message, the client sends this query to the server:

```
[
    "get.messages",
    {
        "begin": {
            "parent_ids": [],
            "id": "0"
        },
        "max_messages": 10
    }
]
```

This query requests the first message with an id >= 0. The server returns the
first found message, along with the next 9 messages in thread order:

```
{
    "more": {...},
    "messages": [
        {
            "id": "100000",
            "reply_to": "0",
            "user": "sue",
            "content": "accusantium odit molestiae suscipit enim modi suscipit molestiae vel laboriosam, quis fugiat dolorem fugiat doloremque "
        },
        {
            "id": "100001",
            "reply_to": "0",
            "user": "joe",
            "content": "velit Ut molestiae "
        },
        {
            "id": "100002",
            "reply_to": "100001",
            "user": "bob",
            "content": "sit vitae voluptatem. rem illo eaque numquam "
        },
        ...
```

The `more` field has information that the server needs to resume the query:

```
    "more": {
        "parent_ids": [
            "100001", "100002", "100008", "100014", "100026", "100039", "100140", "100171", "100316"
        ],
        "id": "0"
    },
```

To continue the search, the client places the data in `more` into the `begin` field in the next query.

```
[
    "get.messages",
    {
        "begin": {
            "parent_ids": [
                "100001", "100002", "100008", "100014", "100026", "100039", "100140", "100171", "100316"
            ],
            "id": "0"
        },
        "max_messages": 10
    }
]
```
