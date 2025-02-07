#include "driver.hpp"
#include "parser.hpp"

LLVMContext* context=new LLVMContext; //contesto necessario per la definizione di tipi, costanti e buildingBlock
IRBuilder<>* builder=new IRBuilder(*context); //classe che permette di costruire il codice IR
Module* module=new Module("Kaleidoscope",*context); //modulo che contiene le funzioni e le variabili globali

//---------------# Funzioni di supporto #---------------
/*
Funzione CreateEntryBlockAlloca:
  Restituisce un'istruzione IR che alloca un double in memoria e ne memorizza il puntatore
  in un registro SSA con il nome "name". L'istruzione è inserita all'inizio del blocco di ingresso
  della funzione "fun".
*/
static AllocaInst* CreateEntryBlockAlloca(Function* fun,StringRef name){
  IRBuilder<> builderTemp(&fun->getEntryBlock(),fun->getEntryBlock().begin());
  return builderTemp.CreateAlloca(Type::getDoubleTy(*context),nullptr,name);
};

//---------------# Implementazione classe Driver #---------------
//Costruttore
Driver::Driver(): trace_parsing(false),trace_scanning(false){};

//Metodo parse
int Driver::parse(const std::string& file){
  f=file;
  location.initialize(&f);
  scan_begin();
  yy::parser parser(*this);
  parser.set_debug_level(trace_parsing);
  int res=parser.parse();
  scan_end();
  return res;
};

//Metodo codegen
void Driver::codegen() {
  root->codegen(*this);
};

//---------------# Implementazione classe InitAST #---------------
//Metodo isDef
bool InitAST::getIsDef(){
  return isDef;
};

void InitAST::setIsDef(bool isDef){
  this->isDef=isDef;
};

//---------------# Implementazione classe SeqAST #---------------
//Costruttore
SeqAST::SeqAST(RootAST* First,RootAST* Continuation): First(First),Continuation(Continuation){};

/*
Metodo codegen:
  Viene generato il codice di first e poi quello di continuation se esiste
*/
Value* SeqAST::codegen(Driver& driver){
  if(First!=nullptr){
    Value* f=First->codegen(driver);
  }else if(Continuation==nullptr){
    return nullptr;
  }
  Value* c=Continuation->codegen(driver);
  return nullptr;
};

//---------------# Implementazione classe NumberExprAST #---------------
//Costruttore
NumberExprAST::NumberExprAST(double Val): Val(Val){};

/*
Metodo codegen:
  Ritorna il valore float contenuto nel nodo, utilizzato in futuro nella generazione
*/
Value* NumberExprAST::codegen(Driver& driver){
  return ConstantFP::get(*context,APFloat(Val));
};

//---------------# Implementazione classe VariableExprAST #---------------
//Costruttore
VariableExprAST::VariableExprAST(const std::string &Name): Name(Name){};

//Metodo getLexVal
lexval VariableExprAST::getLexVal() const{
  lexval lval=Name;
  return lval;
};

/*
Metodo codegen:
  La tabella "variables" associa a ogni variabile una funzione che alloca memoria
  e restituisce un registro SSA con il puntatore a tale memoria.
  Per generare il codice di una variabile, si deve:
    1)Ottenere il tipo della variabile e il registro SSA del puntatore dalla tabella associativa.
    2)Creare un'istruzione "load" con:
      - Il tipo della variabile,
      - Il registro SSA con il puntatore alla memoria,
      - Il nome del nuovo registro SSA dove salvare il valore caricato.
*/
Value* VariableExprAST::codegen(Driver& driver){
  AllocaInst* alloca=driver.variables[Name];
  if(!alloca){
    GlobalVariable* global=module->getNamedGlobal(Name);
    if(global){
      return builder->CreateLoad(global->getValueType(),global,Name.c_str());
    }
  }else{
    return builder->CreateLoad(alloca->getAllocatedType(),alloca,Name.c_str());
  }
  std::cerr<<"'"+Name+"' -> variabile non definita"<<std::endl;
  return nullptr;
};

//---------------# Implementazione classe BinaryExprAST #---------------
//Costruttore
BinaryExprAST::BinaryExprAST(char Op,ExprAST* Lhs,ExprAST* Rhs): Op(Op),Lhs(Lhs),Rhs(Rhs){};

