/*
######################################
          PRIMO ASSIGNMENT                                                                              
######################################
*/

#include "llvm/Transforms/Utils/LocalOpts.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include <llvm/IR/Constants.h>
#include "cmath"

using namespace llvm;

/*
  Funzione di controllo per verificare l'esistenza di un valore numerico
  costante all'interno dell'istruzione
*/
bool check(BinaryOperator *instr,ConstantInt *&num,Value *&op){
  num=dyn_cast<ConstantInt>(instr->getOperand(0));
  op=instr->getOperand(1);
  if(not num){                                                    // Nel caso non si sia trovato il valore numerico al primo operando,
    num=dyn_cast<ConstantInt>(instr->getOperand(1));              // controllo l'esistenza di esso all'interno del secondo.
    op=instr->getOperand(0);
  }
  if(not num){                                                      
    return false;                                                 // Restituiamo false in caso non esista alcun valore costante,
  }
  return true;                                                    // true nel caso si sia trovato almeno in un operando
}

/*
------------------- 1. Algebraic Identity -------------------
                        x+0 = 0+x => x
                        x*1 = 1*x => x
-------------------------------------------------------------
*/
bool algebraicIdentity(llvm::BasicBlock::iterator &i) {
  ConstantInt *num=nullptr;                                       // Valore numerico costante passato successivamente dalla funzione check
  Value *op=nullptr;                                              // Variabile passata successivamente dalla funzione check
  if(i->getOpcode()==BinaryOperator::Add){                        // Caso di addizione
    BinaryOperator *add=nullptr;
    add=dyn_cast<BinaryOperator>(i);
    if(check(add,num,op)){
      if(num->getValue().isZero()){                               // Controlliamo se la costante è zero
        add->replaceAllUsesWith(op);                              // Sostituisco in tutte le istruzioni in cui viene utilizzato il valore restituito 
                                                                  // dall'istruzione 'add', con la variabile utilizzata all'interno di essa.
                                                                  // Quindi se Y=X+0 (o 0+X) => X=Y
        i++;                                                      // Dopo aver incrementato l'iteratore del BasicBlock,
        add->eraseFromParent();                                   // eliminiamo l'istruzione 'add', ormai inutilizzata
        return true;
      }
    }
  }else if(i->getOpcode()==BinaryOperator::Mul){                  // Caso di Moltiplicazione
    BinaryOperator *mul=nullptr;
    mul=dyn_cast<BinaryOperator>(i);
    if(check(mul,num,op)){
      if(num->getValue().isOne()){                                // Controlliamo se la costante è uno                    
        mul->replaceAllUsesWith(op);                              // Sostituisco in tutte le istruzioni in cui viene utilizzato il valore restituito 
                                                                  // dall'istruzione 'mul', con la variabile utilizzata all'interno di essa.
                                                                  // Quindi se Y=X*1 (o 1*X) => X=Y
        i++;                                                      // Dopo aver incrementato l'iteratore del BasicBlock,
        mul->eraseFromParent();                                   // eliminiamo l'istruzione 'mul', ormai inutilizzata
        return true;
      }
    }
  }
  return false;
}

