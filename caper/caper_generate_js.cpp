// Copyright (C) 2008 Naoyuki Hirayama.
// All Rights Reserved.

// $Id$

#include "caper_ast.hpp"
#include "caper_generate_cpp.hpp"
#include "caper_format.hpp"
#include "caper_stencil.hpp"
#include "caper_finder.hpp"
#include <algorithm>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

namespace {

std::string make_type_name(const Type& x) {
    switch(x.extension) {
        case Extension::None:
            return x.name;
        case Extension::Star:
        case Extension::Plus:
        case Extension::Slash:
            return "Sequence<" + x.name + ">";
        case Extension::Question:
            return "Optional<" + x.name + ">";
        default:
            assert(0);
            return "";
    }
}
        
void make_signature(
    const std::map<std::string, Type>&      nonterminal_types,
    const tgt::parsing_table::rule_type&    rule,
    const SemanticAction&                   sa,
    std::vector<std::string>&               signature) {
    // function name
    signature.push_back(sa.name);

    // return value
    signature.push_back(
        make_type_name(*finder(nonterminal_types, rule.left().name())));

    // arguments
    for (const auto& arg: sa.args) {
        signature.push_back(make_type_name(arg.type));
    }
}

}

void generate_javascript(
    const std::string&                  src_filename,
    std::ostream&                       os,
    const GenerateOptions&              options,
    const std::map<std::string, Type>&,
    const std::map<std::string, Type>&  nonterminal_types,
    const std::vector<std::string>&     tokens,
    const action_map_type&              actions,
    const tgt::parsing_table&           table) {

    // notice / URL
    stencil(
        os, R"(
// This file was automatically generated by Caper.
// (http://jonigata.github.io/caper/caper.html)

var ${namespace_name} = (function() {

    var exports = {};

)",
        {"namespace_name", options.namespace_name}
        
        );

    if (!options.external_token) {
        // token enumeration
        stencil(
            os, R"(
    var Token = {
$${tokens}
        null : null
    };
    exports.Token = Token;

    var getTokenLabel = function(t) {
        var labels = [
$${labels}
            null
        ];
        return labels[t];
    };
    exports.getTokenLabel = getTokenLabel;

)",
            {"tokens", [&](std::ostream& os){
                    int index = 0;
                    for(const auto& token: tokens) {
                        stencil(
                            os, R"(
        ${prefix}${token}: ${index},
)",
                            {"prefix", options.token_prefix},
                            {"token", token},
                            {"index", index}
                            );
                        index++;
                    }
                }},
            {"labels", [&](std::ostream& os){
                    for(const auto& token: tokens) {
                        stencil(
                            os, R"(
            "${prefix}${token}",
)",
                            {"prefix", options.token_prefix},
                            {"token", token}
                            );
                    }
                }}
            );

    }

    // nonterminal
    stencil(
        os, R"(
    var Nonterminal = {
)"
        );
    {
        int index = 0;
        for (const auto& nonterminal_type: nonterminal_types) {
            stencil(
                os, R"(
        ${nonterminal_name}: ${index},
)",
                {"nonterminal_name", nonterminal_type.first},
                {"index", index}
                );
            index++;
        }
    }
    stencil(
        os, R"(
        null: null
    };
)"
        );
    
    stencil(
        os, R"(

    function Range(b, e) {
        this.begin = b;
        this.end = e;
    }

    function Stack() {
        this.stack = [];
        this.tmp = [];
        this.gap = 0;
    }

    Stack.prototype = {
        rollbackTmp : function() {
            this.gap = this.stack.length;
            this.tmp = [];
        },
        commitTmp : function() {
            this.stack.splice(this.gap, this.stack.length - this.gap);
            this.stack = this.stack.concat(this.tmp);
            this.tmp = [];
        },
        push : function(x) {
            this.tmp.push(x);
            return true;
        },
        pop : function(n) {
            if (this.tmp.length < n) {
                n -= this.tmp.length;
                this.tmp = [];
                this.gap -= n;
            } else {
                this.tmp.splice(this.tmp.length - n, n);
            }
        },
        top : function() {
            if (this.tmp.length != 0) {
                return this.tmp[this.tmp.length-1];
            } else {
                return this.stack[this.gap - 1];
            }
        },
        getArg : function(base, index) {
            var n = this.tmp.length;
            if (base - index <= n) {
                return this.tmp[n - (base - index)];
            } else {
                return this.stack[this.gap - (base - n) + index];
            }
        },
        clear : function() {
            this.stack = [];
            this.tmp = [];
            this.gap = 0;
        },
        empty : function() {
            if (0 < this.tmp.length) {
                return false;
            } else {
                return this.gap == 0;
            }
        },
        depth : function() {
            return this.gap + this.tmp.length;
        },
        nth : function(index) {	   
            if (this.gap <= index) {
                return this.tmp[index - this.gap];
            } else {
                return this.stack[index];
            }
        },
        setNth : function(index, o) {
            if (this.gap <= index) {
                this.tmp[index - this.gap] = o;
            } else {
                this.stack[index] = o;
            }
        },
        swapTopAndSecond : function() {
            var d = this.depth();
            var x = this.nth(d - 1);
            this.setNth(d - 1, this.nth(d - 2));
            this.setNth(d - 2, x);
        }
    };

    function StackFrame(entry, v, sl) {
        this.entry = entry;
        this.value = v;
        this.sequenceLength = sl;
    }

)");

    // parser constructor
    stencil(
        os, R"(
    function Parser(sa) {
        this.sa = sa;

)"
        );

    // table
    stencil(
        os, R"(
        var entries = [
$${entries}
            null
        ];

        this.entry = function(n) {
            return entries[n];
        };

)",
        {"entries", [&](std::ostream& os) {
                int i = 0;
                for (const auto& state: table.states()) {
                    stencil(
                        os, R"(
            { state: this.state_${i}, gotof: this.gotof_${i}, handleError: ${handle_error} },
)",
                            
                        {"i", i},
                        {"handle_error", state.handle_error}
                        );
                    ++i;
                }                    
            }}
        );

    
    stencil(
        os, R"(
        this.reset();
    }
    exports.Parser = Parser;

    Parser.prototype = {
        reset : function() {
            this.stack = new Stack();
            this.accepted = false;
            this.error = false;
            this.acceptedValue = null;
            this.clearStack();
            this.rollbackTmpStack();
            if (this.pushStack(${first_state}, null, 0)) {
                this.commitTmpStack();
            } else {
                this.sa.stackOverflow();
                this.error = true;
            }
        },
        post : function(token, value) {
            this.rollbackTmpStack();
            this.error = false;
            while (this.stackTop().entry.state.apply(this, [token, value]));
            if (!this.error) {
                this.commitTmpStack();
            } else {
                this.recover(token, value);
            }
            return this.accepted || this.error;
        },
        accept : function() {
            if (this.error) { return null; }
            return this.acceptedValue;
        },
        gotError : function() {
            return this.error;
        },

)",
        {"first_state", table.first_state()}
        );

    // stack operation
    stencil(
        os, R"(
        pushStack : function(stateIndex, v, sl) {
            var f = this.stack.push(new StackFrame(this.entry(stateIndex), v, sl));
            if (!f) { 
                this.error = true;
                this.sa.stackOverflow();
            }
            return f;
        },
        popStack : function(n) {
            while(n--) {
                this.stack.pop(1 + this.stack.top().sequenceLength);
            }
        },
        stackTop : function() {
            return this.stack.top();
        },
        getArg : function(base, index) {
            return this.stack.getArg(base, index).value;
        },
        clearStack : function() {
            this.stack.clear();
        },
        rollbackTmpStack : function() {
            this.stack.rollbackTmp();
        },
        commitTmpStack : function() {
            this.stack.commitTmp();
        },

)"
        );

    if (options.recovery) {
        stencil(
            os, R"(
        recover : function(token, value) {
            this.rollbackTmpStack();
            this.error = false;
$${debmes:start}
            while(!this.stackTop().entry.handleError) {
                this.popStack(1);
                if (this.stack.empty()) {
$${debmes:failed}
                    this.error = true;
                    return;
                }
            }
$${debmes:done}
            // post error_token;
$${debmes:post_error_start}
            while (this.stackTop().entry.state.apply(this, [Token.${recovery_token}, null]));
$${debmes:post_error_done}
            this.commitTmpStack();
            // repost original token
            // if it still causes error, discard it;
$${debmes:repost_start}
            while (this.stackTop().entry.state.apply(this, [token, value]));
$${debmes:repost_done}
            if (!this.error) {
                this.commitTmpStack();
            }
            if (token != Token.${token_eof}) {
                this.error = false;
            }
        },

)",
            {"recovery_token", options.token_prefix + options.recovery_token},
            {"token_eof", options.token_prefix + "eof"},
            {"debmes:start", {
                    options.debug_parser ?
                        R"(        console.log("recover rewinding start: stack depth = " + this.stack.depth());
)" :
                        ""}},
            {"debmes:failed", {
                    options.debug_parser ?
                        R"(        console.log("recover rewinding failed");
)" :
                        ""}},
            {"debmes:done", {
                    options.debug_parser ?
                        R"(        console.log("recover rewinding done: stack depth = " + this.stack.depth());
)" :
                        ""}},
            {"debmes:post_error_start", {
                    options.debug_parser ?
                        R"(        console.log("posting error token");
)" :
                        ""}},
            {"debmes:post_error_done", {
                    options.debug_parser ?
                        R"(        console.log("posting error token done");
)" :
                        ""}},
            {"debmes:repost_start", {
                    options.debug_parser ?
                        R"(        console.log("reposting original token");
)" :
                        ""}},
            {"debmes:repost_done", {
                    options.debug_parser ? 
                        R"(        console.log("reposting original token done");
)" :
                        ""}}
            );
    } else {
        stencil(
            os, R"(
        recover : function(token, value) {
        },

)"
            );
    }

    if (options.allow_ebnf) {
        stencil(
            os, R"(
        // EBNF support member functions
        seq_head : function(nonterminal, base) {
            // case '*': base == 0
            // case '+': base == 1
            var dest = this.stack_nth_top(base).entry.gotof.apply(this, [nonterminal]);
            return this.pushStack(dest, null, base);
        },
        seq_trail : function(nonterminal, base) {
            // '*', '+' trailer
            this.stack.swapTopAndSecond();
            this.stackTop().sequenceLength++;
            return true;
        },
        seq_trail2 : function(nonterminal, base) {
            // '/' trailer
            this.stack.swapTopAndSecond();
            this.popStack(1); // erase delimiter
            this.stack.swapTopAndSecond();
            this.stackTop().sequenceLength++;
            return true;
        },
        opt_nothing : function(nonterminal, base) {
            // same as head of '*'
            return this.seq_head(nonterminal, base);
        },
        opt_just : function(nonterminal, base) {
            // same as head of '+'
            return this.seq_head(nonterminal, base);
        },
        seq_get_range : function(base, index) {
            // returns beg = end if length = 0 (includes scalar value)
            // distinguishing 0-length-vector against scalar value is
            // caller's responsibility
            var n = base - index;
            var prevActualIndex;
            var actualIndex = this.stack.depth();
            while(n--) {
                actualIndex--;
                prevActualIndex = actualIndex;
                actualIndex -= this.stack.nth(actualIndex).sequenceLength;
            }
            return new Range(actualIndex, prevActualIndex);
        },
        seq_get_arg : function(base, index) {
            var r = this.seq_get_range(base, index);
            // multiple value appearing here is not supported now
            return this.stack.nth(r.begin).value;
        },
        seq_get_seq : function(base, index) {
            var r = this.seq_get_range(base, index);

            var a = [];
            for(var i = r.begin ; i < r.end ; i++) {
                a.push(this.stack.nth(i).value);
            }        
            return a;
        },
        stack_nth_top : function(n) {
            var r = this.seq_get_range(n + 1, 0);
            // multiple value appearing here is not supported now
            return this.stack.nth(r.begin);
        },
)"
            );
    }

    stencil(
        os, R"(
        call_nothing : function(nonterminal, base) {
            this.popStack(base);
            var destIndex = this.stackTop().entry.gotof.apply(this, [nonterminal]);
            return this.pushStack(destIndex, null, 0);
        },

)"
        );

    // member function signature -> index
    std::map<std::vector<std::string>, int> stub_indices;
    {
        // member function name -> count
        std::unordered_map<std::string, int> stub_counts; 

        // action handler stub
        for (const auto& pair: actions) {
            const auto& rule = pair.first;
            const auto& sa = pair.second;

            if (sa.special) {
                continue;
            }

            // make signature
            std::vector<std::string> signature;
            make_signature(
                nonterminal_types,
                rule,
                sa,
                signature);

            // skip duplicated
            if (0 < stub_indices.count(signature)) {
                continue;
            }

            // make function name
            if (stub_counts.count(sa.name) == 0) {
                stub_counts[sa.name] = 0;
            }
            int stub_index = stub_counts[sa.name];
            stub_indices[signature] = stub_index;
            stub_counts[sa.name] = stub_index+1;

            // header
            stencil(
                os, R"(
        call_${stub_index}_${sa_name} : function(nonterminal, base${args}) {
)",
                {"stub_index", stub_index},
                {"sa_name", sa.name},
                {"args", [&](std::ostream& os) {
                        for (size_t l = 0 ; l < sa.args.size() ; l++) {
                            os << ", argIndex" << l;
                        }
                    }}
                );

            // check sequence conciousness
            std::string get_arg = "getArg";
            for (const auto& arg: sa.args) {
                if (arg.type.extension != Extension::None) {
                    get_arg = "seqGetArg";
                    break;
                }
            }

            // automatic argument conversion
            for (size_t l = 0 ; l < sa.args.size() ; l++) {
                const auto& arg = sa.args[l];
                if (arg.type.extension == Extension::None) {
                    stencil(
                        os, R"(
            var arg${index} = this.${get_arg}(base, argIndex${index});
)",
                        {"get_arg", get_arg},
                        {"index", l}
                        );
                } else {
                    stencil(
                        os, R"(
            var arg${index} = this.seq_get_seq(base, argIndex${index});
)",
                        {"index", l}
                        );
                }
            }

            // semantic action / automatic value conversion
            stencil(
                os, R"(
            var v = this.sa.${semantic_action_name}(${args});
            this.popStack(base);
            var destIndex = this.stackTop().entry.gotof.apply(this, [nonterminal]);
            return this.pushStack(destIndex, v, 0);
        },

)",
                {"semantic_action_name", sa.name},
                {"args", [&](std::ostream& os) {
                        bool first = true;
                        for (size_t l = 0 ; l < sa.args.size() ; l++) {
                            if (first) { first = false; }
                            else { os << ", "; }
                            os << "arg" << l;
                        }
                    }}
                );
        }
    }

    // states handler
    for (const auto& state: table.states()) {
        // state header
        stencil(
            os, R"(
        state_${state_no} : function(token, value) {
$${debmes:state}
            switch(token) {
)",
            {"state_no", state.no},
            {"debmes:state", [&](std::ostream& os){
                    if (options.debug_parser) {
                        stencil(
                            os, R"(
            console.log("state_${state_no} << " + getTokenLabel(token));
)",
                            {"state_no", state.no}
                            );
                    }}}
            );

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
            const auto& token = pair.first;
            const auto& action = pair.second;

            const auto& rule = action.rule;

            // action header 
            std::string case_tag =
                "Token." + options.token_prefix + tokens[token];

            // action
            switch (action.type) {
                case zw::gr::action_shift:
                    stencil(
                        os, R"(
            case ${case_tag}:
                // shift
                this.pushStack(/*state*/ ${dest_index}, value, 0);
                return false;
)",
                        {"case_tag", case_tag},
                        {"dest_index", action.dest_index}
                        );
                    break;
                case zw::gr::action_reduce: {
                    size_t base = rule.right().size();
                    const std::string& rule_name = rule.left().name();

                    auto k = finder(actions, rule);
                    if (k && !(*k).special) {
                        const auto& sa = *k;

                        std::vector<std::string> signature;
                        make_signature(
                            nonterminal_types,
                            rule,
                            sa,
                            signature);

                        reduce_action_cache_key_type key =
                            boost::make_tuple(
                                signature,
                                rule_name,
                                base,
                                sa.source_indices);

                        reduce_action_cache[key].push_back(case_tag);
                    } else {
                        stencil(
                            os, R"(
            case ${case_tag}:
)",
                            {"case_tag", case_tag}
                            );
                        std::string funcname = "this.call_nothing";
                        if (k) {
                            const auto& sa = *k;
                            assert(sa.special);
                            funcname = "this." + sa.name;
                        }
                        stencil(
                            os, R"(
                // reduce
                return ${funcname}(Nonterminal.${nonterminal}, /*pop*/ ${base});
)",
                            {"funcname", funcname},
                            {"nonterminal", rule.left().name()},
                            {"base", base}
                            );
                    }
                }
                    break;
                case zw::gr::action_accept:
                    stencil(
                        os, R"(
            case ${case_tag}:
                // accept
                this.accepted = true;
                this.acceptedValue = this.getArg(1, 0);
                return false;
)",
                        {"case_tag", case_tag}
                        );
                    break;
                case zw::gr::action_error:
                    stencil(
                        os, R"(
            case ${case_tag}:
                this.sa.syntaxError();
                this.error = true;
                return false;
)",
                        {"case_tag", case_tag}
                        );
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
                // fall through, be aware when port to other language
                stencil(
                    os, R"(
            case ${case}:
)",
                    {"case", cases[j]}
                    );
            }

            int index = stub_indices[signature];

            stencil(
                os, R"(
                // reduce
                return this.call_${index}_${sa_name}(Nonterminal.${nonterminal}, /*pop*/ ${base}${args});
)",
                {"index", index},
                {"sa_name", signature[0]},
                {"nonterminal", nonterminal_name},
                {"base", base},
                {"args", [&](std::ostream& os) {
                        for(const auto& x: arg_indices) {
                            os  << ", " << x;
                        }
                    }}
                );
        }

        // dispatcher footer / state footer
        stencil(
            os, R"(
            default:
                this.sa.syntaxError();
                this.error = true;
                return false;
            }
        },

)"
            );
        
        // gotof header
        stencil(
            os, R"(
        gotof_${state_no} : function(nonterminal) {
)",
            {"state_no", state.no}
            );
            
        // gotof dispatcher
        std::stringstream ss;
        stencil(
            ss, R"(
            switch(nonterminal) {
)"
            );
        bool output_switch = false;
        for (const auto& pair: state.goto_table) {
            stencil(
                ss, R"(
            case Nonterminal.${nonterminal}: return ${state_index};
)",
                {"nonterminal", pair.first.name()},
                {"state_index", pair.second}
                );
            output_switch = true;
        }

        // gotof footer
        stencil(
            ss, R"(
            default: return false;
            }
)"
            );
        if (output_switch) {
            os << ss.str();
        } else {
            stencil(
                os, R"(
            return true;
)"
                );
        }
        stencil(os, R"(
        },

)"
            );
    }
    stencil(os, R"(
        dummy : null
    };

    return exports;
})();

)"
        );
}
