%skeleton "lalr1.cc" /* -*- C++ -*- */ //Scheletro di tipo LALR(1) per il parser
%require "3.2" //Versione minima richiesta di Bison 
%defines //Genera un file .hpp con le definizioni delle costanti dei token

%define api.location.file none
%define api.token.constructor //Necessario per la crezione delle funzioni make
%define api.value.type variant

%locations //Attiva il supporto per la gestione delle posizioni dei token

//Funzionalità di debug
%define parse.assert
%define parse.trace
%define parse.error verbose

//Riferimento al driver per ottenere, finito il parsing, l'AST
%param{Driver& driver}

//Forward declarations delle classi AST
%code requires{
    class Driver;
    class RootAST;
    class ExprAST;
    class SeqAST;
    class FunctionAST;
    class PrototypeAST;
    class GlobalVariableExprAST;
    class StmtAST;
    class AssignmentAST;
    class BlockAST;
    class BindingAST;
    class BinaryExprAST;
    class NumberExprAST;
    class ExpIfAST;
    class VariableExprAST;
    class CallExprAST;
    class IfAST;
    class ForAST;
    class InitAST;
    class UnaryExprAST;
}
//Forward declarations della classe del driver
%code{
  #include "driver.hpp"
  #include <string>
}

//Definizione dei token
%define api.token.prefix {TOK_}
%token
    ADD     "+"
    SUB     "-"
    MUL     "*"
    DIV     "/"
    ASS     "="
    EQ      "=="
    LT      "<"
    OBRACE  "{"
    CBRACE  "}"
    OPAREN  "("
    CPAREN  ")"
    CSTMT   ";"
    SEP     ","
    COND    "?"
    COL     ":"
    DEF     "def"
    EXT     "extern"
    VAR     "var"
    GLOB    "global"
    IF      "if"
    ELSE    "else"
    FOR     "for"
    NOT     "not"
    AND     "and"
    OR      "or"
    END  0  "end of file"
;
%token <std::string> ID "id"
%token <double> NUM "number"

//Definizione dei tipi non terminali
%type <RootAST*> program
%type <RootAST*> top
%type <FunctionAST*> definition
%type <PrototypeAST*> external
%type <PrototypeAST*> proto
%type <GlobalVariableExprAST*> globalvar
%type <std::vector<std::string>> idseq
%type <std::vector<StmtAST*>> stmts
%type <StmtAST*> stmt
%type <AssignmentAST*> assignment 
%type <BlockAST*> block
%type <std::vector<BindingAST*>> vardefs
%type <BindingAST*> binding
%type <ExprAST*> exp
%type <ExprAST*> initexp
%type <ExpIfAST*> expif
%type <ExprAST*> condexp
%type <ExprAST*> idexp
%type <std::vector<ExprAST*>> optexp
%type <std::vector<ExprAST*>> explist
%type <IfAST*> ifstmt
%type <ForAST*> forstmt
%type <InitAST*> init
%type <BinaryExprAST*> relexp

%%

//Assioma
%start startsymb;

startsymb:
    program                     {driver.root=$1;}
;

program:
    %empty                      {$$=new SeqAST(nullptr,nullptr);}
|   top ";" program             {$$=new SeqAST($1,$3);}
;

top:
    %empty                      {$$=nullptr;}
|   definition                  {$$=$1;}
|   external                    {$$=$1;}
|   globalvar                   {$$=$1;}
;

definition:
    "def" proto block           {$$=new FunctionAST($2,$3); $2->noemit();}
;

external:
    "extern" proto              {$$=$2;}
;

proto:
    "id" "(" idseq ")"          {$$=new PrototypeAST($1,$3);}
;

globalvar:
    "global" "id"               {$$=new GlobalVariableExprAST($2);}
;    

idseq:
    %empty                      {std::vector<std::string> ids; $$=ids;}
|   "id" idseq                  {$2.insert($2.begin(),$1); $$=$2;}
;

%left ":";
%left "<" "==";
%left "+" "-";
%left "*" "/";
%left "and" "or";
%left "not";