/*
------------------- 2. Strength Reduction -------------------
                    15*x=x*15 => (x<<4)-x
                    y=x/8 => y=x>>3
-------------------------------------------------------------
*/
bool StrengthReduction(llvm::BasicBlock::iterator &i){
  ConstantInt *num=nullptr;
  Value *op=nullptr;
  if(i->getOpcode()==BinaryOperator::Mul){                                                          // Caso di moltiplicazione
    BinaryOperator *mul=nullptr;
    mul=dyn_cast<BinaryOperator>(i);                                                                
    if(check(mul,num,op)){
      if(num->getValue().isPowerOf2()){                                                             // Nel caso in cui il valore costante è una potenza di 2,
        ConstantInt *shiftValue=ConstantInt::get(num->getType(),num->getValue().exactLogBase2());   // calcolo il valore dello shift estraendo il log2 del valore
        Instruction *shiftSx=BinaryOperator::Create(BinaryOperator::Shl,op,shiftValue);             // e creo la nuova operazione con shift SX.
        shiftSx->insertAfter(mul);                                                                  // Inserisco l'operazione dopo l'istruzione di moltiplicazione,
        mul->replaceAllUsesWith(shiftSx);                                                           // sostituendo le occorrenze di 'mul' con la nuova operazione di shift
        i++;                                                                                        // e dopo aver incrementato l'iteratore del BasicBlock,
        mul->eraseFromParent();                                                                     // elimino l'istruzione di 'mul', ormai inutilizzata
        return true;
      }else{
        unsigned int ceilLog=num->getValue().ceilLogBase2();                                        // Nel caso in cui il valore costante non è una potenza di 2, calcolo il log2 più vicino,
        ConstantInt *shiftValue=ConstantInt::get(num->getType(), ceilLog);                          // assegnandolo ad un puntatore di tipo ConstantInt
        Instruction *shiftSx=BinaryOperator::Create(BinaryOperator::Shl, op, shiftValue);           // e creo la nuova operazione con shift DX
        shiftSx->insertAfter(mul);                                                                  // Inserisco l'istruzione appena creata dopo quella di 'mul'
        int difference=(pow(2, ceilLog))-(num->getSExtValue());                                     // Calcolo la differenza 2^log2(x) e x, trovando il resto
        ConstantInt *subValue=ConstantInt::get(num->getType(), difference);                         // e assegno ad una variabile intera la differenza
        Instruction *sub=BinaryOperator::Create(BinaryOperator::Sub,shiftSx,subValue);              // Creo la nuova operazione 'sub' tra il valore di shiftSX e la differenza precedente
        sub->insertAfter(shiftSx);                                                                  
        mul->replaceAllUsesWith(sub);
        i++;
        mul->eraseFromParent();
        return true;
      }
    }
  }else if(i->getOpcode()==BinaryOperator::SDiv){                                                   // Caso di divisione                               
    BinaryOperator *sdiv=nullptr;
    sdiv=dyn_cast<BinaryOperator>(i);
    if(check(sdiv,num,op)){
      if(num->getValue().isPowerOf2()){                                                             // Nel caso in cui il valore costante è una potenza di 2,
        ConstantInt *shiftValue=ConstantInt::get(num->getType(),num->getValue().exactLogBase2());   // calcolo il valore dello shift estraendo il log2 del valore
        Instruction *shiftDx=BinaryOperator::Create(BinaryOperator::LShr, op, shiftValue);          // e creo la nuova operazione con shift DX.
        shiftDx->insertAfter(sdiv);                                                                 // Inserisco l'operazione dopo l'istruzione di divisione,
        sdiv->replaceAllUsesWith(shiftDx);                                                          // sostituendo le occorrenze di 'sdiv' con la nuova operazione di shift
        i++;                                                                                        // e dopo aver incrementato l'iteratore del BasicBlock,
        sdiv->eraseFromParent();                                                                    // elimino l'istruzione di 'sdiv', ormai inutilizzata
        return true;
      }else{
        unsigned int ceilLog=num->getValue().ceilLogBase2();                                        // Nel caso in cui il valore costante non è una potenza di 2, calcolo il log2 più vicino,
        ConstantInt *shiftValue=ConstantInt::get(num->getType(), ceilLog);                          // assegnandolo ad un puntatore di tipo ConstantInt
        Instruction *shiftDx=BinaryOperator::Create(BinaryOperator::LShr, op, shiftValue);          // e creo la nuova operazione con shift DX
        shiftDx->insertAfter(sdiv);                                                                 // Inserisco l'istruzione appena creata dopo quella di 'sdiv'
        int difference=(pow(2, ceilLog))-(num->getSExtValue());                                     // Calcolo la differenza 2^log2(x) e x, trovando il resto
        ConstantInt *addValue=ConstantInt::get(num->getType(), difference);                         // e assegno ad una variabile intera la differenza
        Instruction *add=BinaryOperator::Create(BinaryOperator::Add, shiftDx, addValue);            // Creo la nuova operazione 'add' tra il valore di shiftDX e la differenza precedente
        add->insertAfter(shiftDx);
        sdiv->replaceAllUsesWith(add);
        i++;
        sdiv->eraseFromParent();
        return true;
      }
    }
  }
  return false;
}

