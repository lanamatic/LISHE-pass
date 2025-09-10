; ModuleID = '/Users/lanamatic/LISHE-pass/tests/03_alias_write_blocker_before.ll'
source_filename = "/Users/lanamatic/LISHE-pass/tests/03_alias_write_blocker.c"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macosx15.0.0"

; Function Attrs: noinline nounwind ssp uwtable(sync)
define void @alias_block(ptr noundef %0, ptr noundef %1, i32 noundef %2, i32 noundef %3) #0 {
  %5 = icmp sle i32 %2, 0
  br i1 %5, label %6, label %7

6:                                                ; preds = %4
  br label %13

7:                                                ; preds = %4
  br label %8

8:                                                ; preds = %11, %7
  %.0 = phi i32 [ 0, %7 ], [ %12, %11 ]
  %9 = icmp slt i32 %.0, %2
  br i1 %9, label %10, label %13

10:                                               ; preds = %8
  store i32 %.0, ptr %1, align 4
  store i32 %3, ptr %0, align 4
  br label %11

11:                                               ; preds = %10
  %12 = add nsw i32 %.0, 1
  br label %8, !llvm.loop !6

13:                                               ; preds = %8, %6
  ret void
}

attributes #0 = { noinline nounwind ssp uwtable(sync) "frame-pointer"="non-leaf" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+altnzcv,+ccdp,+ccidx,+ccpp,+complxnum,+crc,+dit,+dotprod,+flagm,+fp-armv8,+fp16fml,+fptoint,+fullfp16,+jsconv,+lse,+neon,+pauth,+perfmon,+predres,+ras,+rcpc,+rdm,+sb,+sha2,+sha3,+specrestrict,+ssbs,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8a" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 15, i32 2]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 1}
!5 = !{!"Homebrew clang version 21.1.0"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
