//
//  ASTtoCode.cpp
//  uscc
//

#include "ASTNodes.h"
#include "Symbols.h"
#include "Emitter.h"

using namespace uscc::parse;
using namespace uscc::scan;

using std::shared_ptr;

#define ASTTOCODE(a) void a::ASTtoCode(std::ostream& output, int depth) const noexcept \
{

#define OUTS() for (int i = 0; i < depth; i++) \
{ \
	output << "    "; \
}

ASTTOCODE(ASTProgram)
    // PA1: Implement
	
	for( auto f : mFuncs ){
		f->ASTtoCode(output, depth);
	}
	
}

ASTTOCODE(ASTFunction) // "Function: "
    // PA1: Implement
	
	switch (mReturnType)
	{
		case Type::Void:
			output << "void ";
			break;
		case Type::Int:
			output << "int ";
			break;
		case Type::Char:
			output << "char ";
			break;
		default:
			output <<  "Shouldn't have gotten here. ";
			break;
	}
	output << mIdent.getName() << "(" ;

	auto delim = "";
	auto actual_delim = ", ";
	for (auto arg : mArgs)
	{
		output << delim;
		arg->ASTtoCode(output, depth);
		delim = actual_delim;
	}
	output << "){" << std::endl;
	
	mBody->ASTtoCode(output, depth + 1);
	
	output << "}" << std::endl;

}

ASTTOCODE(ASTArgDecl) // "ArgDecl: "
    // PA1: Implement
	switch (mIdent.getType())
	{
		case Type::Void:
			output << "void ";
			break;
		case Type::Int:
			output << "int ";
			break;
		case Type::Char:
			output << "char ";
			break;
		case Type::IntArray:
			output << "int[] ";
			break;
		case Type::CharArray:
			output << "char[] ";
			break;
		default:
			output << "Shouldn't have gotten here...";
			break;
	}
	output << mIdent.getName() << std::endl;
}

ASTTOCODE(ASTArraySub) // "ArraySub: "
    // PA1: Implement
	output << mIdent.getName() << "[";
	mExpr->ASTtoCode(output, depth + 1);
	output << "]";
}

ASTTOCODE(ASTBadExpr) // "BadExpr:"
    // nothing to print
}

ASTTOCODE(ASTLogicalAnd)
    // PA1: Implement
	mLHS->ASTtoCode(output, depth + 1);
	output << " && ";
	mRHS->ASTtoCode(output, depth + 1);
}

ASTTOCODE(ASTLogicalOr)
    // PA1: Implement
	mLHS->ASTtoCode(output, depth);
	output << " || ";
	mRHS->ASTtoCode(output, depth + 1);
}

ASTTOCODE(ASTBinaryCmpOp)
    // PA1: Implement
	mLHS->ASTtoCode(output, depth);
	output << " " << Token::Values[mOp]<< " ";
	mRHS->ASTtoCode(output, depth + 1);
	//output << ';';
}

ASTTOCODE(ASTBinaryMathOp)
    // PA1: Implement
	mLHS->ASTtoCode(output, depth);
	output << " " << Token::Values[mOp]<< " ";
	mRHS->ASTtoCode(output, depth + 1);
	//output << ';';
}

ASTTOCODE(ASTNotExpr)
    // PA1: Implement
	output << "!(";
	mExpr->ASTtoCode(output, depth + 1);
	output << ")";
}

ASTTOCODE(ASTConstantExpr)
    // PA1: Implement
	output << mValue;
}

ASTTOCODE(ASTStringExpr)
    // PA1: Implement
	output  << '"' << mString->getText();
}

ASTTOCODE(ASTIdentExpr) // IdentExpr:
    // PA1: Implement
	output  << mIdent.getName();
}

ASTTOCODE(ASTArrayExpr) // "ArrayExpr: "
    // PA1: Implement
	mArray->ASTtoCode(output, depth + 1);
}

ASTTOCODE(ASTFuncExpr) // "FuncExpr: "
    // PA1: Implement
	output << mIdent.getName() << "(";
	auto delim = "";
	auto actual_delim = ", ";
	for (auto arg : mArgs)
	{
		output << delim;
		arg->ASTtoCode(output, depth);
		if(arg->getNodeType() == "string") output << "\\n\"" ;
		delim = actual_delim;
	}
	output << ")" << std::endl;
}