/*
Metodo codegen:
  Vengono generati i codici per i 2 operandi e infine viene costruita l'istruzione
  relativa all'operatore binario con i valori memorizzati nei registri SSA.
*/
Value *BinaryExprAST::codegen(Driver& driver){
  Value* l=Lhs->codegen(driver);
  Value* r=Rhs->codegen(driver);
  if(!l || !r){
    return nullptr;
  }
  switch(Op){
    case '+':
      return builder->CreateFAdd(l,r,"addinstr");
    case '-':
      return builder->CreateFSub(l,r,"subinstr");
    case '*':
      return builder->CreateFMul(l,r,"mulinstr");
    case '/':
      return builder->CreateFDiv(l,r,"divinstr");
    case '=':
      return builder->CreateFCmpOEQ(l,r,"eqinstr");
    case '<':
      return builder->CreateFCmpULT(l,r,"ltinstr");
    case '&':
      return builder->CreateAnd(l,r,"andinstr");
    case '|':
      return builder->CreateOr(l,r,"orinstr");
    default:
      std::cerr <<"'"+std::string(1, Op)+"' -> operatore non utilizzabile"<< std::endl;
      return nullptr;
  }
};

//---------------# Implementazione classe CallExprAST #---------------
//Costruttore
CallExprAST::CallExprAST(std::string Callee,std::vector<ExprAST*> Args): Callee(Callee),Args(std::move(Args)){};

//Metodo getLexVal
lexval CallExprAST::getLexVal() const{
  lexval lval=Callee;
  return lval;
};

/*
Metodo codegen:
  Per generare il codice di una chiamata di funzione si deve:
    1)Cercare la funzione nel modulo corrente, se non viene trovata viene generato un errore.
    2)Controllare che il numero di argomenti corrisponda a quello previsto dalla funzione.
    3)Calcolare il valore degli argomenti nella chiamata e infine vengono inseriti in un vettore
      passato successivamente al metodo CreateCall del builder.
*/
Value* CallExprAST::codegen(Driver& driver){
  Function* fun=module->getFunction(Callee);
  if(!fun){
    std::cerr<<"La funzione '"+Callee+"' non è definita"<<std::endl;
    return nullptr;
  }
  if(fun->arg_size()!=Args.size()){
    std::cerr<<"Il numero di argomenti non corrisponde "<<std::endl;
    return nullptr;
  }
  std::vector<Value*> args;
  for(int i=0;i<Args.size();i++){
    args.push_back(Args[i]->codegen(driver));
    if(!args.back()){
      return nullptr;
    }
  }
  return builder->CreateCall(fun, args, "callinst");
};

//---------------# Implementazione classe ExpIfAST #---------------
//Costruttore
ExpIfAST::ExpIfAST(ExprAST* Cond, RootAST* Then, RootAST* Else): Cond(Cond), Then(Then), Else(Else){};

/*
Metodo codegen:
  Per generare il codice di un'espressione if-then-else si deve:
    1)Ottenere il codice della condizione, memorizzando il risultato 
      del registro SSA nella variabile "cond".
    2)Creare 3 basic block per rappresentare le possibili diramazioni del condizionale: 
        - "thenBB" per la parte "then",
        - "elseBB" per la parte "else"
        - "mergeBB" dove i due flussi si riuniscono dopo l'esecuzione del condizionale.
    3)Generare un'istruzione di salto condizionato che salta al blocco "thenBB" se la 
      condizione è vera, altrimenti al blocco "elseBB".
    4)Generare il codice per il blocco "then" ricorsivamente e farlo terminare con un 
      salto incondizionato al blocco "mergeBB".
    5)Aggiornare il blocco corrente (che potrebbe essere cambiato durante la
      generazione del codice del blocco "then") e inserire il blocco "elseBB" alla fine
      nella funzione.
    6)Generare il codice per il blocco "else" facendolo terminare con un salto 
      incondizionato al blocco "mergeBB".
    7)Aggiornare nuovamente il blocco corrente e inserire il blocco "mergeBB" alla fine 
      della funzione.
    8)Infine, generare un'istruzione PHI che selezionerà il valore corretto da restituire 
      a seconda del blocco da cui proviene il flusso di esecuzione.
*/
Value* ExpIfAST::codegen(Driver& driver){
  Value* cond=Cond->codegen(driver);
  if(cond){
    Function* fun=builder->GetInsertBlock()->getParent();
    BasicBlock* thenBB=BasicBlock::Create(*context,"then",fun);
    BasicBlock* elseBB=BasicBlock::Create(*context,"else");
    BasicBlock* mergeBB=BasicBlock::Create(*context,"exit");
    builder->CreateCondBr(cond,thenBB,elseBB);
    builder->SetInsertPoint(thenBB);
    Value* thenVal=Then->codegen(driver);
    if(thenVal){
      builder->CreateBr(mergeBB);
      thenBB=builder->GetInsertBlock();
      fun->insert(fun->end(),elseBB);
      builder->SetInsertPoint(elseBB);
      Value* elseVal=Else->codegen(driver);
      if(elseVal){
        builder->CreateBr(mergeBB);
        elseBB=builder->GetInsertBlock();
        fun->insert(fun->end(),mergeBB);
        builder->SetInsertPoint(mergeBB);
        PHINode* phi=builder->CreatePHI(Type::getDoubleTy(*context),2,"phires");
        phi->addIncoming(thenVal,thenBB);
        phi->addIncoming(elseVal,elseBB);
        return phi;
      }else
        return nullptr;
    }else
      return nullptr;
  }else
    return nullptr; 
};

