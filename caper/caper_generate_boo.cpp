#include "caper_ast.hpp"
#include "caper_error.hpp"
#include "caper_generate_boo.hpp"
#include "caper_format.hpp"
#include "caper_stencil.hpp"
#include "caper_finder.hpp"
#include <algorithm>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

struct semantic_action_entry {
    std::string                     name;
    std::vector< std::string >      args;

    bool operator<( const semantic_action_entry& x ) const {
        if (name < x.name) { return true; }
        if (x.name < name) { return false; }
        return args < x.args;
    }
};

void generate_boo(
    const std::string&                  src_filename,
    std::ostream&                       os,
    const GenerateOptions&              options,
    const std::map<std::string, Type>&  terminal_types,
    const std::map<std::string, Type>&  nonterminal_types,
    const std::vector<std::string>&     tokens,
    const action_map_type&              actions,
    const tgt::parsing_table&           table) {

    if (options.allow_ebnf) {
        throw unsupported_feature("Boo", "EBNF");
    }

    // notice / URL
    stencil(
        os, R"(
// This file was automatically generated by Caper.
// (http://jonigata.github.io/caper/caper.html)

namespace ${namespace_name}

import System.Collections.Generic

)",
        {"namespace_name", options.namespace_name});

    // enum Token
    if (!options.external_token) {
        stencil(
            os, R"(
enum Token:
$${tokens}

)",
            {"tokens", [&](std::ostream& os) {
                for (const auto& token: tokens) {
                    stencil(
                        os, R"(
  ${prefix}${token}
)",
                        {"prefix", options.token_prefix},
                        {"token", token});
                }
            }
        });
    }

    // ISemanticAction interface
    std::set< semantic_action_entry > ss;

    for (action_map_type::const_iterator it = actions.begin(); it != actions.end(); ++it) {
        const tgt::parsing_table::rule_type& rule = it->first;
        const SemanticAction& sa = it->second;

        semantic_action_entry sae;
        sae.name = sa.name;

        // 1st argument = out parameter
        sae.args.push_back( (*nonterminal_types.find( rule.left().name() )).second.name );

        for (size_t l = 0; l < sa.args.size(); l++) {
            sae.args.push_back( sa.args[l].type.name );
        }

        ss.insert( sae );
    }

    std::unordered_set<std::string> types;
    for (const auto& x: terminal_types) {
        if (x.second.name != "") {
            types.insert(x.second.name);
        }
    }

    for (const auto& x: nonterminal_types) {
        if (x.second.name != "") {
            types.insert(x.second.name);
        }
    }

    //os << options.access_modifier << "interface ISemanticAction:\n";
    //os << "  def syntax_error() as void\n";
    //os << "  def stack_overflow() as void\n";
    stencil(
        os, R"(
${access_modifier}interface ISemanticAction:
  def syntax_error() as void
  def stack_overflow() as void
  // actions
$${methods}

)",
        {"access_modifier", options.access_modifier},
        {"methods",
            [&](std::ostream& os) {
                for (std::set< semantic_action_entry >::const_iterator it = ss.begin(); it != ss.end(); ++it) {
                    std::stringstream args;
                    bool first = true;
                    for (size_t l = 0; l < (*it).args.size(); l++) {
                        if (first) { first = false; } else { args << ", "; }
                        args << "arg" << l << " as " << ((*it).args[l]);
                    }
                    stencil(
                        os, R"(
  def ${name}(ref ${args}) as void
)",
                        {"name", (*it).name},
                        {"args", args.str()});
                }
            }
        });

    // parser class
    stencil(
        os, R"(
${access_modifier}class Parser ():
  // stack_frame clas
  private class stack_frame ():
    public state as state_type
    public gotof as gotof_type
    public value as object

    public def constructor(s as state_type, g as gotof_type, v as object):
      state = s
      gotof = g
      value = v

  // Stack class
  private class Stack ():
    private stack = List[of stack_frame]()
    private tmp = List[of stack_frame]()
    private gap as int

    public def constructor():
      self.gap = 0

    public def reset_tmp():
      self.gap = self.stack.Count
      self.tmp.Clear()

    public def commit_tmp():
      size = self.gap + self.tmp.Count
      self.stack.Capacity = size if size > self.stack.Capacity
      self.stack.RemoveRange(self.gap, self.stack.Count - self.gap)
      self.stack.AddRange(self.tmp)

    public def push(f as stack_frame) as bool:
      self.tmp.Add(f)
      return true

    public def pop(n as int):
      if self.tmp.Count < n:
        n -= self.tmp.Count
        self.tmp.Clear()
        self.gap -= n
      else:
        self.tmp.RemoveRange(self.tmp.Count - n, n)

    public def top() as stack_frame:
      if self.tmp.Count != 0:
        return self.tmp[self.tmp.Count - 1]
      else:
        return self.stack[self.gap - 1]

    public def my_get_arg(b as int, i as int) as stack_frame:
      n = self.tmp.Count
      if b - i <= n:
        return self.tmp[n - (b - i)]
      else:
        return self.stack[self.gap - (b - n) + i]

    public def clear():
      self.stack.Clear()

  // delegate
  private callable state_type(token as Token, value as object) as bool
  private callable gotof_type(i as int, value as object) as bool

  public def constructor(sa as ISemanticAction):
    self.stack = Stack()
    self.sa = sa
    self.reset()

  public def reset():
    self.error = false
    self.accepted = false
    self.clear_stack()
    self.reset_tmp_stack()
    if self.push_stack(self.state_${first_state}, self.gotof_${first_state}, object()):
      self.commit_tmp_stack()
    else:
      self.sa.stack_overflow()
      self.error = true

  public def post(token as Token, value as object) as bool:
    System.Diagnostics.Debug.Assert(not self.error)
    self.reset_tmp_stack()
    while stack_top().state(token, value):
      pass
    unless self.error:
      self.commit_tmp_stack()
    return self.accepted

  public def accept(ref v as object) as bool:
    System.Diagnostics.Debug.Assert(self.accepted)
    if self.error:
      v = object()
      return false
    v = self.accepted_value
    return true

  public def Error() as bool:
    return self.error

  // private member
  private sa as ISemanticAction
  private stack as Stack
  private accepted as bool
  private error as bool
  private accepted_value as object

  private def push_stack(s as state_type, g as gotof_type, v as object) as bool:
    f = self.stack.push(stack_frame(s, g, v))
    System.Diagnostics.Debug.Assert(not self.error)
    unless f:
      self.error = true
      self.sa.stack_overflow()
    return f

  private def pop_stack(n as int):
    self.stack.pop(n)

  private def stack_top() as stack_frame:
    return self.stack.top()

  private def my_get_arg(b as int, i as int) as object:
    return stack.my_get_arg(b, i).value

  private def clear_stack():
    self.stack.clear()

  private def reset_tmp_stack():
    self.stack.reset_tmp()

  private def commit_tmp_stack():
    self.stack.commit_tmp()

)",
        {"access_modifier", options.access_modifier},
        {"first_state", table.first_state()});

    // states handler
    for (tgt::parsing_table::states_type::const_iterator i = table.states().begin(); i != table.states().end(); ++i) {
        const tgt::parsing_table::state& s = *i;
        // gotof header
        stencil(
            os, R"(
  def gotof_${num}(nonterminal_index as int, v as object) as bool:
)",
            {"num", s.no});
        // gotof dispatcher
        std::stringstream ss;
        //ss << "			switch(nonterminal_index)\n"
        //   << "			{\n";
        bool output_switch = false;
        std::set<size_t> generated;
        bool first = true;
        for (const auto& rule: table.get_grammar()) {
            size_t nonterminal_index = std::distance(
                nonterminal_types.begin(),
                nonterminal_types.find( rule.left().name() ) );

            if (generated.find( nonterminal_index ) != generated.end()) {
                continue;
            }

            tgt::parsing_table::state::goto_table_type::const_iterator k =
                (*i).goto_table.find(rule.left());

            if (k != (*i).goto_table.end()) {
                stencil(
                    ss, R"(
    ${if} nonterminal_index == ${nonterminal_index}:
)",
                    {"if", (first)? "if" : "elif"},
                    {"nonterminal_index", nonterminal_index});
                first = false;
                stencil(
                    ss, "      return push_stack(self.state_${next_state}, self.gotof_${next_state}, v)\n",
                    {"next_state", (*k).second});
                output_switch = true;
                generated.insert( nonterminal_index );
            }
        }
        stencil(
            ss, R"(
    else:
      System.Diagnostics.Debug.Assert(false)
      return false

)");
        if (output_switch) {
            os << ss.str();
        } else {
            stencil(
                os, R"(
    System.Diagnostics.Debug.Assert(false)
    return true

)");
        }

        // state header
        stencil(
            os, R"(
  def state_${num}(token as Token, value as object) as bool:
)",
            {"num", s.no});

        // dispatcher header
        //os << "			switch(token)\n"
        //   << "			{\n";
        // action table
        first = true;
        int ridx = 0;
        for (tgt::parsing_table::state::action_table_type::const_iterator j = s.action_table.begin(); j != s.action_table.end(); ++j) {
            // action header 
            stencil(
                os, R"(
    ${if} token == Token.${token_prefix}${token}:
)",
                {"if", (first)? "if" : "elif"},
                {"token_prefix", options.token_prefix},
                {"token", tokens[(*j).first]});
            first = false;
            // action
            const tgt::parsing_table::action* a = &(*j).second;
            switch( a->type ) {
            case zw::gr::action_shift:
                stencil(
                    os, R"(
      // shift
      push_stack(self.state_${dest_index}, self.gotof_${dest_index}, value)
      return false
)",
                    {"dest_index", a->dest_index});
                break;
            case zw::gr::action_reduce:
                stencil(
                    os, R"(
      // reduce
)");
                {
                    size_t base = a->rule.right().size();

                    const tgt::parsing_table::rule_type& rule = a->rule;
                    action_map_type::const_iterator k = actions.find( rule );

                    size_t nonterminal_index = std::distance(
                        nonterminal_types.begin(),
                        nonterminal_types.find( rule.left().name() ) );

                    if( k != actions.end() ) {
                        const SemanticAction& sa = (*k).second;

                        //os << "				{\n";
                        // automatic argument conversion
                        for( size_t l = 0 ; l < sa.args.size() ; l++ ) {
                            const SemanticAction::Argument& arg = sa.args[l];
                            stencil(
                                os, R"(
      arg${idx} = my_get_arg(${base}, ${source}) cast ${type}
)",
                                {"idx", l},
                                {"base", base},
                                {"source", arg.source_index},
                                {"type", arg.type.name});
                        }

                        // semantic action
                        stencil(
                            os, R"(
      r${ridx} as ${type}
      self.sa.${sa_name}(r${ridx}$${args} )
)",
                            {"ridx", ridx},
                            {"type", (*nonterminal_types.find( rule.left().name() )).second.name},
                            {"sa_name", sa.name},
                            {"args",
                                [&](std::ostream& os) {
                                    for (size_t l = 0; l < sa.args.size(); l++) {
                                        stencil(
                                            os, R"(, arg${idx})",
                                            {"idx", l});
                                    }
                                    }
                                });

                        // automatic return value conversion
                        stencil(
                            os, R"(
      v = r${ridx} cast object
      pop_stack(${base})
      return stack_top().gotof(${nonterminal_index}, v)
)",
                            {"ridx", ridx},
                            {"base", base},
                            {"nonterminal_index", nonterminal_index});
                    } else {
                        stencil(
                            os, R"(
      // run_semantic_action()
      pop_stack(${base})
      return stack_top().gotof(${nonterminal_index}, object())
)",
                            {"base", base},
                            {"nonterminal_index", nonterminal_index});
                    }
                }
                break;
            case zw::gr::action_accept:
                stencil(
                    os, R"(
      // accept
      // run_semantic_action()
      self.accepted = true
      self.accepted_value = my_get_arg(1, 0) // implicit root
      return false
)");
                break;
            case zw::gr::action_error:
                stencil(
                    os, R"(
      self.sa.syntax_error()
      self.error = true
      return false
)");
                break;
            }

            // action footer
            ++ridx;
        }

        // dispatcher footer
        stencil(
            os, R"(
    else:
      self.sa.syntax_error()
      self.error = true
      return false

)");
    }

    stencil(
        os, R"(
// class Parser)");

    // namespace footer
    //os << "} // namespace " << options.namespace_name;

    // once footer
}

