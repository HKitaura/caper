// Copyright (C) 2006 Naoyuki Hirayama.
// All Rights Reserved.

// $Id$

#include "caper_ast.hpp"
#include "caper_generate_php.hpp"

void generate_php(
        const std::string&                      filename,
        std::ostream&                           os,
        const GenerateOptions&                  options,
        const symbol_map_type&                  terminal_types,
        const symbol_map_type&                  nonterminal_types,
        const std::map< size_t, std::string >&  token_id_map,
        const action_map_type&                  actions,
        const tgt::parsing_table&               table )
{
        if( !options.external_token ) {
                // token enumeration
                os << "final class Token {\n";
                for( size_t i = 0 ; i < token_id_map.size() ; i++ ) {
                        os << "    const " << options.token_prefix << (*token_id_map.find( i )).second
                           << " = " << i << ";\n";
                }
                os << "}\n";
        }

        // parser class header
        os << "class Parser {\n"
           << "{\n"
           << "    Parser( $sa ) {\n"
           << "        $this->semantic_action = $sa;\n"
           << "        $this->this.stack = array();\n"
           << "        $this->this.tmp_stack = array();\n"
           << "    }\n"
           <<
                ;

        // public interface
        os << "    function reset()\n"
           << "    {\n"
           << "        $this->error = false;\n"
           << "        $this->accepted_value = null;\n"
           << "        $this->clear_stack();\n"
           << "        $this->reset_tmp_stack();\n"
           << "        $this->push_stack( "
           << table.first_state() << ", "
           << table.first_state() << ", null );\n"
           << "        $this->commit_tmp_stack();\n"
           << "    }\n\n"
           << "    function post( $token, $value )\n"
           << "    {\n"
           << "        $this->reset_tmp_stack();\n"
           << "        while( $this->post_aux( $token, $value ) );\n"
           << "        if( !$this->error ) {\n"
           << "            $this->commit_tmp_stack();\n"
           << "        }\n"
           << "        return $this->accepted_value;\n"
           << "    }\n\n"
                ;

        // stack operation
        os << "    function push_stack( $s, $g, $v )\n"
           << "    {\n"
           << "        $this->stack->push( $s, $g, $v );\n"
           << "    }\n\n"
           << "    $this->pop_stack = function( n )\n"
           << "    {\n"
           << "        $this->stack.length -= n * 3;\n"
           << "    }\n\n"
           << "    $this->stack_top_state = function()\n"
           << "    {\n"
           << "        return $this->stack[ $this->stack.length - 3 ];\n"
           << "    }\n\n"
           << "    $this->stack_top_gotof = function()\n"
           << "    {\n"
           << "        return $this->stack[ $this->stack.length - 2 ];\n"
           << "    }\n\n"
           << "    $this->get_arg = function( base, index )\n"
           << "    {\n"
           << "        return $this->stack[ $this->stack.length - ( 3 * ( base-index ) + 2 ) ];\n"
           << "    }\n\n"
           << "    $this->clear_stack = function()\n"
           << "    {\n"
           << "        $this->stack.length = 0;\n"
           << "    }\n\n"
           << "    $this->reset_tmp_stack = function()\n"
           << "    {\n"
           << "    }\n\n"
           << "    $this->commit_tmp_stack = function()\n"
           << "    {\n"
           << "    }\n\n"
                ;

        // states handler
        for( tgt::parsing_table::states_type::const_iterator i = table.states().begin();
             i != table.states().end() ;
             ++i ) {
                const tgt::parsing_table::state& s = *i;

                // gotof header
                os << "    $this->gotof_" << s.no << " = new Array( " << table.rules().size() << " );\n";

                // gotof dispatcher
                int rule_index = 0;
                for( tgt::parsing_table::rules_type::const_iterator j = table.rules().begin() ;
                     j != table.rules().end() ;
                     ++j ) {

                        // �{���� nonterminal 1�ɂ�1�s�ł悢���A�卷�Ȃ����s�ւȂ̂� rule 1�ɂ�1�s�Ƃ���
                        tgt::parsing_table::state::goto_table_type::const_iterator k =
                                (*i).goto_table.find( (*j).left() );

                        if( k != (*i).goto_table.end() ) {
                                os << "    $this->gotof_" << s.no << "[ " << rule_index << " ] = "
                                   << "function( o, v ) {\n"
                                   << "        o.push_stack( o.state_" << (*k).second
                                   << ", o.gotof_" << (*k).second << ", v );\n"
                                   << "        return true;\n"
                                   << "    };\n";
                        }
                        rule_index++;
                }

                // gotof footer

                // state header
                os << "    $this->state_" << s.no << " = new Array( " << token_id_map.size() << " );\n";

                // dispatcher header

                // action table
                for( tgt::parsing_table::state::action_table_type::const_iterator j = s.action_table.begin();
                     j != s.action_table.end() ;
                     ++j ) {
                        // action header 
                        os << "    $this->state_" << s.no << "[ " << options.token_prefix 
                           << (*token_id_map.find( (*j).first )).second << " ] = function( o, v )\n"
                           << "        {\n";

                        // action
                        const tgt::parsing_table::action* a = &(*j).second;
                        switch( a->type ) {
                        case zw::gr::action_shift:
                                os << "            // shift\n"
                                   << "            o.push_stack( o.state_" << a->dest_index << ", "
                                   << "o.gotof_" << a->dest_index << ", v);\n"
                                   << "            return false;\n";
                                break;
                        case zw::gr::action_reduce:
                                os << "            // reduce\n";
                                {
                                        size_t base = table.rules()[ a->rule_index ].right().size();
                                        
                                        const tgt::parsing_table::rule_type& rule = table.rules()[a->rule_index];
                                        action_map_type::const_iterator k = actions.find( rule );
                                        if( k != actions.end() ) {
                                                const semantic_action& sa = (*k).second;

                                                // automatic argument conversion
                                                os << "            var r = o.sa." << sa.name << "( ";
                                                for( size_t l = 0 ; l < sa.args.size() ; l++ ) {
                                                        const semantic_action_argument& arg =
                                                                (*sa.args.find( l )).second;
                                                        if( l != 0 ) { os << ", "; }
                                                        os << "o.get_arg( " << base << ", " << arg.src_index << ")";
                                                }
                                                os << " );\n";
                                                
                                                os << "            o.pop_stack( " << base << " );\n";
                                                os << "            return ( o.stack_top_gotof()[ "
                                                   << a->rule_index << " ] )( o, r );\n";
                                        } else {
                                                os << "            // o.sa.run_semantic_action();\n";
                                                os << "            o.pop_stack( " << base << " );\n";
                                                os << "            return ( o.stack_top_gotof()[ "
                                                   << a->rule_index << " ] )( nil );\n";
                                        }
                                }
                                break;
                        case zw::gr::action_accept:
                                os << "            // accept\n"
                                   << "            // run_semantic_action();\n"
                                   << "            o.accepted_value = o.get_arg( 1, 0 );\n" // implicit root
                                   << "            return false;\n";
                                break;
                        case zw::gr::action_error:
                                os << "            o.sa.syntax_error();\n";
                                os << "            o.error = true;\n"; 
                                os << "            return false;\n";
                                break;
                        }

                        // action footer
                        os << "        }\n";
                }

                // state footer
        }

        // parser class footer
        os << "    $this->reset();\n";
        os << "}\n\n";

        // namespace footer


        // once footer
}


