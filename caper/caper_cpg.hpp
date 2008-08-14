#ifndef CAPER_CPG_HPP
#define CAPER_CPG_HPP

#include "fastlalr.hpp"
#include "caper_ast.hpp"

////////////////////////////////////////////////////////////////
// cpg semantic actions
// �S��
struct document_action { // sections;
        value_type operator()( const cpg::parser::arguments& args ) const;
};
struct sections_action { // declarations << rules;
        value_type operator()( const cpg::parser::arguments& args ) const;
};


// .�錾�Z�N�V����
struct declarations0_action { // declaration;
        value_type operator()( const cpg::parser::arguments& args ) const;
};
struct declarations1_action { // declarations << declaration;
        value_type operator()( const cpg::parser::arguments& args ) const;
};

// ..�錾
struct declaration0_action { // token_decl << semicolon;
        value_type operator()( const cpg::parser::arguments& args ) const;

};
struct declaration1_action { // token_prefix_decl << semicolon;
        value_type operator()( const cpg::parser::arguments& args ) const;
};
struct declaration2_action { // external_token_decl << semicolon;
        value_type operator()( const cpg::parser::arguments& args ) const;
};
struct declaration3_action { // namespace_decl << semicolon;
        value_type operator()( const cpg::parser::arguments& args ) const;
};
struct declaration4_action { // dont_use_stl_decl << semicolon;
        value_type operator()( const cpg::parser::arguments& args ) const;
};
struct declaration5_action { // access_modifier_decl << semicolon;
        value_type operator()( const cpg::parser::arguments& args ) const;
};

// ..%token�錾
struct token_decl0_action { // directive_token;
        value_type operator()( const cpg::parser::arguments& args ) const;
};
struct token_decl1_action { // token_decl << token_decl_element;
        value_type operator()( const cpg::parser::arguments& args ) const;
};
struct token_decl_element0_action { // identifier;
        value_type operator()( const cpg::parser::arguments& args ) const;
};
struct token_decl_element1_action { // identifier << typetag;
        value_type operator()( const cpg::parser::arguments& args ) const;
};

// ..%token_prefix�錾
/*
struct token_prefix_decl_action { // directive_token_prefix << identifier;
        value_type operator()( const cpg::parser::arguments& args ) const;
};
*/
struct token_pfx_decl0_action { // directive_token_prefix << identifier;
        value_type operator()( const cpg::parser::arguments& args ) const;
};
struct token_pfx_decl1_action { // directive_token_prefix << identifier;
        value_type operator()( const cpg::parser::arguments& args ) const;
};

// ..%external_token�錾
struct external_token_decl_action { // directive_namespace << identifier;
        value_type operator()( const cpg::parser::arguments& args ) const;
};

// ..%access_modifier�錾
struct access_modifier_decl_action { // directive_access_modifier << identifier;
        value_type operator()( const cpg::parser::arguments& args ) const;
};

// ..%namespace�錾
struct namespace_decl_action { // directive_namespace << identifier;
        value_type operator()( const cpg::parser::arguments& args ) const;
};

// ..%dont_use_stl�錾
struct dont_use_stl_decl_action { // directive_namespace << identifier;
        value_type operator()( const cpg::parser::arguments& args ) const;
};

// .���@�Z�N�V����
struct entries0_action { // entry;
        value_type operator()( const cpg::parser::arguments& args ) const;
};
struct entries1_action { // entries << entry;
        value_type operator()( const cpg::parser::arguments& args ) const;
};

// ..���@
struct entry_action { // identifier << typetag << derivations << semicolon;
        value_type operator()( const cpg::parser::arguments& args ) const;
};

// ...�E��
struct derivations0_action { // colon << derivation;
        value_type operator()( const cpg::parser::arguments& args ) const;
};
struct derivations1_action { // derivations << pipe << derivation; 
        value_type operator()( const cpg::parser::arguments& args ) const;
};

// ...�E�ӂ̍���
struct derivation0_action { // lbracket  << rbracket;
        value_type operator()( const cpg::parser::arguments& args ) const;
};
struct derivation1_action { // lbracket << identifier << rbracket;
        value_type operator()( const cpg::parser::arguments& args ) const;
};
struct derivation2_action { // derivation << term;
        value_type operator()( const cpg::parser::arguments& args ) const;
};
struct term0_action { // identifier;
        value_type operator()( const cpg::parser::arguments& args ) const;
};
struct term1_action { // identifier << lparen << integer << rparen;
        value_type operator()( const cpg::parser::arguments& args ) const;
};

////////////////////////////////////////////////////////////////
// make_cpg_parser
void make_cpg_parser( cpg::parser& p );


////////////////////////////////////////////////////////////////
// collect_informations
void collect_informations(
        GenerateOptions&        options,
        symbol_map_type&        terminal_types,
        symbol_map_type&        nonterminal_types,
        const value_type&       ast );


#endif // CAPER_CPG_HPP
