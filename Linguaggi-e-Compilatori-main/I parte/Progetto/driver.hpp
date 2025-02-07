#ifndef DRIVER_HPP
#define DRIVER_HPP

#include <variant>
#include <string>
#include <vector>
#include <map>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Verifier.h"

#include "parser.hpp"

using namespace llvm;

#define YY_DECL \
    yy::parser::symbol_type yylex(Driver& driver)
YY_DECL;

class Driver{
    public:
        Driver();

        RootAST* root; //Radice dell'AST generato
        yy::location location;
        bool trace_parsing; //Per trace debug nel parser
        bool trace_scanning; //Per trace debug nello scanner
        std::string f;
        std::map<std::string,AllocaInst*> variables;

        void scan_begin(); //Implementata nello scanner
        void scan_end(); //Implementata nello scanner
        int parse(const std::string& file);
        void codegen();
};

typedef std::variant<std::string,double> lexval;
const lexval NONE=0.0;

//Classe base per l'AST
class RootAST{
    public:
        virtual ~RootAST(){}
        virtual lexval getLexVal(){
            return NONE;
        };
        virtual Value* codegen(Driver& driver){
            return nullptr;
        }    
};

//Rappresentazione di -> INIZIALIZZAZIONE
class InitAST : public RootAST{
    private:
        bool isDef;
    public:
        virtual Value* codegen(Driver& driver) override = 0;
        virtual const std::string& getName() const = 0;
        bool getIsDef();
        void setIsDef(bool isDef);
};

//Classe base per le espressioni
class ExprAST : public RootAST{};

//Rappresentazione di -> SEQUENZA DI ISTRUZIONI
class SeqAST : public RootAST{
    private:
        RootAST* First;
        RootAST* Continuation;
    public:
        SeqAST(RootAST* First,RootAST* Continuation);
        Value* codegen(Driver& driver) override;
};

//Rappresentazione di -> FUNZIONE
class FunctionAST : public RootAST{
    private:
        PrototypeAST* Proto;
        BlockAST* Body;
        bool External;
    public:
        FunctionAST(PrototypeAST* Proto,BlockAST* Body);
        Function* codegen(Driver& driver) override;
};

//Rappresentazione di -> PROTOTIPO
class PrototypeAST : public RootAST{
    private:
        std::string Name;
        std::vector<std::string> Args;
        bool emitcode;
    public:
        PrototypeAST(std::string Name,std::vector<std::string> Args);
        lexval getLexVal() const;
        Function* codegen(Driver& driver) override;
        void noemit();
};

//Rappresentazione di -> VARIABILE GLOBALE
class GlobalVariableExprAST : public ExprAST{
    private:
        std::string Name;
    public:
        GlobalVariableExprAST(const std::string& Name);
        lexval getLexVal() const;
        Value* codegen(Driver& driver) override;
};

//Rappresentazione di -> ISTRUZIONE
class StmtAST : public RootAST{
    private:
        RootAST* Stmt;
    public:
        StmtAST(RootAST* Stmt);
        Value* codegen(Driver& driver) override;
};

//Rappresentazione di -> ASSEGNAMENTO
class AssignmentAST : public InitAST{
    private:
        const std::string Name;
        ExprAST* Val; 
    public:
        AssignmentAST(const std::string Name,ExprAST* Val);
        Value* codegen(Driver& driver) override;
        const std::string& getName() const override;
};

//Rappresentazione di -> BLOCCO DI ISTRUZIONI
class BlockAST : public RootAST{
    private:
       std::vector<BindingAST*> Defs;
       std::vector<StmtAST*> Stmts;
    public:
        BlockAST(std::vector<BindingAST*> Defs, std::vector<StmtAST*> Stmts);
        Value* codegen(Driver& driver) override;
};

//Rappresentazione di -> BINDING
class BindingAST : public InitAST{
    private:
        const std::string Name;
        ExprAST* Val;
    public:
        BindingAST(const std::string& Name,ExprAST* Val);
        AllocaInst* codegen(Driver& driver) override;
        const std::string& getName() const override;
};

//Rappresentazione di -> OPERAZIONE BINARIA
class BinaryExprAST : public ExprAST{
    private:
        char Op;
        ExprAST* Lhs;
        ExprAST* Rhs;
    public:
        BinaryExprAST(char Op,ExprAST* Lhs,ExprAST* Rhs);
        Value* codegen(Driver& driver) override;
};

//Rappresentazione di -> NUMERO
class NumberExprAST : public ExprAST{
    private:
        double Val;
    public:
        NumberExprAST(double Val);
        lexval getLexVal() const;
        Value* codegen(Driver& driver) override;
};

//Rappresentazione di -> IF
class ExpIfAST : public ExprAST{
    private:
        ExprAST* Cond;
        RootAST* Then;
        RootAST* Else;
    public:
        ExpIfAST(ExprAST* Cond,RootAST* Then,RootAST* Else);
        Value* codegen(Driver& driver) override;
};

//Rappresentazione di -> VARIABILE
class VariableExprAST : public ExprAST{
    private:
        std::string Name;
    public:
        VariableExprAST(const std::string& Name);
        lexval getLexVal() const;
        Value* codegen(Driver& driver) override;
};

//Rappresentazione di -> CHIAMATA A FUNZIONE
class CallExprAST : public ExprAST{
    private:
        std::string Callee;
        std::vector<ExprAST*> Args;
    public:
        CallExprAST(const std::string Callee,std::vector<ExprAST*> Args);
        lexval getLexVal() const;
        Value* codegen(Driver& driver) override;
};

//Rappresentazione di -> IF
class IfAST : public RootAST{
    private:
        ExprAST* Cond;
        StmtAST* True;
        StmtAST* False;
    public:
        IfAST(ExprAST* Cond,StmtAST* True,StmtAST* False);
        Value* codegen(Driver& driver) override;
};

//Rappresentazione di -> CICLO FOR
class ForAST : public RootAST{
    private:
        InitAST* Init;
        ExprAST* Cond;
        RootAST* Assgnmnt;
        StmtAST* Stmt;
    public:
        ForAST(InitAST* Init,ExprAST* Cond,RootAST* Assgnmnt,StmtAST* Stmt);
        Value* codegen(Driver& driver) override;
};

//Rappresentazione di -> OPERAZIONE UNARIA
class UnaryExprAST : public ExprAST{
    private:
        char Op;
        ExprAST* Val;
    public:
        UnaryExprAST(char Op,ExprAST* Val);
        Value* codegen(Driver& driver) override;
};

#endif