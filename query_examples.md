# Query performance

fill-postgresql creates primary keys, but doesn't create any indexes. It's up to you to design the optimal query and index combinations for your particular app.

# Helper Functions

Queries on this page use these functions:

```sql
drop function if exists int64_bin_to_value;
create function int64_bin_to_value(value bytea, pos int) returns bigint as $$
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

drop function if exists symbol_bin_to_precision;
create function symbol_bin_to_precision(value bytea, pos int) returns int as $$
begin
    return ('x' || encode(substring(value from pos + 1 for 1), 'hex'))::bit(8)::int;
end; $$
language plpgsql;

drop function if exists symbol_bin_to_name;
create function symbol_bin_to_name(value bytea, pos int) returns varchar as $$
begin
    return encode(trim('\x00'::bytea from substring(value from pos + 2 for 7)), 'escape');
end; $$
language plpgsql;

drop function if exists asset_bin_to_str;
create function asset_bin_to_str(value bytea, pos int) returns varchar as $$
begin
    return
        left(int64_bin_to_value(value, pos)::varchar, -symbol_bin_to_precision(value, pos + 8)) || '.' ||
        right(int64_bin_to_value(value, pos)::varchar, symbol_bin_to_precision(value, pos + 8)) || ' ' ||
        symbol_bin_to_name(value, pos + 8);
end; $$
language plpgsql;

drop function if exists conditional_asset_bin_to_str;
create function conditional_asset_bin_to_str(present boolean, value bytea, pos int) returns varchar as $$
begin
    if present then
        return asset_bin_to_str(value, pos);
    else
        return '';
    end if;
end; $$
language plpgsql;
```

# Block, Transaction, and Action history

## Get current head and LIB

```sql
select * from chain.fill_status
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
  100
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
  100
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
  10
```

## Get the 100 most-recent executed transactions

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
  100
```

## Get the 100 most-recent executed actions

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
  100
```

## Get the 100 most-recent `eosio.token` transfers

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
  100
```

## Get the 100 most-recent `eosio.token` transfer notifications sent to `eosio.ramfee`

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
  100
```

## Get the 100 most-recent actions authorized by `eosio`

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
  100
```

# State History

## Get the 100 most-recent `eosio.ramfee` balance values

`primary_key=5459781` limits result to the `EOS` token

```sql
select 
  *, 
  conditional_asset_bin_to_str(present, value, 0) 
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
  100
```