//---------------# Implementazione classe BlockAST #---------------
//Costruttore
BlockAST::BlockAST(std::vector<BindingAST*> Defs,std::vector<StmtAST*> Stmts): Defs(std::move(Defs)),Stmts(std::move(Stmts)){};  

/*
Metodo codegen:
  Per generare il codice di un blocco di istruzioni e gestire la visibilità di esse si deve:
    1)Per ogni definizione di variabile nel blocco:
      - Generare il codice per l'espressione associata alla variabile. Il risultato di questa 
        valutazione è un registro SSA che rappresenta il valore dell'espressione.
      - Creare un'istruzione di allocazione per la variabile nel blocco corrente, gestendo 
        la visibilità: se la variabile è già definita, la vecchia definizione viene temporaneamente
        rimossa dalla tabella dei simboli e salvata in un vettore temporaneo (`AllocaTmp`) per essere
        ripristinata dopo l'uscita dal blocco.
    2)Generare il codice per ciascun statement nel blocco controllando che il valore di ritorno
      di ciascuno sia diverso da nullptr.
    3)Ripristinare le vecchie definizioni delle variabili nella tabella dei simboli riprendendo la 
      visibilità esterna al blocco.
    4)Restituire l'ultimo valore del blocco (l'ultimo valore SSA prodotto da uno statement all'interno 
      del blocco).
*/
Value* BlockAST::codegen(Driver& driver){
  std::vector<AllocaInst*> oldBindings;
  for(auto &def:Defs){
    AllocaInst* instr=def->codegen(driver);
    if(!instr)
      return nullptr;
    oldBindings.push_back(driver.variables[def->getName()]);
    driver.variables[def->getName()]=instr;
  }
  Value* val;
  for(auto &stmt:Stmts){
    val=stmt->codegen(driver);
    if(!val)
      return nullptr;
  }
  for(auto i=0;i<Defs.size();i++){
    driver.variables[Defs[i]->getName()]=oldBindings[i];
  }
  return val;
};

//---------------# Implementazione classe BindingAST #---------------
//Costruttore
BindingAST::BindingAST(const std::string& Name,ExprAST* Val): Name(Name),Val(Val){
  this->setIsDef(true);
};

//Metodo getName
const std::string& BindingAST::getName() const{
  return Name;
};

/*
Metodo codegen:
  Per generare il codice di una dichiarazione di variabile si deve:
    1)Recuperare il riferimento alla funzione corrente per l'allocazione di memoria nell'entry block
      della funzione.
    2)Generare il codice che definisce la variabile
    3)Creare l'istruzione "alloca" con "CreateEntryBlockAlloca()" che alloca memoria per la variabile
      nell'entry block della funzione corrente.
    4)Creare l'istruzione `store` per memorizzare il valore dell'espressione nell'area di memoria allocata 
      per la variabile.
    5)Ritornare l'istruzione di allocazione, così da aggiornare la symbol table.
*/
AllocaInst* BindingAST::codegen(Driver& driver){
  Function* fun=builder->GetInsertBlock()->getParent();
  Value* val=Val->codegen(driver);
  if(val){
    AllocaInst* alloca=CreateEntryBlockAlloca(fun,Name);
    builder->CreateStore(val,alloca);
    return alloca;
  }else{
    return nullptr;
  }
};

