#include "llvm/Transforms/Utils/LoopFusionPass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/Utils/LoopRotationUtils.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/GenericCycleImpl.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/DependenceAnalysis.h"

using namespace llvm;

/*
    Controllo se il loop1 è adiacente al loop2 nel caso in cui siano guarded
    Esso lo è se il successore non loop del guard branch equivale all'header del loop2
*/
bool guardedLoopAdjacent(Loop *loop1, Loop *loop2) {
    
    BasicBlock *loop1Preheader = loop1->getLoopPreheader();
    if(!loop1Preheader)
        return false;

    Instruction *loop1Terminator = loop1Preheader->getTerminator();
    if(!loop1Terminator)
        return false;

    BranchInst *guardBranch = dyn_cast<BranchInst>(loop1Terminator);
    if(!guardBranch || guardBranch->isUnconditional())
        return false;

    BasicBlock *trueSucc = guardBranch->getSuccessor(0); 
    BasicBlock *falseSucc = guardBranch->getSuccessor(1);

    if (trueSucc == loop2->getHeader() || falseSucc == loop2->getHeader())
        return true;

    return false;
}

/*
    Controllo se il preheader del loop1 è adiacente al loop2
    Se gli exit blocks del loop1 sono TUTTI uguali al preheader del loop2 allora sono adiacenti
*/
bool notGuardedLoopAdjacent(Loop *loop1, Loop *loop2){

    SmallVector<BasicBlock*> BBuscita;
    loop1->getExitBlocks(BBuscita);

    for(auto BB : BBuscita){
        if(BB != loop2->getLoopPreheader())
            return false;
    }
    return true;
}

/*
    Controllo se loop1 e loop2 hanno lo stesso control flow
    Quindi, se loop1 domina il loop2 e loop2 postdomina loop1, hanno lo stesso control flow
*/
bool isSameControlFlow(DominatorTree& DT, PostDominatorTree& PDT, Loop* loop1, Loop* loop2){
    if (DT.dominates(loop1->getHeader(),loop2->getHeader()) && PDT.dominates(loop2->getHeader(),loop1->getHeader()))
        return true;
    return false;
}

/*
    Ottengo il trip count dei loop
*/
int getLoopTripCount(Loop *loop, ScalarEvolution &SE){
    return SE.getSmallConstantTripCount(loop);
}

/*
    Funzione di ricerca per verificare che un BB sia presente in un insieme di BB
*/
bool search(ArrayRef<BasicBlock*> BBs, BasicBlock *BBsearch){
    bool found=false;
    for(auto BB : BBs){
        if(BB == BBsearch){
            found=true;
            break;
        }
    }
    return found;
}

/*
    Funzione per ottenere i BB interni al body di un loop
*/
SmallVector<BasicBlock*> getLoopBodyBlocks(Loop *loop){
    SmallVector<BasicBlock*> bodyBlocks;
    ArrayRef<BasicBlock*> BBs = loop->getBlocks();
    SmallVector<BasicBlock*> latches;
    loop->getLoopLatches(latches);
    SmallVector<BasicBlock*> exitings;
    loop->getExitingBlocks(exitings);
    for(BasicBlock* BB : BBs){
        if (BB == loop->getHeader() || search(latches, BB) || search(exitings, BB))
            continue;
        bodyBlocks.push_back(BB);
    }
    return bodyBlocks;
}

/*
    Funzione per ottenere le istruzioni interne ad un BB
*/
SmallVector<Instruction*> getInstructionsFromBlock(SmallVector<BasicBlock*> BBs){
    SmallVector<Instruction*> instrVect;
    for(auto BB : BBs){
        for(auto i=BB->begin(); i != BB->end(); i++){
            instrVect.push_back(dyn_cast<Instruction>(i));
        }
    }
    return instrVect;
}

/*
    Controllo se ci siano dipendenze negative tra il loop1 e loop2
    Ci sono dipendenze negative se un'istruzione nel loop2 accede ad un dato che è stato
    precedentemente acceduto dal loop1 in un'iterazione successiva.
    NON GIUSTO
*/
bool checkInverseDependency(Loop *loop1, Loop *loop2, DependenceInfo &DI){
    SmallVector<BasicBlock*> bodyBlocksLoop1 = getLoopBodyBlocks(loop1);
    SmallVector<BasicBlock*> bodyBlocksLoop2 = getLoopBodyBlocks(loop2);
    SmallVector<Instruction*> bodyInstrLoop1 = getInstructionsFromBlock(bodyBlocksLoop1);
    SmallVector<Instruction*> bodyInstrLoop2 = getInstructionsFromBlock(bodyBlocksLoop2);

    for(Instruction *innerInstr : bodyInstrLoop2){
        for(Instruction *outerInstr : bodyInstrLoop1){
            auto dep = DI.depends(dyn_cast<Instruction>(innerInstr), dyn_cast<Instruction>(outerInstr), true);
            if(dep){
                outs()<<"Dependency found: "<<*innerInstr<<" -> "<<*outerInstr<<"\n";
                return false;
            }
        }
    }
    return true;
}

