// Copyright (C) 2006 Naoyuki Hirayama.
// All Rights Reserved.

#include "caper_tgt.hpp"
#include "caper_error.hpp"

struct sr_conflict_reporter {
    typedef tgt::rule rule_type;

    void operator()(const rule_type& x, const rule_type& y) {
        std::cerr << "shift/reduce conflict: " << x << " vs " << y
                  << std::endl;
    }
};

struct rr_conflict_reporter {
    typedef tgt::rule rule_type;

    void operator()(const rule_type& x, const rule_type& y) {
        std::cerr << "reduce/reduce conflict: " << x << " vs " << y
                  << std::endl;
    }
};

////////////////////////////////////////////////////////////////
// collect_informations
void collect_informations(
    GenerateOptions&    options,
    symbol_map_type&    terminal_types,
    symbol_map_type&    nonterminal_types,
    const value_type&   ast) {
    std::unordered_set<std::string> known;      // �m�莯�ʎq��
    std::unordered_set<std::string> unknown;    // ���m�莯�ʎq��

    auto doc = get_node<Document>(ast);

    std::string recover_token = "";

    // �錾
    for(const auto& x: doc->declarations->declarations) {
        if (auto tokendecl = downcast<TokenDecl>(x)) {
            // %token�錾
            for (const auto& y: tokendecl->elements) {
                //std::cerr << "token: " <<y->name << std::endl;
                if (0 < known.count(y->name)) {
                    throw duplicated_symbol(tokendecl->range.beg,y->name);
                }
                known.insert(y->name);
                terminal_types[y->name] =y->type.s;
            }
        }
        if (auto tokenprefixdecl = downcast<TokenPrefixDecl>(x)) {
            // %token_prefix�錾
            options.token_prefix = tokenprefixdecl->prefix;
        }
        if (auto externaltokendecl = downcast<ExternalTokenDecl>(x)) {
            // %external_token�錾
            options.external_token = true;
        }
        if (auto allow_ebnf = downcast<AllowEBNF>(x)) {
            // %allow_ebnf�錾
            options.allow_ebnf = true;
        }
        if (auto namespacedecl = downcast<NamespaceDecl>(x)) {
            // %namespace�錾
            options.namespace_name = namespacedecl->name;
        }
        if (auto recoverdecl = downcast<RecoverDecl>(x)) {
            if (0 < known.count(recoverdecl->name)) {
                throw duplicated_symbol(
                    recoverdecl->range.beg, recoverdecl->name);
            }
            known.insert(recoverdecl->name);
            terminal_types[recoverdecl->name] = "$error";
            options.recovery = true;
            options.recovery_token = recoverdecl->name;
        }
        if (auto accessmodifierdecl = downcast<AccessModifierDecl>(x)) {
            options.access_modifier = accessmodifierdecl->modifier;
        }
        if (auto dontusestldecl = downcast<DontUseSTLDecl>(x)) {
            // %dont_use_stl�錾
            options.dont_use_stl = true;
        }
    }

    // �K��
    for (const auto& rule: doc->rules->rules) {
        if (known.find(rule->name) != known.end()) {
            throw duplicated_symbol(rule->range.beg, rule->name);
        }
        known.insert(rule->name);
        nonterminal_types[rule->name] = rule->type.s;

        for (const auto& choise: rule->choises->choises) {
            for(const auto& term: choise->elements) {
                unknown.insert(term->item->name);
            }
        }
    }

    // ���m�莯�ʎq���c���Ă�����G���[
    for (const auto& x: unknown) {
        if (known.count(x) == 0) {
            throw undefined_symbol(-1, x);
        }
    }
}

template <class T, class V>
class find_iterator {
public:
    typedef typename T::const_iterator  original_iterator_type;

public:
    find_iterator(const T& c, const V& v)
        : c_(c), it_(c.find(v)) {
    }

    operator bool() const {
        return it_ != c_.end();
    }

    const typename T::value_type::second_type& operator*() const {
        return (*it_).second;
    }

private:
    const T&                c_;
    original_iterator_type  it_;
    
};

template <class T, class V>
find_iterator<T, V> finder(const T& c, const V& v) {
    return find_iterator<T, V>(c, v);
}

std::string make_sequence_name(
    const std::string source_name,
    std::unordered_map<std::string, tgt::terminal>&     used_terminals,
    std::unordered_map<std::string, tgt::nonterminal>&  used_nonterminals) {

    int n = 0;
    while(true) {
        std::string x = source_name + "_seq" + std::to_string(n++);
        if (used_terminals.count(x) == 0 && used_nonterminals.count(x) == 0) {
            return x;
        }            
    }
}
    