//---------------# Implementazione classe PrototypeAST #---------------
//Costruttore
PrototypeAST::PrototypeAST(std::string Name,std::vector<std::string> Args): Name(Name),Args(std::move(Args)),emitcode(true){};

//Metodo getLexVal
lexval PrototypeAST::getLexVal() const{
  lexval lval=Name;
  return lval;
};

//Metodo noemit per evitare la doppia emissione del codice
void PrototypeAST::noemit() { 
    emitcode=false; 
};

/*
Metodo codegen:
  Per generare il codice di un prototipo di funzione si deve:
    1)Creare un vettore di tipi per gli argomenti della funzione.
    2)Creare un tipo di funzione con il tipo di ritorno double e il vettore di tipi 
      degli argomenti.
    3)Creare la funzione con il tipo appena creato, il nome e il modulo corrente.
    4)Per ogni argomento della funzione, assegnare un nome all'argomento.
    5)Emettere il codice se esso corrisponde ad una dichiaraizone di tipo extern. 
      In caso contrario, il prototipo corrisponde ad una definizione di funzione perciò
      l'emissione viene fatta nel metodo "FunctionAST::codegen".
*/
Function* PrototypeAST::codegen(Driver& driver){
  std::vector<Type*> args(Args.size(),Type::getDoubleTy(*context));
  FunctionType* type=FunctionType::get(Type::getDoubleTy(*context), args, false);
  Function* fun=Function::Create(type,Function::ExternalLinkage,Name,*module);
  auto i=0;
  for(auto &arg : fun->args())
      arg.setName(Args[i++]);
  if(emitcode){
      fun->print(errs());
      fprintf(stderr, "\n");
  };
  return fun;
};

//---------------# Implementazione classe FunctionAST #---------------
//Costruttore
FunctionAST::FunctionAST(PrototypeAST* Proto,BlockAST* Body): Proto(Proto),Body(Body){};

/*
Metodo codegen:
  Per generare il codice di una funzione si deve:
    1)Controllare se la funzione nel modulo corrente sia stata già definita, nel caso, viene generato 
      un errore altimenti si procede con la definizione della funzione.
    2)Creare un basic block dove inserire il codice della funzione.
    3)Per ogni parametro formale della funzione, si aggiunge alla symbol table una coppia in cui la chiave 
      è il nome del parametro e il valore è un'istruzione `alloca`. Questa istruzione `alloca` viene generata 
      utilizzando `CreateEntryBlockAlloca()`.
    4)Generare il codice del corpo della funzione e farlo terminare con un'istruzione `ret`.
    5)Verificare la correttezza della funzione e stamparla.
*/
Function* FunctionAST::codegen(Driver& driver){
  Function* fun=module->getFunction(std::get<std::string>(Proto->getLexVal()));
  if(!fun){
    fun=Proto->codegen(driver);
    if(!fun)
      return nullptr;
  }else{
    std::cerr<<"La funzione '"+std::get<std::string>(Proto->getLexVal())+"' è già definita"<<std::endl;
    return nullptr;
  }
  BasicBlock* BB=BasicBlock::Create(*context,"entry",fun);
  builder->SetInsertPoint(BB);
  for(auto &arg:fun->args()){
    AllocaInst* alloca=CreateEntryBlockAlloca(fun,arg.getName());
    builder->CreateStore(&arg,alloca);
    driver.variables[std::string(arg.getName())]=alloca;
  }
  Value* body=Body->codegen(driver);
  if(body){
    builder->CreateRet(body);
    verifyFunction(*fun);
    fun->print(errs());
    fprintf(stderr, "\n");
    return fun;
  }else{
    fun->eraseFromParent();
    return nullptr;
  }
};

//---------------# Implementazione classe StmtAST #---------------
//Costruttore
StmtAST::StmtAST(RootAST* Stmt): Stmt(Stmt){};

//Metodo codegen
//Genera il codice dell'istruzione contenuta nello statement
Value *StmtAST::codegen(Driver& driver){
    return Stmt->codegen(driver);
};

//---------------# Implementazione classe AssignmentAST #---------------
//Costruttore
AssignmentAST::AssignmentAST(const std::string Name,ExprAST* Val): Name(Name),Val(Val){
  this->setIsDef(false);
};

//Metodo getName
const std::string& AssignmentAST::getName() const{
  return Name;
};

