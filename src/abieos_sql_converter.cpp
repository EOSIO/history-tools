#include "abieos_sql_converter.hpp"
#include <pqxx/util.hxx>

bool remove_trailing_question_mark(std::string& abi_type_name) {
    if (abi_type_name.size() >= 1 && abi_type_name.back() == '?') {
        abi_type_name.resize(abi_type_name.size() - 1);
        return true;
    }
    return false;
}

std::string quote_name(std::string x) { return "\"" + x + "\""; }

inline constexpr char number_to_digit(int i) noexcept { return static_cast<char>(i + '0'); }

std::string escape_table_field(const std::string& s) {
    std::string result;
    result.reserve(s.size() * 2 + 2);
    for (auto c : s) {
        switch (c) {
        case '\b': result += "\\b"; break;  // Backspace
        case '\f': result += "\\f"; break;  // Vertical tab
        case '\n': result += "\\n"; break;  // Form feed
        case '\r': result += "\\r"; break;  // Newline
        case '\t': result += "\\t"; break;  // Tab
        case '\v': result += "\\v"; break;  // Carriage return
        case '\\': result += "\\\\"; break; // Backslash
        default:
            if (c < ' ' or c > '~') {
                // Non-ASCII.  Escape as octal number.
                result += "\\";
                auto u{static_cast<unsigned char>(c)};
                for (auto i = 2; i >= 0; --i)
                    result += number_to_digit((u >> (3 * i)) & 0x07);
            } else {
                result += c;
            }
            break;
        }
    }
    return result;
}

std::string escape_composite_field(std::string elem) {
    std::string result;
    result.resize(2 * elem.size() + 2);
    auto here = result.begin();
    *here++   = '"';
    for (char const c : elem) {
        if (c == '\\' or c == '"')
            *here++ = '\\';
        *here++ = c;
    }
    *here++ = '"';
    result.erase(here, result.end());
    return result;
}

std::string escape_field(std::string elem, abieos_sql_converter::field_kind_t field_kind) {
    if (field_kind == abieos_sql_converter::table_field)
        return escape_table_field(elem);
    else
        return escape_composite_field(elem);
}

abieos_sql_converter::field_def
get_field_def(std::string schema_name, const eosio::abi_field& field, const abieos_sql_converter::basic_converters_t& basic_converters) {
    auto        type = field.type;
    std::string type_suffix;
    if (type->optional_of())
        type = type->optional_of();
    else if (type->array_of()) {
        type        = type->array_of();
        type_suffix = "[]";
    }

    if (type->as_variant()) {
        return {field.name, schema_name + "." + type->name + type_suffix};
    } else if (type->as_struct()) {
        if (type->as_struct()->fields.size()>1)
            return {field.name, schema_name + "." + type->name + type_suffix};
        else {
            auto nested_field = get_field_def(schema_name, type->as_struct()->fields[0], basic_converters);
            return {field.name, nested_field.type + type_suffix};
        }
    }
    else {
        auto abi_type_name = type->name;
        auto it            = basic_converters.find(abi_type_name);
        if (it != basic_converters.end()) {
            std::string type_name = it->second.name;
            if (type_name == "transaction_status_type")
                type_name = schema_name + "." + type_name;
            return {field.name, type_name + type_suffix};
        } else {
            throw std::runtime_error("don't know sql type for abi type: " + abi_type_name);
        }
    }
}

std::vector<abieos_sql_converter::field_def> get_field_defs(
    std::string schema_name, const eosio::abi_type::struct_* struct_abi_type,
    const abieos_sql_converter::basic_converters_t& basic_converters) {
    std::vector<abieos_sql_converter::field_def> result;
    for (auto& f : struct_abi_type->fields) {
        result.push_back(get_field_def(schema_name, f, basic_converters));
    }
    return result;
}

