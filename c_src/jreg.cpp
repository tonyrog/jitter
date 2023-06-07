
#include "jitter_regalloc_x86.h"

void format_operand(String& sb, Operand operand)
{
    sb.reset();
    Formatter::formatOperand(sb, FormatFlags::kNone, NULL, Arch::kX64, operand);
}

void mov_(Operand dst, Operand src)
{
    if (dst != src) {
	String d, s;
	format_operand(d, dst);
	format_operand(s, src);
	printf("mov %s, %s\n", d.data(), s.data());
    }
}

void add_(Operand dst, Operand src)
{
    String d, s;
    format_operand(d, dst);
    format_operand(s, src);
    printf("add %s, %s\n", d.data(), s.data());
}

void add(ZAssembler &a, RegAlloc &r, int d, int s1, int s2)
{
    r.ensure_loaded(a, s1);
    r.ensure_loaded(a, s2);
    r.ensure_mapped(a, d);

    mov_(reg(r.r_map[d]), reg(r.r_map[s1]));
    add_(reg(r.r_map[d]), reg(r.r_map[s2]));
}

void mov(ZAssembler &a, RegAlloc &r, int d, int s)
{
    r.ensure_loaded(a, s);
    r.ensure_mapped(a, d);

    mov_(reg(r.r_map[d]), reg(r.r_map[s]));
}

void test(ZAssembler &a, RegAlloc &r)
{
    // simulate a couple of instructions
    // add r1..r15 to  r0
    
    add(a, r, 0, 1, 2);     // r0=1+2
    add(a, r, 2, 3, 4);     // r2=3+4
    add(a, r, 4, 5, 6);     // r4=5+6
    add(a, r, 6, 7, 8);     // r6=7+8
    add(a, r, 8, 9, 10);    // r8=9+10
    add(a, r, 10, 11, 12);  // r10=11+12
    add(a, r, 12, 13, 14);  // r12=13+14

    {
	TmpAlloc t1(a, r);
	TmpAlloc t2(a, r);
	mov_(reg(t1.reg()), reg(r.r_map[10]));
	add_(reg(t2.reg()), reg(r.r_map[12]));
	mov_(reg(r.r_map[12]), reg(t2.reg()));
	// auto release of t1 and t2!
    }
       
    add(a, r, 1,  0, 2);    // r1=1+2+3+4
    add(a, r, 3,  4, 6);    // r3=5+6+7+8
    add(a, r, 5,  8,10);    // r5=9+10+11+12
    add(a, r, 7, 12,15);    // r7=13+14+15

    add(a, r, 2,  1, 3);    // r2=1+2+3+4+5+6+7+8
    add(a, r, 4,  5, 7);    // r4=9+10+11+12+13+14+15

    add(a, r, 0,  2, 4);
	
    r.dump();
}


int main()
{
    JitRuntime rt;           // Runtime designed for JIT code execution
    CodeHolder code;         // Holds code and relocation information
    FileLogger logger(stdout);

    code.init(rt.environment(), rt.cpuFeatures());
    ZAssembler a(&code, 1024);
    a.setLogger(&logger);

    RegAlloc_x86 alloc(a);

    test(a, alloc);
}
