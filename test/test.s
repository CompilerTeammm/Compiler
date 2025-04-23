BasicBlock: .4
    .file  "/home/lmc/Compiler-3/Compiler/test/test.s"
    .attribute arch, "rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_zicsr2p0_zifencei2p0_zba1p0_zbb1p0"
    .attribute unaligned_access, 0
    .attribute stack_align, 16
    .text
    .globl  .G.a
    .text
    .align  2
    .type  .G.a, @object
    .size  .G.a, 4
.G.a:
    .word  3
    .globl  .G.b
    .text
    .align  2
    .type  .G.b, @object
    .size  .G.b, 4
.G.b:
    .word  5
    .text
    .align  1
    .globl  main
    .type  main, @funtion
main:
