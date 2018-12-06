'use strict';

let indexes = '';
let functions = '';

function f({ index_name, keys, sort_keys }) {
    indexes += `
create index if not exists ${index_name} on chain.contract_row(
    "${sort_keys[0].name}",
    "${sort_keys[1].name}",
    "${sort_keys[2].name}",
    "${sort_keys[3].name}",
    block_index desc,
    present desc
);
`

    functions += `
drop function if exists chain.contract_row_range_${sort_keys[0].short_name}_${sort_keys[1].short_name}_${sort_keys[2].short_name}_${sort_keys[3].short_name};
create function chain.contract_row_range_${sort_keys[0].short_name}_${sort_keys[1].short_name}_${sort_keys[2].short_name}_${sort_keys[3].short_name}(
    max_block_index bigint,
    first_${sort_keys[0].name} ${sort_keys[0].type},
    first_${sort_keys[1].name} ${sort_keys[1].type},
    first_${sort_keys[2].name} ${sort_keys[2].type},
    first_${sort_keys[3].name} ${sort_keys[3].type},
    last_${sort_keys[0].name} ${sort_keys[0].type},
    last_${sort_keys[1].name} ${sort_keys[1].type},
    last_${sort_keys[2].name} ${sort_keys[2].type},
    last_${sort_keys[3].name} ${sort_keys[3].type},
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
                "${sort_keys[0].name}", "${sort_keys[1].name}", "${sort_keys[2].name}", "${sort_keys[3].name}"
            from
                chain.contract_row
            where
                ("${sort_keys[0].name}", "${sort_keys[1].name}", "${sort_keys[2].name}", "${sort_keys[3].name}") >= (first_${sort_keys[0].name}, first_${sort_keys[1].name}, first_${sort_keys[2].name}, first_${sort_keys[3].name})
            order by
                "${sort_keys[0].name}",
                "${sort_keys[1].name}",
                "${sort_keys[2].name}",
                "${sort_keys[3].name}",
                block_index desc,
                present desc
            limit 1
        loop
            if (key_search."${sort_keys[0].name}", key_search."${sort_keys[1].name}", key_search."${sort_keys[2].name}", key_search."${sort_keys[3].name}") > (last_${sort_keys[0].name}, last_${sort_keys[1].name}, last_${sort_keys[2].name}, last_${sort_keys[3].name}) then
                return;
            end if;
            found_key = true;
            found_block = false;
            first_${sort_keys[0].name} = key_search."${sort_keys[0].name}";
            first_${sort_keys[1].name} = key_search."${sort_keys[1].name}";
            first_${sort_keys[2].name} = key_search."${sort_keys[2].name}";
            first_${sort_keys[3].name} = key_search."${sort_keys[3].name}";
            for block_search in
                select
                    *
                from
                    chain.contract_row
                where
                    contract_row."${sort_keys[0].name}" = key_search."${sort_keys[0].name}"
                    and contract_row."${sort_keys[1].name}" = key_search."${sort_keys[1].name}"
                    and contract_row."${sort_keys[2].name}" = key_search."${sort_keys[2].name}"
                    and contract_row."${sort_keys[3].name}" = key_search."${sort_keys[3].name}"
                    and contract_row.block_index <= max_block_index
                order by
                    "${sort_keys[0].name}",
                    "${sort_keys[1].name}",
                    "${sort_keys[2].name}",
                    "${sort_keys[3].name}",
                    block_index desc,
                    present desc
                limit 1
            loop
                if block_search.present then
                    return next block_search;
                else
                    return next row(block_search.block_index, false, key_search."${keys[0].name}"::varchar(13), key_search."${keys[3].name}"::varchar(13), key_search."${keys[1].name}"::varchar(13), key_search."${keys[2].name}", ''::varchar(13), ''::bytea);
                end if;
                num_results = num_results + 1;
                found_block = true;
            end loop;
            if not found_block then
                return next row(0::bigint, false, key_search."${keys[0].name}"::varchar(13), key_search."${keys[3].name}"::varchar(13), key_search."${keys[1].name}"::varchar(13), key_search."${keys[2].name}", ''::varchar(13), ''::bytea);
                num_results = num_results + 1;
            end if;
        end loop;

        loop
            exit when not found_key or num_results >= max_results;

            found_key = false;
            for key_search in
                select
                    "${sort_keys[0].name}", "${sort_keys[1].name}", "${sort_keys[2].name}", "${sort_keys[3].name}"
                from
                    chain.contract_row
                where
                    ("${sort_keys[0].name}", "${sort_keys[1].name}", "${sort_keys[2].name}", "${sort_keys[3].name}") > (first_${sort_keys[0].name}, first_${sort_keys[1].name}, first_${sort_keys[2].name}, first_${sort_keys[3].name})
                order by
                    "${sort_keys[0].name}",
                    "${sort_keys[1].name}",
                    "${sort_keys[2].name}",
                    "${sort_keys[3].name}",
                    block_index desc,
                    present desc
                limit 1
            loop
                if (key_search."${sort_keys[0].name}", key_search."${sort_keys[1].name}", key_search."${sort_keys[2].name}", key_search."${sort_keys[3].name}") > (last_${sort_keys[0].name}, last_${sort_keys[1].name}, last_${sort_keys[2].name}, last_${sort_keys[3].name}) then
                    return;
                end if;
                found_key = true;
                found_block = false;
                first_${sort_keys[0].name} = key_search."${sort_keys[0].name}";
                first_${sort_keys[1].name} = key_search."${sort_keys[1].name}";
                first_${sort_keys[2].name} = key_search."${sort_keys[2].name}";
                first_${sort_keys[3].name} = key_search."${sort_keys[3].name}";
                for block_search in
                    select
                        *
                    from
                        chain.contract_row
                    where
                        contract_row."${sort_keys[0].name}" = key_search."${sort_keys[0].name}"
                        and contract_row."${sort_keys[1].name}" = key_search."${sort_keys[1].name}"
                        and contract_row."${sort_keys[2].name}" = key_search."${sort_keys[2].name}"
                        and contract_row."${sort_keys[3].name}" = key_search."${sort_keys[3].name}"
                        and contract_row.block_index <= max_block_index
                    order by
                        "${sort_keys[0].name}",
                        "${sort_keys[1].name}",
                        "${sort_keys[2].name}",
                        "${sort_keys[3].name}",
                        block_index desc,
                        present desc
                    limit 1
                loop
                    if block_search.present then
                        return next block_search;
                    else
                        return next row(block_search.block_index, false, key_search."${keys[0].name}"::varchar(13), key_search."${keys[3].name}"::varchar(13), key_search."${keys[1].name}"::varchar(13), key_search."${keys[2].name}", ''::varchar(13), ''::bytea);
                    end if;
                    num_results = num_results + 1;
                    found_block = true;
                end loop;
                if not found_block then
                    return next row(0::bigint, false, key_search."${keys[0].name}"::varchar(13), key_search."${keys[3].name}"::varchar(13), key_search."${keys[1].name}"::varchar(13), key_search."${keys[2].name}", ''::varchar(13), ''::bytea);
                    num_results = num_results + 1;
                end if;
            end loop;
        end loop;
    end 
$$ language plpgsql;
`;
}

