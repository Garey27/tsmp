#include "prefix_splitter.hpp"


#include "types.hpp"
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <iterator>

#warning "dont include iostream"
#include <iostream>
namespace data {

prefix_splitter_t::prefix_splitter_t(const std::vector<record_decl_t>& records) {
    for(const auto& record : records) {
        add_record(record);
    }
}

void prefix_splitter_t::add_record(const record_decl_t& record) {
    for(auto& r : m_records) {
        if(field_list_match(r, record)) {
            r.name = "<unknown>";
            std::cout << "Reject introspection for " << record.name << " because of duplication.\n";
            return;
        } else {
            std::cout << "Add introspection for class " << record.name << '\n';
        }
    }
    for(const auto& field : record.fields) {
        add_field(field);
    }
    for(const auto& function : record.functions) {
        add_function(function);
    }
    m_records.emplace_back(record);
}

const std::vector<field_decl_t>& prefix_splitter_t::fields() const {
    return m_fields;
};

const std::vector<record_decl_t>& prefix_splitter_t::records() const {
    return m_records;
};

std::string prefix_splitter_t::strip_special_chars(std::string input) {
    auto is_special = [](char c) { 
        return !((c>='a'&& c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c =='_');
    };
    auto pos = input.begin();
    while((pos=std::find_if(pos, input.end(), is_special)) != input.end()) {
        auto& c = *pos;
        switch(c) {
            case '~': 
                c = 't';
                break;
            case '=':
                c = 'e';
                break;
            case '+':
                c = 'p';
                break;
            case '-':
                c = 'm';
                break;
            case '*':
                c = 'u';
                break;
            case '/':
                c = 'd';
                break;
            case '|':
                c = 'l';
                break;                 
            default:
                c = 'x';
        }
    }
    return input;
}

std::string prefix_splitter_t::render() const {
    std::string result;
    for(auto field : m_fields) {
        result += fmt::format(
            "template<class T> concept has_field_{0} = requires(T) {{ T::{0}; }};\n",
                field.name
        );
    }

    std::vector<std::string> escaped_function_names;
    std::transform(
        m_functions.begin(),
        m_functions.end(),
        std::back_inserter(escaped_function_names),
        [](const auto& fn_decl) {
            return strip_special_chars(fn_decl.name);
        }
    );

    for(auto function_name : escaped_function_names) {
        result += fmt::format(
            "template<class T> concept has_function_{0} = requires(T) {{ &T::{0}; }};\n",
            function_name
        );
    }
    result += '\n';

    for(auto record : m_records) {
        // trait impl ...
        std::string fields, functions;
        int i = 0;
        for(const auto& f : record.fields) {
            fields += fmt::format("\t\t\tfield_description_t{{ {0}, \"{1}\", &T::{1} }},\n", i++, f.name);
        }
        i = 0;
        for(const auto& f : record.functions) {
            if(f.name == "~") continue;
            functions += fmt::format("\t\t\tfield_description_t{{ {0}, \"{1}\", &T::{1} }},\n", i++, f.name);
        }
        std::string requirements = (!record.functions.empty()||!record.fields.empty()) ? "requires " : "";
        if(!record.fields.empty()) {
            fields.resize(fields.size()-2);
            
            requirements += fmt::format("has_field_{}<T>", fmt::join(record.fields, "<T> && has_field_"));
        }
        if(!record.functions.empty()) {
            functions.resize(functions.size()-2);
            if(!record.fields.empty()) {
                requirements += " && " ;
            }
            requirements += fmt::format("has_function_{}<T>", fmt::join(escaped_function_names, "<T> && has_function_"));
        }

        result += 
            fmt::format(
R"(template <class T>
{}
struct reflect{} {{
static constexpr bool reflectable = true;
constexpr static auto name() {{
    return "{}";
}}
constexpr static auto fields() {{
    return std::make_tuple(
{}
    );
}}

constexpr static auto functions() {{
    return std::make_tuple(
{}
    );
}}
}};

)",
                requirements,
                requirements.empty() ? "" : "<T>", 
                record.name,
                fields,
                functions
            );
    }
    return result;
}

bool prefix_splitter_t::has_field(const field_decl_t& field, const std::vector<field_decl_t>& field_list) const {
    const auto it = std::find_if(
        field_list.begin(),
        field_list.end(),
        [name = field.name](const auto& f){
                return f.name == name; 
        }
    );
    return it != field_list.end();
}

bool prefix_splitter_t::has_function(const function_decl_t& function, const std::vector<function_decl_t>& function_list) const {
    const auto it = std::find_if(
        function_list.begin(),
        function_list.end(),
        [name = function.name](const auto& f){
                return f.name == name; 
        }
    );
    return it != function_list.end();
}

void prefix_splitter_t::add_field(const field_decl_t& field) {
    if(!has_field(field, m_fields)) {
        m_fields.emplace_back(field);
    }
}

void prefix_splitter_t::add_function(const function_decl_t& function) {
    if(function.name == "~") return;
    if(!has_function(function, m_functions)) {
        m_functions.emplace_back(function);
    }
}

bool prefix_splitter_t::field_list_match(const record_decl_t& lhs, const record_decl_t& rhs) const {
    if(lhs.fields.size() != rhs.fields.size() || lhs.functions.size() != rhs.functions.size() ) {
        return false;
    }

    for(auto l : lhs.fields) {
        if(!has_field(l, rhs.fields)) {
            return false;
        }
    }

    for(auto l : lhs.functions) {
        if(!has_function(l, rhs.functions)) {
            return false;
        }
    }

    return true;
}

}