stmts:
    stmt                        {std::vector<StmtAST*> stmts; stmts.insert(stmts.begin(),$1); $$=stmts; }
|   stmt ";" stmts              {$3.insert($3.begin(),$1); $$=$3;}
;

stmt:
    assignment                  {$$=new StmtAST($1);}
|   block                       {$$=new StmtAST($1);}
|   ifstmt                      {$$=new StmtAST($1);} //secondo livello
|   forstmt                     {$$=new StmtAST($1);} //secondo livello
|   exp                         {$$=new StmtAST($1);}
;

assignment:
    "id" "=" exp                {$$=new AssignmentAST($1,$3);}
|   "+" "+" "id"                {$$=new AssignmentAST($3, new BinaryExprAST('+',new VariableExprAST($3),new NumberExprAST(1.0)));} //secondo livello
;

block:
    "{" stmts "}"               {std::vector<BindingAST*> defs; $$=new BlockAST(defs,$2);}
|   "{" vardefs ";" stmts "}"   {$$=new BlockAST($2,$4);}
;

//secondo livello
ifstmt:
    "if" "(" condexp ")" stmt                       {$$=new IfAST($3,$5,nullptr);}
|   "if" "(" condexp ")" stmt "else" stmt           {$$=new IfAST($3,$5,$7);};

//secondo livello
forstmt:
    "for" "(" init ";" condexp ";" assignment ")" stmt          {$$=new ForAST($3,$5,$7,$9);};

//secondo livello
init:
    binding                 { $$ = $1; }
|   assignment              { $$ = $1; };

vardefs:
    binding                     {std::vector<BindingAST*> bindings; bindings.push_back($1); $$=bindings;}
|   vardefs ";" binding         {$1.push_back($3); $$=$1;}
;

binding:
    "var" "id" initexp          {$$=new BindingAST($2,$3);}
;
  
exp:
    exp "+" exp                 {$$=new BinaryExprAST('+',$1,$3);}
|   exp "-" exp                 {$$=new BinaryExprAST('-',$1,$3);}
|   exp "*" exp                 {$$=new BinaryExprAST('*',$1,$3);}
|   exp "/" exp                 {$$=new BinaryExprAST('/',$1,$3);}
|   idexp                       {$$=$1;}
|   "(" exp ")"                 {$$=$2;}
|   "number"                    {$$=new NumberExprAST($1);}
|   expif                       {$$=$1;}
|   "-" idexp                   {$$=new BinaryExprAST('-',new NumberExprAST(0.0),$2);} //secondo livello
|   "-" "number"                {$$=new BinaryExprAST('-',new NumberExprAST(0.0),new NumberExprAST($2));} //secondo livello
;

//terzo livello
condexp:
    relexp                      {$$=$1;}
|   relexp "and" condexp        {$$=new BinaryExprAST('&',$1,$3);}
|   relexp "or" condexp         {$$=new BinaryExprAST('|',$1,$3);}
|   "not" condexp               {$$=new UnaryExprAST('!',$2);}
|   "(" condexp ")"             {$$=$2;}
;

initexp:
    %empty                      {$$=nullptr;}
|   "=" exp                     {$$=$2;}
;
                      
expif:
    condexp "?" exp ":" exp     {$$=new ExpIfAST($1,$3,$5);}
;

//terzo livello (modificato condexp -> relexp)
relexp:
    exp "<" exp                 {$$=new BinaryExprAST('<',$1,$3);}
|   exp "==" exp                {$$=new BinaryExprAST('=',$1,$3);}
;

idexp:
    "id"                        {$$=new VariableExprAST($1);}
|   "id" "(" optexp ")"         {$$=new CallExprAST($1,$3);}
;

optexp:
    %empty                      {std::vector<ExprAST*> args; $$=args;}
|   explist                     {$$=$1;}
;

explist:
    exp                         {std::vector<ExprAST*> exprs; exprs.push_back($1); $$=exprs;}
|   exp "," explist             {$3.insert($3.begin(),$1); $$=$3;}
;
 
%%

void yy::parser::error(const location_type& l,const std::string& s){
  std::cerr<<l<<": "<<s<<'\n';
}