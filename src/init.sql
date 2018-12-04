create index if not exists contract_row_code_table_primary_key_scope_block_index_prese_idx on chain.contract_row(
    code,
    "table",
    primary_key,
    scope,
    block_index desc,
    present desc
);

drop function if exists chain.contract_row_range_scope;
create function chain.contract_row_range_scope(
    max_block_index bigint,
    code varchar(13),
    min_scope varchar(13),
    max_scope varchar(13),
    "table" varchar(13),
    primary_key numeric,
    max_results integer
) returns setof chain.contract_row
as $$
    #variable_conflict use_variable
    declare
        key_search record;
        block_search record;
		num_results integer = 0;
        found_key bool = false;
    begin
        for key_search in
            select
                contract_row.scope
            from
                chain.contract_row
            where
                contract_row.code = code
                and contract_row."table" = "table"
                and contract_row.primary_key = primary_key
                and contract_row.scope >= min_scope
                and contract_row.scope <= max_scope
            order by
                code,
                "table",
                primary_key,
                scope,
                block_index desc,
                present desc
            limit 1
        loop
            found_key = true;
            min_scope = key_search.scope;
            for block_search in
                select
                    *
                from
                    chain.contract_row
                where
                    contract_row.code = code
                    and contract_row."table" = "table"
                    and contract_row.primary_key = primary_key
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
                return next block_search;
                num_results = num_results + 1;
            end loop;
        end loop;

        loop
            exit when not found_key or num_results >= max_results;

            found_key = false;
            for key_search in
                select
                    contract_row.scope
                from
                    chain.contract_row
                where
                    contract_row.code = code
                    and contract_row."table" = "table"
                    and contract_row.primary_key = primary_key
                    and contract_row.scope > min_scope
                    and contract_row.scope <= max_scope
                order by
                    code,
                    "table",
                    primary_key,
                    scope,
                    block_index desc,
                    present desc
                limit 1
            loop
                found_key = true;
                min_scope = key_search.scope;
                for block_search in
                    select
                        *
                    from
                        chain.contract_row
                    where
                        contract_row.code = code
                        and contract_row."table" = "table"
                        and contract_row.primary_key = primary_key
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
                    return next block_search;
                    num_results = num_results + 1;
                end loop;
            end loop;
        end loop;
    end 
$$ language plpgsql;
