

        create index if not exists at_executed_range_name_receiver_account_block_trans_action_idx on chain.action_trace(
            "name",
            "receipt_receiver",
            "account",
            "block_index",
            "transaction_id",
            "action_index"
        )
        where
            transaction_status = 'executed';

        create index if not exists account_name_block_present_idx on chain.account(
            "name",
            "block_index" desc,
            "present" desc
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


        drop function if exists chain.block_info_range_index;
        create function chain.block_info_range_index(
            
            first_block_index bigint,
            last_block_index bigint,
            max_results integer
        ) returns setof chain.block_info
        as $$
            declare
                arg_first_block_index bigint = "first_block_index";
                arg_last_block_index bigint = "last_block_index";
                search record;
            begin
                
                for search in
                    select
                        *
                    from
                        chain.block_info
                    where
                        ("block_index") >= ("arg_first_block_index")
                        
                        
                    order by
                        "block_index"
                    limit max_results
                loop
                    if (search."block_index") > ("arg_last_block_index") then
                        return;
                    end if;
                    return next search;
                end loop;
    
            end 
        $$ language plpgsql;
    
        drop function if exists chain.at_executed_range_name_receiver_account_block_trans_action;
        create function chain.at_executed_range_name_receiver_account_block_trans_action(
            max_block_index bigint,
            first_name varchar(13),
            first_receipt_receiver varchar(13),
            first_account varchar(13),
            first_block_index bigint,
            first_transaction_id varchar(64),
            first_action_index bigint,
            last_name varchar(13),
            last_receipt_receiver varchar(13),
            last_account varchar(13),
            last_block_index bigint,
            last_transaction_id varchar(64),
            last_action_index bigint,
            max_results integer
        ) returns setof chain.action_trace
        as $$
            declare
                arg_first_name varchar(13) = "first_name";
                arg_first_receipt_receiver varchar(13) = "first_receipt_receiver";
                arg_first_account varchar(13) = "first_account";
                arg_first_block_index bigint = "first_block_index";
                arg_first_transaction_id varchar(64) = "first_transaction_id";
                arg_first_action_index bigint = "first_action_index";
                arg_last_name varchar(13) = "last_name";
                arg_last_receipt_receiver varchar(13) = "last_receipt_receiver";
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
                        ("name","receipt_receiver","account","block_index","transaction_id","action_index") >= ("arg_first_name", "arg_first_receipt_receiver", "arg_first_account", "arg_first_block_index", "arg_first_transaction_id", "arg_first_action_index")
                        and transaction_status = 'executed'
                        
                        and action_trace.block_index <= max_block_index
                    order by
                        "name","receipt_receiver","account","block_index","transaction_id","action_index"
                    limit max_results
                loop
                    if (search."name",search."receipt_receiver",search."account",search."block_index",search."transaction_id",search."action_index") > ("arg_last_name", "arg_last_receipt_receiver", "arg_last_account", "arg_last_block_index", "arg_last_transaction_id", "arg_last_action_index") then
                        return;
                    end if;
                    return next search;
                end loop;
    
            end 
        $$ language plpgsql;
    
        drop function if exists chain.account_range_name;
        create function chain.account_range_name(
            max_block_index bigint,
            first_name varchar(13),
            last_name varchar(13),
            max_results integer
        ) returns table("block_index" bigint, "present" bool, "name" varchar(13), "vm_type" smallint, "vm_version" smallint, "privileged" bool, "last_code_update" timestamp, "code_version" varchar(64), "creation_date" timestamp, "code" bytea, "abi" bytea)
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
                        account."name"
                    from
                        chain.account
                    where
                        (account."name") >= ("first_name")
                    order by
                        account."name",
                        account."block_index" desc,
                        account."present" desc
                    limit 1
                loop
                    if (key_search."name") > (last_name) then
                        return;
                    end if;
                    found_key = true;
                    found_block = false;
                    first_name = key_search."name";
                    for block_search in
                        select
                            *
                        from
                            chain.account
                        where
                            account."name" = key_search."name"
                            and account.block_index <= max_block_index
                        order by
                            account."name",
                            account."block_index" desc,
                            account."present" desc
                        limit 1
                    loop
                        if block_search.present then
                            "block_index" = block_search."block_index";
                            "present" = block_search."present";
                            "name" = block_search."name";
                            "vm_type" = block_search."vm_type";
                            "vm_version" = block_search."vm_version";
                            "privileged" = block_search."privileged";
                            "last_code_update" = block_search."last_code_update";
                            "code_version" = block_search."code_version";
                            "creation_date" = block_search."creation_date";
                            "code" = block_search."code";
                            "abi" = block_search."abi";
                            return next;
                        else
                            "block_index" = block_search."block_index";
                            "present" = false;
                            "name" = key_search."name";
                            "vm_type" = 0::smallint;
                            "vm_version" = 0::smallint;
                            "privileged" = false::bool;
                            "last_code_update" = null::timestamp;
                            "code_version" = ''::varchar(64);
                            "creation_date" = null::timestamp;
                            "code" = ''::bytea;
                            "abi" = ''::bytea;
                            return next;
                        end if;
                        num_results = num_results + 1;
                        found_block = true;
                    end loop;
                    if not found_block then
                        "block_index" = 0;
                        "present" = false;
                        "name" = key_search."name";
                        "vm_type" = 0::smallint;
                        "vm_version" = 0::smallint;
                        "privileged" = false::bool;
                        "last_code_update" = null::timestamp;
                        "code_version" = ''::varchar(64);
                        "creation_date" = null::timestamp;
                        "code" = ''::bytea;
                        "abi" = ''::bytea;
                        return next;
                        num_results = num_results + 1;
                    end if;
                end loop;
    
                loop
                    exit when not found_key or num_results >= max_results;
                    found_key = false;
                    
                    for key_search in
                        select
                            account."name"
                        from
                            chain.account
                        where
                            (account."name") > ("first_name")
                        order by
                            account."name",
                            account."block_index" desc,
                            account."present" desc
                        limit 1
                    loop
                        if (key_search."name") > (last_name) then
                            return;
                        end if;
                        found_key = true;
                        found_block = false;
                        first_name = key_search."name";
                        for block_search in
                            select
                                *
                            from
                                chain.account
                            where
                                account."name" = key_search."name"
                                and account.block_index <= max_block_index
                            order by
                                account."name",
                                account."block_index" desc,
                                account."present" desc
                            limit 1
                        loop
                            if block_search.present then
                                "block_index" = block_search."block_index";
                                "present" = block_search."present";
                                "name" = block_search."name";
                                "vm_type" = block_search."vm_type";
                                "vm_version" = block_search."vm_version";
                                "privileged" = block_search."privileged";
                                "last_code_update" = block_search."last_code_update";
                                "code_version" = block_search."code_version";
                                "creation_date" = block_search."creation_date";
                                "code" = block_search."code";
                                "abi" = block_search."abi";
                                return next;
                            else
                                "block_index" = block_search."block_index";
                                "present" = false;
                                "name" = key_search."name";
                                "vm_type" = 0::smallint;
                                "vm_version" = 0::smallint;
                                "privileged" = false::bool;
                                "last_code_update" = null::timestamp;
                                "code_version" = ''::varchar(64);
                                "creation_date" = null::timestamp;
                                "code" = ''::bytea;
                                "abi" = ''::bytea;
                                return next;
                            end if;
                            num_results = num_results + 1;
                            found_block = true;
                        end loop;
                        if not found_block then
                            "block_index" = 0;
                            "present" = false;
                            "name" = key_search."name";
                            "vm_type" = 0::smallint;
                            "vm_version" = 0::smallint;
                            "privileged" = false::bool;
                            "last_code_update" = null::timestamp;
                            "code_version" = ''::varchar(64);
                            "creation_date" = null::timestamp;
                            "code" = ''::bytea;
                            "abi" = ''::bytea;
                            return next;
                            num_results = num_results + 1;
                        end if;
                    end loop;
    
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
        ) returns table("block_index" bigint, "present" bool, "code" varchar(13), "scope" varchar(13), "table" varchar(13), "primary_key" decimal, "payer" varchar(13), "value" bytea)
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
                        contract_row."code",contract_row."table",contract_row."primary_key",contract_row."scope"
                    from
                        chain.contract_row
                    where
                        (contract_row."code", contract_row."table", contract_row."primary_key", contract_row."scope") >= ("first_code", "first_table", "first_primary_key", "first_scope")
                    order by
                        contract_row."code",
                        contract_row."table",
                        contract_row."primary_key",
                        contract_row."scope",
                        contract_row."block_index" desc,
                        contract_row."present" desc
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
                            contract_row."code",
                            contract_row."table",
                            contract_row."primary_key",
                            contract_row."scope",
                            contract_row."block_index" desc,
                            contract_row."present" desc
                        limit 1
                    loop
                        if block_search.present then
                            "block_index" = block_search."block_index";
                            "present" = block_search."present";
                            "code" = block_search."code";
                            "scope" = block_search."scope";
                            "table" = block_search."table";
                            "primary_key" = block_search."primary_key";
                            "payer" = block_search."payer";
                            "value" = block_search."value";
                            return next;
                        else
                            "block_index" = block_search."block_index";
                            "present" = false;
                            "code" = key_search."code";
                            "scope" = key_search."scope";
                            "table" = key_search."table";
                            "primary_key" = key_search."primary_key";
                            "payer" = ''::varchar(13);
                            "value" = ''::bytea;
                            return next;
                        end if;
                        num_results = num_results + 1;
                        found_block = true;
                    end loop;
                    if not found_block then
                        "block_index" = 0;
                        "present" = false;
                        "code" = key_search."code";
                        "scope" = key_search."scope";
                        "table" = key_search."table";
                        "primary_key" = key_search."primary_key";
                        "payer" = ''::varchar(13);
                        "value" = ''::bytea;
                        return next;
                        num_results = num_results + 1;
                    end if;
                end loop;
    
                loop
                    exit when not found_key or num_results >= max_results;
                    found_key = false;
                    
                    for key_search in
                        select
                            contract_row."code",contract_row."table",contract_row."primary_key",contract_row."scope"
                        from
                            chain.contract_row
                        where
                            (contract_row."code", contract_row."table", contract_row."primary_key", contract_row."scope") > ("first_code", "first_table", "first_primary_key", "first_scope")
                        order by
                            contract_row."code",
                            contract_row."table",
                            contract_row."primary_key",
                            contract_row."scope",
                            contract_row."block_index" desc,
                            contract_row."present" desc
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
                                contract_row."code",
                                contract_row."table",
                                contract_row."primary_key",
                                contract_row."scope",
                                contract_row."block_index" desc,
                                contract_row."present" desc
                            limit 1
                        loop
                            if block_search.present then
                                "block_index" = block_search."block_index";
                                "present" = block_search."present";
                                "code" = block_search."code";
                                "scope" = block_search."scope";
                                "table" = block_search."table";
                                "primary_key" = block_search."primary_key";
                                "payer" = block_search."payer";
                                "value" = block_search."value";
                                return next;
                            else
                                "block_index" = block_search."block_index";
                                "present" = false;
                                "code" = key_search."code";
                                "scope" = key_search."scope";
                                "table" = key_search."table";
                                "primary_key" = key_search."primary_key";
                                "payer" = ''::varchar(13);
                                "value" = ''::bytea;
                                return next;
                            end if;
                            num_results = num_results + 1;
                            found_block = true;
                        end loop;
                        if not found_block then
                            "block_index" = 0;
                            "present" = false;
                            "code" = key_search."code";
                            "scope" = key_search."scope";
                            "table" = key_search."table";
                            "primary_key" = key_search."primary_key";
                            "payer" = ''::varchar(13);
                            "value" = ''::bytea;
                            return next;
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
        ) returns table("block_index" bigint, "present" bool, "code" varchar(13), "scope" varchar(13), "table" varchar(13), "primary_key" decimal, "payer" varchar(13), "value" bytea)
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
                        contract_row."code",contract_row."table",contract_row."scope",contract_row."primary_key"
                    from
                        chain.contract_row
                    where
                        (contract_row."code", contract_row."table", contract_row."scope", contract_row."primary_key") >= ("first_code", "first_table", "first_scope", "first_primary_key")
                    order by
                        contract_row."code",
                        contract_row."table",
                        contract_row."scope",
                        contract_row."primary_key",
                        contract_row."block_index" desc,
                        contract_row."present" desc
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
                            contract_row."code",
                            contract_row."table",
                            contract_row."scope",
                            contract_row."primary_key",
                            contract_row."block_index" desc,
                            contract_row."present" desc
                        limit 1
                    loop
                        if block_search.present then
                            "block_index" = block_search."block_index";
                            "present" = block_search."present";
                            "code" = block_search."code";
                            "scope" = block_search."scope";
                            "table" = block_search."table";
                            "primary_key" = block_search."primary_key";
                            "payer" = block_search."payer";
                            "value" = block_search."value";
                            return next;
                        else
                            "block_index" = block_search."block_index";
                            "present" = false;
                            "code" = key_search."code";
                            "scope" = key_search."scope";
                            "table" = key_search."table";
                            "primary_key" = key_search."primary_key";
                            "payer" = ''::varchar(13);
                            "value" = ''::bytea;
                            return next;
                        end if;
                        num_results = num_results + 1;
                        found_block = true;
                    end loop;
                    if not found_block then
                        "block_index" = 0;
                        "present" = false;
                        "code" = key_search."code";
                        "scope" = key_search."scope";
                        "table" = key_search."table";
                        "primary_key" = key_search."primary_key";
                        "payer" = ''::varchar(13);
                        "value" = ''::bytea;
                        return next;
                        num_results = num_results + 1;
                    end if;
                end loop;
    
                loop
                    exit when not found_key or num_results >= max_results;
                    found_key = false;
                    
                    for key_search in
                        select
                            contract_row."code",contract_row."table",contract_row."scope",contract_row."primary_key"
                        from
                            chain.contract_row
                        where
                            (contract_row."code", contract_row."table", contract_row."scope", contract_row."primary_key") > ("first_code", "first_table", "first_scope", "first_primary_key")
                        order by
                            contract_row."code",
                            contract_row."table",
                            contract_row."scope",
                            contract_row."primary_key",
                            contract_row."block_index" desc,
                            contract_row."present" desc
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
                                contract_row."code",
                                contract_row."table",
                                contract_row."scope",
                                contract_row."primary_key",
                                contract_row."block_index" desc,
                                contract_row."present" desc
                            limit 1
                        loop
                            if block_search.present then
                                "block_index" = block_search."block_index";
                                "present" = block_search."present";
                                "code" = block_search."code";
                                "scope" = block_search."scope";
                                "table" = block_search."table";
                                "primary_key" = block_search."primary_key";
                                "payer" = block_search."payer";
                                "value" = block_search."value";
                                return next;
                            else
                                "block_index" = block_search."block_index";
                                "present" = false;
                                "code" = key_search."code";
                                "scope" = key_search."scope";
                                "table" = key_search."table";
                                "primary_key" = key_search."primary_key";
                                "payer" = ''::varchar(13);
                                "value" = ''::bytea;
                                return next;
                            end if;
                            num_results = num_results + 1;
                            found_block = true;
                        end loop;
                        if not found_block then
                            "block_index" = 0;
                            "present" = false;
                            "code" = key_search."code";
                            "scope" = key_search."scope";
                            "table" = key_search."table";
                            "primary_key" = key_search."primary_key";
                            "payer" = ''::varchar(13);
                            "value" = ''::bytea;
                            return next;
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
        ) returns table("block_index" bigint, "present" bool, "code" varchar(13), "scope" varchar(13), "table" varchar(13), "primary_key" decimal, "payer" varchar(13), "value" bytea)
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
                        contract_row."scope",contract_row."table",contract_row."primary_key",contract_row."code"
                    from
                        chain.contract_row
                    where
                        (contract_row."scope", contract_row."table", contract_row."primary_key", contract_row."code") >= ("first_scope", "first_table", "first_primary_key", "first_code")
                    order by
                        contract_row."scope",
                        contract_row."table",
                        contract_row."primary_key",
                        contract_row."code",
                        contract_row."block_index" desc,
                        contract_row."present" desc
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
                            contract_row."scope",
                            contract_row."table",
                            contract_row."primary_key",
                            contract_row."code",
                            contract_row."block_index" desc,
                            contract_row."present" desc
                        limit 1
                    loop
                        if block_search.present then
                            "block_index" = block_search."block_index";
                            "present" = block_search."present";
                            "code" = block_search."code";
                            "scope" = block_search."scope";
                            "table" = block_search."table";
                            "primary_key" = block_search."primary_key";
                            "payer" = block_search."payer";
                            "value" = block_search."value";
                            return next;
                        else
                            "block_index" = block_search."block_index";
                            "present" = false;
                            "code" = key_search."code";
                            "scope" = key_search."scope";
                            "table" = key_search."table";
                            "primary_key" = key_search."primary_key";
                            "payer" = ''::varchar(13);
                            "value" = ''::bytea;
                            return next;
                        end if;
                        num_results = num_results + 1;
                        found_block = true;
                    end loop;
                    if not found_block then
                        "block_index" = 0;
                        "present" = false;
                        "code" = key_search."code";
                        "scope" = key_search."scope";
                        "table" = key_search."table";
                        "primary_key" = key_search."primary_key";
                        "payer" = ''::varchar(13);
                        "value" = ''::bytea;
                        return next;
                        num_results = num_results + 1;
                    end if;
                end loop;
    
                loop
                    exit when not found_key or num_results >= max_results;
                    found_key = false;
                    
                    for key_search in
                        select
                            contract_row."scope",contract_row."table",contract_row."primary_key",contract_row."code"
                        from
                            chain.contract_row
                        where
                            (contract_row."scope", contract_row."table", contract_row."primary_key", contract_row."code") > ("first_scope", "first_table", "first_primary_key", "first_code")
                        order by
                            contract_row."scope",
                            contract_row."table",
                            contract_row."primary_key",
                            contract_row."code",
                            contract_row."block_index" desc,
                            contract_row."present" desc
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
                                contract_row."scope",
                                contract_row."table",
                                contract_row."primary_key",
                                contract_row."code",
                                contract_row."block_index" desc,
                                contract_row."present" desc
                            limit 1
                        loop
                            if block_search.present then
                                "block_index" = block_search."block_index";
                                "present" = block_search."present";
                                "code" = block_search."code";
                                "scope" = block_search."scope";
                                "table" = block_search."table";
                                "primary_key" = block_search."primary_key";
                                "payer" = block_search."payer";
                                "value" = block_search."value";
                                return next;
                            else
                                "block_index" = block_search."block_index";
                                "present" = false;
                                "code" = key_search."code";
                                "scope" = key_search."scope";
                                "table" = key_search."table";
                                "primary_key" = key_search."primary_key";
                                "payer" = ''::varchar(13);
                                "value" = ''::bytea;
                                return next;
                            end if;
                            num_results = num_results + 1;
                            found_block = true;
                        end loop;
                        if not found_block then
                            "block_index" = 0;
                            "present" = false;
                            "code" = key_search."code";
                            "scope" = key_search."scope";
                            "table" = key_search."table";
                            "primary_key" = key_search."primary_key";
                            "payer" = ''::varchar(13);
                            "value" = ''::bytea;
                            return next;
                            num_results = num_results + 1;
                        end if;
                    end loop;
    
                end loop;
            end 
        $$ language plpgsql;
    
