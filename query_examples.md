# Query performance

fill-postgresql creates primary keys, but doesn't create any indexes. It's up to you to design the optimal query and index combinations for your particular app.

# Helper Functions

Queries on this page use these functions:

```sql
drop function if exists chain.int64_bin_to_value;
create function chain.int64_bin_to_value(value bytea, pos int) returns bigint as $$
begin
    return
        (get_byte(value, pos + 7)::bigint << 56) |
        (get_byte(value, pos + 6)::bigint << 48) |
        (get_byte(value, pos + 5)::bigint << 40) |
        (get_byte(value, pos + 4)::bigint << 32) |
        (get_byte(value, pos + 3)::bigint << 24) |
        (get_byte(value, pos + 2)::bigint << 16) |
        (get_byte(value, pos + 1)::bigint << 8) |
        (get_byte(value, pos + 0)::bigint << 0);
end; $$
language plpgsql;

drop function if exists chain.symbol_bin_to_precision;
create function chain.symbol_bin_to_precision(value bytea, pos int) returns int as $$
begin
    return ('x' || encode(substring(value from pos + 1 for 1), 'hex'))::bit(8)::int;
end; $$
language plpgsql;

drop function if exists chain.symbol_bin_to_name;
create function chain.symbol_bin_to_name(value bytea, pos int) returns varchar as $$
begin
    return encode(trim('\x00'::bytea from substring(value from pos + 2 for 7)), 'escape');
end; $$
language plpgsql;

drop function if exists chain.asset_bin_to_str;
create function chain.asset_bin_to_str(value bytea, pos int) returns varchar as $$
declare
    prec int     := chain.symbol_bin_to_precision(value, pos + 8);
    num  varchar := lpad(chain.int64_bin_to_value(value, pos)::varchar, prec + 1, '0');
begin
    return
        left(num, -prec) || '.' ||
        right(num, prec) || ' ' ||
        chain.symbol_bin_to_name(value, pos + 8);
end; $$
language plpgsql;

drop function if exists chain.conditional_asset_bin_to_str;
create function chain.conditional_asset_bin_to_str(present boolean, value bytea, pos int) returns varchar as $$
begin
    if present then
        return chain.asset_bin_to_str(value, pos);
    else
        return '';
    end if;
end; $$
language plpgsql;
```

# Block, Transaction, and Action history

## Get current head and LIB

```sql
select * from chain.fill_status;
```

## Get the 100 most-recent block ids

```sql
select 
  * 
from 
  chain.received_block 
order by 
  block_index desc 
limit 
  100;
```

## Get the 100 most-recent irreversible block ids

```sql
select 
  * 
from 
  chain.received_block 
where 
  block_index <= (select irreversible from chain.fill_status)
order by 
  block_index desc 
limit 
  100;
```

## Get the 10 most-recent producer schedule changes
```sql
select 
  * 
from 
  chain.block_info 
where 
  new_producers is not null 
order by 
  block_index desc 
limit 
  10;
```

## Get the 100 most-recent executed transactions

todo: add new column to fill-postgresql implementation to identify order of transactions within blocks. Adjust this query.

```sql
select 
  * 
from 
  chain.transaction_trace 
where 
  status = 'executed' 
order by 
  block_index desc 
limit 
  100;
```

## Get the 100 most-recent executed actions

todo: add new column to fill-postgresql implementation to identify order of transactions within blocks. Adjust this query.

```sql
select 
  * 
from 
  chain.action_trace 
where 
  transaction_status = 'executed' 
order by 
  block_index desc 
limit 
  100;
```

## Get the 100 most-recent `eosio.token` transfers

todo: add new column to fill-postgresql implementation to identify order of transactions within blocks. Adjust this query.

```sql
select 
  * 
from 
  chain.action_trace 
where 
  receipt_receiver = 'eosio.token' 
  and account = 'eosio.token' 
  and name = 'transfer' 
  and transaction_status = 'executed' 
order by 
  block_index desc 
limit 
  100;
```

## Get the 100 most-recent `eosio.token` transfer notifications sent to `eosio.ramfee`

todo: add new column to fill-postgresql implementation to identify order of transactions within blocks. Adjust this query.

```sql
select 
  * 
from 
  chain.action_trace 
where 
  receipt_receiver = 'eosio.ramfee' 
  and account = 'eosio.token' 
  and name = 'transfer' 
  and transaction_status = 'executed' 
order by 
  block_index desc 
limit 
  100;
```

## Get the 100 most-recent actions authorized by `eosio`

todo: add new column to fill-postgresql implementation to identify order of transactions within blocks. Adjust this query.

```sql
select 
  * 
from 
  chain.transaction_trace 
  left join chain.action_trace_authorization
    on transaction_trace.block_index = action_trace_authorization.block_index 
    and transaction_trace.transaction_id = action_trace_authorization.transaction_id 
where 
  actor = 'eosio' 
order by 
  transaction_trace.block_index desc 
limit 
  100;
```

# State History

## Get the 100 most-recent `eosio.ramfee` balance values

`primary_key=5459781` limits result to the `EOS` token

```sql
select 
  *, 
  chain.conditional_asset_bin_to_str(present, value, 0) 
from 
  chain.contract_row 
where 
  code = 'eosio.token' 
  and scope = 'eosio.ramfee' 
  and "table" = 'accounts' 
  and primary_key = 5459781 
order by 
  block_index desc 
limit 
  100;
```

## <a name="mo-simple"></a> Balance (EOS) of all accounts beginning with "mo", as of block 20500000 ("distinct on" method)

Use this index to speed up this query:

```sql
create index if not exists contract_row_code_table_scope_primary_key_block_index_prese_idx on chain.contract_row(code, "table", scope, primary_key, block_index, present);
```

