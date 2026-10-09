; ModuleID = 'edg_module'
source_filename = "edg_module"
target datalayout = "E-m:e-p:64:64-i8:8-i16:16-i32:32-i64:64-f32:32-f64:64-f128:64"
target triple = "x86_64-unknown-linux-gnu"

define i32 @read_b(ptr %s) {
entry:
  %s.addr = alloca ptr, align 8
  store ptr %s, ptr %s.addr, align 8
  %0 = load ptr, ptr %s.addr, align 8
  %1 = getelementptr inbounds i8, ptr %0, i64 0
  %2 = load i32, ptr %1, align 4
  %3 = lshr i32 %2, 3
  %4 = and i32 %3, 31
  %5 = shl i32 %4, 27
  %6 = ashr i32 %5, 27
  ret i32 %6
}

define void @write_b(ptr %s, i32 %val) {
entry:
  %s.addr = alloca ptr, align 8
  store ptr %s, ptr %s.addr, align 8
  %val.addr = alloca i32, align 4
  store i32 %val, ptr %val.addr, align 4
  %0 = load ptr, ptr %s.addr, align 8
  %1 = getelementptr inbounds i8, ptr %0, i64 0
  %2 = load i32, ptr %val.addr, align 4
  %3 = load i32, ptr %1, align 4
  %4 = and i32 %2, 31
  %5 = shl i32 %4, 3
  %6 = and i32 %3, -249
  %7 = or i32 %6, %5
  store i32 %7, ptr %1, align 4
  ret void
}