////////////////////////////////////////////////////////////////
// make_target_rule
void make_target_rule(
    action_map_type&                                    actions,
    tgt::grammar&                                       g,
    const tgt::nonterminal&                             rule_left,
    std::shared_ptr<Choise>                             choise,
    const symbol_map_type&                              terminal_types,
    const symbol_map_type&                              nonterminal_types,
    std::unordered_map<std::string, tgt::terminal>&     terminals,
    std::unordered_map<std::string, tgt::nonterminal>&  nonterminals) {

    tgt::rule r(rule_left);

    std::unordered_map<size_t, semantic_action_argument> args;

    int source_index = 0;
    int max_index = -1;
    for (const auto& term: choise->elements) {
        if (0 <= term->argument_index) {
            // �Z�}���e�B�b�N�A�N�V�����̈����Ƃ��ėp������
            if (0 < args.count(term->argument_index)) {
                // duplicated
                throw duplicated_semantic_action_argument(
                    term->range.beg, choise->action_name, term->argument_index);
            }

            std::string name = term->item->name;
            std::string type;
            if (term->item->extension != Extension::None) {
                // EBNF���𐶐�
                name = make_sequence_name(name, terminals, nonterminals);
            }

            // �����ɂȂ�ꍇ�A�^���K�v
            if (auto l = finder(nonterminal_types, term->item->name)) {
                type = *l;
            }
            if (auto l = finder(terminal_types, term->item->name)) {
                if (*l == "") {
                    throw untyped_terminal(term->range.beg, term->item->name);
                }
                type = *l;
            }
            assert(type != "");

            if (term->item->extension != Extension::None) {
                // EBNF�^�𐶐�
                type = type + extension_label(term->item->extension);
            }            

            semantic_action_argument arg(source_index, type);
            args[term->argument_index] = arg;
            max_index = (std::max)(max_index, term->argument_index);
        }

        if (auto l = finder(terminals, term->item->name)) {
            r << *l;
        }
        if (auto l = finder(nonterminals, term->item->name)) {
            r << *l;
        }

        source_index++;
    }

    // �����ɔ�т���������G���[
    for (int k = 0 ; k <= max_index ; k++) {
        if (args.count(k) == 0) {
            throw skipped_semantic_action_argument(
                choise->range.beg, choise->action_name, k);
        }
    }

    // ���łɑ��݂��Ă���K����������G���[
    if (g.exists(r)) {
        throw duplicated_rule(choise->range.beg, r);
    }

    if (!choise->action_name.empty()) {
        semantic_action sa(choise->action_name);
        for (int k = 0 ; k <= max_index ; k++) {
            sa.args.push_back(args[k]);
            sa.source_indices.push_back(args[k].source_index);
        }
        actions[r] = sa;
    }
    g << r;
}

void make_target_parser(
    tgt::parsing_table&             table,
    std::map<std::string, size_t>&  token_id_map,
    action_map_type&                actions,
    const value_type&               ast,
    const symbol_map_type&          terminal_types,
    const symbol_map_type&          nonterminal_types) {

    auto doc = get_node<Document>(ast);

    // �e��f�[�^
    // ...�I�[�L���\(���O��terminal)
    std::unordered_map<std::string, tgt::terminal>      terminals;
    // ...��I�[�L���\(���O��nonterminal)
    std::unordered_map<std::string, tgt::nonterminal>   nonterminals;

    int error_token = -1;

    // terminals�̍쐬
    token_id_map["eof"] = 0;
    int id_seed = 1;
    for (const auto& x: terminal_types) {
        if (x.second != "$error") { continue; }
        token_id_map[x.first] = error_token = id_seed;
        terminals[x.first] = tgt::terminal(x.first, id_seed++);
    }
    for (const auto& x: terminal_types) {
        if (x.second == "$error") { continue; }
        token_id_map[x.first] = id_seed;
        terminals[x.first] = tgt::terminal(x.first, id_seed++);
    }

    // nonterminals�̍쐬
    for (const auto& x: nonterminal_types) {
        nonterminals[x.first] = tgt::nonterminal(x.first);
    }

    // �K��
    tgt::grammar g;
    for (const auto& rule: doc->rules->rules) {
        const tgt::nonterminal& rule_left = nonterminals[rule->name];
        if (g.size() == 0) {
            g << (tgt::rule(tgt::nonterminal("$implicit_root")) << rule_left);
        }

        for (const auto& choise: rule->choises->choises) {
            make_target_rule(
                actions,
                g,
                rule_left,
                choise,
                terminal_types,
                nonterminal_types,
                terminals,
                nonterminals);
        }
    }

    zw::gr::make_lalr_table(
        table,
        g,
        error_token,
        sr_conflict_reporter(),
        rr_conflict_reporter());
}

void expand_ebnf(const GenerateOptions& options, const value_type& ast) {
    auto doc = get_node<Document>(ast);

    for (const auto& rule: doc->rules->rules) {
        for (const auto& choise: rule->choises->choises) {
            for (const auto& term: choise->elements) {
                switch (term->item->extension) {
                    case Extension::None:
                        break;
                    case Extension::Star:
                        if (!options.allow_ebnf) {
                            throw unallowed_ebnf(term->range.beg);
                        }
                    case Extension::Plus:
                        if (!options.allow_ebnf) {
                            throw unallowed_ebnf(term->range.beg);
                        }
                    case Extension::Question:
                        if (!options.allow_ebnf) {
                            throw unallowed_ebnf(term->range.beg);
                        }
                        break;
                }
            }
        }
    }
}
