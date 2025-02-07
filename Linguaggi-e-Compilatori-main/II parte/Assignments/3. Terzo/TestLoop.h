#ifndef LLVM_TRANSFORMS_TESTLOOP_H
#define LLVM_TRANSFORMS_TESTLOOP_H
#include "llvm/IR/PassManager.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"

namespace llvm {
  class TestLoop : public PassInfoMixin<TestLoop> {
    public:
      PreservedAnalyses run(Loop &L,LoopAnalysisManager &LAM,LoopStandardAnalysisResults &LAR,LPMUpdater &LU);
  };
}
#endif