/*
Metodo codegen:
  Per generare il codice di un'assegnazione si deve:
    1)Recuperare il valore della variabile dalla tabella associativa "variables" passando gli argomenti,
      se non è presente, si cerca tra le variabili globali.
    2)Generare il codice per l'espressione associata alla variabile.
    3)Creare un'istruzione `store` per memorizzare il valore dell'espressione ovvero il contenuto del
      registro `Val`.
*/
Value* AssignmentAST::codegen(Driver& driver){
  Value* variable=driver.variables[Name];
  if(!variable){
    variable=module->getNamedGlobal(Name);
      if(!variable)
        return nullptr;
  }
  Value* val=Val->codegen(driver);
  if(val){
    builder->CreateStore(val,variable);
    return variable;
  }else{
    std::cerr<<"'"+Name+"' -> variabile non definita"<<std::endl;
    return nullptr;
  }
};

//---------------# Implementazione classe GlobalVariableExprAST #---------------
//Costruttore
GlobalVariableExprAST::GlobalVariableExprAST(const std::string& Name): Name(Name){};

//Metodo getLexVal
lexval GlobalVariableExprAST::getLexVal() const{
  lexval lval=Name;
  return lval;
};

/*
Metodo codegen:
  Per generare il codice di una variabile globale si deve:
    1)Creare una variabile globale con il nome e il modulo corrente initializzata a 0.
    2)Controllare se la variabile è stata definita, in caso contrario viene generato un errore.
    3)Stampare la variabile globale.
*/
Value* GlobalVariableExprAST::codegen(Driver& driver){
  GlobalVariable* global=new GlobalVariable(*module,Type::getDoubleTy(*context),false,GlobalValue::CommonLinkage,ConstantFP::get(*context, APFloat(0.0)),Name);
  if(!global){
    std::cerr<<"'"+Name+"' -> variabile globale non definita"<<std::endl;
    return nullptr;
  }else{
    global->print(errs());
    fprintf(stderr, "\n");
    return global;
  }
};

//---------------# Implementazione classe IfAST #---------------
//Costruttore
IfAST::IfAST(ExprAST* Cond,StmtAST* True,StmtAST* False): Cond(Cond),True(True),False(False){};

Value* IfAST::codegen(Driver& driver){
  //Generazione del codice per la valutazione della condizione memorizzando il risultato
  //in un registro SSA memorizzato a sua volta in una variabile
  Value* cond=Cond->codegen(driver);
  if(cond){
    //Creazione dei basic block del costrutto if
    Function* fun=builder->GetInsertBlock()->getParent();
    //Inserimento del blocco 'true' nella funzione
    BasicBlock* trueBB=BasicBlock::Create(*context,"true",fun);
    BasicBlock* mergeBB=BasicBlock::Create(*context,"endif");
    BasicBlock* falseBB;
    //Se è presente la parte else
    if(False){
      //Creazione del blocco else
      falseBB=BasicBlock::Create(*context,"else");
      //Creazione del salto condizionato al blocco true o false
      builder->CreateCondBr(cond,trueBB,falseBB);
    }else{
      //Se non è presente la parte else, il salto condizionato va direttamente al blocco di fine
      builder->CreateCondBr(cond,trueBB,mergeBB);
    }
    //Generazione del codice per la parte true
    builder->SetInsertPoint(trueBB);
    Value* trueVal=True->codegen(driver);
    if(trueVal){
      //Creazione del salto incondizionato al blocco di fine
      builder->CreateBr(mergeBB);
      //Recupero del blocco corrente poichè, nel caso ci siano più if innestati, il blocco corrente
      //potrebbe essere cambiato però il blocco di fine è sempre lo stesso
      trueBB=builder->GetInsertBlock();
      Value* falseVal;
      if(False){
        //Se il ramo else è presente, generazione il codice per la parte false
        fun->insert(fun->end(),falseBB);
        builder->SetInsertPoint(falseBB);
        falseVal=False->codegen(driver);
        if(falseVal){
          //Creazione del salto incondizionato al blocco di fine
          builder->CreateBr(mergeBB);
          //Recupero del blocco corrente per lo stesso motivo di prima
          falseBB=builder->GetInsertBlock();
        }
      }
      //Aggancio il blocco di fine alla funzione
      fun->insert(fun->end(),mergeBB);
      builder->SetInsertPoint(mergeBB);
      if(False){
        //Se il ramo else è presente, generazione dell'istruzione PHI per selezionare il valore
        //da restituire a seconda del blocco da cui proviene il flusso di esecuzione
        PHINode* phi=builder->CreatePHI(Type::getDoubleTy(*context),2,"mergeval");
        phi->addIncoming(trueVal,trueBB);
        phi->addIncoming(falseVal,falseBB);
        return phi;
      }else{
        //Se il ramo else non è presente, restituisco il valore del blocco true
        return trueVal;
      }
    }
  }
  return nullptr;
};

