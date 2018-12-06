
        create index if not exists contract_row_code_table_primary_key_scope_block_index_prese_idx on chain.contract_row(
            "code",
            "table",
            "primary_key",
            "scope",
            block_index desc,
            present desc
        );
    
        create index if not exists contract_row_code_table_scope_primary_key_block_index_prese_idx on chain.contract_row(
            "code",
            "table",
            "scope",
            "primary_key",
            block_index desc,
            present desc
        );
    
        create index if not exists contract_row_scope_table_primary_key_code_block_index_prese_idx on chain.contract_row(
            "scope",
            "table",
            "primary_key",
            "code",
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
                        "code", "table", "primary_key", "scope"
                    from
                        chain.contract_row
                    where
                        ("code", "table", "primary_key", "scope") >= ("first_code", "first_table", "first_primary_key", "first_scope")
                    order by
                        "code",
                        "table",
                        "primary_key",
                        "scope",
                        block_index desc,
                        present desc
                    limit 1
                loop
                    if (key_search."code", key_search."table", key_search."primary_key", key_search."scope") > (last_code, last_table, last_primary_key, last_scope) then
                        return;
                    end if;
                    found_key = true;
                    found_block = false;
                    first_code = key_search."code";
                    first_table = key_search."table";
                    first_primary_key = key_search."primary_key";
                    first_scope = key_search."scope";
                    for block_search in
                        select
                            *
                        from
                            chain.contract_row
                        where
                            contract_row."code" = key_search."code"
                            and contract_row."table" = key_search."table"
                            and contract_row."primary_key" = key_search."primary_key"
                            and contract_row."scope" = key_search."scope"
                            and contract_row.block_index <= max_block_index
                        order by
                            "code",
                            "table",
                            "primary_key",
                            "scope",
                            block_index desc,
                            present desc
                        limit 1
                    loop
                        if block_search.present then
                            return next block_search;
                        else
                            return next row(block_search.block_index, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::numeric, ''::varchar(13), ''::bytea);
                        end if;
                        num_results = num_results + 1;
                        found_block = true;
                    end loop;
                    if not found_block then
                        return next row(0::bigint, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::numeric, ''::varchar(13), ''::bytea);
                        num_results = num_results + 1;
                    end if;
                end loop;

                loop
                    exit when not found_key or num_results >= max_results;

                    found_key = false;
                    for key_search in
                        select
                            "code", "table", "primary_key", "scope"
                        from
                            chain.contract_row
                        where
                            ("code", "table", "primary_key", "scope") > ("first_code", "first_table", "first_primary_key", "first_scope")
                        order by
                            "code",
                            "table",
                            "primary_key",
                            "scope",
                            block_index desc,
                            present desc
                        limit 1
                    loop
                        if (key_search."code", key_search."table", key_search."primary_key", key_search."scope") > (last_code, last_table, last_primary_key, last_scope) then
                            return;
                        end if;
                        found_key = true;
                        found_block = false;
                        first_code = key_search."code";
                        first_table = key_search."table";
                        first_primary_key = key_search."primary_key";
                        first_scope = key_search."scope";
                        for block_search in
                            select
                                *
                            from
                                chain.contract_row
                            where
                                contract_row."code" = key_search."code"
                                and contract_row."table" = key_search."table"
                                and contract_row."primary_key" = key_search."primary_key"
                                and contract_row."scope" = key_search."scope"
                                and contract_row.block_index <= max_block_index
                            order by
                                "code",
                                "table",
                                "primary_key",
                                "scope",
                                block_index desc,
                                present desc
                            limit 1
                        loop
                            if block_search.present then
                                return next block_search;
                            else
                                return next row(block_search.block_index, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::numeric, ''::varchar(13), ''::bytea);
                            end if;
                            num_results = num_results + 1;
                            found_block = true;
                        end loop;
                        if not found_block then
                            return next row(0::bigint, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::numeric, ''::varchar(13), ''::bytea);
                            num_results = num_results + 1;
                        end if;
                    end loop;
                end loop;
            end 
        $$ language plpgsql;
    
        drop function if exists chain.contract_row_range_code_table_scope_pk;
        create function chain.contract_row_range_code_table_scope_pk(
            max_block_index bigint,
            first_code varchar(13),
            first_table varchar(13),
            first_scope varchar(13),
            first_primary_key numeric,
            last_code varchar(13),
            last_table varchar(13),
            last_scope varchar(13),
            last_primary_key numeric,
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
                        "code", "table", "scope", "primary_key"
                    from
                        chain.contract_row
                    where
                        ("code", "table", "scope", "primary_key") >= ("first_code", "first_table", "first_scope", "first_primary_key")
                    order by
                        "code",
                        "table",
                        "scope",
                        "primary_key",
                        block_index desc,
                        present desc
                    limit 1
                loop
                    if (key_search."code", key_search."table", key_search."scope", key_search."primary_key") > (last_code, last_table, last_scope, last_primary_key) then
                        return;
                    end if;
                    found_key = true;
                    found_block = false;
                    first_code = key_search."code";
                    first_table = key_search."table";
                    first_scope = key_search."scope";
                    first_primary_key = key_search."primary_key";
                    for block_search in
                        select
                            *
                        from
                            chain.contract_row
                        where
                            contract_row."code" = key_search."code"
                            and contract_row."table" = key_search."table"
                            and contract_row."scope" = key_search."scope"
                            and contract_row."primary_key" = key_search."primary_key"
                            and contract_row.block_index <= max_block_index
                        order by
                            "code",
                            "table",
                            "scope",
                            "primary_key",
                            block_index desc,
                            present desc
                        limit 1
                    loop
                        if block_search.present then
                            return next block_search;
                        else
                            return next row(block_search.block_index, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::numeric, ''::varchar(13), ''::bytea);
                        end if;
                        num_results = num_results + 1;
                        found_block = true;
                    end loop;
                    if not found_block then
                        return next row(0::bigint, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::numeric, ''::varchar(13), ''::bytea);
                        num_results = num_results + 1;
                    end if;
                end loop;

                loop
                    exit when not found_key or num_results >= max_results;

                    found_key = false;
                    for key_search in
                        select
                            "code", "table", "scope", "primary_key"
                        from
                            chain.contract_row
                        where
                            ("code", "table", "scope", "primary_key") > ("first_code", "first_table", "first_scope", "first_primary_key")
                        order by
                            "code",
                            "table",
                            "scope",
                            "primary_key",
                            block_index desc,
                            present desc
                        limit 1
                    loop
                        if (key_search."code", key_search."table", key_search."scope", key_search."primary_key") > (last_code, last_table, last_scope, last_primary_key) then
                            return;
                        end if;
                        found_key = true;
                        found_block = false;
                        first_code = key_search."code";
                        first_table = key_search."table";
                        first_scope = key_search."scope";
                        first_primary_key = key_search."primary_key";
                        for block_search in
                            select
                                *
                            from
                                chain.contract_row
                            where
                                contract_row."code" = key_search."code"
                                and contract_row."table" = key_search."table"
                                and contract_row."scope" = key_search."scope"
                                and contract_row."primary_key" = key_search."primary_key"
                                and contract_row.block_index <= max_block_index
                            order by
                                "code",
                                "table",
                                "scope",
                                "primary_key",
                                block_index desc,
                                present desc
                            limit 1
                        loop
                            if block_search.present then
                                return next block_search;
                            else
                                return next row(block_search.block_index, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::numeric, ''::varchar(13), ''::bytea);
                            end if;
                            num_results = num_results + 1;
                            found_block = true;
                        end loop;
                        if not found_block then
                            return next row(0::bigint, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::numeric, ''::varchar(13), ''::bytea);
                            num_results = num_results + 1;
                        end if;
                    end loop;
                end loop;
            end 
        $$ language plpgsql;
    
        drop function if exists chain.contract_row_range_scope_table_pk_code;
        create function chain.contract_row_range_scope_table_pk_code(
            max_block_index bigint,
            first_scope varchar(13),
            first_table varchar(13),
            first_primary_key numeric,
            first_code varchar(13),
            last_scope varchar(13),
            last_table varchar(13),
            last_primary_key numeric,
            last_code varchar(13),
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
                        "scope", "table", "primary_key", "code"
                    from
                        chain.contract_row
                    where
                        ("scope", "table", "primary_key", "code") >= ("first_scope", "first_table", "first_primary_key", "first_code")
                    order by
                        "scope",
                        "table",
                        "primary_key",
                        "code",
                        block_index desc,
                        present desc
                    limit 1
                loop
                    if (key_search."scope", key_search."table", key_search."primary_key", key_search."code") > (last_scope, last_table, last_primary_key, last_code) then
                        return;
                    end if;
                    found_key = true;
                    found_block = false;
                    first_scope = key_search."scope";
                    first_table = key_search."table";
                    first_primary_key = key_search."primary_key";
                    first_code = key_search."code";
                    for block_search in
                        select
                            *
                        from
                            chain.contract_row
                        where
                            contract_row."scope" = key_search."scope"
                            and contract_row."table" = key_search."table"
                            and contract_row."primary_key" = key_search."primary_key"
                            and contract_row."code" = key_search."code"
                            and contract_row.block_index <= max_block_index
                        order by
                            "scope",
                            "table",
                            "primary_key",
                            "code",
                            block_index desc,
                            present desc
                        limit 1
                    loop
                        if block_search.present then
                            return next block_search;
                        else
                            return next row(block_search.block_index, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::numeric, ''::varchar(13), ''::bytea);
                        end if;
                        num_results = num_results + 1;
                        found_block = true;
                    end loop;
                    if not found_block then
                        return next row(0::bigint, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::numeric, ''::varchar(13), ''::bytea);
                        num_results = num_results + 1;
                    end if;
                end loop;

                loop
                    exit when not found_key or num_results >= max_results;

                    found_key = false;
                    for key_search in
                        select
                            "scope", "table", "primary_key", "code"
                        from
                            chain.contract_row
                        where
                            ("scope", "table", "primary_key", "code") > ("first_scope", "first_table", "first_primary_key", "first_code")
                        order by
                            "scope",
                            "table",
                            "primary_key",
                            "code",
                            block_index desc,
                            present desc
                        limit 1
                    loop
                        if (key_search."scope", key_search."table", key_search."primary_key", key_search."code") > (last_scope, last_table, last_primary_key, last_code) then
                            return;
                        end if;
                        found_key = true;
                        found_block = false;
                        first_scope = key_search."scope";
                        first_table = key_search."table";
                        first_primary_key = key_search."primary_key";
                        first_code = key_search."code";
                        for block_search in
                            select
                                *
                            from
                                chain.contract_row
                            where
                                contract_row."scope" = key_search."scope"
                                and contract_row."table" = key_search."table"
                                and contract_row."primary_key" = key_search."primary_key"
                                and contract_row."code" = key_search."code"
                                and contract_row.block_index <= max_block_index
                            order by
                                "scope",
                                "table",
                                "primary_key",
                                "code",
                                block_index desc,
                                present desc
                            limit 1
                        loop
                            if block_search.present then
                                return next block_search;
                            else
                                return next row(block_search.block_index, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::numeric, ''::varchar(13), ''::bytea);
                            end if;
                            num_results = num_results + 1;
                            found_block = true;
                        end loop;
                        if not found_block then
                            return next row(0::bigint, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::numeric, ''::varchar(13), ''::bytea);
                            num_results = num_results + 1;
                        end if;
                    end loop;
                end loop;
            end 
        $$ language plpgsql;
    
