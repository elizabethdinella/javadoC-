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

int addCommentsToWrite(int lineno, const string& comment);
bool printDebug = false;
bool nodeDebug = printDebug && false;
bool curFileDebug = printDebug && false;
string inputFname;
string inputFileBuffer = ""; 
map<int, string> commentsToWrite;
int lastLineNo;
bool prevFunc = false;

//TODO: FIX FOR HW3 and HW4 (classes), get rid of comments if empty

class ASTMatcherVisitor : public RecursiveASTVisitor<ASTMatcherVisitor> {
	public:
		explicit ASTMatcherVisitor(ASTContext *Context){
			this->Context = Context;
		}

		bool DeclHelper(Decl *D){
			string fname;
			if(!D || (!isInCurFile(Context, D, fname) && fname.size() > 0)){ return false; }
			else if(fname.size() == 0) { return true; }

			string nodeType = D->getDeclKindName();

			
			if(prevFunc && !isParentDecl(getParentIfExists(D), "Function") && !(nodeType == "ParmVar")){
				prevFunc = false;
				addCommentsToWrite(lastLineNo-1, "*/\n");
				if(printDebug){ cerr << "decl ending comments: " << nodeType << " " 
					<< "parent: " << getDeclParent(D, Context) << " " << endl; } 
			}


			if(nodeType == "Function" || nodeType == "CXXConstructor" || nodeType == "CXXMethod"){
				prevFunc = true;
				string comment = "/*\n";

				string sourceLoc = D->getLocStart().printToString(Context->getSourceManager());
				int pos = sourceLoc.find(":");	
				sourceLoc = sourceLoc.substr(pos+1);
				pos = sourceLoc.find(":");
				sourceLoc = sourceLoc.substr(0, pos);	

				stringstream s(sourceLoc);
				int lineno;
				s >> lineno;
				lastLineNo = lineno;

				if(printDebug) { cerr << sourceLoc << " lineno " << lineno << endl; }

				FunctionDecl* fd = (FunctionDecl*) D;
				ArrayRef<ParmVarDecl*> params = fd->parameters();
				ArrayRef<ParmVarDecl*>::iterator itr = params.begin();
				for(; itr != params.end(); itr++){
					comment += "* @param\t";
					comment += (*itr)->getName().data();
					comment += "\n";
				}

				if(!fd->hasBody()){
					comment += "*/\n";
				}

				if(printDebug){ cerr << "adding params for function: " << fd->getNameInfo().getName().getAsString() << endl; }
				addCommentsToWrite(lineno-1, comment);
				if(printDebug){ cerr << "added params"  << endl; }

			}else{
				if(nodeDebug){
					cerr << "node type: " << nodeType << endl;
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
			string comment = "";

			if(!S || (!isInCurFile(Context, S, fname) && fname.size() > 0)){ return false; }
			else if(fname.size() == 0) { return true; }

			
			string nodeType = S->getStmtClassName();


			const Decl* declParent = getDeclParent(S, Context);
			if(prevFunc && !isParentStmt(S, "Function") && !(declParent && declParent->getDeclKindName() == "ParmVar")){
				prevFunc = false;
				addCommentsToWrite(lastLineNo-1, "*/\n");
				if(printDebug){ cerr << "stmt ending comments: " << nodeType << "for line: " << lastLineNo-1;}
			}

			if(nodeType == "ReturnStmt"){
				ReturnStmt* rs = (ReturnStmt*) S;
				comment += "* @return\t";
				if(rs->getRetValue()){
					comment += rs->getRetValue()->getStmtClassName();
				}
				comment += "\n";
			}else if(nodeType == "CXXThrowExpr"){
				comment += "* @throws\t";
			}else if(nodeType == "StringLiteral" && isParentStmt(S, "CXXThrowExpr")){
				clang::StringLiteral* sl = (clang::StringLiteral*) S;
				comment += sl->getString().data();
				comment += "\n";
			}else{
				if(nodeDebug){
					cerr << "node type: " << nodeType << endl;
				}
			}

			if(isParentStmt(S, "Function")){
				addCommentsToWrite(lastLineNo-1, comment);
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

		const Decl* getParentIfExists(Decl* D){
			const Decl* parent = getDeclParent(D, Context);
			if(parent){
				return parent;
			}else{
				return D;	
			}
		}

		bool isInCurFile(ASTContext *Context, const Decl* D, string& filename){
			if(curFileDebug) { cerr << "decl: " << D->getDeclKindName() << " checking if in cur file" << endl; }

			if(!D) { return false; }

			SourceManager &sm = Context->getSourceManager();
			SourceLocation loc = D->getLocation();
			StringRef filenameRef = sm.getFilename(loc);
			filename = filenameRef.str();

			if(curFileDebug) { cerr << filename << " : " << inputFname << endl; }
			return filename == inputFname || filename == "input.cc"; 

		}

		bool isInCurFile(ASTContext *Context, const Stmt* S, string& filename){
			if(curFileDebug) { cerr << "stmt: checking if in cur file" << endl; }

			SourceManager &sm = Context->getSourceManager();
			SourceLocation loc = S->getLocStart();
			StringRef filenameRef = sm.getFilename(loc);
			filename = filenameRef.str();

			if(curFileDebug) { cerr << filename << endl; }

			return filename == inputFname || filename == "input.cc"; 
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


			//if(printDebug){ cerr << S->getStmtClassName() << " : " << nodeToFind  << endl; }

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

			//if(printDebug){ cerr << D->getDeclKindName() << " : " << nodeToFind  << endl; }

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


void writeComments(){
	map<int, string>::reverse_iterator itr = commentsToWrite.rbegin();
	while(itr != commentsToWrite.rend()){
		inputFileBuffer.insert(itr->first, itr->second);
		itr++;
	}

}

int addCommentsToWrite(int lineno, const string& comment){
	int count = 0;
	size_t pos = -1;
	while(count < lineno){
		pos = inputFileBuffer.find("\n", pos+1);
		if(pos == string::npos){
			cerr << "error in lineno: " << lineno << "! only " << count << " lines" << endl;
		}
		count++;
	}

	if(commentsToWrite.find(pos+1) != commentsToWrite.end()){
		commentsToWrite[pos+1] += comment;
	}else{
		commentsToWrite[pos+1] = comment;
	}

	return pos+1;
}	

void removeEmptyComments(){
	map<int, string>::iterator itr = commentsToWrite.begin();
	while(itr != commentsToWrite.end()){
		if(itr->second == "/*\n*/\n"){
			itr->second = "";
		}
		itr++;
	}
}



int main(int argc, char** argv){

	string temp;

	while(getline(cin, temp)){
		inputFileBuffer += temp;
		inputFileBuffer += "\n";
		//reading text
	}

	int* a = new int;

	if(printDebug) { cerr << inputFileBuffer << endl; }


	stringstream buffer;
	buffer << inputFileBuffer; 

	if(printDebug) { cerr << "running tool on code" << endl; }

	clang::tooling::runToolOnCode(new ASTMatcherAction, buffer.str());

	if(printDebug){ cerr << commentsToWrite[lastLineNo-1] << endl; }
	int index = addCommentsToWrite(lastLineNo-1, "");
	if(commentsToWrite[index].find("*/") == string::npos){
		if(printDebug){ cerr << "adding ending comments to line: " << lastLineNo-1 << endl; }
		addCommentsToWrite(lastLineNo-1, "*/\n");
	}

	removeEmptyComments();

	if(printDebug) { cerr << "done running tool on code" << endl; }

	writeComments();
	cout << inputFileBuffer;

}