```sql
select
    distinct on(code, "table", scope, primary_key)
    contract_row.*,
    chain.conditional_asset_bin_to_str(contract_row.present, contract_row.value, 0) 
from
    chain.contract_row
where
    block_index <= 20500000
    and code='eosio.token'
    and "table"='accounts'
    and scope>='mo'
    and scope<='mozzzzzzzzzz'
    and primary_key = 5459781
order by
    code,
    "table",
    scope,
    primary_key,
    block_index,
    present
desc;
```

`distinct on` only includes the first row in each set of rows with duplicate fields. The `order by` clause includes `block_index, present` in descending order to make sure the most-recent state is the one included.

## Balance (EOS) of all accounts beginning with "mo", as of block 20500000 (nested query method)

Use this index to speed up this query:

```sql
create index if not exists contract_row_code_table_scope_primary_key_block_index_prese_idx on chain.contract_row(code, "table", scope, primary_key, block_index, present);
```

```sql
select
    contract_row.*,
    chain.conditional_asset_bin_to_str(contract_row.present, contract_row.value, 0) 
from (select
        max(block_index) as block_index,
        (max(block_index*2 + present::int) & 1) = 1::bigint as present,
        code,
        "table",
        scope,
        primary_key
    from
        chain.contract_row
    where
        block_index <= 20500000
    group by
        code, "table", scope, primary_key
) as subquery
join chain.contract_row
    on  subquery.block_index = contract_row.block_index
    and subquery.present = contract_row.present
    and subquery.code = contract_row.code
    and subquery.scope = contract_row.scope
    and subquery."table" = contract_row."table"
    and subquery.primary_key = contract_row.primary_key
where
    subquery.code='eosio.token'
    and subquery."table"='accounts'
    and subquery.scope>='mo'
    and subquery.scope<='mozzzzzzzzzz'
    and subquery.primary_key = 5459781;
```

There's a lot going on here; let's build up a similar query.

`contract_row` contains a history of every row in every contract over time. e.g. here is part of the history of a well-known account's token balance:

```sql
select
    *,
    chain.conditional_asset_bin_to_str(present, value, 0)
from
    chain.contract_row
where
    code='eosio.token'
    and "table"='accounts'
    and scope='eosio'
    and primary_key = 5459781
    and block_index>=9378 and block_index<=200000
order by
    block_index;
```

```
 block_index | present |    code     | scope |  table   | primary_key |    payer     |               value                | conditional_asset_bin_to_str
-------------+---------+-------------+-------+----------+-------------+--------------+------------------------------------+------------------------------
        9378 | t       | eosio.token | eosio | accounts |     5459781 | eosio        | \xf0daa2a80700000004454f5300000000 | 3.2894 EOS
       11975 | t       | eosio.token | eosio | accounts |     5459781 | eosio        | \x7940aeaf0700000004454f5300000000 | 3.3012 EOS
       13076 | f       | eosio.token | eosio | accounts |     5459781 | eosio        | \x7940aeaf0700000004454f5300000000 |
      169740 | t       | eosio.token | eosio | accounts |     5459781 | hezdimjxgyge | \x0a0000000000000004454f5300000000 | 0.0010 EOS
      171012 | t       | eosio.token | eosio | accounts |     5459781 | hezdimjxgyge | \x0b0000000000000004454f5300000000 | 0.0011 EOS
```

`block_index` identifies a block where the row changed. `present` is `f` if the row was removed at that block.

Let's get the most-recent row as of block 200000:

```sql
select
    max(block_index) as block_index,
    (max(block_index*2 + present::int) & 1) = 1::bigint as present,
    code,
    "table",
    scope,
    primary_key
from
    chain.contract_row
where
    code='eosio.token'
    and "table"='accounts'
    and scope='eosio'
    and primary_key = 5459781
    and block_index <= 200000
group by
    code, "table", scope, primary_key;
```

```
 block_index | present |    code     |  table   | scope | primary_key
-------------+---------+-------------+----------+-------+-------------
      171012 | t       | eosio.token | accounts | eosio |     5459781
```

Since we're using `group by`, we can only select columns which appear in the `group by` clause. We can also select aggregation functions, such as `max`. The `max` for computing `present` treats it as a compound key. This handles the case where a single block removed then added a row; we want the added row in this case. To get the remaining fields, turn the above query into a subquery, join it with the original table, and move most of the `where` clause to the outer query.

```sql
select
    contract_row.*,
    chain.conditional_asset_bin_to_str(contract_row.present, contract_row.value, 0) 
from (select
        max(block_index) as block_index,
        (max(block_index*2 + present::int) & 1) = 1::bigint as present,
        code,
        "table",
        scope,
        primary_key
    from
        chain.contract_row
    where
        block_index <= 200000
    group by
        code, "table", scope, primary_key
) as subquery
join chain.contract_row
    on  subquery.block_index = contract_row.block_index
    and subquery.present = contract_row.present
    and subquery.code = contract_row.code
    and subquery.scope = contract_row.scope
    and subquery."table" = contract_row."table"
    and subquery.primary_key = contract_row.primary_key
where
    subquery.code='eosio.token'
    and subquery."table"='accounts'
    and subquery.scope='eosio'
    and subquery.primary_key = 5459781;
```

```
 block_index | present |    code     | scope |  table   | primary_key |    payer     |               value                | conditional_asset_bin_to_str
-------------+---------+-------------+-------+----------+-------------+--------------+------------------------------------+------------------------------
      171012 | t       | eosio.token | eosio | accounts |     5459781 | hezdimjxgyge | \x0b0000000000000004454f5300000000 | 0.0011 EOS
```
