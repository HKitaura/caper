// 2008/08/13 Naoyuki Hirayama

#include <llvm/Module.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Constants.h>
#include <llvm/Instructions.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Assembly/Writer.h>

#include <iostream>
#include <algorithm>
#include <map>
#include <string>
#include "leaf_node.hpp"
#include "leaf_ast.hpp"
#include "leaf_error.hpp"

// NOTICE:
//	�֐��̖߂�l�ƂȂ�R���e�L�X�g�ł͈ȉ��̂悤�ɐ��K������B
//	�v�f0�̃^�v�� => NULL ( void�֐�������Ԃ�l�͎̂Ă��� )
//	�v�f1�̃^�v�� => �J��グ( �P�l�֐� )

namespace leaf {

template < class T >
class Environment {
public:
	typedef std::map< Symbol*, T > dic_type;

public:
	Environment(){}
	~Environment(){}

	void push()
	{
		scope_.resize( scope_.size()+1 );
		//std::cerr << "push" << std::endl;
	}
	void pop()
	{
		scope_.pop_back();
		//std::cerr << "pop" << std::endl;
	}
	void bind( Symbol* i, T v )
	{
		//std::cerr << "bind " << i->s << "(" << i << ")" << std::endl;
		scope_.back().m[i] = v;
	}
	T find( Symbol* ident )
	{
		//std::cerr << "find " << ident->s << "(" << ident << ")" << std::endl;
		for( int i = int(scope_.size()) - 1 ; 0 <= i ; i-- ) {
			typename dic_type::const_iterator j = scope_[i].m.find( ident );
			if( j != scope_[i].m.end() ) {
				return (*j).second;
			}
		}
		return NULL;
	}

	void print( std::ostream& os )
	{
		for( int i = 0 ; i < int(scope_.size()) ; ++i ) {
			//os << "scope_ " << i << ":" << std::endl;
			for( typename dic_type::const_iterator j = scope_[i].m.begin() ;
				 j != scope_[i].m.end() ;
				 ++j ) {
				os << "	 " << (*j).first->s << "(" << (*j).first << ")"
				   << std::endl;
			}
		}
	}

private:
	struct Scope {
		std::map< Symbol*, T > m;
	};
	std::vector< Scope > scope_;

};


struct EncodeContext {
	llvm::Module*		m;
	llvm::Function*		f;
	llvm::BasicBlock*	bb;

