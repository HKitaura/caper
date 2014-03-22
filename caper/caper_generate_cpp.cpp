// Copyright (C) 2008 Naoyuki Hirayama.
// All Rights Reserved.

// $Id$

#include "caper_ast.hpp"
#include "caper_generate_cpp.hpp"
#include "caper_format.hpp"
#include "caper_stencil.hpp"
#include <algorithm>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

namespace {

void make_signature(
    const symbol_map_type&                  nonterminal_types,
    const tgt::parsing_table::rule_type&    rule,
    const semantic_action&                  sa,
    std::vector<std::string>&             signature) {
    // function name
    signature.push_back(sa.name);

    // return value
    signature.push_back(
        (*nonterminal_types.find(rule.left().name())).second);

    // arguments
    for (size_t l = 0 ; l <sa.args.size() ; l++) {
        signature.push_back((*sa.args.find(l)).second.type);
    }
}

} // unnamed namespace

void generate_cpp(
    const std::string&                      src_filename,
    std::ostream&                           os,
    const GenerateOptions&                  options,
    const symbol_map_type&                  ,
    const symbol_map_type&                  nonterminal_types,
    const std::map<size_t, std::string>&    token_id_map,
    const action_map_type&                  actions,
    const tgt::parsing_table&               table) {

    const char* ind1 = "    ";

#ifdef _WINDOWS
    char basename[_MAX_PATH];
    char extension[_MAX_PATH];
    _splitpath(src_filename.c_str(), NULL, NULL, basename, extension);
    std::string filename = std::string(basename)+ extension;
# else
    std::string filename = src_filename;
#endif

    std::string headername = filename;
    for (auto& x: headername){
        if (!isalpha(x) && !isdigit(x)) {
            x = '_';
        } else {
            x = toupper(x);
        }
    }

    // once header / notice / URL / includes / namespace header
    stencil(
        os, R"(
#ifndef ${headername}_
#define ${headername}_

// This file was automatically generated by Caper.
// (http://jonigata.github.io/caper/caper.html)

#include <cstdlib>
#include <cassert>
$${debug_include}
$${use_stl}

namespace ${namespace_name} {

)",
        {
            {"headername", headername},
            {"debug_include",
                {options.debug_parser ? "#include <iostream>" : ""}},
            {"use_stl",
                {options.dont_use_stl ? "" : "#include <vector>"}},
            {"namespace_name", options.namespace_name}
        });

    if (!options.external_token) {
        // token enumeration
        stencil(
            os, R"(
enum Token {
$${tokens}
};

inline const char* token_label(Token t) {
    static const char* labels[] = {
$${labels}
    };
    return labels[t];
}

)",
            {
                {"tokens", [&](std::ostream& os){
                            for (size_t i = 0 ;
                                 i < token_id_map.size() ; i++) {
                                os << "    " << options.token_prefix
                                   << (*token_id_map.find(i)).second
                                   << ",\n";
                            }
                        }},
                {"labels", [&](std::ostream& os){
                            for (size_t i = 0 ;
                                 i < token_id_map.size() ; i++) {
                                os << "        \"" << options.token_prefix
                                   << (*token_id_map.find(i)).second
                                   << "\",\n";
                            }
                        }}
            });

    }

    // stack class header
    if (!options.dont_use_stl) {
        // STL version
        stencil(
            os, R"(
template <class T, unsigned int StackSize>
class Stack {
public:
    Stack() { gap_ = 0; }

    void rollback_tmp() {
        gap_ = stack_.size();
        tmp_.clear();
    }

    void commit_tmp() {
        // may throw
        stack_.reserve(gap_ + tmp_.size());
	   
        // expect not to throw
        stack_.erase(stack_.begin()+ gap_, stack_.end());
        stack_.insert(stack_.end(), tmp_.begin(), tmp_.end());
    }
    bool push(const T& f) {
        if (StackSize != 0 && StackSize <= stack_.size()+ tmp_.size()) {
            return false;
        }
        tmp_.push_back(f);
        return true;
    }
	   
    void pop(size_t n) {
        if (tmp_.size() < n) {
            n -= tmp_.size();
            tmp_.clear();
            gap_ -= n;
        } else {
            tmp_.erase(tmp_.end() - n, tmp_.end());
        }
    }

    const T& top() {
        if (!tmp_.empty()) {
            return tmp_.back();
        } else {
            return stack_[gap_ - 1];
        }
    }
	   
    const T& get_arg(size_t base, size_t index) {
        size_t n = tmp_.size();
        if (base - index <= n) {
            return tmp_[n - (base - index)];
        } else {
            return stack_[gap_ - (base - n) + index];
        }
    }
	   
    void clear() {
        stack_.clear();
    }
	   
    bool empty() const {
        if (!tmp_.empty()) {
            return false;
        } else {
            return gap_ == 0;
        }
    }
	   
    size_t depth() const {
        return gap_ + tmp_.size();
    }
	   
private:
    std::vector<T> stack_;
    std::vector<T> tmp_;
    size_t gap_;
	   
};

)",
        {});
    } else {
        // bulkmemory version
        stencil(
            os, R"(
template <class T, unsigned int StackSize>
class Stack {
public:
    Stack() { top_ = 0; gap_ = 0; tmp_ = 0; }
    ~Stack() {}

    void rollback_tmp() {
        for (size_t i = 0 ; i <tmp_ ; i++) {
            at(StackSize - 1 - i).~T(); // explicit destructor
        }
        tmp_ = 0;
        gap_ = top_;
    }

    void commit_tmp() {
        for (size_t i = 0 ; i <tmp_ ; i++) {
            if (gap_ + i <top_) {
                at(gap_ + i) = at(StackSize - 1 - i);
            } else {
                // placement new copy constructor
                new (&at(gap_ + i)) T(at(StackSize - 1 - i));
            }
            at(StackSize - 1 - i).~T(); // explicit destructor
        }
        if (gap_ + tmp_ <top_) {
            for (int i = 0 ; i <int(top_ - gap_ - tmp_); i++) {
                at(top_ - 1 - i).~T(); // explicit destructor
            }
        }

        top_ = gap_ = gap_ + tmp_;
        tmp_ = 0;
    }

    bool push( const T& f ) {
        if (StackSize <= top_ + tmp_) { return false; }
        // placement new copy constructor
        new (&at(StackSize - 1 - tmp_++)) T(f);
        return true;
    }

    void pop(size_t n) {
        size_t m = n; if (m > tmp_) { m = tmp_; }

        for (size_t i = 0 ; i <m ; i++) {
            at(StackSize - tmp_ + i).~T(); // explicit destructor
        }

        tmp_ -= m;
        gap_ -= n - m;
    }

    const T& top() {
        if (0 <tmp_) {
            return at(StackSize - 1 -(tmp_-1));
        } else {
            return at(gap_ - 1);
        }
    }

    const T& get_arg(size_t base, size_t index) {
        if (base - index <= tmp_) {
            return at(StackSize-1 - (tmp_ -(base - index)));
        } else {
            return at(gap_ -(base - tmp_) + index);
        }
    }

    void clear() {
        rollback_tmp();
        for (size_t i = 0 ; i <top_ ; i++) {
            at(i).~T(); // explicit destructor
        }
        top_ = gap_ = tmp_ = 0;
    }

    bool empty() const {
        if (0 < tmp_) {
            return false;
        } else {
            return gap_ == 0;
        }
    }

    size_t depth() const {
        return gap_ + tmp_;
    }

private:
    T& at(size_t n) {
        return *(T*)(stack_ +(n * sizeof(T)));
    }

private:
    char stack_[ StackSize * sizeof(T) ];
    size_t top_;
    size_t gap_;
    size_t tmp_;

};

)",
            {});
    }

    // parser class header
    std::string default_stacksize = "0";
    if (options.dont_use_stl) {
        default_stacksize = "1024";
    }
        
    std::string template_parameters =
        "class Value, class SemanticAction,\n          unsigned int StackSize = " +
        default_stacksize;
    if (options.external_token) {
        template_parameters = "class Token, " + template_parameters;
    }

    stencil(
        os, R"(
template <${template_parameters}>
class Parser {
public:
    typedef Token token_type;
    typedef Value value_type;

    enum Nonterminal {
)",
        {
            {"template_parameters", template_parameters}
        });

    for (const auto& nonterminal_type: nonterminal_types) {
        stencil(
            os, R"(
        Nonterminal_${nonterminal_name},
)",
            {
                {"nonterminal_name", nonterminal_type.first}
            });
    }
    
    stencil(
        os, R"(
    };

