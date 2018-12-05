create index if not exists contract_row_code_table_primary_key_scope_block_index_prese_idx on chain.contract_row(
    code,
    "table",
    primary_key,
    scope,
    block_index desc,
    present desc
);

create index if not exists contract_row_code_table_scope_primary_key_block_index_prese_idx on chain.contract_row(
    code,
    "table",
    scope,
    primary_key,
    block_index desc,
    present desc
);

drop function if exists chain.contract_row_range_code_table_pk_scope;
create function chain.contract_row_range_code_table_pk_scope(
    max_block_index bigint,
    first_code varchar(13),
    first_table varchar(13),
    first_primary_key numeric,
    first_scope varchar(13),
    last_code varchar(13),
    last_table varchar(13),
    last_primary_key numeric,
    last_scope varchar(13),
    max_results integer
) returns setof chain.contract_row
as $$
    declare
        key_search record;
        block_search record;
		num_results integer = 0;
        found_key bool = false;
        found_block bool = false;
    begin
        if max_results <= 0 then
            return;
        end if;
        for key_search in
            select
                code, "table", primary_key, scope
            from
                chain.contract_row
            where
                (code, "table", primary_key, scope) >= (first_code, first_table, first_primary_key, first_scope)
            order by
                code,
                "table",
                primary_key,
                scope,
                block_index desc,
                present desc
            limit 1
        loop
            if (key_search.code, key_search."table", key_search.primary_key, key_search.scope) > (last_code, last_table, last_primary_key, last_scope) then
                return;
            end if;
            found_key = true;
            found_block = false;
            first_code = key_search.code;
            first_table = key_search."table";
            first_primary_key = key_search.primary_key;
            first_scope = key_search.scope;
            for block_search in
                select
                    *
                from
                    chain.contract_row
                where
                    contract_row.code = key_search.code
                    and contract_row."table" = key_search."table"
                    and contract_row.primary_key = key_search.primary_key
                    and contract_row.scope = key_search.scope
                    and contract_row.block_index <= max_block_index
                order by
                    code,
                    "table",
                    primary_key,
                    scope,
                    block_index desc,
                    present desc
                limit 1
            loop
                if block_search.present then
                    return next block_search;
                    num_results = num_results + 1;
                    found_block = true;
                end if;
            end loop;
            if not found_block then
                return next row(0::bigint, false, key_search.code::varchar(13), key_search.scope::varchar(13), key_search."table"::varchar(13), key_search.primary_key, ''::varchar(13), ''::bytea);
                num_results = num_results + 1;
            end if;
        end loop;

        loop
            exit when not found_key or num_results >= max_results;

            found_key = false;
            for key_search in
                select
                    code, "table", primary_key, scope
                from
                    chain.contract_row
                where
                    (code, "table", primary_key, scope) > (first_code, first_table, first_primary_key, first_scope)
                order by
                    code,
                    "table",
                    primary_key,
                    scope,
                    block_index desc,
                    present desc
                limit 1
            loop
                if (key_search.code, key_search."table", key_search.primary_key, key_search.scope) > (last_code, last_table, last_primary_key, last_scope) then
                    return;
                end if;
                found_key = true;
                found_block = false;
                first_code = key_search.code;
                first_table = key_search."table";
                first_primary_key = key_search.primary_key;
                first_scope = key_search.scope;
                for block_search in
                    select
                        *
                    from
                        chain.contract_row
                    where
                        contract_row.code = key_search.code
                        and contract_row."table" = key_search."table"
                        and contract_row.primary_key = key_search.primary_key
                        and contract_row.scope = key_search.scope
                        and contract_row.block_index <= max_block_index
                    order by
                        code,
                        "table",
                        primary_key,
                        scope,
                        block_index desc,
                        present desc
                    limit 1
                loop
                    if block_search.present then
                        return next block_search;
                        num_results = num_results + 1;
                        found_block = true;
                    end if;
                end loop;
                if not found_block then
                    return next row(0::bigint, false, key_search.code::varchar(13), key_search.scope::varchar(13), key_search."table"::varchar(13), key_search.primary_key, ''::varchar(13), ''::bytea);
                    num_results = num_results + 1;
                end if;
            end loop;
        end loop;
    end 
$$ language plpgsql;