abieos_sql_converter::union_fields_t::union_fields_t(std::string schema_name, const eosio::abi_type::variant& elements,
    const abieos_sql_converter::basic_converters_t& basic_converters) {
    for (unsigned i = 0; i < elements.size(); ++i) {
        const auto& variant_element = elements[i];
        if (variant_element.type->as_struct()) {
            const auto&                                  struct_fields = variant_element.type->as_struct()->fields;
            std::vector<abieos_sql_converter::field_def> pending_fields;
            for (unsigned j = 0; j < struct_fields.size(); ++j) {
                auto& f     = struct_fields[j];
                auto  f_def = get_field_def(schema_name, f, basic_converters);
                auto  itr   = std::find_if(this->begin(), this->end(), [nm = f_def.name](const auto& item) { return item.name == nm; });
                if (itr != this->end()) {
                    if (pending_fields.size()) {
                        itr = this->insert(itr, pending_fields.begin(), pending_fields.end());
                        itr += pending_fields.size();
                        pending_fields.clear();
                    }

                    // same field name
                    if (itr->type != f_def.type) {
                        // different field type
                        itr->name.insert(0, elements[i - 1].name + "_");
                        this->insert(++itr, f_def);
                    } 
                } else {
                    pending_fields.push_back(f_def);
                }
            }
            this->insert(this->end(), pending_fields.begin(), pending_fields.end());
        } else {
            this->push_back(get_field_def(schema_name, variant_element, basic_converters));
        }
    }
}

void abieos_sql_converter::create_sql_type(const eosio::abi_type* type, const std::function<void(std::string)>& exec, bool is_union_field) {
    if (type->as_struct()) {
        create_sql_type(type->name, type->as_struct(), exec, is_union_field);
    } else if (type->array_of()) {
        create_sql_type(type->array_of(), exec, false);
    } else if (type->optional_of()) {
        create_sql_type(type->optional_of(), exec, is_union_field);
    } else if (type->as_variant()) {
        create_sql_type(type->name, type->as_variant(), exec);
    }
}

std::string abieos_sql_converter::create_sql_type(
    std::string name, const eosio::abi_type::struct_* struct_abi_type, const std::function<void(std::string)>& exec, bool is_union_field) {
    std::string sql_type = schema_name + "." + quote_name(name);
    if (created_composite_types.count(sql_type) == 0) {
        std::string sub_fields;
        for (auto& f : struct_abi_type->fields) {
            create_sql_type(f.type, exec, false);
            auto f_def = get_field_def(schema_name, f, basic_converters);
            sub_fields += ", " + quote_name(f_def.name) + " " + f_def.type;
        }
        if (!is_union_field) {
            if (struct_abi_type->fields.size() > 1) {
                std::string query = "create type " + sql_type + " as (" + sub_fields.substr(2) + ")";
                exec(query);
            }   
            else {
                // if we have only one filed, just flattern it. 
                auto pos = sub_fields.rfind(" ");
                sql_type = sub_fields.substr(pos+1);
            }
        } 
        created_composite_types.emplace(sql_type);
    }
    return sql_type;
}

std::string abieos_sql_converter::create_sql_type(
    std::string name, const eosio::abi_type::variant* variant_abi_type, const std::function<void(std::string)>& exec) {
    std::string sql_type = schema_name + "." + quote_name(name);
    if (created_composite_types.count(sql_type) == 0) {

        for (auto& elem : *variant_abi_type) {
            create_sql_type(elem.type, exec, true);
        }

        auto        union_fields = variant_union_fields.try_emplace(name, schema_name, *variant_abi_type, basic_converters).first->second;
        std::string query        = "create type " + sql_type + " as (";
        for (const auto& field : union_fields) {
            query += quote_name(field.name) + " " + field.type + ",";
        }
        query.back() = ')';
        exec(query);
        created_composite_types.emplace(sql_type);
    }
    return sql_type;
}

void abieos_sql_converter::create_table(
    std::string table_name, const eosio::abi_type& type, std::string fields_prefix, const std::vector<std::string>& keys,
    const std::function<void(std::string)>& exec) {
    std::string fields = fields_prefix;
    if (type.as_struct()) {
        for (auto& field : type.as_struct()->fields) {
            create_sql_type(field.type, exec, false);
            auto f_def = get_field_def(schema_name, field, basic_converters);
            fields += ", " + quote_name(f_def.name) + " " + f_def.type;
        }
    } else if (type.as_variant()) {
        for (auto& elem : *type.as_variant()) {
            create_sql_type(elem.type, exec, true);
        }

        auto union_fields = variant_union_fields.try_emplace(type.name, schema_name, *type.as_variant(), basic_converters).first->second;

        for (auto& field : union_fields) {
            fields += ", " + quote_name(field.name) + " " + field.type;
        }
    }
    std::string query = "create table " + schema_name + "." + quote_name(table_name) + " (" + fields + ", primary key(" +
                        pqxx::separated_list(",", keys.begin(), keys.end(), [](auto x) { return quote_name(*x); }) + "))";
    exec(query);
}