f({
    index_name: 'contract_row_code_table_primary_key_scope_block_index_prese_idx',
    keys: [
        { name: 'code', short_name: 'code', type: 'varchar(13)' },
        { name: 'table', short_name: 'table', type: 'varchar(13)' },
        { name: 'primary_key', short_name: 'pk', type: 'numeric' },
        { name: 'scope', short_name: 'scope', type: 'varchar(13)' },
    ],
    sort_keys: [
        { name: 'code', short_name: 'code', type: 'varchar(13)' },
        { name: 'table', short_name: 'table', type: 'varchar(13)' },
        { name: 'primary_key', short_name: 'pk', type: 'numeric' },
        { name: 'scope', short_name: 'scope', type: 'varchar(13)' },
    ]
});

f({
    index_name: 'contract_row_code_table_scope_primary_key_block_index_prese_idx',
    keys: [
        { name: 'code', short_name: 'code', type: 'varchar(13)' },
        { name: 'table', short_name: 'table', type: 'varchar(13)' },
        { name: 'primary_key', short_name: 'pk', type: 'numeric' },
        { name: 'scope', short_name: 'scope', type: 'varchar(13)' },
    ],
    sort_keys: [
        { name: 'code', short_name: 'code', type: 'varchar(13)' },
        { name: 'table', short_name: 'table', type: 'varchar(13)' },
        { name: 'scope', short_name: 'scope', type: 'varchar(13)' },
        { name: 'primary_key', short_name: 'pk', type: 'numeric' },
    ]
});

f({
    index_name: 'contract_row_scope_table_primary_key_code_block_index_prese_idx',
    keys: [
        { name: 'code', short_name: 'code', type: 'varchar(13)' },
        { name: 'table', short_name: 'table', type: 'varchar(13)' },
        { name: 'primary_key', short_name: 'pk', type: 'numeric' },
        { name: 'scope', short_name: 'scope', type: 'varchar(13)' },
    ],
    sort_keys: [
        { name: 'scope', short_name: 'scope', type: 'varchar(13)' },
        { name: 'table', short_name: 'table', type: 'varchar(13)' },
        { name: 'primary_key', short_name: 'pk', type: 'numeric' },
        { name: 'code', short_name: 'code', type: 'varchar(13)' },
    ]
});

console.log(indexes);
console.log(functions);
