'use strict';

const fs = require('fs');

const type_map = {
    'bool': 'bool',
    'varuint32': 'bigint',
    'varint32': 'integer',
    'uint8': 'smallint',
    'uint16': 'integer',
    'uint32': 'bigint',
    'uint64': 'decimal',
    'uint128': 'decimal',
    'int8': 'smallint',
    'int16': 'smallint',
    'int32': 'integer',
    'int64': 'bigint',
    'int128': 'decimal',
    'double': 'float8',
    'float128': 'bytea',
    'name': 'varchar(13)',
    'string': 'varchar',
    'time_point': 'timestamp',
    'time_point_sec': 'timestamp',
    'block_timestamp': 'timestamp',
    'checksum256': 'varchar(64)',
    'public_key': 'varchar',
    'bytes': 'bytea',
    'transaction_status': 'transaction_status_type',
};

let indexes = '';
let functions = '';

function generate_index({ table, index, sort_keys, history_keys }) {
    indexes += `
        create index if not exists ${index} on chain.${table}(
            ${sort_keys.map(x => `"${x.name}",`).join('\n            ')}
            ${history_keys.map(x => `"${x.name + (x.desc ? '" desc' : '"')}`).join(',\n            ')}
        );
    `;
}

// todo: This is a stripped-down version of generate_state. It likely needs reoptimization.
function generate({ table, index, keys, sort_keys, history_keys, ...rest }) {
    generate_index({ table, index, sort_keys, history_keys });

    const fn_name = 'chain.' + rest['function'];
    const fn_args = prefix => sort_keys.map(x => `${prefix}${x.name} ${x.type},`).join('\n            ');
    const keys_tuple_type = (prefix, suffix, sep) => keys.map(x => `${prefix}${x.name}${suffix}::${x.type}`).join(sep);
    const sort_keys_tuple = (prefix, suffix, sep) => sort_keys.map(x => `${prefix}${x.name}${suffix}`).join(sep);

    const search = (compare, indent) => `
        ${indent}for search in
        ${indent}    select
        ${indent}        *
        ${indent}    from
        ${indent}        chain.${table}
        ${indent}    where
        ${indent}        (${sort_keys_tuple('"', '"', ', ')}) ${compare} (${sort_keys_tuple('"first_', '"', ', ')})
        ${indent}        and ${table}.block_index <= max_block_index
        ${indent}    order by
        ${indent}        ${sort_keys_tuple('"', '"', ',\n                ' + indent)},
        ${indent}        ${history_keys.map(x => `"${x.name + (x.desc ? '" desc' : '"')}`).join(',\n                ' + indent)}
        ${indent}    limit 1
        ${indent}loop
        ${indent}    if (${sort_keys_tuple('search."', '"', ', ')}) > (${sort_keys_tuple('last_', '', ', ')}) then
        ${indent}        return;
        ${indent}    end if;
        ${indent}    found_result = true;
        ${indent}    return next search;
        ${indent}    num_results = num_results + 1;
        ${indent}end loop;
    `;

    functions += `
        drop function if exists ${fn_name};
        create function ${fn_name}(
            max_block_index bigint,
            ${fn_args('first_')}
            ${fn_args('last_')}
            max_results integer
        ) returns setof chain.${table}
        as $$
            declare
                search record;
                num_results integer = 0;
                found_result bool = false;
            begin
                if max_results <= 0 then
                    return;
                end if;
                ${search('>=', '        ')}
                loop
                    exit when not found_result or num_results >= max_results;
                    found_result = false;
                    ${search('>', '            ')}
                end loop;
            end 
        $$ language plpgsql;
    `;
} // generate

function generate_state({ table, index, keys, sort_keys, history_keys, ...rest }) {
    generate_index({ table, index, sort_keys, history_keys });

    const fn_name = 'chain.' + rest['function'];
    const fn_args = prefix => sort_keys.map(x => `${prefix}${x.name} ${x.type},`).join('\n            ');
    const keys_tuple_type = (prefix, suffix, sep) => keys.map(x => `${prefix}${x.name}${suffix}::${x.type}`).join(sep);
    const sort_keys_tuple = (prefix, suffix, sep) => sort_keys.map(x => `${prefix}${x.name}${suffix}`).join(sep);

    const key_search = (compare, indent) => `
        ${indent}for key_search in
        ${indent}    select
        ${indent}        ${sort_keys_tuple('"', '"', ', ')}
        ${indent}    from
        ${indent}        chain.${table}
        ${indent}    where
        ${indent}        (${sort_keys_tuple('"', '"', ', ')}) ${compare} (${sort_keys_tuple('"first_', '"', ', ')})
        ${indent}    order by
        ${indent}        ${sort_keys_tuple('"', '"', ',\n                ' + indent)},
        ${indent}        ${history_keys.map(x => `"${x.name + (x.desc ? '" desc' : '"')}`).join(',\n                ' + indent)}
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
        ${indent}            chain.${table}
        ${indent}        where
        ${indent}            ${sort_keys.map(x => `${table}."${x.name}" = key_search."${x.name}"`).join('\n                    ' + indent + 'and ')}
        ${indent}            and ${table}.block_index <= max_block_index
        ${indent}        order by
        ${indent}            ${sort_keys_tuple('"', '"', ',\n                    ' + indent)},
        ${indent}            ${history_keys.map(x => `"${x.name + (x.desc ? '" desc' : '"')}`).join(',\n                    ' + indent)}
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
        ) returns setof chain.${table}
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
} // generate_state

const config = JSON.parse(fs.readFileSync('../src/query-config.json', 'utf8'));
const tables = {};
for (let table of config.tables) {
    const fields = [];
    for (let field of table.fields)
        fields[field.name] = field;
    tables[table.name] = { fields };
}

function get_type(type) {
    return type_map[type] || '???' + type;
}

function fill_types(query, fields) {
    for (let field of fields)
        field.type = get_type(tables[query.table].fields[field.name].type);
}

for (let query of config.queries) {
    fill_types(query, query.keys);
    fill_types(query, query.sort_keys);
    fill_types(query, query.history_keys);
    generate_state(query);
}

console.log(indexes);
console.log(functions);