bool is_numeric_type(std::string type_name) {
    const std::string numeric_types[] = {"int8",   "uint8",   "int16",   "uint16",  "int32",    "uint32",    "int64",   "uint64",
                                         "int128", "uint128", "float32", "float64", "float128", "varuint32", "varint32"};
    auto              e               = std::end(numeric_types);
    return std::find(std::begin(numeric_types), e, type_name) != e;
}

std::string abieos_sql_converter::to_sql_value(eosio::input_stream& bin, const eosio::abi_type& type_ref, field_kind_t field_kind) {
    const eosio::abi_type* type    = &type_ref;
    bool                   present = true;
    if (type->optional_of()) {
        bin.read_raw(present);
        type = type->optional_of();
        if (!present) {
            if (field_kind == abieos_sql_converter::table_field)
                return "\\N";
            return "";
        }
    }

    std::vector<std::string> values;

    if (type->as_struct()) {
        to_sql_values(bin, *type->as_struct(), values, composite_field);
        if (values.size() > 1)
            return escape_field("(" + pqxx::separated_list(",", values.begin(), values.end()) + ")", field_kind);
        return values[0];
    } else if (type->as_variant()) {
        to_sql_values(bin, type->name, *type->as_variant(), values, composite_field);
        return escape_field("(" + pqxx::separated_list(",", values.begin(), values.end()) + ")", field_kind);
    } else if (type->array_of()) {
        uint32_t n;
        varuint32_from_bin(n, bin);
        values.reserve(n);
        for (int i = 0; i < n; ++i) {
            values.push_back(to_sql_value(bin, *type->array_of(), composite_field));
        }
        return escape_field("{" + pqxx::separated_list(",", values.begin(), values.end()) + "}", field_kind);
    } else {
        auto abi_type_name = type->name;
        auto it            = basic_converters.find(abi_type_name);
        if (it != basic_converters.end()) {
            if (!it->second.bin_to_sql)
                throw std::runtime_error("don't know how to process " + abi_type_name);
            std::string r = it->second.bin_to_sql(bin);
            auto& sql_type_name = it->second.name;
            if (field_kind == composite_field) {
                auto sql_type_name = it->second.name;
                if ( strncmp(sql_type_name,"varchar", 7) == 0 || strcmp(sql_type_name,"bytea") == 0 )
                    return escape_composite_field(r);
            } else if (r.empty() && strcmp(sql_type_name, "timestamp") == 0) {
                return "\\N";
            }
            return r;
        }
        __builtin_unreachable();
    }
}

void abieos_sql_converter::to_sql_values(
    eosio::input_stream& bin, const eosio::abi_type::struct_& struct_abi_type, std::vector<std::string>& values, field_kind_t field_kind) {
    values.reserve(values.size() + struct_abi_type.fields.size());
    for (auto& f : struct_abi_type.fields)
        values.push_back(to_sql_value(bin, *f.type, field_kind));
}

inline bool ends_with(std::string const & value, std::string const & ending)
{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

void abieos_sql_converter::to_sql_values(
    eosio::input_stream& bin, std::string type_name, const eosio::abi_type::variant& variant_abi_type, std::vector<std::string>& values,
    field_kind_t field_kind) {
    uint32_t v;
    varuint32_from_bin(v, bin);
    auto&       alternative  = variant_abi_type.at(v);
    const auto& union_fields = variant_union_fields.try_emplace(type_name, schema_name, variant_abi_type, basic_converters).first->second;
    auto const& alternative_fields = alternative.type->as_struct()->fields;
    auto        field_itr          = alternative_fields.begin();
    values.reserve(values.size() + union_fields.size());
    for (const auto& field : union_fields) {
        auto fname = field.name;
        if (field_itr != alternative_fields.end() && (fname == field_itr->name || fname == (alternative.name + "_" + field_itr->name))) {
            values.push_back(to_sql_value(bin, *field_itr->type, field_kind));
            ++field_itr;
        } else if (ends_with(field.type, "[]"))
            values.emplace_back(escape_field("{}", field_kind));
        else if (field_kind == table_field && field.type.find(schema_name) == 0) {
            // For a value of composite value and when it is a field of the top level table, it must use "\\N" to represent the empty value;
            // however, it must use empty string to represent empty value when it's a field of a type.
            values.emplace_back("\\N");
        }
        else
            values.emplace_back("");
    }
}
