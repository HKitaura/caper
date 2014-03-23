// Copyright (C) 2006 Naoyuki Hirayama.
// All Rights Reserved.

// $Id$

#include "empty_calc.hpp"
#include <iostream>

class unexpected_char : public std::exception {};

template < class It >
class scanner {
public:
        typedef int char_type;

public:
        scanner( It b, It e ) : b_(b), e_(e), c_(b), unget_(EOF) { }

        empty_calc::Token get( int& v )
        {
                int c;
                do {
                        c = getc();
                } while( isspace( c ) );

                // �L����
                switch( c ) {
                case '+': return empty_calc::token_Add;
                case '-': return empty_calc::token_Sub;
                case '*': return empty_calc::token_Mul;
                case '/': return empty_calc::token_Div;
                case EOF: return empty_calc::token_eof;
                }

                // ����
                if( isdigit( c ) ) {
                        int n = 0;
                        while( c != EOF && isdigit( c ) ) {
                                n *= 10;
                                n += c - '0';
                                c = getc();
                        }
                        ungetc( c );
                        v = n;
                        return empty_calc::token_Number;
                }


                std::cerr << char(c) << std::endl;
                throw unexpected_char();
        }

private:
        char_type getc()
        {
                int c;
                if( unget_ != EOF ) {
                        c = unget_;
                        unget_ = EOF;
                } else if( c_ == e_ ) {
                        c = EOF; 
                } else {
                        c = *c_++;
                }
                return c;
        }

        void ungetc( char_type c )
        {
                if( c != EOF ) {
                        unget_ = c;
                }
        }

private:
        It              b_;
        It              e_;
        It              c_;
        char_type       unget_;

};

struct SemanticAction {
        void syntax_error(){}
        void stack_overflow(){}
        void downcast( int& x, int y ) { x = y; }
        void upcast( int& x, int y ) { x = y; }

        int Identity( int n ) { return n; }
        int MakeAdd( int x, int y ) { std::cerr << "expr " << x << " + " << y << std::endl; return x + y ; }
        int MakeSub( int x, int y ) { std::cerr << "expr " << x << " - " << y << std::endl; return x - y ; }
        int MakeMul( int x, int y ) { std::cerr << "expr " << x << " * " << y << std::endl; return x * y ; }
        int MakeDiv( int x, int y ) { std::cerr << "expr " << x << " / " << y << std::endl; return x / y ; }
        int DoEmpty0(){ return 0; }
        int DoEmpty1(){ return 0; }
        int DoS(){ return 0; }
};

int main( int, char** )
{
        // �X�L���i
        typedef std::istreambuf_iterator<char> is_iterator;
        is_iterator b( std::cin );   // ���l�ɂ����VC++���ڒ����Ȃ��Ƃ�����
        is_iterator e;
        scanner< is_iterator > s( b, e );

        SemanticAction sa;

        empty_calc::Parser< int, SemanticAction > parser( sa );

        empty_calc::Token token;
        for(;;) {
                int v;
                token = s.get( v );
                if( parser.post( token, v ) ) { break; }
        }

        int v;
        if( parser.accept( v ) ) {
                std::cerr << "accepted\n";
                std::cerr << v << std::endl;
        }

        return 0;
}