	Environment< llvm::Value* > env;
};

struct EntypeContext {
	Environment< Type* > env;
};

inline
void make_reg( char* s, int id )
{
	sprintf( s, "reg%d", id );
}

template < class LHS, class RHS >
inline
llvm::Value*
binary_operator(
	int								id,
	EncodeContext&					context,
	LHS*							lhs,
	RHS*							rhs,
	llvm::Instruction::BinaryOps	op )
{
	char reg[256]; make_reg( reg, id );
	llvm::Value* v0 = lhs->encode( context );
	llvm::Value* v1 = rhs->encode( context );

	llvm::Instruction* inst = llvm::BinaryOperator::create(
		op, v0, v1, reg );
	context.bb->getInstList().push_back(inst);
	return inst;
}

inline
void check_bool_expr_type( const Header& h, type_t t )
{
	if( t && t != Type::getBoolType() ) {
		throw context_mismatch( h.beg, Type::getDisplay( t ), "<bool>" );
	}
}

inline
void check_int_expr_type( const Header& h, type_t t )
{
	if( t && t != Type::getIntType() ) {
		throw context_mismatch( h.beg, Type::getDisplay( t ), "<int>" );
	}
}

template < class T > inline
void entype_int_compare( EntypeContext& te, T& x )
{
	x.lhs->entype( te, Type::getIntType() );
	x.rhs->entype( te, Type::getIntType() );
	x.h.t = Type::getBoolType();
}

template < class T > inline
void entype_int_binary_arithmetic( EntypeContext& te, T& x )
{
	x.lhs->entype( te, Type::getIntType() );
	x.rhs->entype( te, Type::getIntType() );
	x.h.t = Type::getIntType();
}

////////////////////////////////////////////////////////////////
// Node
void Node::encode( llvm::Module* m )
{
	leaf::EncodeContext context;
	context.m = m;
	context.f = NULL;
	context.bb = NULL;
	encode( context );
}
void Node::entype()
{
	EntypeContext te;
	te.env.push();
	entype( te, NULL );
	te.env.pop();
}

////////////////////////////////////////////////////////////////
// Module
llvm::Value* Module::encode( EncodeContext& context )
{
	return topelems->encode( context );
}
void Module::entype( EntypeContext& te, type_t t )
{
	topelems->entype( te, t );
}

////////////////////////////////////////////////////////////////
// TopElems
llvm::Value* TopElems::encode( EncodeContext& context )
{
	for( size_t i = 0 ; i < v.size() ; i++ ) {
		v[i]->encode( context );
	}
	return NULL;
}
void TopElems::entype( EntypeContext& te, type_t t )
{
	for( size_t i = 0 ; i < v.size() ; i++ ) {
		v[i]->entype( te, t );
	}
}

////////////////////////////////////////////////////////////////
// TopElem
llvm::Value* TopElem::encode( EncodeContext& context )
{
	assert(0);
	return NULL;
}
void TopElem::entype( EntypeContext& te, type_t t )
{
	assert(0);
}

////////////////////////////////////////////////////////////////
// Require
llvm::Value* Require::encode( EncodeContext& context )
{
	module->encode( context );
	return NULL;
}
void Require::entype( EntypeContext& te, type_t t )
{
	module->entype( te, t );
}

////////////////////////////////////////////////////////////////
// FunDecl
llvm::Value* FunDecl::encode( EncodeContext& context )
{
	// signature
	std::vector< const llvm::Type* > types;
	for( size_t i = 0 ; i < sig->fargs->v.size() ; i++ ) {
		types.push_back( llvm::Type::Int32Ty );
	}

	llvm::FunctionType* ft =
		llvm::FunctionType::get(
			llvm::Type::Int32Ty, types, /* not vararg */ false );

	// function
	llvm::Function::Create(
		ft, llvm::Function::ExternalLinkage,
		sig->name->s->s,
		context.m );

	return NULL;
}
void FunDecl::entype( EntypeContext& te, type_t t )
{
	// �߂�l�̌^
	if( !sig->result_type ) {
		throw inexplicit_return_type( h.beg );
	}
	for( size_t i = 0 ; i < sig->result_type->v.size() ; i++ ) {
		if( !sig->result_type->v[i] ) {
			throw imcomplete_return_type( h.beg );
		}
	}

	typevec rtypes;
	for( size_t i = 0 ; i < sig->result_type->v.size() ; i++ ) {
		rtypes.push_back( sig->result_type->v[i]->t );
	}
	type_t result_type = Type::getTupleType( rtypes );

	// �����̌^
	typevec atypes;
	for( size_t i = 0 ; i < sig->fargs->v.size() ; i++ ) {
		if( !sig->fargs->v[i]->t ) {
			throw inexplicit_argument_type( h.beg );
		}
		atypes.push_back( sig->fargs->v[i]->t->t );
	}

	type_t args_type = Type::getTupleType( atypes );

	h.t = Type::getFunctionType( result_type, args_type );
	te.env.bind( sig->name->s, h.t );
}

////////////////////////////////////////////////////////////////
// FunDef
llvm::Value* FunDef::encode( EncodeContext& context )
{
	// signature
	std::vector< const llvm::Type* > types;
	for( size_t i = 0 ; i < sig->fargs->v.size() ; i++ ) {
		types.push_back( llvm::Type::Int32Ty );
	}

	llvm::FunctionType* ft =
		llvm::FunctionType::get(
			llvm::Type::Int32Ty, types, /* not vararg */ false );

	// function
	llvm::Function* f =
		llvm::Function::Create(
			ft, llvm::Function::ExternalLinkage,
			sig->name->s->s,
			context.m );

	// get actual arguments
	context.env.push();
	{
		int j = 0;
		for( llvm::Function::arg_iterator i = f->arg_begin();
			 i != f->arg_end() ;
			 ++i, ++j ) {
			i->setName( "arg_" + sig->fargs->v[j]->name->s->s );
			context.env.bind( sig->fargs->v[j]->name->s, i );
		}
	}

	// basic block
	llvm::BasicBlock* bb = llvm::BasicBlock::Create("ENTRY", f);

	context.bb = bb;
	context.f = f;
	llvm::Value* v = body->encode( context );

	context.env.pop();

	if( !v ) {
		throw noreturn( h.end );
	}
	context.bb->getInstList().push_back(llvm::ReturnInst::Create(v));


	return NULL;
}
void FunDef::entype( EntypeContext& te, type_t t )
{
	// �֐���`�ł̓R���e�L�X�g�^�𖳎�

	// �߂�l�̌^
	if( !sig->result_type ) {
		throw inexplicit_return_type( h.beg );
	}
	for( size_t i = 0 ; i < sig->result_type->v.size() ; i++ ) {
		if( !sig->result_type->v[i] ) {
			throw imcomplete_return_type( h.beg );
		}
	}

	typevec rtypes;
	for( size_t i = 0 ; i < sig->result_type->v.size() ; i++ ) {
		rtypes.push_back( sig->result_type->v[i]->t );
	}
	type_t result_type = Type::getTupleType( rtypes );

	// �����̌^
	typevec atypes;
	for( size_t i = 0 ; i < sig->fargs->v.size() ; i++ ) {
		if( !sig->fargs->v[i]->t ) {
			throw inexplicit_argument_type( h.beg );
		}
		atypes.push_back( sig->fargs->v[i]->t->t );
	}

	type_t args_type = Type::getTupleType( atypes );

	// �ċN�֐��̂��߂ɖ{�̂����bind
	h.t = Type::getFunctionType( result_type, args_type );
	te.env.bind( sig->name->s, h.t );

	te.env.push();
	for( size_t i = 0 ; i < sig->fargs->v.size() ; i++ ) {
		te.env.bind( sig->fargs->v[i]->name->s, sig->fargs->v[i]->t->t );
	}

	// �֐��̖߂�l�ɂȂ�R���e�L�X�g�ł͐��K��
	body->entype( te, Type::normalize( result_type ) );
	te.env.pop();
}

////////////////////////////////////////////////////////////////
// FunSig
llvm::Value* FunSig::encode( EncodeContext& context )
{
	assert(0);
	return NULL;
}
void FunSig::entype( EntypeContext& te, type_t t )
{
	assert(0);
}

////////////////////////////////////////////////////////////////
// FormalArgs
llvm::Value* FormalArgs::encode( EncodeContext& context )
{
	assert(0);
	return NULL;
}
void FormalArgs::entype( EntypeContext& te, type_t t )
{
	assert(0);
}

////////////////////////////////////////////////////////////////
// FormalArg
llvm::Value* FormalArg::encode( EncodeContext& context )
{
	assert(0);
	return NULL;
}
void FormalArg::entype( EntypeContext& te, type_t t )
{
	assert(0);
}

////////////////////////////////////////////////////////////////
// Block
llvm::Value* Block::encode( EncodeContext& context )
{
	return statements->encode( context );
}
void Block::entype( EntypeContext& te, type_t t )
{
	//std::cerr << "block " << Type::getDisplay( t ) << std::endl;
	statements->entype( te, t );

	if( !statements->v.empty() ) {
		h.t = statements->v.back()->h.t;
	}
}

////////////////////////////////////////////////////////////////
// Statements
llvm::Value* Statements::encode( EncodeContext& context )
{
	llvm::Value* vv;
	for( size_t i = 0 ; i < v.size() ; i++ ) {
		vv = v[i]->encode( context );

#if 0
		// call puti
		if( vv ) {
			llvm::CallInst::Create(
				context.m->getFunction( "puti" ), vv, "", context.bb );
		}
#endif
	}
	return vv;
}
void Statements::entype( EntypeContext& te, type_t t )
{
	for( size_t i = 0 ; i < v.size() ; i++ ) {
		v[i]->entype( te, i == v.size() - 1 ? t : NULL );
	}
}

////////////////////////////////////////////////////////////////
// Statement
llvm::Value* Statement::encode( EncodeContext& context )
{
	assert(0);
	return NULL;
}
void Statement::entype( EntypeContext& te, type_t t )
{
	assert(0);
}

////////////////////////////////////////////////////////////////
// VarDecl
llvm::Value* VarDecl::encode( EncodeContext& context )
{
	context.env.bind( name->s, value->encode( context ) );
	return NULL;
}
void VarDecl::entype( EntypeContext& te, type_t t )
{
	te.env.bind( name->s, this->t->t );
	h.t = NULL;
}

////////////////////////////////////////////////////////////////
// IfThenElse
llvm::Value* IfThenElse::encode( EncodeContext& context )
{
	llvm::Value* cond_value = cond->encode( context );

	char if_r[256]; sprintf( if_r, "if_r%d", h.id );
	char if_v[256]; sprintf( if_v, "if_v%d", h.id );

	llvm::Instruction* ifresult =
		new llvm::AllocaInst( 
			llvm::Type::Int32Ty,
			if_r,
			context.bb );

	char if_t_label[256]; sprintf( if_t_label, "IF_T%d", h.id );
	char if_f_label[256]; sprintf( if_f_label, "IF_F%d", h.id );
	char if_j_label[256]; sprintf( if_j_label, "IF_J%d", h.id );

	llvm::BasicBlock* tbb = llvm::BasicBlock::Create(
		if_t_label, context.f );
	llvm::BasicBlock* fbb = llvm::BasicBlock::Create(
		if_f_label, context.f );
	llvm::BasicBlock* jbb = llvm::BasicBlock::Create(
		if_j_label, context.f );

	llvm::BranchInst::Create( tbb, fbb, cond_value, context.bb );

	context.bb = tbb;
	new llvm::StoreInst( 
		iftrue->encode( context ),
		ifresult,
		context.bb );
	llvm::BranchInst::Create( jbb, context.bb );
	
	context.bb = fbb;
	new llvm::StoreInst(
		iffalse->encode( context ),
		ifresult,
		context.bb );
	llvm::BranchInst::Create( jbb, context.bb );

	context.bb = jbb;

	llvm::Instruction* loaded_result =
		new llvm::LoadInst( ifresult, if_v, context.bb );
	
	return loaded_result;
}
void IfThenElse::entype( EntypeContext& te, type_t t )
{
	cond->entype( te, Type::getBoolType() );

	iftrue->entype( te, t );
	iffalse->entype( te, t );

	if( t ) {
		if( !iftrue->h.t && !iffalse->h.t ) {

		} else if( iftrue->h.t && iffalse->h.t ) {
			if( iftrue->h.t != iffalse->h.t ) {
				throw type_mismatch(
					h.beg,
					Type::getDisplay( iftrue->h.t ) + " at true-clause" ,
					Type::getDisplay( iffalse->h.t ) + " at false-clause" );
			}
		} else if( iftrue->h.t && !iffalse->h.t ) {
			iffalse->entype( te, iftrue->h.t );
		} else if( !iftrue->h.t && iffalse->h.t ) {
			iftrue->entype( te, iffalse->h.t );
		}
	}

	h.t = iftrue->h.t;
}

////////////////////////////////////////////////////////////////
// Types
llvm::Value* Types::encode( EncodeContext& context )
{
	assert(0);
	return NULL;
}
void Types::entype( EntypeContext& te, type_t t )
{
	assert(0);
}

////////////////////////////////////////////////////////////////
// TypeRef
llvm::Value* TypeRef::encode( EncodeContext& context )
{
	assert(0);
	return NULL;
}
void TypeRef::entype( EntypeContext& te, type_t t )
{
	assert(0);
}

////////////////////////////////////////////////////////////////
// Expr
llvm::Value* Expr::encode( EncodeContext& context )
{
	assert(0);
	return NULL;
}
void Expr::entype( EntypeContext& te, type_t t )
{
	assert(0);
}

////////////////////////////////////////////////////////////////
// LogicalOr
llvm::Value* LogicalOr::encode( EncodeContext& context )
{
	if( v.size() == 1 ) {
		return v[0]->encode( context );
	}

	// ���ʃ��W�X�^
	char label[256];
	sprintf( label, "or_s%d_result", h.id );
	llvm::Instruction* orresult =
		new llvm::AllocaInst( 
			llvm::Type::Int1Ty,
			label,
			context.bb );

	// ���̐�����BasicBlock���쐬(�Ō�̃u���b�N�͎��s�u���b�N)
	std::vector< llvm::BasicBlock* > bb;
	for( size_t i = 0 ; i < v.size() ; i++ ) {
		sprintf( label, "or_s%d_%d", h.id, int(i) );
		bb.push_back( llvm::BasicBlock::Create( label, context.f ) );
	}

	// ���s�u���b�N
	new llvm::StoreInst( llvm::ConstantInt::getFalse(), orresult, bb.back() );

	// �����u���b�N
	sprintf( label, "or_s%d_ok", h.id );
	llvm::BasicBlock* success = llvm::BasicBlock::Create( label, context.f );
	new llvm::StoreInst( llvm::ConstantInt::getTrue(), orresult, success );

	// �ŏI�u���b�N
	sprintf( label, "or_s%d_final", h.id );
	llvm::BasicBlock* final = llvm::BasicBlock::Create( label, context.f );
	llvm::BranchInst::Create( final, bb.back() );
	llvm::BranchInst::Create( final, success );

	for( size_t i = 0 ; i < v.size() ; i++ ) {
		llvm::Value* vv = v[i]->encode( context );
		
		llvm::BranchInst::Create( success, bb[i], vv, context.bb );
		context.bb = bb[i];
	}

	context.bb = final;
	sprintf( label, "or_s%d_value", h.id );
	return new llvm::LoadInst( orresult, label, final );
}
void LogicalOr::entype( EntypeContext& te, type_t t )
{
	if( v.size() == 1 ) {
		v[0]->entype( te, t );
		h.t = v[0]->h.t;
	} else {
		check_bool_expr_type( h, t );
		for( size_t i = 0 ; i < v.size(); i++ ) {
			v[i]->entype( te, Type::getBoolType() );
		}
	}
}

////////////////////////////////////////////////////////////////
// LogicalAnd
llvm::Value* LogicalAnd::encode( EncodeContext& context )
{
	if( v.size() == 1 ) {
		return v[0]->encode( context );
	}

	// ���ʃ��W�X�^
	char label[256];
	sprintf( label, "and_s%d_result", h.id );
	llvm::Instruction* andresult =
		new llvm::AllocaInst( 
			llvm::Type::Int1Ty,
			label,
			context.bb );

	// ���̐�����BasicBlock���쐬(�Ō�̃u���b�N�͐����u���b�N)
	std::vector< llvm::BasicBlock* > bb;
	for( size_t i = 0 ; i < v.size() ; i++ ) {
		sprintf( label, "and_s%d_%d", h.id, int(i) );
		bb.push_back( llvm::BasicBlock::Create( label, context.f ) );
	}

	// �����u���b�N
	new llvm::StoreInst( llvm::ConstantInt::getTrue(), andresult, bb.back() );

	// ���s�u���b�N
	sprintf( label, "and_s%d_ng", h.id );
	llvm::BasicBlock* failure = llvm::BasicBlock::Create( label, context.f );
	new llvm::StoreInst( llvm::ConstantInt::getFalse(), andresult, failure );

	// �ŏI�u���b�N
	sprintf( label, "and_s%d_final", h.id );
	llvm::BasicBlock* final = llvm::BasicBlock::Create( label, context.f );
	llvm::BranchInst::Create( final, bb.back() );
	llvm::BranchInst::Create( final, failure );

	for( size_t i = 0 ; i < v.size() ; i++ ) {
		llvm::Value* vv = v[i]->encode( context );
		
		llvm::BranchInst::Create( bb[i], failure, vv, context.bb );
		context.bb = bb[i];
	}

	context.bb = final;
	sprintf( label, "and_s%d_value", h.id );
	return new llvm::LoadInst( andresult, label, final );
}
void LogicalAnd::entype( EntypeContext& te, type_t t )
{
	if( v.size() == 1 ) {
		v[0]->entype( te, t );
		h.t = v[0]->h.t;
	} else {
		check_bool_expr_type( h, t );
		for( size_t i = 0 ; i < v.size(); i++ ) {
			v[i]->entype( te, Type::getBoolType() );
		}
	}
}

////////////////////////////////////////////////////////////////
// Equality
llvm::Value* Equality::encode( EncodeContext& context )
{
	assert(0);
	return NULL;
}
void Equality::entype( EntypeContext& te, type_t t )
{
	assert(0);
}

////////////////////////////////////////////////////////////////
// EqualityEq
llvm::Value* EqualityEq::encode( EncodeContext& context )
{
	return 
		new llvm::ICmpInst(
			llvm::ICmpInst::ICMP_EQ,
			lhs->encode( context ),
			rhs->encode( context ),
			"eq",
			context.bb );
}
void EqualityEq::entype( EntypeContext& te, type_t t )
{
	check_bool_expr_type( h, t );
	entype_int_compare( te, *this );
}

////////////////////////////////////////////////////////////////
// EqualityNe
llvm::Value* EqualityNe::encode( EncodeContext& context )
{
	return 
		new llvm::ICmpInst(
			llvm::ICmpInst::ICMP_NE,
			lhs->encode( context ),
			rhs->encode( context ),
			"ne",
			context.bb );
}
void EqualityNe::entype( EntypeContext& te, type_t t )
{
	check_bool_expr_type( h, t );
	entype_int_compare( te, *this );
}

////////////////////////////////////////////////////////////////
// Relational
llvm::Value* Relational::encode( EncodeContext& context )
{
	assert(0);
	return NULL;
}
void Relational::entype( EntypeContext& te, type_t t )
{
	assert(0);
}

////////////////////////////////////////////////////////////////
// RelationalLt
llvm::Value* RelationalLt::encode( EncodeContext& context )
{
	return 
		new llvm::ICmpInst(
			llvm::ICmpInst::ICMP_SLT,
			lhs->encode( context ),
			rhs->encode( context ),
			"lt",
			context.bb );
}
void RelationalLt::entype( EntypeContext& te, type_t t )
{
	check_bool_expr_type( h, t );
	entype_int_compare( te, *this );
}

////////////////////////////////////////////////////////////////
// RelationalGt
llvm::Value* RelationalGt::encode( EncodeContext& context )
{
	return 
		new llvm::ICmpInst(
			llvm::ICmpInst::ICMP_SGT,
			lhs->encode( context ),
			rhs->encode( context ),
			"gt",
			context.bb );
}
void RelationalGt::entype( EntypeContext& te, type_t t )
{
	check_bool_expr_type( h, t );
	entype_int_compare( te, *this );
}

////////////////////////////////////////////////////////////////
// RelationalLe
llvm::Value* RelationalLe::encode( EncodeContext& context )
{
	return 
		new llvm::ICmpInst(
			llvm::ICmpInst::ICMP_SLE,
			lhs->encode( context ),
			rhs->encode( context ),
			"le",
			context.bb );
}
void RelationalLe::entype( EntypeContext& te, type_t t )
{
	check_bool_expr_type( h, t );
	entype_int_compare( te, *this );
}

////////////////////////////////////////////////////////////////
// RelationalGe
llvm::Value* RelationalGe::encode( EncodeContext& context )
{
	return 
		new llvm::ICmpInst(
			llvm::ICmpInst::ICMP_SGE,
			lhs->encode( context ),
			rhs->encode( context ),
			"ge",
			context.bb );
}
void RelationalGe::entype( EntypeContext& te, type_t t )
{
	check_bool_expr_type( h, t );
	entype_int_compare( te, *this );
}

////////////////////////////////////////////////////////////////
// Additive
llvm::Value* Additive::encode( EncodeContext& context )
{
	assert(0);
	return NULL;
}
void Additive::entype( EntypeContext& te, type_t t )
{
	assert(0);
}

////////////////////////////////////////////////////////////////
// AddExpr
llvm::Value* AddExpr::encode( EncodeContext& context )
{
	return binary_operator( h.id, context, lhs, rhs, llvm::Instruction::Add );
}
void AddExpr::entype( EntypeContext& te, type_t t )
{
	check_int_expr_type( h, t );
	entype_int_binary_arithmetic( te, *this );
}

////////////////////////////////////////////////////////////////
// SubExpr
llvm::Value* SubExpr::encode( EncodeContext& context )
{
	return binary_operator( h.id, context, lhs, rhs, llvm::Instruction::Sub );
}
void SubExpr::entype( EntypeContext& te, type_t t )
{
	check_int_expr_type( h, t );
	entype_int_binary_arithmetic( te, *this );
}

////////////////////////////////////////////////////////////////
// Multiplicative
llvm::Value* Multiplicative::encode( EncodeContext& context )
{
	assert(0);
	return NULL;
}
void Multiplicative::entype( EntypeContext& te, type_t t )
{
	assert(0);
}

////////////////////////////////////////////////////////////////
// MulExpr
llvm::Value* MulExpr::encode( EncodeContext& context )
{
	return binary_operator( h.id, context, lhs, rhs, llvm::Instruction::Mul );
}
void MulExpr::entype( EntypeContext& te, type_t t )
{
	check_int_expr_type( h, t );
	entype_int_binary_arithmetic( te, *this );
}

////////////////////////////////////////////////////////////////
// DivExpr
llvm::Value* DivExpr::encode( EncodeContext& context )
{
	return binary_operator( h.id, context, lhs, rhs, llvm::Instruction::SDiv );
}
void DivExpr::entype( EntypeContext& te, type_t t )
{
	check_int_expr_type( h, t );
	entype_int_binary_arithmetic( te, *this );
}

////////////////////////////////////////////////////////////////
// PrimExpr
llvm::Value* PrimExpr::encode( EncodeContext& context )
{
	assert(0);
	return NULL;
}
void PrimExpr::entype( EntypeContext& te, type_t t )
{
	assert(0);
}

////////////////////////////////////////////////////////////////
// LiteralBoolean
llvm::Value* LiteralBoolean::encode( EncodeContext& context )
{
	return value ?
		llvm::ConstantInt::getTrue() :
		llvm::ConstantInt::getFalse();
}
void LiteralBoolean::entype( EntypeContext& te, type_t t )
{
	if( t && t != Type::getBoolType() ) {
		throw context_mismatch( h.beg, "<bool>", Type::getDisplay( t ) );
	}
	h.t = Type::getBoolType();
}

////////////////////////////////////////////////////////////////
// LiteralInteger
llvm::Value* LiteralInteger::encode( EncodeContext& context )
{
	return llvm::ConstantInt::get( llvm::Type::Int32Ty, value );
}
void LiteralInteger::entype( EntypeContext& te, type_t t )
{
	if( t && t != Type::getIntType() ) {
		throw context_mismatch( h.beg, "<int>", Type::getDisplay( t ) );
	}
	h.t = Type::getIntType();
}

////////////////////////////////////////////////////////////////
// LiteralChar
llvm::Value* LiteralChar::encode( EncodeContext& context )
{
	return llvm::ConstantInt::get( llvm::Type::Int32Ty, value );
}
void LiteralChar::entype( EntypeContext& te, type_t t )
{
	if( t && t != Type::getIntType() ) {
		throw context_mismatch( h.beg, "<int>", Type::getDisplay( t ) );
	}
	h.t = Type::getIntType();
}

////////////////////////////////////////////////////////////////
// VarRef
llvm::Value* VarRef::encode( EncodeContext& context )
{
	llvm::Value* v = context.env.find( name->s );
	if( !v ) {
		//context.print( std::cerr );
		throw no_such_variable( h.beg, name->s->s );
	}

	return v;
}
void VarRef::entype( EntypeContext& te, type_t t )
{
	if( !t ) {
		type_t vt = te.env.find( name->s );
		if( !vt && t ) {
			vt = t;
			te.env.bind( name->s, t );
		} else if( vt && t && vt != t ) {
			throw context_mismatch(
				h.beg, Type::getDisplay( t ), Type::getDisplay( vt ) );
		}
		h.t = vt;
	}
}

////////////////////////////////////////////////////////////////
// Parenthized
llvm::Value* Parenthized::encode( EncodeContext& context )
{
	return expr->encode( context );
}
void Parenthized::entype( EntypeContext& te, type_t t )
{
	expr->entype( te, t );
	h.t = expr->h.t;
}

////////////////////////////////////////////////////////////////
// FunCall
llvm::Value* FunCall::encode( EncodeContext& context )
{
	llvm::Function* f = context.m->getFunction( func->s->s );
	if( !f ) {
		throw no_such_function( h.beg, func->s->s );
	}

	std::vector< llvm::Value* > args;
	for( size_t i = 0 ; i < aargs->v.size() ; i++ ) {
		args.push_back( aargs->v[i]->encode( context ) );
	}

	char reg[256];
	sprintf( reg, "ret%d", h.id );

	return llvm::CallInst::Create(
		context.m->getFunction( func->s->s ),
		args.begin(), args.end(), reg, context.bb );
}
void FunCall::entype( EntypeContext& te, type_t t )
{
	type_t ft = te.env.find( func->s );
	if( !Type::isFunction( ft ) ) {
		throw uncallable(
			h.beg, func->s->s + "(" + Type::getDisplay( ft ) + ")" );
	}

	// �߂�l�ƃR���e�L�X�g�^���~�X�}�b�`
	std::vector< type_t > vt;
	vt.push_back( t );
	if( t && ft->getReturnType() != Type::getTupleType( vt ) ) {
		throw context_mismatch(
			h.beg,
			func->s->s + "(" + Type::getDisplay( ft->getReturnType() ) + ")",
			Type::getDisplay( t ) );
	}

	// �����̌�������Ȃ�
	if( aargs->v.size() != ft->getArgumentType()->getElements().size() ) {
		throw wrong_arity(
			h.beg,
			aargs->v.size(),
			ft->getArgumentType()->getElements().size() );
	}

	// �������̌^�t��
	for( size_t i = 0 ; i < aargs->v.size() ; i++ ) {
		aargs->v[i]->entype( te, ft->getArgumentType()->getElements()[i] );
	}

	h.t = ft->getReturnType();
}

////////////////////////////////////////////////////////////////
// ActualArgs
llvm::Value* ActualArgs::encode( EncodeContext& context )
{
	assert(0);
	return NULL;
}
void ActualArgs::entype( EntypeContext& te, type_t t )
{
	assert(0);
}

////////////////////////////////////////////////////////////////
// ActualArg
llvm::Value* ActualArg::encode( EncodeContext& context )
{
	return expr->encode( context );
}
void ActualArg::entype( EntypeContext& te, type_t t )
{
	expr->entype( te, t );
	h.t = expr->h.t;
}

////////////////////////////////////////////////////////////////
// Identifier
llvm::Value* Identifier::encode( EncodeContext& context )
{
	assert(0);
	return NULL;
}
void Identifier::entype( EntypeContext& te, type_t t )
{
	assert(0);
}

} // namespace leaf
