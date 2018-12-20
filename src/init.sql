
        drop function if exists chain.little8;
        create function chain.little8(value bytea, pos int) returns bytea immutable as $$
        begin
            return
                substring(value from pos + 8 for 1) ||
                substring(value from pos + 7 for 1) ||
                substring(value from pos + 6 for 1) ||
                substring(value from pos + 5 for 1) ||
                substring(value from pos + 4 for 1) ||
                substring(value from pos + 3 for 1) ||
                substring(value from pos + 2 for 1) ||
                substring(value from pos + 1 for 1);
        end; $$
        language plpgsql;


        create index if not exists at_nonnotify_executed_name_le8_0_account_block_trans_act_idx on chain.action_trace(
            "name",
            chain.little8(data, 0),
            "account",
            "block_index",
            "transaction_id",
            "action_index"
        )
        where
            receipt_receiver = account
            and transaction_status = 'executed'
            and octet_length(data) >= 8;

        create index if not exists at_nonnotify_executed_name_le8_8_account_block_trans_act_idx on chain.action_trace(
            "name",
            chain.little8(data, 8),
            "account",
            "block_index",
            "transaction_id",
            "action_index"
        )
        where
            receipt_receiver = account
            and transaction_status = 'executed'
            and octet_length(data) >= 16;

        create index if not exists action_trace_receipt_receiver_name_account_block_index_idx on chain.action_trace(
            "receipt_receiver",
            "name",
            "account",
            "block_index"
        );

        create index if not exists contract_row_code_table_primary_key_scope_block_index_prese_idx on chain.contract_row(
            "code",
            "table",
            "primary_key",
            "scope",
            "block_index" desc,
            "present" desc
        );

        create index if not exists contract_row_code_table_scope_primary_key_block_index_prese_idx on chain.contract_row(
            "code",
            "table",
            "scope",
            "primary_key",
            "block_index" desc,
            "present" desc
        );

        create index if not exists contract_row_scope_table_primary_key_code_block_index_prese_idx on chain.contract_row(
            "scope",
            "table",
            "primary_key",
            "code",
            "block_index" desc,
            "present" desc
        );


        drop function if exists chain.at_nonnotify_executed_range_name_le8_0_account_block_trans_act;
        create function chain.at_nonnotify_executed_range_name_le8_0_account_block_trans_act(
            max_block_index bigint,
            first_name varchar(13),
            first_le8_0 bytea,
            first_account varchar(13),
            first_block_index bigint,
            first_transaction_id varchar(64),
            first_action_index bigint,
            last_name varchar(13),
            last_le8_0 bytea,
            last_account varchar(13),
            last_block_index bigint,
            last_transaction_id varchar(64),
            last_action_index bigint,
            max_results integer
        ) returns setof chain.action_trace
        as $$
            declare
                arg_first_name varchar(13) = "first_name";
                arg_first_le8_0 bytea = chain.little8(first_le8_0, 0);
                arg_first_account varchar(13) = "first_account";
                arg_first_block_index bigint = "first_block_index";
                arg_first_transaction_id varchar(64) = "first_transaction_id";
                arg_first_action_index bigint = "first_action_index";
                arg_last_name varchar(13) = "last_name";
                arg_last_le8_0 bytea = chain.little8(last_le8_0, 0);
                arg_last_account varchar(13) = "last_account";
                arg_last_block_index bigint = "last_block_index";
                arg_last_transaction_id varchar(64) = "last_transaction_id";
                arg_last_action_index bigint = "last_action_index";
                search record;
            begin
                
                for search in
                    select
                        *
                    from
                        chain.action_trace
                    where
                        ("name",chain.little8(data, 0),"account","block_index","transaction_id","action_index") >= ("arg_first_name", "arg_first_le8_0", "arg_first_account", "arg_first_block_index", "arg_first_transaction_id", "arg_first_action_index")
                        and receipt_receiver = account
                        and transaction_status = 'executed'
                        and octet_length(data) >= 8
                        
                        and action_trace.block_index <= max_block_index
                    order by
                        "name",chain.little8(data, 0),"account","block_index","transaction_id","action_index"
                    limit max_results
                loop
                    if (search."name",chain.little8(search.data, 0),search."account",search."block_index",search."transaction_id",search."action_index") > ("arg_last_name", "arg_last_le8_0", "arg_last_account", "arg_last_block_index", "arg_last_transaction_id", "arg_last_action_index") then
                        return;
                    end if;
                    return next search;
                end loop;
    
            end 
        $$ language plpgsql;
    
        drop function if exists chain.at_nonnotify_executed_range_name_le8_8_account_block_trans_act;
        create function chain.at_nonnotify_executed_range_name_le8_8_account_block_trans_act(
            max_block_index bigint,
            first_name varchar(13),
            first_le8_8 bytea,
            first_account varchar(13),
            first_block_index bigint,
            first_transaction_id varchar(64),
            first_action_index bigint,
            last_name varchar(13),
            last_le8_8 bytea,
            last_account varchar(13),
            last_block_index bigint,
            last_transaction_id varchar(64),
            last_action_index bigint,
            max_results integer
        ) returns setof chain.action_trace
        as $$
            declare
                arg_first_name varchar(13) = "first_name";
                arg_first_le8_8 bytea = chain.little8(first_le8_8, 0);
                arg_first_account varchar(13) = "first_account";
                arg_first_block_index bigint = "first_block_index";
                arg_first_transaction_id varchar(64) = "first_transaction_id";
                arg_first_action_index bigint = "first_action_index";
                arg_last_name varchar(13) = "last_name";
                arg_last_le8_8 bytea = chain.little8(last_le8_8, 0);
                arg_last_account varchar(13) = "last_account";
                arg_last_block_index bigint = "last_block_index";
                arg_last_transaction_id varchar(64) = "last_transaction_id";
                arg_last_action_index bigint = "last_action_index";
                search record;
            begin
                
                for search in
                    select
                        *
                    from
                        chain.action_trace
                    where
                        ("name",chain.little8(data, 8),"account","block_index","transaction_id","action_index") >= ("arg_first_name", "arg_first_le8_8", "arg_first_account", "arg_first_block_index", "arg_first_transaction_id", "arg_first_action_index")
                        and receipt_receiver = account
                        and transaction_status = 'executed'
                        and octet_length(data) >= 16
                        
                        and action_trace.block_index <= max_block_index
                    order by
                        "name",chain.little8(data, 8),"account","block_index","transaction_id","action_index"
                    limit max_results
                loop
                    if (search."name",chain.little8(search.data, 8),search."account",search."block_index",search."transaction_id",search."action_index") > ("arg_last_name", "arg_last_le8_8", "arg_last_account", "arg_last_block_index", "arg_last_transaction_id", "arg_last_action_index") then
                        return;
                    end if;
                    return next search;
                end loop;
    
            end 
        $$ language plpgsql;
    
        drop function if exists chain.action_trace_range_receipt_receiver_name_account;
        create function chain.action_trace_range_receipt_receiver_name_account(
            max_block_index bigint,
            first_receipt_receiver varchar(13),
            first_name varchar(13),
            first_account varchar(13),
            last_receipt_receiver varchar(13),
            last_name varchar(13),
            last_account varchar(13),
            max_results integer
        ) returns setof chain.action_trace
        as $$
            declare
                arg_first_receipt_receiver varchar(13) = "first_receipt_receiver";
                arg_first_name varchar(13) = "first_name";
                arg_first_account varchar(13) = "first_account";
                arg_last_receipt_receiver varchar(13) = "last_receipt_receiver";
                arg_last_name varchar(13) = "last_name";
                arg_last_account varchar(13) = "last_account";
                search record;
            begin
                
                for search in
                    select
                        *
                    from
                        chain.action_trace
                    where
                        ("receipt_receiver","name","account") >= ("arg_first_receipt_receiver", "arg_first_name", "arg_first_account")
                        
                        and action_trace.block_index <= max_block_index
                    order by
                        "receipt_receiver","name","account"
                    limit max_results
                loop
                    if (search."receipt_receiver",search."name",search."account") > ("arg_last_receipt_receiver", "arg_last_name", "arg_last_account") then
                        return;
                    end if;
                    return next search;
                end loop;
    
            end 
        $$ language plpgsql;
    
        drop function if exists chain.contract_row_range_code_table_pk_scope;
        create function chain.contract_row_range_code_table_pk_scope(
            max_block_index bigint,
            first_code varchar(13),
            first_table varchar(13),
            first_primary_key decimal,
            first_scope varchar(13),
            last_code varchar(13),
            last_table varchar(13),
            last_primary_key decimal,
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
                        "code","table","primary_key","scope"
                    from
                        chain.contract_row
                    where
                        ("code", "table", "primary_key", "scope") >= ("first_code", "first_table", "first_primary_key", "first_scope")
                    order by
                        "code",
                        "table",
                        "primary_key",
                        "scope",
                        "block_index" desc,
                        "present" desc
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
                            "block_index" desc,
                            "present" desc
                        limit 1
                    loop
                        if block_search.present then
                            return next block_search;
                        else
                            return next row(block_search.block_index, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::decimal, ''::varchar(13), ''::bytea);
                        end if;
                        num_results = num_results + 1;
                        found_block = true;
                    end loop;
                    if not found_block then
                        return next row(0::bigint, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::decimal, ''::varchar(13), ''::bytea);
                        num_results = num_results + 1;
                    end if;
                end loop;
    
                loop
                    exit when not found_key or num_results >= max_results;
                    found_key = false;
                    
                    for key_search in
                        select
                            "code","table","primary_key","scope"
                        from
                            chain.contract_row
                        where
                            ("code", "table", "primary_key", "scope") > ("first_code", "first_table", "first_primary_key", "first_scope")
                        order by
                            "code",
                            "table",
                            "primary_key",
                            "scope",
                            "block_index" desc,
                            "present" desc
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
                                "block_index" desc,
                                "present" desc
                            limit 1
                        loop
                            if block_search.present then
                                return next block_search;
                            else
                                return next row(block_search.block_index, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::decimal, ''::varchar(13), ''::bytea);
                            end if;
                            num_results = num_results + 1;
                            found_block = true;
                        end loop;
                        if not found_block then
                            return next row(0::bigint, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::decimal, ''::varchar(13), ''::bytea);
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
            first_primary_key decimal,
            last_code varchar(13),
            last_table varchar(13),
            last_scope varchar(13),
            last_primary_key decimal,
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
                        "code","table","scope","primary_key"
                    from
                        chain.contract_row
                    where
                        ("code", "table", "scope", "primary_key") >= ("first_code", "first_table", "first_scope", "first_primary_key")
                    order by
                        "code",
                        "table",
                        "scope",
                        "primary_key",
                        "block_index" desc,
                        "present" desc
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
                            "block_index" desc,
                            "present" desc
                        limit 1
                    loop
                        if block_search.present then
                            return next block_search;
                        else
                            return next row(block_search.block_index, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::decimal, ''::varchar(13), ''::bytea);
                        end if;
                        num_results = num_results + 1;
                        found_block = true;
                    end loop;
                    if not found_block then
                        return next row(0::bigint, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::decimal, ''::varchar(13), ''::bytea);
                        num_results = num_results + 1;
                    end if;
                end loop;
    
                loop
                    exit when not found_key or num_results >= max_results;
                    found_key = false;
                    
                    for key_search in
                        select
                            "code","table","scope","primary_key"
                        from
                            chain.contract_row
                        where
                            ("code", "table", "scope", "primary_key") > ("first_code", "first_table", "first_scope", "first_primary_key")
                        order by
                            "code",
                            "table",
                            "scope",
                            "primary_key",
                            "block_index" desc,
                            "present" desc
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
                                "block_index" desc,
                                "present" desc
                            limit 1
                        loop
                            if block_search.present then
                                return next block_search;
                            else
                                return next row(block_search.block_index, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::decimal, ''::varchar(13), ''::bytea);
                            end if;
                            num_results = num_results + 1;
                            found_block = true;
                        end loop;
                        if not found_block then
                            return next row(0::bigint, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::decimal, ''::varchar(13), ''::bytea);
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
            first_primary_key decimal,
            first_code varchar(13),
            last_scope varchar(13),
            last_table varchar(13),
            last_primary_key decimal,
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
                        "scope","table","primary_key","code"
                    from
                        chain.contract_row
                    where
                        ("scope", "table", "primary_key", "code") >= ("first_scope", "first_table", "first_primary_key", "first_code")
                    order by
                        "scope",
                        "table",
                        "primary_key",
                        "code",
                        "block_index" desc,
                        "present" desc
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
                            "block_index" desc,
                            "present" desc
                        limit 1
                    loop
                        if block_search.present then
                            return next block_search;
                        else
                            return next row(block_search.block_index, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::decimal, ''::varchar(13), ''::bytea);
                        end if;
                        num_results = num_results + 1;
                        found_block = true;
                    end loop;
                    if not found_block then
                        return next row(0::bigint, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::decimal, ''::varchar(13), ''::bytea);
                        num_results = num_results + 1;
                    end if;
                end loop;
    
                loop
                    exit when not found_key or num_results >= max_results;
                    found_key = false;
                    
                    for key_search in
                        select
                            "scope","table","primary_key","code"
                        from
                            chain.contract_row
                        where
                            ("scope", "table", "primary_key", "code") > ("first_scope", "first_table", "first_primary_key", "first_code")
                        order by
                            "scope",
                            "table",
                            "primary_key",
                            "code",
                            "block_index" desc,
                            "present" desc
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
                                "block_index" desc,
                                "present" desc
                            limit 1
                        loop
                            if block_search.present then
                                return next block_search;
                            else
                                return next row(block_search.block_index, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::decimal, ''::varchar(13), ''::bytea);
                            end if;
                            num_results = num_results + 1;
                            found_block = true;
                        end loop;
                        if not found_block then
                            return next row(0::bigint, false, key_search."code"::varchar(13), key_search."scope"::varchar(13), key_search."table"::varchar(13), key_search."primary_key"::decimal, ''::varchar(13), ''::bytea);
                            num_results = num_results + 1;
                        end if;
                    end loop;
    
                end loop;
            end 
        $$ language plpgsql;
    
