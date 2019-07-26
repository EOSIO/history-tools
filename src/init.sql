

        create index if not exists at_executed_range_name_receiver_account_block_trans_action_idx on chain.action_trace(
            "act_name",
            "receiver",
            "act_account",
            "block_num",
            "transaction_id",
            "action_ordinal"
        )
        where
            transaction_status = 'executed';

        create index if not exists executed_receipt_receiver_idx on chain.action_trace(
            "receiver",
            "block_num",
            "transaction_id",
            "action_ordinal"
        )
        where
            transaction_status = 'executed';

        create index if not exists executed_transaction_idx on chain.action_trace(
            "transaction_id",
            "block_num",
            "action_ordinal"
        )
        where
            transaction_status = 'executed';

        create index if not exists account_name_block_present_idx on chain.account(
            "name",
            "block_num" desc,
            "present" desc
        );

        create index if not exists acctmeta_joined_name_block_present_idx on chain.account_metadata(
            "name",
            "block_num" desc,
            "present" desc
        );

        create index if not exists code_type_ver_hash_block_present_idx on chain.code(
            "vm_type",
            "vm_version",
            "code_hash",
            "block_num" desc,
            "present" desc
        );

        create index if not exists contract_row_code_table_primary_key_scope_block_num_prese_idx on chain.contract_row(
            "code",
            "table",
            "primary_key",
            "scope",
            "block_num" desc,
            "present" desc
        );

        create index if not exists contract_row_code_table_scope_primary_key_block_num_prese_idx on chain.contract_row(
            "code",
            "table",
            "scope",
            "primary_key",
            "block_num" desc,
            "present" desc
        );

        create index if not exists contract_row_scope_table_primary_key_code_block_num_prese_idx on chain.contract_row(
            "scope",
            "table",
            "primary_key",
            "code",
            "block_num" desc,
            "present" desc
        );

        create index if not exists contract_index64_code_table_scope_sk_pk_block_num_prese_idx on chain.contract_index64(
            "code",
            "table",
            "scope",
            "secondary_key",
            "primary_key",
            "block_num" desc,
            "present" desc
        );


        drop function if exists chain.block_info_range_index;
        create function chain.block_info_range_index(
            
            first_block_num bigint,
            last_block_num bigint,
            max_results integer
        ) returns setof chain.block_info
        as $$
            declare
                arg_first_block_num bigint = "first_block_num";
                arg_last_block_num bigint = "last_block_num";
                search record;
            begin
                
                for search in
                    select
                        *
                    from
                        chain.block_info
                    where
                        ("block_num") >= ("arg_first_block_num")
                        
                        
                    order by
                        "block_num"
                    limit max_results
                loop
                    if (search."block_num") > ("arg_last_block_num") then
                        return;
                    end if;
                    return next search;
                end loop;
    
            end 
        $$ language plpgsql;
    
        drop function if exists chain.at_executed_range_name_receiver_account_block_trans_action;
        create function chain.at_executed_range_name_receiver_account_block_trans_action(
            max_block_num bigint,
            first_act_name varchar(13),
            first_receiver varchar(13),
            first_act_account varchar(13),
            first_block_num bigint,
            first_transaction_id varchar(64),
            first_action_ordinal bigint,
            last_act_name varchar(13),
            last_receiver varchar(13),
            last_act_account varchar(13),
            last_block_num bigint,
            last_transaction_id varchar(64),
            last_action_ordinal bigint,
            max_results integer
        ) returns setof chain.action_trace
        as $$
            declare
                arg_first_act_name varchar(13) = "first_act_name";
                arg_first_receiver varchar(13) = "first_receiver";
                arg_first_act_account varchar(13) = "first_act_account";
                arg_first_block_num bigint = "first_block_num";
                arg_first_transaction_id varchar(64) = "first_transaction_id";
                arg_first_action_ordinal bigint = "first_action_ordinal";
                arg_last_act_name varchar(13) = "last_act_name";
                arg_last_receiver varchar(13) = "last_receiver";
                arg_last_act_account varchar(13) = "last_act_account";
                arg_last_block_num bigint = "last_block_num";
                arg_last_transaction_id varchar(64) = "last_transaction_id";
                arg_last_action_ordinal bigint = "last_action_ordinal";
                search record;
            begin
                
                for search in
                    select
                        *
                    from
                        chain.action_trace
                    where
                        ("act_name","receiver","act_account","block_num","transaction_id","action_ordinal") >= ("arg_first_act_name", "arg_first_receiver", "arg_first_act_account", "arg_first_block_num", "arg_first_transaction_id", "arg_first_action_ordinal")
                        and transaction_status = 'executed'
                        
                        and action_trace.block_num <= max_block_num
                    order by
                        "act_name","receiver","act_account","block_num","transaction_id","action_ordinal"
                    limit max_results
                loop
                    if (search."act_name",search."receiver",search."act_account",search."block_num",search."transaction_id",search."action_ordinal") > ("arg_last_act_name", "arg_last_receiver", "arg_last_act_account", "arg_last_block_num", "arg_last_transaction_id", "arg_last_action_ordinal") then
                        return;
                    end if;
                    return next search;
                end loop;
    
            end 
        $$ language plpgsql;
    
        drop function if exists chain.executed_receipt_receiver;
        create function chain.executed_receipt_receiver(
            max_block_num bigint,
            first_receiver varchar(13),
            first_block_num bigint,
            first_transaction_id varchar(64),
            first_action_ordinal bigint,
            last_receiver varchar(13),
            last_block_num bigint,
            last_transaction_id varchar(64),
            last_action_ordinal bigint,
            max_results integer
        ) returns setof chain.action_trace
        as $$
            declare
                arg_first_receiver varchar(13) = "first_receiver";
                arg_first_block_num bigint = "first_block_num";
                arg_first_transaction_id varchar(64) = "first_transaction_id";
                arg_first_action_ordinal bigint = "first_action_ordinal";
                arg_last_receiver varchar(13) = "last_receiver";
                arg_last_block_num bigint = "last_block_num";
                arg_last_transaction_id varchar(64) = "last_transaction_id";
                arg_last_action_ordinal bigint = "last_action_ordinal";
                search record;
            begin
                
                for search in
                    select
                        *
                    from
                        chain.action_trace
                    where
                        ("receiver","block_num","transaction_id","action_ordinal") >= ("arg_first_receiver", "arg_first_block_num", "arg_first_transaction_id", "arg_first_action_ordinal")
                        and transaction_status = 'executed'
                        
                        and action_trace.block_num <= max_block_num
                    order by
                        "receiver","block_num","transaction_id","action_ordinal"
                    limit max_results
                loop
                    if (search."receiver",search."block_num",search."transaction_id",search."action_ordinal") > ("arg_last_receiver", "arg_last_block_num", "arg_last_transaction_id", "arg_last_action_ordinal") then
                        return;
                    end if;
                    return next search;
                end loop;
    
            end 
        $$ language plpgsql;
    
        drop function if exists chain.executed_transaction;
        create function chain.executed_transaction(
            max_block_num bigint,
            first_transaction_id varchar(64),
            first_block_num bigint,
            first_action_ordinal bigint,
            last_transaction_id varchar(64),
            last_block_num bigint,
            last_action_ordinal bigint,
            max_results integer
        ) returns setof chain.action_trace
        as $$
            declare
                arg_first_transaction_id varchar(64) = "first_transaction_id";
                arg_first_block_num bigint = "first_block_num";
                arg_first_action_ordinal bigint = "first_action_ordinal";
                arg_last_transaction_id varchar(64) = "last_transaction_id";
                arg_last_block_num bigint = "last_block_num";
                arg_last_action_ordinal bigint = "last_action_ordinal";
                search record;
            begin
                
                for search in
                    select
                        *
                    from
                        chain.action_trace
                    where
                        ("transaction_id","block_num","action_ordinal") >= ("arg_first_transaction_id", "arg_first_block_num", "arg_first_action_ordinal")
                        and transaction_status = 'executed'
                        
                        and action_trace.block_num <= max_block_num
                    order by
                        "transaction_id","block_num","action_ordinal"
                    limit max_results
                loop
                    if (search."transaction_id",search."block_num",search."action_ordinal") > ("arg_last_transaction_id", "arg_last_block_num", "arg_last_action_ordinal") then
                        return;
                    end if;
                    return next search;
                end loop;
    
            end 
        $$ language plpgsql;
    
        drop function if exists chain.account_range_name;
        create function chain.account_range_name(
            max_block_num bigint,
            first_name varchar(13),
            last_name varchar(13),
            max_results integer
        ) returns table("block_num" bigint, "present" bool, "name" varchar(13), "creation_date" timestamp, "abi" bytea)
        as $$
            declare
                key_search record;
                block_search record;
                join_block_search record;
                num_results integer = 0;
                found_key bool = false;
                found_block bool = false;
                found_join_block bool = false;
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
                        account."block_num" desc,
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
                            and account.block_num <= max_block_num
                        order by
                            account."name",
                            account."block_num" desc,
                            account."present" desc
                        limit 1
                    loop
                        if block_search.present then
                            
                            "block_num" = block_search."block_num";
                            "present" = block_search."present";
                            "name" = block_search."name";
                            "creation_date" = block_search."creation_date";
                            "abi" = block_search."abi";
                            return next;
    
                        else
                            "block_num" = block_search."block_num";
                            "present" = false;
                            "name" = key_search."name";
                            "creation_date" = null::timestamp;
                            "abi" = ''::bytea;
                            
                            return next;
                        end if;
                        num_results = num_results + 1;
                        found_block = true;
                    end loop;
                    if not found_block then
                        "block_num" = 0;
                        "present" = false;
                        "name" = key_search."name";
                        "creation_date" = null::timestamp;
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
                            account."block_num" desc,
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
                                and account.block_num <= max_block_num
                            order by
                                account."name",
                                account."block_num" desc,
                                account."present" desc
                            limit 1
                        loop
                            if block_search.present then
                                
                                "block_num" = block_search."block_num";
                                "present" = block_search."present";
                                "name" = block_search."name";
                                "creation_date" = block_search."creation_date";
                                "abi" = block_search."abi";
                                return next;
    
                            else
                                "block_num" = block_search."block_num";
                                "present" = false;
                                "name" = key_search."name";
                                "creation_date" = null::timestamp;
                                "abi" = ''::bytea;
                                
                                return next;
                            end if;
                            num_results = num_results + 1;
                            found_block = true;
                        end loop;
                        if not found_block then
                            "block_num" = 0;
                            "present" = false;
                            "name" = key_search."name";
                            "creation_date" = null::timestamp;
                            "abi" = ''::bytea;
                            
                            return next;
                            num_results = num_results + 1;
                        end if;
                    end loop;
    
                end loop;
            end 
        $$ language plpgsql;
    
        drop function if exists chain.acctmeta_joined_range_name;
        create function chain.acctmeta_joined_range_name(
            max_block_num bigint,
            first_name varchar(13),
            last_name varchar(13),
            max_results integer
        ) returns table("block_num" bigint, "present" bool, "name" varchar(13), "privileged" bool, "last_code_update" timestamp, "code_present" bool, "code_vm_type" smallint, "code_vm_version" smallint, "code_code_hash" varchar(64), "account_block_num" bigint, "account_present" bool, "account_creation_date" timestamp, "account_abi" bytea)
        as $$
            declare
                key_search record;
                block_search record;
                join_block_search record;
                num_results integer = 0;
                found_key bool = false;
                found_block bool = false;
                found_join_block bool = false;
            begin
                if max_results <= 0 then
                    return;
                end if;
                
                for key_search in
                    select
                        account_metadata."name"
                    from
                        chain.account_metadata
                    where
                        (account_metadata."name") >= ("first_name")
                    order by
                        account_metadata."name",
                        account_metadata."block_num" desc,
                        account_metadata."present" desc
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
                            chain.account_metadata
                        where
                            account_metadata."name" = key_search."name"
                            and account_metadata.block_num <= max_block_num
                        order by
                            account_metadata."name",
                            account_metadata."block_num" desc,
                            account_metadata."present" desc
                        limit 1
                    loop
                        if block_search.present then
                            
                            found_join_block = false;
                            for join_block_search in
                                select
                                    account."block_num",
                                    account."present",
                                    account."creation_date",
                                    account."abi"
                                from
                                    chain.account
                                where
                                    account."name" = block_search."name"
                                    and account.block_num <= max_block_num
                                order by
                                    account."name",
                                    account."block_num" desc,
                                    account."present" desc
                                limit 1
                            loop
                                if join_block_search.present then
                                    found_join_block = true;
                                    "block_num" = block_search."block_num";
                                    "present" = block_search."present";
                                    "name" = block_search."name";
                                    "privileged" = block_search."privileged";
                                    "last_code_update" = block_search."last_code_update";
                                    "code_present" = block_search."code_present";
                                    "code_vm_type" = block_search."code_vm_type";
                                    "code_vm_version" = block_search."code_vm_version";
                                    "code_code_hash" = block_search."code_code_hash";
                                    "account_block_num" = join_block_search."block_num";
                                    "account_present" = join_block_search."present";
                                    "account_creation_date" = join_block_search."creation_date";
                                    "account_abi" = join_block_search."abi";
                                    return next;
                                end if;
                            end loop;
                            if not found_join_block then
                                "block_num" = block_search."block_num";
                                "present" = block_search."present";
                                "name" = block_search."name";
                                "privileged" = block_search."privileged";
                                "last_code_update" = block_search."last_code_update";
                                "code_present" = block_search."code_present";
                                "code_vm_type" = block_search."code_vm_type";
                                "code_vm_version" = block_search."code_vm_version";
                                "code_code_hash" = block_search."code_code_hash";
                                "account_block_num" = 0::bigint;
                                "account_present" = false::bool;
                                "account_creation_date" = null::timestamp;
                                "account_abi" = ''::bytea;
                                return next;
                            end if;
    
                        else
                            "block_num" = block_search."block_num";
                            "present" = false;
                            "name" = key_search."name";
                            "privileged" = false::bool;
                            "last_code_update" = null::timestamp;
                            "code_present" = false::bool;
                            "code_vm_type" = 0::smallint;
                            "code_vm_version" = 0::smallint;
                            "code_code_hash" = ''::varchar(64);
                            "account_block_num" = 0::bigint;
                            "account_present" = false::bool;
                            "account_creation_date" = null::timestamp;
                            "account_abi" = ''::bytea;
                            return next;
                        end if;
                        num_results = num_results + 1;
                        found_block = true;
                    end loop;
                    if not found_block then
                        "block_num" = 0;
                        "present" = false;
                        "name" = key_search."name";
                        "privileged" = false::bool;
                        "last_code_update" = null::timestamp;
                        "code_present" = false::bool;
                        "code_vm_type" = 0::smallint;
                        "code_vm_version" = 0::smallint;
                        "code_code_hash" = ''::varchar(64);
                        "account_block_num" = 0::bigint;
                        "account_present" = false::bool;
                        "account_creation_date" = null::timestamp;
                        "account_abi" = ''::bytea;
                        return next;
                        num_results = num_results + 1;
                    end if;
                end loop;
    
                loop
                    exit when not found_key or num_results >= max_results;
                    found_key = false;
                    
                    for key_search in
                        select
                            account_metadata."name"
                        from
                            chain.account_metadata
                        where
                            (account_metadata."name") > ("first_name")
                        order by
                            account_metadata."name",
                            account_metadata."block_num" desc,
                            account_metadata."present" desc
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
                                chain.account_metadata
                            where
                                account_metadata."name" = key_search."name"
                                and account_metadata.block_num <= max_block_num
                            order by
                                account_metadata."name",
                                account_metadata."block_num" desc,
                                account_metadata."present" desc
                            limit 1
                        loop
                            if block_search.present then
                                
                                found_join_block = false;
                                for join_block_search in
                                    select
                                        account."block_num",
                                        account."present",
                                        account."creation_date",
                                        account."abi"
                                    from
                                        chain.account
                                    where
                                        account."name" = block_search."name"
                                        and account.block_num <= max_block_num
                                    order by
                                        account."name",
                                        account."block_num" desc,
                                        account."present" desc
                                    limit 1
                                loop
                                    if join_block_search.present then
                                        found_join_block = true;
                                        "block_num" = block_search."block_num";
                                        "present" = block_search."present";
                                        "name" = block_search."name";
                                        "privileged" = block_search."privileged";
                                        "last_code_update" = block_search."last_code_update";
                                        "code_present" = block_search."code_present";
                                        "code_vm_type" = block_search."code_vm_type";
                                        "code_vm_version" = block_search."code_vm_version";
                                        "code_code_hash" = block_search."code_code_hash";
                                        "account_block_num" = join_block_search."block_num";
                                        "account_present" = join_block_search."present";
                                        "account_creation_date" = join_block_search."creation_date";
                                        "account_abi" = join_block_search."abi";
                                        return next;
                                    end if;
                                end loop;
                                if not found_join_block then
                                    "block_num" = block_search."block_num";
                                    "present" = block_search."present";
                                    "name" = block_search."name";
                                    "privileged" = block_search."privileged";
                                    "last_code_update" = block_search."last_code_update";
                                    "code_present" = block_search."code_present";
                                    "code_vm_type" = block_search."code_vm_type";
                                    "code_vm_version" = block_search."code_vm_version";
                                    "code_code_hash" = block_search."code_code_hash";
                                    "account_block_num" = 0::bigint;
                                    "account_present" = false::bool;
                                    "account_creation_date" = null::timestamp;
                                    "account_abi" = ''::bytea;
                                    return next;
                                end if;
    
                            else
                                "block_num" = block_search."block_num";
                                "present" = false;
                                "name" = key_search."name";
                                "privileged" = false::bool;
                                "last_code_update" = null::timestamp;
                                "code_present" = false::bool;
                                "code_vm_type" = 0::smallint;
                                "code_vm_version" = 0::smallint;
                                "code_code_hash" = ''::varchar(64);
                                "account_block_num" = 0::bigint;
                                "account_present" = false::bool;
                                "account_creation_date" = null::timestamp;
                                "account_abi" = ''::bytea;
                                return next;
                            end if;
                            num_results = num_results + 1;
                            found_block = true;
                        end loop;
                        if not found_block then
                            "block_num" = 0;
                            "present" = false;
                            "name" = key_search."name";
                            "privileged" = false::bool;
                            "last_code_update" = null::timestamp;
                            "code_present" = false::bool;
                            "code_vm_type" = 0::smallint;
                            "code_vm_version" = 0::smallint;
                            "code_code_hash" = ''::varchar(64);
                            "account_block_num" = 0::bigint;
                            "account_present" = false::bool;
                            "account_creation_date" = null::timestamp;
                            "account_abi" = ''::bytea;
                            return next;
                            num_results = num_results + 1;
                        end if;
                    end loop;
    
                end loop;
            end 
        $$ language plpgsql;
    
        drop function if exists chain.code_range_type_ver_hash;
        create function chain.code_range_type_ver_hash(
            max_block_num bigint,
            first_vm_type smallint,
            first_vm_version smallint,
            first_code_hash varchar(64),
            last_vm_type smallint,
            last_vm_version smallint,
            last_code_hash varchar(64),
            max_results integer
        ) returns table("block_num" bigint, "present" bool, "vm_type" smallint, "vm_version" smallint, "code_hash" varchar(64), "code" bytea)
        as $$
            declare
                key_search record;
                block_search record;
                join_block_search record;
                num_results integer = 0;
                found_key bool = false;
                found_block bool = false;
                found_join_block bool = false;
            begin
                if max_results <= 0 then
                    return;
                end if;
                
                for key_search in
                    select
                        code."vm_type",code."vm_version",code."code_hash"
                    from
                        chain.code
                    where
                        (code."vm_type", code."vm_version", code."code_hash") >= ("first_vm_type", "first_vm_version", "first_code_hash")
                    order by
                        code."vm_type",
                        code."vm_version",
                        code."code_hash",
                        code."block_num" desc,
                        code."present" desc
                    limit 1
                loop
                    if (key_search."vm_type", key_search."vm_version", key_search."code_hash") > (last_vm_type, last_vm_version, last_code_hash) then
                        return;
                    end if;
                    found_key = true;
                    found_block = false;
                    first_vm_type = key_search."vm_type";
                    first_vm_version = key_search."vm_version";
                    first_code_hash = key_search."code_hash";
                    for block_search in
                        select
                            *
                        from
                            chain.code
                        where
                            code."vm_type" = key_search."vm_type"
                            and code."vm_version" = key_search."vm_version"
                            and code."code_hash" = key_search."code_hash"
                            and code.block_num <= max_block_num
                        order by
                            code."vm_type",
                            code."vm_version",
                            code."code_hash",
                            code."block_num" desc,
                            code."present" desc
                        limit 1
                    loop
                        if block_search.present then
                            
                            "block_num" = block_search."block_num";
                            "present" = block_search."present";
                            "vm_type" = block_search."vm_type";
                            "vm_version" = block_search."vm_version";
                            "code_hash" = block_search."code_hash";
                            "code" = block_search."code";
                            return next;
    
                        else
                            "block_num" = block_search."block_num";
                            "present" = false;
                            "vm_type" = key_search."vm_type";
                            "vm_version" = key_search."vm_version";
                            "code_hash" = key_search."code_hash";
                            "code" = ''::bytea;
                            
                            return next;
                        end if;
                        num_results = num_results + 1;
                        found_block = true;
                    end loop;
                    if not found_block then
                        "block_num" = 0;
                        "present" = false;
                        "vm_type" = key_search."vm_type";
                        "vm_version" = key_search."vm_version";
                        "code_hash" = key_search."code_hash";
                        "code" = ''::bytea;
                        
                        return next;
                        num_results = num_results + 1;
                    end if;
                end loop;
    
                loop
                    exit when not found_key or num_results >= max_results;
                    found_key = false;
                    
                    for key_search in
                        select
                            code."vm_type",code."vm_version",code."code_hash"
                        from
                            chain.code
                        where
                            (code."vm_type", code."vm_version", code."code_hash") > ("first_vm_type", "first_vm_version", "first_code_hash")
                        order by
                            code."vm_type",
                            code."vm_version",
                            code."code_hash",
                            code."block_num" desc,
                            code."present" desc
                        limit 1
                    loop
                        if (key_search."vm_type", key_search."vm_version", key_search."code_hash") > (last_vm_type, last_vm_version, last_code_hash) then
                            return;
                        end if;
                        found_key = true;
                        found_block = false;
                        first_vm_type = key_search."vm_type";
                        first_vm_version = key_search."vm_version";
                        first_code_hash = key_search."code_hash";
                        for block_search in
                            select
                                *
                            from
                                chain.code
                            where
                                code."vm_type" = key_search."vm_type"
                                and code."vm_version" = key_search."vm_version"
                                and code."code_hash" = key_search."code_hash"
                                and code.block_num <= max_block_num
                            order by
                                code."vm_type",
                                code."vm_version",
                                code."code_hash",
                                code."block_num" desc,
                                code."present" desc
                            limit 1
                        loop
                            if block_search.present then
                                
                                "block_num" = block_search."block_num";
                                "present" = block_search."present";
                                "vm_type" = block_search."vm_type";
                                "vm_version" = block_search."vm_version";
                                "code_hash" = block_search."code_hash";
                                "code" = block_search."code";
                                return next;
    
                            else
                                "block_num" = block_search."block_num";
                                "present" = false;
                                "vm_type" = key_search."vm_type";
                                "vm_version" = key_search."vm_version";
                                "code_hash" = key_search."code_hash";
                                "code" = ''::bytea;
                                
                                return next;
                            end if;
                            num_results = num_results + 1;
                            found_block = true;
                        end loop;
                        if not found_block then
                            "block_num" = 0;
                            "present" = false;
                            "vm_type" = key_search."vm_type";
                            "vm_version" = key_search."vm_version";
                            "code_hash" = key_search."code_hash";
                            "code" = ''::bytea;
                            
                            return next;
                            num_results = num_results + 1;
                        end if;
                    end loop;
    
                end loop;
            end 
        $$ language plpgsql;
    
        drop function if exists chain.acctmeta_joincode_range_name;
        create function chain.acctmeta_joincode_range_name(
            max_block_num bigint,
            first_name varchar(13),
            last_name varchar(13),
            max_results integer
        ) returns table("block_num" bigint, "present" bool, "name" varchar(13), "privileged" bool, "last_code_update" timestamp, "code_present" bool, "code_vm_type" smallint, "code_vm_version" smallint, "code_code_hash" varchar(64), "join_block_num" bigint, "join_present" bool, "join_vm_type" smallint, "join_vm_version" smallint, "join_code_hash" varchar(64), "join_code" bytea)
        as $$
            declare
                key_search record;
                block_search record;
                join_block_search record;
                num_results integer = 0;
                found_key bool = false;
                found_block bool = false;
                found_join_block bool = false;
            begin
                if max_results <= 0 then
                    return;
                end if;
                
                for key_search in
                    select
                        account_metadata."name"
                    from
                        chain.account_metadata
                    where
                        (account_metadata."name") >= ("first_name")
                    order by
                        account_metadata."name",
                        account_metadata."block_num" desc,
                        account_metadata."present" desc
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
                            chain.account_metadata
                        where
                            account_metadata."name" = key_search."name"
                            and account_metadata.block_num <= max_block_num
                        order by
                            account_metadata."name",
                            account_metadata."block_num" desc,
                            account_metadata."present" desc
                        limit 1
                    loop
                        if block_search.present then
                            
                            found_join_block = false;
                            for join_block_search in
                                select
                                    code."block_num",
                                    code."present",
                                    code."vm_type",
                                    code."vm_version",
                                    code."code_hash",
                                    code."code"
                                from
                                    chain.code
                                where
                                    code."vm_type" = block_search."code_vm_type"
                                    and code."vm_version" = block_search."code_vm_version"
                                    and code."code_hash" = block_search."code_code_hash"
                                    and code.block_num <= max_block_num
                                order by
                                    code."vm_type",
                                    code."vm_version",
                                    code."code_hash",
                                    code."block_num" desc,
                                    code."present" desc
                                limit 1
                            loop
                                if join_block_search.present then
                                    found_join_block = true;
                                    "block_num" = block_search."block_num";
                                    "present" = block_search."present";
                                    "name" = block_search."name";
                                    "privileged" = block_search."privileged";
                                    "last_code_update" = block_search."last_code_update";
                                    "code_present" = block_search."code_present";
                                    "code_vm_type" = block_search."code_vm_type";
                                    "code_vm_version" = block_search."code_vm_version";
                                    "code_code_hash" = block_search."code_code_hash";
                                    "join_block_num" = join_block_search."block_num";
                                    "join_present" = join_block_search."present";
                                    "join_vm_type" = join_block_search."vm_type";
                                    "join_vm_version" = join_block_search."vm_version";
                                    "join_code_hash" = join_block_search."code_hash";
                                    "join_code" = join_block_search."code";
                                    return next;
                                end if;
                            end loop;
                            if not found_join_block then
                                "block_num" = block_search."block_num";
                                "present" = block_search."present";
                                "name" = block_search."name";
                                "privileged" = block_search."privileged";
                                "last_code_update" = block_search."last_code_update";
                                "code_present" = block_search."code_present";
                                "code_vm_type" = block_search."code_vm_type";
                                "code_vm_version" = block_search."code_vm_version";
                                "code_code_hash" = block_search."code_code_hash";
                                "join_block_num" = 0::bigint;
                                "join_present" = false::bool;
                                "join_vm_type" = 0::smallint;
                                "join_vm_version" = 0::smallint;
                                "join_code_hash" = ''::varchar(64);
                                "join_code" = ''::bytea;
                                return next;
                            end if;
    
                        else
                            "block_num" = block_search."block_num";
                            "present" = false;
                            "name" = key_search."name";
                            "privileged" = false::bool;
                            "last_code_update" = null::timestamp;
                            "code_present" = false::bool;
                            "code_vm_type" = 0::smallint;
                            "code_vm_version" = 0::smallint;
                            "code_code_hash" = ''::varchar(64);
                            "join_block_num" = 0::bigint;
                            "join_present" = false::bool;
                            "join_vm_type" = 0::smallint;
                            "join_vm_version" = 0::smallint;
                            "join_code_hash" = ''::varchar(64);
                            "join_code" = ''::bytea;
                            return next;
                        end if;
                        num_results = num_results + 1;
                        found_block = true;
                    end loop;
                    if not found_block then
                        "block_num" = 0;
                        "present" = false;
                        "name" = key_search."name";
                        "privileged" = false::bool;
                        "last_code_update" = null::timestamp;
                        "code_present" = false::bool;
                        "code_vm_type" = 0::smallint;
                        "code_vm_version" = 0::smallint;
                        "code_code_hash" = ''::varchar(64);
                        "join_block_num" = 0::bigint;
                        "join_present" = false::bool;
                        "join_vm_type" = 0::smallint;
                        "join_vm_version" = 0::smallint;
                        "join_code_hash" = ''::varchar(64);
                        "join_code" = ''::bytea;
                        return next;
                        num_results = num_results + 1;
                    end if;
                end loop;
    
                loop
                    exit when not found_key or num_results >= max_results;
                    found_key = false;
                    
                    for key_search in
                        select
                            account_metadata."name"
                        from
                            chain.account_metadata
                        where
                            (account_metadata."name") > ("first_name")
                        order by
                            account_metadata."name",
                            account_metadata."block_num" desc,
                            account_metadata."present" desc
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
                                chain.account_metadata
                            where
                                account_metadata."name" = key_search."name"
                                and account_metadata.block_num <= max_block_num
                            order by
                                account_metadata."name",
                                account_metadata."block_num" desc,
                                account_metadata."present" desc
                            limit 1
                        loop
                            if block_search.present then
                                
                                found_join_block = false;
                                for join_block_search in
                                    select
                                        code."block_num",
                                        code."present",
                                        code."vm_type",
                                        code."vm_version",
                                        code."code_hash",
                                        code."code"
                                    from
                                        chain.code
                                    where
                                        code."vm_type" = block_search."code_vm_type"
                                        and code."vm_version" = block_search."code_vm_version"
                                        and code."code_hash" = block_search."code_code_hash"
                                        and code.block_num <= max_block_num
                                    order by
                                        code."vm_type",
                                        code."vm_version",
                                        code."code_hash",
                                        code."block_num" desc,
                                        code."present" desc
                                    limit 1
                                loop
                                    if join_block_search.present then
                                        found_join_block = true;
                                        "block_num" = block_search."block_num";
                                        "present" = block_search."present";
                                        "name" = block_search."name";
                                        "privileged" = block_search."privileged";
                                        "last_code_update" = block_search."last_code_update";
                                        "code_present" = block_search."code_present";
                                        "code_vm_type" = block_search."code_vm_type";
                                        "code_vm_version" = block_search."code_vm_version";
                                        "code_code_hash" = block_search."code_code_hash";
                                        "join_block_num" = join_block_search."block_num";
                                        "join_present" = join_block_search."present";
                                        "join_vm_type" = join_block_search."vm_type";
                                        "join_vm_version" = join_block_search."vm_version";
                                        "join_code_hash" = join_block_search."code_hash";
                                        "join_code" = join_block_search."code";
                                        return next;
                                    end if;
                                end loop;
                                if not found_join_block then
                                    "block_num" = block_search."block_num";
                                    "present" = block_search."present";
                                    "name" = block_search."name";
                                    "privileged" = block_search."privileged";
                                    "last_code_update" = block_search."last_code_update";
                                    "code_present" = block_search."code_present";
                                    "code_vm_type" = block_search."code_vm_type";
                                    "code_vm_version" = block_search."code_vm_version";
                                    "code_code_hash" = block_search."code_code_hash";
                                    "join_block_num" = 0::bigint;
                                    "join_present" = false::bool;
                                    "join_vm_type" = 0::smallint;
                                    "join_vm_version" = 0::smallint;
                                    "join_code_hash" = ''::varchar(64);
                                    "join_code" = ''::bytea;
                                    return next;
                                end if;
    
                            else
                                "block_num" = block_search."block_num";
                                "present" = false;
                                "name" = key_search."name";
                                "privileged" = false::bool;
                                "last_code_update" = null::timestamp;
                                "code_present" = false::bool;
                                "code_vm_type" = 0::smallint;
                                "code_vm_version" = 0::smallint;
                                "code_code_hash" = ''::varchar(64);
                                "join_block_num" = 0::bigint;
                                "join_present" = false::bool;
                                "join_vm_type" = 0::smallint;
                                "join_vm_version" = 0::smallint;
                                "join_code_hash" = ''::varchar(64);
                                "join_code" = ''::bytea;
                                return next;
                            end if;
                            num_results = num_results + 1;
                            found_block = true;
                        end loop;
                        if not found_block then
                            "block_num" = 0;
                            "present" = false;
                            "name" = key_search."name";
                            "privileged" = false::bool;
                            "last_code_update" = null::timestamp;
                            "code_present" = false::bool;
                            "code_vm_type" = 0::smallint;
                            "code_vm_version" = 0::smallint;
                            "code_code_hash" = ''::varchar(64);
                            "join_block_num" = 0::bigint;
                            "join_present" = false::bool;
                            "join_vm_type" = 0::smallint;
                            "join_vm_version" = 0::smallint;
                            "join_code_hash" = ''::varchar(64);
                            "join_code" = ''::bytea;
                            return next;
                            num_results = num_results + 1;
                        end if;
                    end loop;
    
                end loop;
            end 
        $$ language plpgsql;
    
        drop function if exists chain.contract_row_range_code_table_pk_scope;
        create function chain.contract_row_range_code_table_pk_scope(
            max_block_num bigint,
            first_code varchar(13),
            first_table varchar(13),
            first_primary_key decimal,
            first_scope varchar(13),
            last_code varchar(13),
            last_table varchar(13),
            last_primary_key decimal,
            last_scope varchar(13),
            max_results integer
        ) returns table("block_num" bigint, "present" bool, "code" varchar(13), "scope" varchar(13), "table" varchar(13), "primary_key" decimal, "payer" varchar(13), "value" bytea)
        as $$
            declare
                key_search record;
                block_search record;
                join_block_search record;
                num_results integer = 0;
                found_key bool = false;
                found_block bool = false;
                found_join_block bool = false;
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
                        contract_row."block_num" desc,
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
                            and contract_row.block_num <= max_block_num
                        order by
                            contract_row."code",
                            contract_row."table",
                            contract_row."primary_key",
                            contract_row."scope",
                            contract_row."block_num" desc,
                            contract_row."present" desc
                        limit 1
                    loop
                        if block_search.present then
                            
                            "block_num" = block_search."block_num";
                            "present" = block_search."present";
                            "code" = block_search."code";
                            "scope" = block_search."scope";
                            "table" = block_search."table";
                            "primary_key" = block_search."primary_key";
                            "payer" = block_search."payer";
                            "value" = block_search."value";
                            return next;
    
                        else
                            "block_num" = block_search."block_num";
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
                        "block_num" = 0;
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
                            contract_row."block_num" desc,
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
                                and contract_row.block_num <= max_block_num
                            order by
                                contract_row."code",
                                contract_row."table",
                                contract_row."primary_key",
                                contract_row."scope",
                                contract_row."block_num" desc,
                                contract_row."present" desc
                            limit 1
                        loop
                            if block_search.present then
                                
                                "block_num" = block_search."block_num";
                                "present" = block_search."present";
                                "code" = block_search."code";
                                "scope" = block_search."scope";
                                "table" = block_search."table";
                                "primary_key" = block_search."primary_key";
                                "payer" = block_search."payer";
                                "value" = block_search."value";
                                return next;
    
                            else
                                "block_num" = block_search."block_num";
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
                            "block_num" = 0;
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
            max_block_num bigint,
            first_code varchar(13),
            first_table varchar(13),
            first_scope varchar(13),
            first_primary_key decimal,
            last_code varchar(13),
            last_table varchar(13),
            last_scope varchar(13),
            last_primary_key decimal,
            max_results integer
        ) returns table("block_num" bigint, "present" bool, "code" varchar(13), "scope" varchar(13), "table" varchar(13), "primary_key" decimal, "payer" varchar(13), "value" bytea)
        as $$
            declare
                key_search record;
                block_search record;
                join_block_search record;
                num_results integer = 0;
                found_key bool = false;
                found_block bool = false;
                found_join_block bool = false;
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
                        contract_row."block_num" desc,
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
                            and contract_row.block_num <= max_block_num
                        order by
                            contract_row."code",
                            contract_row."table",
                            contract_row."scope",
                            contract_row."primary_key",
                            contract_row."block_num" desc,
                            contract_row."present" desc
                        limit 1
                    loop
                        if block_search.present then
                            
                            "block_num" = block_search."block_num";
                            "present" = block_search."present";
                            "code" = block_search."code";
                            "scope" = block_search."scope";
                            "table" = block_search."table";
                            "primary_key" = block_search."primary_key";
                            "payer" = block_search."payer";
                            "value" = block_search."value";
                            return next;
    
                        else
                            "block_num" = block_search."block_num";
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
                        "block_num" = 0;
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
                            contract_row."block_num" desc,
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
                                and contract_row.block_num <= max_block_num
                            order by
                                contract_row."code",
                                contract_row."table",
                                contract_row."scope",
                                contract_row."primary_key",
                                contract_row."block_num" desc,
                                contract_row."present" desc
                            limit 1
                        loop
                            if block_search.present then
                                
                                "block_num" = block_search."block_num";
                                "present" = block_search."present";
                                "code" = block_search."code";
                                "scope" = block_search."scope";
                                "table" = block_search."table";
                                "primary_key" = block_search."primary_key";
                                "payer" = block_search."payer";
                                "value" = block_search."value";
                                return next;
    
                            else
                                "block_num" = block_search."block_num";
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
                            "block_num" = 0;
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
            max_block_num bigint,
            first_scope varchar(13),
            first_table varchar(13),
            first_primary_key decimal,
            first_code varchar(13),
            last_scope varchar(13),
            last_table varchar(13),
            last_primary_key decimal,
            last_code varchar(13),
            max_results integer
        ) returns table("block_num" bigint, "present" bool, "code" varchar(13), "scope" varchar(13), "table" varchar(13), "primary_key" decimal, "payer" varchar(13), "value" bytea)
        as $$
            declare
                key_search record;
                block_search record;
                join_block_search record;
                num_results integer = 0;
                found_key bool = false;
                found_block bool = false;
                found_join_block bool = false;
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
                        contract_row."block_num" desc,
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
                            and contract_row.block_num <= max_block_num
                        order by
                            contract_row."scope",
                            contract_row."table",
                            contract_row."primary_key",
                            contract_row."code",
                            contract_row."block_num" desc,
                            contract_row."present" desc
                        limit 1
                    loop
                        if block_search.present then
                            
                            "block_num" = block_search."block_num";
                            "present" = block_search."present";
                            "code" = block_search."code";
                            "scope" = block_search."scope";
                            "table" = block_search."table";
                            "primary_key" = block_search."primary_key";
                            "payer" = block_search."payer";
                            "value" = block_search."value";
                            return next;
    
                        else
                            "block_num" = block_search."block_num";
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
                        "block_num" = 0;
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
                            contract_row."block_num" desc,
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
                                and contract_row.block_num <= max_block_num
                            order by
                                contract_row."scope",
                                contract_row."table",
                                contract_row."primary_key",
                                contract_row."code",
                                contract_row."block_num" desc,
                                contract_row."present" desc
                            limit 1
                        loop
                            if block_search.present then
                                
                                "block_num" = block_search."block_num";
                                "present" = block_search."present";
                                "code" = block_search."code";
                                "scope" = block_search."scope";
                                "table" = block_search."table";
                                "primary_key" = block_search."primary_key";
                                "payer" = block_search."payer";
                                "value" = block_search."value";
                                return next;
    
                            else
                                "block_num" = block_search."block_num";
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
                            "block_num" = 0;
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
    
        drop function if exists chain.contract_index64_range_code_table_scope_sk_pk;
        create function chain.contract_index64_range_code_table_scope_sk_pk(
            max_block_num bigint,
            first_code varchar(13),
            first_table varchar(13),
            first_scope varchar(13),
            first_secondary_key decimal,
            first_primary_key decimal,
            last_code varchar(13),
            last_table varchar(13),
            last_scope varchar(13),
            last_secondary_key decimal,
            last_primary_key decimal,
            max_results integer
        ) returns table("block_num" bigint, "present" bool, "code" varchar(13), "scope" varchar(13), "table" varchar(13), "primary_key" decimal, "payer" varchar(13), "secondary_key" decimal, "row_block_num" bigint, "row_present" bool, "row_payer" varchar(13), "row_value" bytea)
        as $$
            declare
                key_search record;
                block_search record;
                join_block_search record;
                num_results integer = 0;
                found_key bool = false;
                found_block bool = false;
                found_join_block bool = false;
            begin
                if max_results <= 0 then
                    return;
                end if;
                
                for key_search in
                    select
                        contract_index64."code",contract_index64."table",contract_index64."scope",contract_index64."secondary_key",contract_index64."primary_key"
                    from
                        chain.contract_index64
                    where
                        (contract_index64."code", contract_index64."table", contract_index64."scope", contract_index64."secondary_key", contract_index64."primary_key") >= ("first_code", "first_table", "first_scope", "first_secondary_key", "first_primary_key")
                    order by
                        contract_index64."code",
                        contract_index64."table",
                        contract_index64."scope",
                        contract_index64."secondary_key",
                        contract_index64."primary_key",
                        contract_index64."block_num" desc,
                        contract_index64."present" desc
                    limit 1
                loop
                    if (key_search."code", key_search."table", key_search."scope", key_search."secondary_key", key_search."primary_key") > (last_code, last_table, last_scope, last_secondary_key, last_primary_key) then
                        return;
                    end if;
                    found_key = true;
                    found_block = false;
                    first_code = key_search."code";
                    first_table = key_search."table";
                    first_scope = key_search."scope";
                    first_secondary_key = key_search."secondary_key";
                    first_primary_key = key_search."primary_key";
                    for block_search in
                        select
                            *
                        from
                            chain.contract_index64
                        where
                            contract_index64."code" = key_search."code"
                            and contract_index64."table" = key_search."table"
                            and contract_index64."scope" = key_search."scope"
                            and contract_index64."secondary_key" = key_search."secondary_key"
                            and contract_index64."primary_key" = key_search."primary_key"
                            and contract_index64.block_num <= max_block_num
                        order by
                            contract_index64."code",
                            contract_index64."table",
                            contract_index64."scope",
                            contract_index64."secondary_key",
                            contract_index64."primary_key",
                            contract_index64."block_num" desc,
                            contract_index64."present" desc
                        limit 1
                    loop
                        if block_search.present then
                            
                            found_join_block = false;
                            for join_block_search in
                                select
                                    contract_row."block_num",
                                    contract_row."present",
                                    contract_row."payer",
                                    contract_row."value"
                                from
                                    chain.contract_row
                                where
                                    contract_row."code" = block_search."code"
                                    and contract_row."table" = substring(block_search."table" for 12)
                                    and contract_row."scope" = block_search."scope"
                                    and contract_row."primary_key" = block_search."primary_key"
                                    and contract_row.block_num <= max_block_num
                                order by
                                    contract_row."code",
                                    contract_row."table",
                                    contract_row."scope",
                                    contract_row."primary_key",
                                    contract_row."block_num" desc,
                                    contract_row."present" desc
                                limit 1
                            loop
                                if join_block_search.present then
                                    found_join_block = true;
                                    "block_num" = block_search."block_num";
                                    "present" = block_search."present";
                                    "code" = block_search."code";
                                    "scope" = block_search."scope";
                                    "table" = block_search."table";
                                    "primary_key" = block_search."primary_key";
                                    "payer" = block_search."payer";
                                    "secondary_key" = block_search."secondary_key";
                                    "row_block_num" = join_block_search."block_num";
                                    "row_present" = join_block_search."present";
                                    "row_payer" = join_block_search."payer";
                                    "row_value" = join_block_search."value";
                                    return next;
                                end if;
                            end loop;
                            if not found_join_block then
                                "block_num" = block_search."block_num";
                                "present" = block_search."present";
                                "code" = block_search."code";
                                "scope" = block_search."scope";
                                "table" = block_search."table";
                                "primary_key" = block_search."primary_key";
                                "payer" = block_search."payer";
                                "secondary_key" = block_search."secondary_key";
                                "row_block_num" = 0::bigint;
                                "row_present" = false::bool;
                                "row_payer" = ''::varchar(13);
                                "row_value" = ''::bytea;
                                return next;
                            end if;
    
                        else
                            "block_num" = block_search."block_num";
                            "present" = false;
                            "code" = key_search."code";
                            "scope" = key_search."scope";
                            "table" = key_search."table";
                            "primary_key" = key_search."primary_key";
                            "payer" = ''::varchar(13);
                            "secondary_key" = 0::decimal;
                            "row_block_num" = 0::bigint;
                            "row_present" = false::bool;
                            "row_payer" = ''::varchar(13);
                            "row_value" = ''::bytea;
                            return next;
                        end if;
                        num_results = num_results + 1;
                        found_block = true;
                    end loop;
                    if not found_block then
                        "block_num" = 0;
                        "present" = false;
                        "code" = key_search."code";
                        "scope" = key_search."scope";
                        "table" = key_search."table";
                        "primary_key" = key_search."primary_key";
                        "payer" = ''::varchar(13);
                        "secondary_key" = 0::decimal;
                        "row_block_num" = 0::bigint;
                        "row_present" = false::bool;
                        "row_payer" = ''::varchar(13);
                        "row_value" = ''::bytea;
                        return next;
                        num_results = num_results + 1;
                    end if;
                end loop;
    
                loop
                    exit when not found_key or num_results >= max_results;
                    found_key = false;
                    
                    for key_search in
                        select
                            contract_index64."code",contract_index64."table",contract_index64."scope",contract_index64."secondary_key",contract_index64."primary_key"
                        from
                            chain.contract_index64
                        where
                            (contract_index64."code", contract_index64."table", contract_index64."scope", contract_index64."secondary_key", contract_index64."primary_key") > ("first_code", "first_table", "first_scope", "first_secondary_key", "first_primary_key")
                        order by
                            contract_index64."code",
                            contract_index64."table",
                            contract_index64."scope",
                            contract_index64."secondary_key",
                            contract_index64."primary_key",
                            contract_index64."block_num" desc,
                            contract_index64."present" desc
                        limit 1
                    loop
                        if (key_search."code", key_search."table", key_search."scope", key_search."secondary_key", key_search."primary_key") > (last_code, last_table, last_scope, last_secondary_key, last_primary_key) then
                            return;
                        end if;
                        found_key = true;
                        found_block = false;
                        first_code = key_search."code";
                        first_table = key_search."table";
                        first_scope = key_search."scope";
                        first_secondary_key = key_search."secondary_key";
                        first_primary_key = key_search."primary_key";
                        for block_search in
                            select
                                *
                            from
                                chain.contract_index64
                            where
                                contract_index64."code" = key_search."code"
                                and contract_index64."table" = key_search."table"
                                and contract_index64."scope" = key_search."scope"
                                and contract_index64."secondary_key" = key_search."secondary_key"
                                and contract_index64."primary_key" = key_search."primary_key"
                                and contract_index64.block_num <= max_block_num
                            order by
                                contract_index64."code",
                                contract_index64."table",
                                contract_index64."scope",
                                contract_index64."secondary_key",
                                contract_index64."primary_key",
                                contract_index64."block_num" desc,
                                contract_index64."present" desc
                            limit 1
                        loop
                            if block_search.present then
                                
                                found_join_block = false;
                                for join_block_search in
                                    select
                                        contract_row."block_num",
                                        contract_row."present",
                                        contract_row."payer",
                                        contract_row."value"
                                    from
                                        chain.contract_row
                                    where
                                        contract_row."code" = block_search."code"
                                        and contract_row."table" = substring(block_search."table" for 12)
                                        and contract_row."scope" = block_search."scope"
                                        and contract_row."primary_key" = block_search."primary_key"
                                        and contract_row.block_num <= max_block_num
                                    order by
                                        contract_row."code",
                                        contract_row."table",
                                        contract_row."scope",
                                        contract_row."primary_key",
                                        contract_row."block_num" desc,
                                        contract_row."present" desc
                                    limit 1
                                loop
                                    if join_block_search.present then
                                        found_join_block = true;
                                        "block_num" = block_search."block_num";
                                        "present" = block_search."present";
                                        "code" = block_search."code";
                                        "scope" = block_search."scope";
                                        "table" = block_search."table";
                                        "primary_key" = block_search."primary_key";
                                        "payer" = block_search."payer";
                                        "secondary_key" = block_search."secondary_key";
                                        "row_block_num" = join_block_search."block_num";
                                        "row_present" = join_block_search."present";
                                        "row_payer" = join_block_search."payer";
                                        "row_value" = join_block_search."value";
                                        return next;
                                    end if;
                                end loop;
                                if not found_join_block then
                                    "block_num" = block_search."block_num";
                                    "present" = block_search."present";
                                    "code" = block_search."code";
                                    "scope" = block_search."scope";
                                    "table" = block_search."table";
                                    "primary_key" = block_search."primary_key";
                                    "payer" = block_search."payer";
                                    "secondary_key" = block_search."secondary_key";
                                    "row_block_num" = 0::bigint;
                                    "row_present" = false::bool;
                                    "row_payer" = ''::varchar(13);
                                    "row_value" = ''::bytea;
                                    return next;
                                end if;
    
                            else
                                "block_num" = block_search."block_num";
                                "present" = false;
                                "code" = key_search."code";
                                "scope" = key_search."scope";
                                "table" = key_search."table";
                                "primary_key" = key_search."primary_key";
                                "payer" = ''::varchar(13);
                                "secondary_key" = 0::decimal;
                                "row_block_num" = 0::bigint;
                                "row_present" = false::bool;
                                "row_payer" = ''::varchar(13);
                                "row_value" = ''::bytea;
                                return next;
                            end if;
                            num_results = num_results + 1;
                            found_block = true;
                        end loop;
                        if not found_block then
                            "block_num" = 0;
                            "present" = false;
                            "code" = key_search."code";
                            "scope" = key_search."scope";
                            "table" = key_search."table";
                            "primary_key" = key_search."primary_key";
                            "payer" = ''::varchar(13);
                            "secondary_key" = 0::decimal;
                            "row_block_num" = 0::bigint;
                            "row_present" = false::bool;
                            "row_payer" = ''::varchar(13);
                            "row_value" = ''::bytea;
                            return next;
                            num_results = num_results + 1;
                        end if;
                    end loop;
    
                end loop;
            end 
        $$ language plpgsql;
    
