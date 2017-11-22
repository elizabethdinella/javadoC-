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

bool printDebug = true;
string inputFname;


class ASTMatcherVisitor : public RecursiveASTVisitor<ASTMatcherVisitor> {
	public:
		explicit ASTMatcherVisitor(ASTContext *Context){
			this->Context = Context;
		}

		bool DeclHelper(Decl *D){
			string fname;
			if(!D || !isInCurFile(Context, D, fname)){ return false; }

			string nodeType = D->getDeclKindName();

			if(printDebug) { cout << "decl: reached a " << nodeType << " node " << endl; }

			if(nodeType == "Function"){
				FunctionDecl* fd = (FunctionDecl*) D;
				ArrayRef<ParmVarDecl*> params = fd->parameters();
				ArrayRef<ParmVarDecl*>::iterator itr = params.begin();
				for(itr; itr != params.end(); itr++){
					cout << "@param\t" << (*itr)->getName().data() << endl;
				}
			}
			return true;
		}

		bool TraverseDecl(Decl *D) {
			bool continueTraversing = DeclHelper(D);
			if(continueTraversing){
				RecursiveASTVisitor<ASTMatcherVisitor>::TraverseDecl(D); // Forward to base class
			}

			return true;
		}

		bool StmtHelper(Stmt* S){
			string fname;
			if(!S || !isInCurFile(Context, S, fname)){ return false; }

			string nodeType = S->getStmtClassName();

			if(printDebug) { cout << "stmt: reached a " << nodeType << " node " << endl; }

			if(nodeType == "return"){
				cout << "@return\t"<< endl;
			}else if(nodeType == "CXXThrowExpr"){
				cout << "@throws\t";
			}else if(nodeType == "StringLiteral" && isParentStmt(S, "CXXThrowExpr")){
				clang::StringLiteral* sl = (clang::StringLiteral*) S;
				cout << sl->getString().data() << endl;
			}

			return true;
		}             

		bool TraverseStmt(Stmt *S) {
			StmtHelper(S);
			RecursiveASTVisitor<ASTMatcherVisitor>::TraverseStmt(S);
			return true;
		}

	private:
		ASTContext *Context;	


		bool isInCurFile(ASTContext *Context, const Decl* D, string& filename){
			if(printDebug) { cout << "decl: checking if in cur file" << endl; }

			if(!D) { return false; }
			
			SourceManager &sm = Context->getSourceManager();
			SourceLocation loc = D->getLocation();
			if(printDebug) { cout << "decl: got loc" << endl; }
			StringRef filenameRef = sm.getFilename(loc);
			filename = filenameRef.str();
			if(printDebug) { cout << "decl: got filename" << endl; }
			
			if(printDebug) { cout << filename << endl; }
		
			return filename == inputFname; 

			/*
			vector<string>::iterator fileItr = find(includeList.begin(), includeList.end(), filename);
			bool ret = fileItr != includeList.end();
			return ret;*/
		}

		bool isInCurFile(ASTContext *Context, const Stmt* S, string& filename){
			if(printDebug) { cout << "stmt: checking if in cur file" << endl; }

			SourceManager &sm = Context->getSourceManager();
			SourceLocation loc = S->getLocStart();
			StringRef filenameRef = sm.getFilename(loc);
			filename = filenameRef.str();

			if(printDebug) { cout << filename << endl; }

			return filename == inputFname;

			/*
			 vector<string>::iterator fileItr = find(includeList.begin(), includeList.end(), filename);
			bool ret = fileItr != includeList.end();
			if(debugPrint && S->getStmtClassName() == "CXXConstructExpr"){
				cerr << "context is is current file: "  << ret << endl;

			}
			return ret;*/
		}



		/*
		   Function to check is node of type "nodeToFind" is a parent of S
Note: this parent does not have to be a direct parent
It can be a grandparent, great grand parent etc
		 */
		bool isParentStmt(const Stmt *S, const string& nodeToFind){
			//root node
			if(S == NULL){
				return false;
			}

			//found the node we're looking for
			if(strcmp(S->getStmtClassName(), nodeToFind.c_str()) == 0){
				return true;
			}


			const Stmt* parent = getStmtParent(S, Context);
			const Decl* declParent = getDeclParent(S, Context);
			//if there are no more parents of type Stmt.
			//check the parents of type Decl for nodeToFind
			if(parent == NULL && isParentDecl(declParent, nodeToFind)){
				return true;
			}

			//recurse
			return isParentStmt(parent, nodeToFind);
		}

		/*
		   Function to check is node of type "nodeToFind" is a parent of D
Note: this parent does not have to be a direct parent
It can be a grandparent, great grand parent etc
		 */
		bool isParentDecl(const Decl *D, const string& nodeToFind){
			//root node
			if(D == NULL){
				return false;
			}

			//we found the node we're looking for
			if(strcmp(D->getDeclKindName(), nodeToFind.c_str()) == 0){
				return true;
			}


			const Decl* parent = getDeclParent(D, Context);
			const Stmt* stmtParent = getStmtParent(D, Context);
			//if there are no more parents of type Decl, 
			//check the parents of type Stmt for nodeToFind
			if(parent == NULL && isParentStmt(stmtParent, nodeToFind)){
				return true;
			}

			//recurse
			return isParentDecl(parent, nodeToFind);

		}

		/*
		   returns the first parent of input s that is of type Stmt
		 */
		const Stmt* getStmtParent(const Stmt *s, ASTContext *Context){
			const Stmt* ret = NULL;
			if(!s) {
				return ret;
			}
			const ASTContext::DynTypedNodeList parents = Context->getParents(*s);
			if(parents.size() > 0){
				ret = parents[0].get<Stmt>();
			}
			return ret;
		}


		/*
		   returns the first parent of input d that is of type Stmt
		 */
		const Stmt* getStmtParent(const Decl *d, ASTContext *Context){
			const Stmt* ret = NULL;
			if(!d) {
				return ret;
			}
			const ASTContext::DynTypedNodeList parents = Context->getParents(*d);
			if(parents.size() > 0){
				ret = parents[0].get<Stmt>();
			}
			return ret;
		}


		/*
		   returns the first parent of input d that is of type Decl
		 */

		const Decl* getDeclParent(const Decl* d,  ASTContext *Context){
			const Decl* ret = NULL;
			if(!d){
				return ret;
			}

			const ASTContext::DynTypedNodeList parents = Context->getParents(*d);
			if(parents.size() > 0){
				ret = parents[0].get<Decl>();
			}
			return ret;
		}

		/*
		   returns the first parent of input s that is of type Decl
		 */
		const Decl* getDeclParent(const Stmt* s, ASTContext *Context){
			const Decl* ret = NULL;
			if(!s){
				return ret;
			}
			const ASTContext::DynTypedNodeList parents = Context->getParents(*s);
			if(parents.size() > 0){
				ret = parents[0].get<Decl>();
			}
			return ret;
		}


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

		inputFname = argv[1];

		ifstream f(argv[1]);
		if(!f.good()){
			cerr << "can't open: " << argv[1] << endl;
		}
		stringstream buffer;
		buffer << f.rdbuf();

		if(printDebug) { cout << "running tool on code" << endl; }

		clang::tooling::runToolOnCode(new ASTMatcherAction, buffer.str());

		if(printDebug) { cout << "done running tool on code" << endl; }

	}else{
		cerr << "please provide an input file" << endl;
	}

}

