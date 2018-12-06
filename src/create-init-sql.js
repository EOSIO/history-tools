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
    `;

    const key_search = (compare, indent) => `
        ${indent}for key_search in
        ${indent}    select
        ${indent}        ${sort_keys_tuple('"', '"', ', ')}
        ${indent}    from
        ${indent}        chain.${table_name}
        ${indent}    where
        ${indent}        (${sort_keys_tuple('"', '"', ', ')}) ${compare} (${sort_keys_tuple('"first_', '"', ', ')})
        ${indent}    order by
        ${indent}        ${sort_keys_tuple('"', '"', ',\n                ' + indent)},
        ${indent}        block_index desc,
        ${indent}        present desc
        ${indent}    limit 1
        ${indent}loop
        ${indent}    if (${sort_keys_tuple('key_search."', '"', ', ')}) > (${sort_keys_tuple('last_', '', ', ')}) then
        ${indent}        return;
        ${indent}    end if;
        ${indent}    found_key = true;
        ${indent}    found_block = false;
        ${indent}    ${sort_keys.map(x => `first_${x.name} = key_search."${x.name}";`).join('\n            ' + indent)}
        ${indent}    for block_search in
        ${indent}        select
        ${indent}            *
        ${indent}        from
        ${indent}            chain.${table_name}
        ${indent}        where
        ${indent}            ${sort_keys.map(x => `${table_name}."${x.name}" = key_search."${x.name}"`).join('\n                    ' + indent + 'and ')}
        ${indent}            and ${table_name}.block_index <= max_block_index
        ${indent}        order by
        ${indent}            ${sort_keys_tuple('"', '"', ',\n                    ' + indent)},
        ${indent}            block_index desc,
        ${indent}            present desc
        ${indent}        limit 1
        ${indent}    loop
        ${indent}        if block_search.present then
        ${indent}            return next block_search;
        ${indent}        else
        ${indent}            return next row(block_search.block_index, false, ${keys_tuple_type('key_search."', '"', ', ')}, ''::varchar(13), ''::bytea);
        ${indent}        end if;
        ${indent}        num_results = num_results + 1;
        ${indent}        found_block = true;
        ${indent}    end loop;
        ${indent}    if not found_block then
        ${indent}        return next row(0::bigint, false, ${keys_tuple_type('key_search."', '"', ', ')}, ''::varchar(13), ''::bytea);
        ${indent}        num_results = num_results + 1;
        ${indent}    end if;
        ${indent}end loop;
    `;

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
                ${key_search('>=', '        ')}
                loop
                    exit when not found_key or num_results >= max_results;
                    found_key = false;
                    ${key_search('>', '            ')}
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