/*
    Funzione per invertire gli usi degli incrementatori
*/
bool modifyUseInductionVarible(Loop *loop1, Loop *loop2,ScalarEvolution &SE){
    PHINode *ivloop1 = loop1->getCanonicalInductionVariable();
    PHINode *ivloop2 = loop2->getCanonicalInductionVariable();
    if(ivloop1 && ivloop2){
        ivloop2->replaceAllUsesWith(ivloop1);
        return true;
    }
    else
        return false;
}

/*
    Funzione che edita il CFG unendo i 2 loop
*/
void editCFG(Loop *loop1, Loop *loop2){
    BasicBlock *headerL2 = loop2->getHeader();
    BasicBlock *headerL1 = loop1->getHeader();
    BasicBlock* latchL1 = loop1->getLoopLatch();
    SmallVector<BasicBlock*> bodyL1 = getLoopBodyBlocks(loop1);
    SmallVector<BasicBlock*> bodyL2 = getLoopBodyBlocks(loop2);
    SmallVector<BasicBlock*> exitL2;
    loop2->getExitBlocks(exitL2);
    
    //muovo le istruzioni del loop2 prima del latch del loop1
    for(auto BB: bodyL2){
        BB->moveBefore(latchL1);
    }

    //collego il primo BB del body di L2 con l'ultimo BB del body di L1
    Instruction *terminatorL1 = bodyL1.back()->getTerminator();
    terminatorL1->setSuccessor(0,bodyL2.front());

    //collego il latch del loop1 all'ultimo BB del body del loop2
    Instruction *terminatorL2 = bodyL2.back()->getTerminator();
    terminatorL2->setSuccessor(0,latchL1);

    //collego il latch del loop2 (sia ramo true che false) all'header del loop2
    Instruction *headerL2Terminator = headerL2->getTerminator();
    headerL2Terminator->setSuccessor(0, loop2->getLoopLatch());
    headerL2Terminator->setSuccessor(1, loop2->getLoopLatch());

    //collego tutte le uscite del loop1 al BB successore dell'exit di loop2
    Instruction *headerL1Terminator = headerL1->getTerminator();
    for(auto BB : exitL2){
        headerL1Terminator->setSuccessor(1, BB);
    }
}

PreservedAnalyses LoopFusionPass::run(Function &F,FunctionAnalysisManager &AM){

    LoopInfo &loops = AM.getResult<LoopAnalysis>(F);
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
    PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
    ScalarEvolution &SE =AM.getResult<ScalarEvolutionAnalysis>(F);
    DependenceInfo &DI = AM.getResult<DependenceAnalysis>(F);

    for(auto L = loops.rbegin(); L != loops.rend(); L++){
        
        //ottengo loop2
        auto Lnext = L;
        Lnext++;
        if(Lnext == loops.rend()){
            continue;
        }

        if((*L)->isGuarded()){
            outs()<<"Loop 1 guarded\n";
            if(!guardedLoopAdjacent(*L, *(Lnext))){
                outs()<<"Loop 1 e Loop 2 non adiacenti\n";
                continue;
            }else{
                outs()<<"Loop 1 e Loop 2 adiacenti\n";
            }
        }else{
            outs()<<"Loop 1 not guarded\n";
            if(!notGuardedLoopAdjacent(*L, *(Lnext))){
                outs()<<"Loop 1 e Loop 2 non adiacenti\n";
                continue;
            }else{
                outs()<<"Loop 1 e Loop 2 adiacenti\n";
            }
        }

        if(isSameControlFlow(DT,PDT,*L,*Lnext))
            outs()<<"Loop 1 e Loop 2 sono equivalenti a livello di control flow\n";
        else 
            continue;
        
        if(getLoopTripCount(*L,SE) == getLoopTripCount(*Lnext,SE))
            outs()<<"Loop 1 e Loop 2 iterano lo stesso numero di volte\n";
        else 
            continue;

        getLoopBodyBlocks(*L);

        if(!checkInverseDependency(*L, *Lnext, DI)){
            outs()<<"Loop 1 e Loop 2 soffrono di dipendenza inversa\n";
            continue;
        }

        modifyUseInductionVarible(*L, *Lnext, SE);    
        editCFG(*L, *Lnext);
    }
    return PreservedAnalyses::all();
}