public:
    Parser(SemanticAction& sa) : sa_(sa) { reset(); }

    void reset() {
        error_ = false;
        accepted_ = false;
        clear_stack();
        rollback_tmp_stack();
        if (push_stack(${first_state}, value_type())) {
            commit_tmp_stack();
        } else {
            sa_.stack_overflow();
            error_ = true;
        }
    }

    bool post(token_type token, const value_type& value) {
        rollback_tmp_stack();
        error_ = false;
        while ((this->*(stack_top()->entry->state))(token, value))
            ; // may throw
        if (!error_) {
            commit_tmp_stack();
        } else {
            recover(token, value);
        }
        return accepted_ || error_;
    }

    bool accept(value_type& v) {
        assert(accepted_);
        if (error_) { return false; }
        v = accepted_value_;
        return true;
    }

    bool error() { return error_; }

)",
        {
            {"first_state", table.first_state()},
            {"first_state_handle_error",
                    table.states()[table.first_state()].handle_error}
        });

    // implementation
    stencil(
        os, R"(
private:
    ${typedef_self_type}

    typedef bool (self_type::*state_type)(token_type, const value_type&);
    typedef bool (self_type::*gotof_type)(Nonterminal, const value_type&);

    bool            accepted_;
    bool            error_;
    value_type      accepted_value_;
    SemanticAction& sa_;

    struct table_entry {
        state_type  state;
        gotof_type  gotof;
        bool        handle_error;
    };

    struct stack_frame {
        const table_entry*  entry;
        value_type          value;

        stack_frame(const table_entry* e, const value_type& v)
            : entry(e), value(v) {}
    };

)",
        {
            {"typedef_self_type", {
                    options.external_token ? 
                        "typedef Parser<Token, Value, SemanticAction, StackSize> self_type;" :
                        "typedef Parser<Value, SemanticAction, StackSize> self_type;"}
            }
        });

    // stack operation
    stencil(
        os, R"(
    Stack<stack_frame, StackSize> stack_;

    bool push_stack(int state_index, const value_type& v) {
        bool f = stack_.push(stack_frame(entry(state_index), v));
        assert(!error_);
        if (!f) { 
            error_ = true;
            sa_.stack_overflow();
        }
        return f;
    }

    void pop_stack(size_t n) {
        stack_.pop( n );
    }

    const stack_frame* stack_top() {
        return &stack_.top();
    }

    const value_type& get_arg(size_t base, size_t index) {
        return stack_.get_arg(base, index).value;
    }

    void clear_stack() {
        stack_.clear();
    }

    void rollback_tmp_stack() {
        stack_.rollback_tmp();
    }

    void commit_tmp_stack() {
        stack_.commit_tmp();
    }

)",
        {});

    if (options.recovery) {
        stencil(
            os, R"(
    void recover(Token token, const value_type& value) {
        rollback_tmp_stack();
        error_ = false;
$${debmes:start}
        while(!stack_top()->entry->handle_error) {
            pop_stack(1);
            if (stack_.empty()) {
$${debmes:failed}
                error_ = true;
                return;
            }
        }
$${debmes:done}
        // post error_token;
$${debmes:post_error_start}
        while ((this->*(stack_top()->entry->state))(${recovery_token}, value_type()));
$${debmes:post_error_done}
        commit_tmp_stack();
        // repost original token
        // if it still causes error, discard it;
$${debmes:repost_start}
        while ((this->*(stack_top()->entry->state))(token, value));;
$${debmes:repost_done}
        if (!error_) {
            commit_tmp_stack();
        }
        if (token != ${token_eof}) {
            error_ = false;
        }
    }

)",
            {
                {"recovery_token",
                    options.token_prefix + options.recovery_token},
                {"token_eof", options.token_prefix + "eof"},
                {"debmes:start", {
                        options.debug_parser ?
                            R"(        std::cerr << "recover rewinding start: stack depth = " << stack_.depth() << "\n";
)" :
                            ""}},
                {"debmes:failed", {
                        options.debug_parser ?
                            R"(        std::cerr << "recover rewinding failed\n";
)" :
                            ""}},
                {"debmes:done", {
                        options.debug_parser ?
                            R"(        std::cerr << "recover rewinding done: stack depth = " << stack_.depth() << "\n";
)":
                            ""}},
                {"debmes:post_error_start", {
                        options.debug_parser ?
                            R"(        std::cerr << "posting error token\n";
)" :
                            ""}},
                {"debmes:post_error_done", {
                        options.debug_parser ?
                            R"(        std::cerr << "posting error token done\n";
)" :
                            ""}},
                {"debmes:repost_start", {
                        options.debug_parser ?
                            R"(        std::cerr << "reposting original token\n";
)" :
                            ""}},
                {"debmes:repost_done", {
                        options.debug_parser ? 
                            R"(        std::cerr << "reposting original token done\n";
)" :
                            ""}}
            });
    } else {
        stencil(
            os, R"(
    void recover(Token, const value_type&) {
    }

)",
            {});
    }

    stencil(
        os, R"(
    bool call_nothing(Nonterminal nonterminal, int base) {
        pop_stack(base);
        return (this->*(stack_top()->entry->gotof))(nonterminal, value_type());
    }

)",
        {});

    // member function signature -> index
    std::map<std::vector<std::string>, int> stub_index;
    {
        // member function name -> count
        std::unordered_map<std::string, int> stub_count; 

        // action handler stub
        for (const auto& pair: actions) {
            const auto& rule = pair.first;

            const auto& rule_type =
                (*nonterminal_types.find(rule.left().name())).second;
            const semantic_action& sa = pair.second;

            // make signature
            std::vector<std::string> signature;

            // ... function name
            signature.push_back(sa.name);

            // ... return value
            signature.push_back(rule_type);

            // ... arguments
            for (size_t l = 0 ; l < sa.args.size() ; l++) {
                signature.push_back((*sa.args.find(l)).second.type);
            }

            // skip duplicated
            if (0 < stub_index.count(signature)) {
                continue;
            }

            // make function name
            if (stub_count.count(sa.name) == 0) {
                stub_count[sa.name] = 0;
            }
            int index = stub_count[sa.name];
            stub_index[signature] = index;
            stub_count[sa.name] = index+1;

            // header
            stencil(
                os, R"(
    bool call_${index}_${sa_name}(Nonterminal nonterminal, int base${args}) {
)",
                {
                    {"index", index},
                    {"sa_name", sa.name},
                    {"args", [&](std::ostream& os) {
                                for (size_t l = 0 ;
                                     l < sa.args.size() ; l++) {
                                    os << ", int arg_index" << l;
                                }
                            }}
                });

            // automatic argument conversion
            for (size_t l = 0 ; l < sa.args.size() ; l++) {
                const auto& arg = (*sa.args.find(l)).second;
                stencil(
                    os, R"(
        ${arg_type} arg${index}; sa_.downcast(arg${index}, get_arg(base, arg_index${index}));
)",
                    {
                        {"arg_type", {arg.type}},
                        {"index", {l}}
                    });
            }

            // semantic action / automatic value conversion
            stencil(
                os, R"(
        ${nonterminal_type} r = sa_.${semantic_action_name}(${args});
        value_type v; sa_.upcast(v, r);
        pop_stack(base);
        return (this->*(stack_top()->entry->gotof))(nonterminal, v);
    }

)",
                {
                    {"nonterminal_type", rule_type},
                    {"semantic_action_name", sa.name},
                    {"args", [&](std::ostream& os) {
                                bool first = true;
                                for (size_t l = 0 ;
                                     l < sa.args.size() ; l++) {
                                    if (first) { first = false; }
                                    else { os << ", "; }
                                    os << "arg" << l;
                                }
                            }}
                });
        }
    }

    // states handler
    for (const auto& state: table.states()) {
        // state header
        stencil(
            os, R"(
    bool state_${state_no}(token_type token, const value_type& value) {
$${debmes:state}
        switch(token) {
)",
            {
                {"state_no", state.no},
                {"debmes:state", [&](std::ostream& os){
                            if (options.debug_parser) {
                                stencil(
                                    os, R"(
        std::cerr << "state_${state_no} << " << token_label(token) << "\n";
)",
                                    {
                                        {"state_no", state.no}
                                    });
                            }}}
            });

        // reduce action cache
        typedef boost::tuple<
            std::vector<std::string>,
            std::string,
            size_t,
            std::vector<int>>
            reduce_action_cache_key_type;
        typedef 
            std::map<reduce_action_cache_key_type,
                     std::vector<std::string>>
            reduce_action_cache_type;
        reduce_action_cache_type reduce_action_cache;

        // action table
        for (const auto& pair: state.action_table) {
            // action header 
            std::string case_tag =
                options.token_prefix +
                (*token_id_map.find(pair.first)).second;

            // action
            const tgt::parsing_table::action* a = &pair.second;
            switch (a->type) {
                case zw::gr::action_shift:
                    stencil(
                        os, R"(
        case ${case_tag}:
            // shift
            push_stack(/*state*/ ${dest_index}, value);
            return false;
)",
                        {
                            {"case_tag", case_tag},
                            {"dest_index", a->dest_index},
                        });
                    break;
                case zw::gr::action_reduce: {
                    size_t base = a->rule.right().size();

                    auto k = actions.find(a->rule);

                    if (k != actions.end()) {
                        const semantic_action& sa = (*k).second;

                        std::vector<std::string> signature;
                        make_signature(
                            nonterminal_types,
                            a->rule,
                            sa,
                            signature);

                        std::vector<int> arg_indices;
                        for (size_t l = 0 ; l < sa.args.size() ; l++) {
                            const semantic_action_argument& arg =
                                (*sa.args.find(l)).second;
                            arg_indices.push_back(arg.src_index);
                        }

                        reduce_action_cache_key_type key =
                            boost::make_tuple(
                                signature,
                                a->rule.left().name(),
                                base,
                                arg_indices);

                        reduce_action_cache[key].push_back(case_tag);
                    } else {
                        stencil(
                            os, R"(
        case ${case_tag}:
            // reduce
            return call_nothing(Nonterminal_${nonterminal}, /*pop*/ ${base});
)",
                            {
                                {"case_tag", case_tag},
                                {"base", base},
                                {"nonterminal", a->rule.left().name()}
                            });
                    }
                }
                    break;
                case zw::gr::action_accept:
                    stencil(
                        os, R"(
        case ${case_tag}:
            // accept
            accepted_ = true;
            accepted_value_ = get_arg(1, 0);
            return false;
)",
                        {
                            {"case_tag", case_tag}
                        });
                    break;
                case zw::gr::action_error:
                    stencil(
                        os, R"(
        case ${case_tag}:
            sa_.syntax_error();
            error_ = true;
            return false;
)",
                        {
                            {"case_tag", case_tag}
                        });
                    break;
            }

            // action footer
        }

        // flush reduce action cache
        for(const auto& pair: reduce_action_cache) {
            const reduce_action_cache_key_type& key = pair.first;
            const std::vector<std::string>& cases = pair.second;

            const std::vector<std::string>& signature = key.get<0>();
            const std::string& nonterminal_name = key.get<1>();
            size_t base = key.get<2>();
            const std::vector<int>& arg_indices = key.get<3>();

            for (size_t j = 0 ; j < cases.size() ; j++){
                os << ind1 << ind1 << "case " << cases[j] << ":\n";
            }

            int index = stub_index[signature];

            stencil(
                os, R"(
            // reduce
            return call_${index}_${sa_name}(Nonterminal_${nonterminal}, /*pop*/ ${base}${args});
)",
                {
                    {"index", index},
                    {"sa_name", signature[0]},
                    {"nonterminal", nonterminal_name},
                    {"base", base},
                    {"args", [&](std::ostream& os) {
                                for(const auto& x: arg_indices) {
                                    os  << ", " << x;
                                }
                            }}
                });
        }

        // dispatcher footer / state footer
        stencil(
            os, R"(
        default:
            sa_.syntax_error();
            error_ = true;
            return false;
        }
    }

)",
            {});

        // gotof header
        stencil(
            os, R"(
    bool gotof_${state_no}(Nonterminal nonterminal, const value_type& value) {
)",
            {
                {"state_no", state.no}
            });
            
        // gotof dispatcher
        std::stringstream ss;
        stencil(
            ss, R"(
        switch(nonterminal) {
)",
            {});
        bool output_switch = false;
        std::set<size_t> generated;
        for(const auto& rule: table.grammar()) {
            size_t nonterminal_index = std::distance(
                nonterminal_types.begin(),
                nonterminal_types.find(rule.left().name()));
            if (0 < generated.count(nonterminal_index)) {
                continue;
            }

            auto k = state.goto_table.find(rule.left());
            if (k != state.goto_table.end()) {
                int state_index = (*k).second;
                stencil(
                    ss, R"(
        // ${rule}
        case Nonterminal_${nonterminal}: return push_stack(/*state*/ ${state_index}, value);
)",
                    {
                        {"rule", [&](std::ostream& os) { os << rule; }},
                        {"nonterminal", {rule.left().name()}},
                        {"state_index", {state_index}}
                    });
                output_switch = true;
                generated.insert(nonterminal_index);
            }
        }

        // gotof footer
        stencil(
            ss, R"(
        default: assert(0); return false;
        }
)",
            {});
        if (output_switch) {
            os << ss.str();
        } else {
            stencil(
                os, R"(
        assert(0);
        return true;
)", {});
        }
        stencil(os, R"(
    }

)", {});


    }

    // table
    stencil(os, R"(
    const table_entry* entry(int n) const {
        static const table_entry entries[] = {
$${entries}
        };
        return &entries[n];
    }

)",
        {
            {"entries", [&](std::ostream& os) {
                    int i = 0;
                    for (const auto& state: table.states()) {
                        stencil(
                            os, R"(
            { &Parser::state_${i}, &Parser::gotof_${i}, ${handle_error} },
)",
                            {
                                {"i", i},
                                {"handle_error", state.handle_error}
                            });
                        ++i;
                    }                    
                }}
        });

    // parser class footer
    // namespace footer
    // once footer
    stencil(
        os,
        R"(
};

} // namespace ${namespace_name}

#endif // #ifndef ${headername}_

)",
        {
            {"headername", {headername}},
            {"namespace_name", {options.namespace_name}}
        });
}
