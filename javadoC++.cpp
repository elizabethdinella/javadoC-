#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/AST/ASTConsumer.h"
// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/CommandLine.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include "clang/AST/RecursiveASTVisitor.h"
#include <vector>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;
using namespace std;

class ASTMatcherVisitor : public RecursiveASTVisitor<ASTMatcherVisitor> {
	public:
		explicit ASTMatcherVisitor(ASTContext *Context){
			this->Context = Context;
		}

		bool DeclHelper(Decl *D){
			string nodeType = D->getDeclKindName();
			
			if(nodeType == "function"){
				FunctionDecl* fd = (FunctionDecl*) D;
				ArrayRef<ParmVarDecl*> params = fd->parameters();
				ArrayRef<ParmVarDecl*>::iterator itr = params.begin();
				for(itr; itr != params.end(); itr++){
					cout << "@param\t" << (*itr)->getName().data() << endl;
				}
			}
		}

		bool TraverseDecl(Decl *D) {
			bool continueTraversing = DeclHelper(D);
			if(continueTraversing){
				RecursiveASTVisitor<ASTMatcherVisitor>::TraverseDecl(D); // Forward to base class
			}

			return true;
		}

		bool StmtHelper(Stmt* S){
			string nodeType = x->getStmtClassName();

			if(nodeType == "return"){
				cout << "@return\t"<< endl;
			}
		}             

		bool TraverseStmt(Stmt *S) {
			StmtHelper(S);
			RecursiveASTVisitor<ASTMatcherVisitor>::TraverseStmt(S);
			return true;
		}

	private:
		ASTContext *Context;	
};

//AST CONSUMER - an interface that provides actions on the AST
class astConsumer : public clang::ASTConsumer{
	public:
		explicit astConsumer(ASTContext *Context) : Visitor(Context) {}

		virtual void HandleTranslationUnit(clang::ASTContext &Context){
			// Traversing the translation unit decl via a RecursiveASTVisitor
			// will visit all nodes in the AST.
			Visitor.TraverseDecl(Context.getTranslationUnitDecl());
		}

	private:
		//Declare a RecursiveASTVisitor
		ASTMatcherVisitor Visitor;

};



//FRONTEND ACTION - allows excecution of actions as part of the compilation
class ASTMatcherAction : public clang::ASTFrontendAction {
	public:

		//create an astConsumer to perform actions on the AST
		virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
				clang::CompilerInstance &Compiler, llvm::StringRef InFile) {
			return std::unique_ptr<clang::ASTConsumer>(
					new astConsumer(&Compiler.getASTContext()));
		}
};



int main(int argc, char** argv){

	if (argc > 1) {
		ifstream f(argv[1]);
		if(!f.good()){
			cerr << "can't open: " << argv[1] << endl;
		}
		stringstream buffer;
		buffer << f.rdbuf();
		clang::tooling::runToolOnCode(new ASTMatcherAction, buffer.str());
	}else{
		cerr << "please provide an input file" << endl;
	}

}

