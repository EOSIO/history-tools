# Query performance

fill-postgresql creates primary keys, but doesn't create any indexes. It's up to you to design the optimal query and index combinations for your particular app.

# Helper Functions

Queries on this page use these functions:

```sql
create or replace function decode_int64(value bytea) returns bigint as $$
begin
    return
        (get_byte(value, 7)::bigint << 56) |
        (get_byte(value, 6)::bigint << 48) |
        (get_byte(value, 5)::bigint << 40) |
        (get_byte(value, 4)::bigint << 32) |
        (get_byte(value, 3)::bigint << 24) |
        (get_byte(value, 2)::bigint << 16) |
        (get_byte(value, 1)::bigint << 8) |
        (get_byte(value, 0)::bigint << 0);
end; $$
language plpgsql;

create or replace function decode_asset_precision(value bytea) returns int as $$
begin
    return ('x' || encode(substring(value from 9 for 1), 'hex'))::bit(8)::int;
end; $$
language plpgsql;

create or replace function decode_asset_name(value bytea) returns varchar as $$
begin
    return encode(trim('\x00'::bytea from substring(value from 10 for 7)), 'escape');
end; $$
language plpgsql;

create or replace function decode_asset(value bytea) returns varchar as $$
begin
    return
        left(decode_int64(value)::varchar, -decode_asset_precision(value)) || '.' ||
        right(decode_int64(value)::varchar, decode_asset_precision(value)) || ' ' ||
        decode_asset_name(value);
end; $$
language plpgsql;

create or replace function decode_asset_conditional(present boolean, value bytea) returns varchar as $$
begin
    if present then
        return decode_asset(value);
    else
        return '';
	end if;
end; $$
language plpgsql;
```

# Block, Transaction, and Action history

## Get current head and LIB

```
select * from chain.fill_status
```

## Get the 100 most-recent block ids

```
select * from chain.received_block order by block_index desc limit 100
```

## Get the 100 most-recent irreversible block ids

```
select * from chain.received_block where block_index <= (select irreversible from chain.fill_status) order by block_index desc limit 100
```

## Get the 10 most-recent producer schedule changes
```
select * from chain.block_info where new_producers!='{}' order by block_index desc limit 10
```

## Get the 100 most-recent executed transactions

```
select * from chain.transaction_trace where status='executed' order by block_index desc limit 100
```

## Get the 100 most-recent executed actions

```
select * from chain.action_trace where transaction_status='executed' order by block_index desc limit 100
```

## Get the 100 most-recent `eosio.token` transfers

```
select * from chain.action_trace where receipt_receiver='eosio.token' and account='eosio.token' and name='transfer' and transaction_status='executed' order by block_index desc limit 100
```

## Get the 100 most-recent `eosio.token` transfer notifications sent to `eosio.ramfee`

```
select * from chain.action_trace where receipt_receiver='eosio.ramfee' and account='eosio.token' and name='transfer' and transaction_status='executed' order by block_index desc limit 100
```

## Get the 100 most-recent actions authorized by `eosio`

```
select * from chain.transaction_trace left join chain.action_trace_authorization on transaction_trace.block_index=action_trace_authorization.block_index and transaction_trace.transaction_id=action_trace_authorization.transaction_id where actor='eosio' order by transaction_trace.block_index desc limit 100
```

# State History

## Get the 100 most-recent `eosio.ramfee` balance values

`primary_key=5459781` limits result to the `EOS` token

```
select *, decode_asset_conditional(present, value) from chain.contract_row where code='eosio.token' and scope='eosio.ramfee' and "table"='accounts' and primary_key=5459781 order by block_index desc limit 100
```