/*
------------------- 3. Multi-Instruction Optimization -------------------
                        a=b+(x), c=a-(x) => a=b+(x), c=b
-------------------------------------------------------------------------
*/
bool multiInstOpt(llvm::BasicBlock::iterator &i, Value *&opFound, Value *&found, ConstantInt *&val){
  ConstantInt *num=nullptr;
  Value *op=nullptr;
  if(i->getOpcode()==BinaryOperator::Add and not found){                              // Caso di addizione, senza averne già trovata una
    BinaryOperator *add=nullptr;
    add=dyn_cast<BinaryOperator>(i);
    if(check(add,num,op)){
      val=num;                                                                        // Salvo la costante numerica,
      found=add;                                                                      // l'istruzione add
      opFound=op;                                                                     // e la variabile usata nell'addizione
      i++;
      return true;
    }
  }else if(i->getOpcode()==BinaryOperator::Sub and found){                            // Caso di sottrazione, avendo già trovato il caso in cui a=b+(x)
    BinaryOperator *sub=nullptr;
    sub=dyn_cast<BinaryOperator>(i);
    if(check(sub,num,op)){  
      if(val->getSExtValue()==num->getSExtValue() and op==found){                     // Se la costante numerica è uguale a quella della sottrazione e la variabile utilizzata nell'operazione 
                                                                                      // è la stessa utilizzata per salvare il valore dell'addizione trovata precedentemente,
        sub->replaceAllUsesWith(opFound);                                             // sostituisco le occorrenze di 'sub' con la variabile utilizzata nell'operazione 'add'
        i++;                                                                          
        sub->eraseFromParent();
        opFound=nullptr;                                                              // Rinizializzo le variabili di controllo, avendo eseguito l'ottimizzazione 
        found=nullptr;
        val=nullptr;
        return true;
      }
    }
  }
  return false;
}



bool runOnBasicBlock(BasicBlock &B) {
  auto i=B.begin();                                       
  Value *found=nullptr;                                       // Variabile che conterrà l'istruzione da sostituire in MultiInstOpt
  Value *opFound=nullptr;                                     // Variabile che conterrà l'operatore dell'istruzione da sostituire in MultiInstOpt
  ConstantInt *val=nullptr;                                   // Variabile che conterrà la costante numerica dell'istruzione in MultiInstOpt

  while(i!=B.end()){                                          // Per ogni istruzione del BasicBlock, ne richiamo le funzioni di ottimizzazione
    if(algebraicIdentity(i)){
      continue;
    }
    if(multiInstOpt(i, opFound, found, val)){
      continue;
    }
    if(StrengthReduction(i)){
      continue;
    }
    
    i++;
  }
  return true;
}

bool runOnFunction(Function &F){
  bool Transformed = false;
  for(auto Iter=F.begin();Iter!=F.end();++Iter){
    outs()<<"\n";
    outs()<<"Codice originale:";
    outs()<<"\n";
    for(auto i=Iter->begin();i!=Iter->end();++i)                                 // Istruzioni prima della modifica
      outs()<<*i<<"\n";
    outs()<<"\n";
    if (runOnBasicBlock(*Iter)) {
      Transformed = true;
    }
    outs()<<"\n";
    outs()<<"Codice aggiornato: ";
    outs()<<"\n";
    for(auto i=Iter->begin();i!=Iter->end();++i)                                 // Istruzioni dopo la modifica
      outs()<<*i<<"\n";
  }
  return Transformed;
}

PreservedAnalyses LocalOpts::run(Module &M, ModuleAnalysisManager &AM){
  for(auto Fiter=M.begin();Fiter!=M.end();++Fiter)
    if(runOnFunction(*Fiter))
      return PreservedAnalyses::none();
  return PreservedAnalyses::all();
}
