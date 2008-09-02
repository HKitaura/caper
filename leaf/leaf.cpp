// 2008/08/13 Naoyuki Hirayama

#include <string>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include "leaf_parser.hpp"
#include "leaf_compile.hpp"
#include "leaf_error.hpp"
#include "scoped_allocator.hpp"

struct commandline_options {
	std::string		infile;
	std::string		outfile;
};

void get_commandline_options(
	commandline_options&	cmdopt,
	int						argc,
	char**					argv )
{
	int state = 0;
	for( int index = 1 ; index < argc ; index++ ) {
		if( argv[index][0] == '-' ) {
			std::cerr << "leaf: unknown option: " << argv[index] << std::endl;
			exit(1);
		}

		switch( state ) {
		case 0: cmdopt.infile  = argv[index]; state++; break;
		case 1: cmdopt.outfile = argv[index]; state++; break;
		default:
			std::cerr << "leaf: too many arguments" << std::endl;
			exit(1);
		}
	}

	if( state < 2 ) {
		std::cerr << "leaf: usage: leaf input_filename output_filename"
				  << std::endl;;
		exit(1);
	}
		
}

int main( int argc, char** argv )
{
	// �R�}���h���C���I�v�V����
	commandline_options cmdopt;
	get_commandline_options( cmdopt, argc, argv );

	// ���̓t�@�C��
	std::ifstream ifs( cmdopt.infile.c_str() );
	if( !ifs ) {
		std::cerr << "leaf: can't open input file '" << cmdopt.infile << "'"
				  << std::endl;
		return 1;
	}

	// �o�̓t�@�C��
	std::ofstream ofs( cmdopt.outfile.c_str() );
	if( !ofs ) {
		std::cerr << "leaf: can't open output file '" << cmdopt.outfile << "'"
				  << std::endl;
		return 1;
	}

	// compile
	try {
		// �A���P�[�^
		heap_cage cage;

		// �V���{������
		leaf::SymDic symdic;

		// id�V�[�h
		int idseed = 1;

		// �Z�}���e�B�b�N�A�N�V����
		SemanticAction sa( cage, symdic, idseed );

		// �X�L���i
		typedef std::istreambuf_iterator<char> is_iterator;
		is_iterator b( ifs );	 // ���l�ɂ����VC++���ڒ����Ȃ��Ƃ�����
		is_iterator e;
		scanner_type s( cage, symdic, idseed, b, e );

		leaf::Node* v = read_from_file( cmdopt.infile, sa, s );

		compile( cmdopt.infile, s, v, ofs );
	}
	catch( leaf::error& e ) {
		if( e.addr < 0 ) {
			std::cerr << "leaf: " << e.what() << std::endl;
		} else {
			std::cerr << "leaf: " 
					  << e.what()
					  << "; file: " << e.filename
					  << ", line: " << e.lineno
					  << ", column: " << e.column
					  << std::endl;
		}
		return 1;
	}
	
	return 0;
}



