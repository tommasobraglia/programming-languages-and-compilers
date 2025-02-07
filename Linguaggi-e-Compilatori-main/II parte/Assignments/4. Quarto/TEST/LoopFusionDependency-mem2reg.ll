; ModuleID = '../TEST/LoopFusion/LoopFusionDependency-mem2reg.ll'
source_filename = "../TEST/LoopFusion/loopFusionDependency.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@__const.main.a = private unnamed_addr constant [9 x i32] [i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9], align 16

define dso_local void @loopfusion(ptr noundef %0) {
  br label %2

2:                                                ; preds = %7, %1
  %.01 = phi i32 [ 0, %1 ], [ %8, %7 ]
  %3 = icmp slt i32 %.01, 9
  br i1 %3, label %4, label %9

4:                                                ; preds = %2
  %5 = sext i32 %.01 to i64
  %6 = getelementptr inbounds i32, ptr %0, i64 %5
  store i32 %.01, ptr %6, align 4
  br label %7

7:                                                ; preds = %4
  %8 = add nsw i32 %.01, 1
  br label %2, !llvm.loop !6

9:                                                ; preds = %2
  br label %10

10:                                               ; preds = %17, %9
  %.0 = phi i32 [ 0, %9 ], [ %18, %17 ]
  %11 = icmp slt i32 %.0, 9
  br i1 %11, label %12, label %19

12:                                               ; preds = %10
  %13 = sext i32 %.0 to i64
  %14 = getelementptr inbounds i32, ptr %0, i64 %13
  %15 = load i32, ptr %14, align 4
  %16 = add nsw i32 %15, 2
  br label %17

17:                                               ; preds = %12
  %18 = add nsw i32 %.0, 1
  br label %10, !llvm.loop !8

19:                                               ; preds = %10
  ret void
}

define dso_local i32 @main() {
  %1 = alloca [9 x i32], align 16
  call void @llvm.memcpy.p0.p0.i64(ptr align 16 %1, ptr align 16 @__const.main.a, i64 36, i1 false)
  %2 = getelementptr inbounds [9 x i32], ptr %1, i64 0, i64 0
  call void @loopfusion(ptr noundef %2)
  ret i32 0
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #0

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 18.1.3 (1)"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
!8 = distinct !{!8, !7}
