'use strict';

let indexes = '';
let functions = '';

function generate({ table_name, index_name, keys, sort_keys }) {
    const fn_name = `chain.contract_row_range_${sort_keys.map(x => x.short_name).join('_')}`;
    const fn_args = prefix => sort_keys.map(x => `${prefix}${x.name} ${x.type},`).join('\n            ');
    const keys_tuple_type = (prefix, suffix, sep) => keys.map(x => `${prefix}${x.name}${suffix}::${x.type}`).join(sep);
    const sort_keys_tuple = (prefix, suffix, sep) => sort_keys.map(x => `${prefix}${x.name}${suffix}`).join(sep);

    indexes += `
        create index if not exists ${index_name} on chain.${table_name}(
            ${sort_keys.map(x => `"${x.name}",`).join('\n            ')}
            block_index desc,
            present desc
        );
    `
    functions += `
        drop function if exists ${fn_name};
        create function ${fn_name}(
            max_block_index bigint,
            ${fn_args('first_')}
            ${fn_args('last_')}
            max_results integer
        ) returns setof chain.${table_name}
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
                        ${sort_keys_tuple('"', '"', ', ')}
                    from
                        chain.${table_name}
                    where
                        (${sort_keys_tuple('"', '"', ', ')}) >= (${sort_keys_tuple('"first_', '"', ', ')})
                    order by
                        ${sort_keys_tuple('"', '"', ',\n                        ')},
                        block_index desc,
                        present desc
                    limit 1
                loop
                    if (${sort_keys_tuple('key_search."', '"', ', ')}) > (${sort_keys_tuple('last_', '', ', ')}) then
                        return;
                    end if;
                    found_key = true;
                    found_block = false;
                    ${sort_keys.map(x => `first_${x.name} = key_search."${x.name}";`).join('\n                    ')}
                    for block_search in
                        select
                            *
                        from
                            chain.${table_name}
                        where
                            ${sort_keys.map(x => `${table_name}."${x.name}" = key_search."${x.name}"`).join('\n                            and ')}
                            and ${table_name}.block_index <= max_block_index
                        order by
                            ${sort_keys_tuple('"', '"', ',\n                            ')},
                            block_index desc,
                            present desc
                        limit 1
                    loop
                        if block_search.present then
                            return next block_search;
                        else
                            return next row(block_search.block_index, false, ${keys_tuple_type('key_search."', '"', ', ')}, ''::varchar(13), ''::bytea);
                        end if;
                        num_results = num_results + 1;
                        found_block = true;
                    end loop;
                    if not found_block then
                        return next row(0::bigint, false, ${keys_tuple_type('key_search."', '"', ', ')}, ''::varchar(13), ''::bytea);
                        num_results = num_results + 1;
                    end if;
                end loop;

                loop
                    exit when not found_key or num_results >= max_results;

                    found_key = false;
                    for key_search in
                        select
                            ${sort_keys_tuple('"', '"', ', ')}
                        from
                            chain.${table_name}
                        where
                            (${sort_keys_tuple('"', '"', ', ')}) > (${sort_keys_tuple('"first_', '"', ', ')})
                        order by
                            "${sort_keys[0].name}",
                            "${sort_keys[1].name}",
                            "${sort_keys[2].name}",
                            "${sort_keys[3].name}",
                            block_index desc,
                            present desc
                        limit 1
                    loop
                        if (${sort_keys_tuple('key_search."', '"', ', ')}) > (${sort_keys_tuple('last_', '', ', ')}) then
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
                                chain.${table_name}
                            where
                                ${table_name}."${sort_keys[0].name}" = key_search."${sort_keys[0].name}"
                                and ${table_name}."${sort_keys[1].name}" = key_search."${sort_keys[1].name}"
                                and ${table_name}."${sort_keys[2].name}" = key_search."${sort_keys[2].name}"
                                and ${table_name}."${sort_keys[3].name}" = key_search."${sort_keys[3].name}"
                                and ${table_name}.block_index <= max_block_index
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
                                return next row(block_search.block_index, false, ${keys_tuple_type('key_search."', '"', ', ')}, ''::varchar(13), ''::bytea);
                            end if;
                            num_results = num_results + 1;
                            found_block = true;
                        end loop;
                        if not found_block then
                            return next row(0::bigint, false, ${keys_tuple_type('key_search."', '"', ', ')}, ''::varchar(13), ''::bytea);
                            num_results = num_results + 1;
                        end if;
                    end loop;
                end loop;
            end 
        $$ language plpgsql;
    `;
}

generate({
    table_name: 'contract_row',
    index_name: 'contract_row_code_table_primary_key_scope_block_index_prese_idx',
    keys: [
        { name: 'code', short_name: 'code', type: 'varchar(13)' },
        { name: 'scope', short_name: 'scope', type: 'varchar(13)' },
        { name: 'table', short_name: 'table', type: 'varchar(13)' },
        { name: 'primary_key', short_name: 'pk', type: 'numeric' },
    ],
    sort_keys: [
        { name: 'code', short_name: 'code', type: 'varchar(13)' },
        { name: 'table', short_name: 'table', type: 'varchar(13)' },
        { name: 'primary_key', short_name: 'pk', type: 'numeric' },
        { name: 'scope', short_name: 'scope', type: 'varchar(13)' },
    ]
});

generate({
    table_name: 'contract_row',
    index_name: 'contract_row_code_table_scope_primary_key_block_index_prese_idx',
    keys: [
        { name: 'code', short_name: 'code', type: 'varchar(13)' },
        { name: 'scope', short_name: 'scope', type: 'varchar(13)' },
        { name: 'table', short_name: 'table', type: 'varchar(13)' },
        { name: 'primary_key', short_name: 'pk', type: 'numeric' },
    ],
    sort_keys: [
        { name: 'code', short_name: 'code', type: 'varchar(13)' },
        { name: 'table', short_name: 'table', type: 'varchar(13)' },
        { name: 'scope', short_name: 'scope', type: 'varchar(13)' },
        { name: 'primary_key', short_name: 'pk', type: 'numeric' },
    ]
});

generate({
    table_name: 'contract_row',
    index_name: 'contract_row_scope_table_primary_key_code_block_index_prese_idx',
    keys: [
        { name: 'code', short_name: 'code', type: 'varchar(13)' },
        { name: 'scope', short_name: 'scope', type: 'varchar(13)' },
        { name: 'table', short_name: 'table', type: 'varchar(13)' },
        { name: 'primary_key', short_name: 'pk', type: 'numeric' },
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