ASTTOCODE(ASTIncExpr) // "IncExpr: "
    // PA1: Implement
	output << "++" << mIdent.getName();
}

ASTTOCODE(ASTDecExpr) // "DecExpr: "
    // PA1: Implement
	output << "--" << mIdent.getName();
}

ASTTOCODE(ASTAddrOfArray) // "AddrOfArray: "
    // PA1: Implement
	output << " &";
	mArray->ASTtoCode(output, depth);
}
			
ASTTOCODE(ASTToIntExpr) // "ToIntExpr: "
    // PA1: Implement
	mExpr->ASTtoCode(output, depth + 1);
}
			
ASTTOCODE(ASTToCharExpr) // ToCharExpr: "
    // PA1: Implement
	mExpr->ASTtoCode(output, depth + 1);
}

// Declaration
ASTTOCODE(ASTDecl) // "Decl: "
    // PA1: Implement
	switch (mIdent.getType())
	{
		case Type::Void:
			output << "void";
			break;
		case Type::Int:
			output << "int";
			break;
		case Type::Char:
			output << "char";
			break;
		case Type::IntArray:
			output << "int" << ' ' << mIdent.getName() << '[' << mIdent.getArrayCount() << ']';
			if (mExpr)
			{
				output << " = ";
				mExpr->ASTtoCode(output, depth + 1);
			}
			break;
		case Type::CharArray:
			output << "char" << ' ' << mIdent.getName() << '[' << mIdent.getArrayCount() << ']';
			if (mExpr)
			{
				output << " = ";
				mExpr->ASTtoCode(output, depth + 1);
			}
			break;
		default:
			output << "Shouldn't have gotten here...";
			break;
	}
	
	if(mIdent.getType() == Type::Int || mIdent.getType() == Type::Char || mIdent.getType() == Type::Void){
		output << ' ' << mIdent.getName();
		if (mExpr)
		{
			output << " = ";
			mExpr->ASTtoCode(output, depth + 1);
		}
	}
	else if(mIdent.getType() == Type::CharArray) output << "\"";
}

// Statements
ASTTOCODE(ASTCompoundStmt) // CompoundStmt:"
    // PA1: Implement
	for (auto decl : mDecls)
	{
		decl->ASTtoCode(output, depth);
		output << ';';
	}
	for (auto stmt : mStmts)
	{
		stmt->ASTtoCode(output, depth);
		//if(stmt->getNodeType == "while"){

		//}
		if(stmt->getNodeType() != "while" && stmt->getNodeType() != "if") output << ';';
	}
}

ASTTOCODE(ASTReturnStmt) // "ReturnStmt:
    // PA1: Implement
	if (!mExpr)
	{
	}
	else
	{
		output << "return ";
		mExpr->ASTtoCode(output, depth + 1);
		//output << ';';
	}
}

ASTTOCODE(ASTAssignStmt) // "AssignStmt: "
    // PA1: Implement
	output << mIdent.getName() << " = ";
	mExpr->ASTtoCode(output, depth + 1);
}

ASTTOCODE(ASTAssignArrayStmt) // "AssignArrayStmt:"
    // PA1: Implement
	mArray->ASTtoCode(output, depth + 1);
	mExpr->ASTtoCode(output, depth + 1);
}

ASTTOCODE(ASTIfStmt) // "IfStmt: "
    // PA1: Implement
	output << "if(";
	mExpr->ASTtoCode(output, depth + 1);
	output << "){";
	mThenStmt->ASTtoCode(output, depth + 1);
	output << "}";
	if (mElseStmt)
	{
		output << "else";
		if(mElseStmt->getNodeType() != "if") output << "{" ;
		else output << " ";
		mElseStmt->ASTtoCode(output, depth + 1);
		output << "}";
	}
}

ASTTOCODE(ASTWhileStmt) // "WhileStmt"
    // PA1: Implement
	output << "while(";
	mExpr->ASTtoCode(output, depth + 1);
	output << "){";
	mLoopStmt->ASTtoCode(output, depth + 1);
	output << "}";
}

ASTTOCODE(ASTExprStmt) // "ExprStmt"
    // PA1: Implement
	mExpr->ASTtoCode(output, depth + 1);
}

ASTTOCODE(ASTNullStmt) // "NullStmt"
    // PA1: Implement
}