//---------------# Implementazione classe ForAST #---------------
//Costruttore
ForAST::ForAST(InitAST* Init,ExprAST* Cond,RootAST* Assgnmnt,StmtAST* Stmt): Init(Init),Cond(Cond),Assgnmnt(Assgnmnt),Stmt(Stmt){};

Value* ForAST::codegen(Driver& driver){
  //Controllo se la variabile è un'inizializzazione
  if(Init->getIsDef()){
    //BLOCCO DI INIZIALIZZAZIONE
    //Nel caso, genero il codice per l'inizializzazione e casto in AllocaInst così da 
    //poter salvare il valore del registro SSA in una tabella temporanea
    AllocaInst* initVal=dyn_cast<AllocaInst>(Init->codegen(driver));
    if(initVal){
      //La tabella temporanea citata precedentemente, serve per eclissare le variabili
      //che hanno lo stesso nome temporaneamente così da evitare conflitti
      std::map<std::string, AllocaInst*> variablesTmp;
      variablesTmp[Init->getName()]=driver.variables[Init->getName()];
      //Aggiorno la tabella associativa con la variabile inizializzata
      driver.variables[Init->getName()]=initVal;
      //BLOCCO DI CONDIZIONE
      //Creo i blocchi di condizione e di fine ciclo
      Function* fun=builder->GetInsertBlock()->getParent();
      BasicBlock* condBB=BasicBlock::Create(*context,"cond",fun);
      BasicBlock* mergeBB=BasicBlock::Create(*context,"endfor");
      //Salto incodizionato al blocco di condizione
      builder->CreateBr(condBB);
      //Posiziono il builder all'interno del blocco di valutazione della condizione
      builder->SetInsertPoint(condBB);
      //Generazione del codice per la valutazione della condizione
      Value* condVal=Cond->codegen(driver);
      if(condVal){
        //Creo il blocco dove verrà eseguito il corpo del ciclo
        BasicBlock* loopBB=BasicBlock::Create(*context,"loop");
        //Salto condizionato al blocco del corpo del ciclo
        builder->CreateCondBr(condVal,loopBB,mergeBB);
        //CORPO FOR
        //Aggancio il blocco che gestisce il loop creato precedentemente
        fun->insert(fun->end(),loopBB);
        builder->SetInsertPoint(loopBB);
        //Generazione del codice del corpo interno al loop
        Value *stmt=Stmt->codegen(driver);
        if(stmt){
          //Recupero il blocco del loop corrente 
          builder->GetInsertBlock();
          //Generazione del codice dell'assegnamento finale
          Value* var=Assgnmnt->codegen(driver);
          if(var){
            //Salto incondizionato al blocco di valutazione della condizione
            builder->CreateBr(condBB);
            //Aggancio il blocco di fine ciclo
            fun->insert(fun->end(),mergeBB);
            //Posiziono il builder all'interno del blocco di fine ciclo
            builder->SetInsertPoint(mergeBB);
            //Ripristino le vecchie variabili nella tabella associativa
            driver.variables[Init->getName()]=variablesTmp[Init->getName()];
            //Restituisco il valore dell'ultimo registro SSA definito da uno statement
            return var;
          }
        }
      }
    }
  }else{
    if(Init->codegen(driver)) 
      return nullptr;
  }
  return nullptr;
};

//---------------# Implementazione classe UnaryExprAST #---------------
//Costruttore
UnaryExprAST::UnaryExprAST(char Op,ExprAST* Val): Op(Op),Val(Val){};

/*
Metodo codegen:
  Per generare il codice di un'espressione unaria si deve:
    1)Generare il codice per l'operando.
    2)Creare l'istruzione relativa all'operatore unario con il valore memorizzato nel registro SSA.
*/
Value* UnaryExprAST::codegen(Driver& driver){
  Value* var=Val->codegen(driver);
  if(var){
    if(Op=='!')
      return builder->CreateNot(var,"not");
    else
      std::cerr<<"'"+std::string(1, Op)+"' -> operatore non utilizzabile"<<std::endl;
  }
  return nullptr;
};
