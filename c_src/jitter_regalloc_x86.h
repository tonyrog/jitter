#ifndef __JITTER_REGALLOC_X86_H__
#define __JITTER_REGALLOC_X86_H__

#include "jitter_regalloc.h"

#include <asmjit/x86.h>

static x86::Gp reg(int i)
{
    return x86::Gpq(i);
}

/*
static int regno(x86::Reg reg)
{
    return reg.id();
}
*/


class RegAlloc_x86 : public RegAlloc {
    String fmtbuf;
public:
    RegAlloc_x86(ZAssembler &a) : RegAlloc(16, 16)  {
	// assume 7 (rdi on x86) is the only argument pointing to
	// virtual register file (depend on architecture)
	// r0 = AX - fixed
	map_virtual_reg(0, 0); gp_use[0] = 0; load_virtual_register(a, 0, 0);
	// r1 = CX - fixed
	map_virtual_reg(1, 1); gp_use[1] = 0; load_virtual_register(a, 1, 1);    
	// SP - not used
	gp_use[4] = 0;
	// DI - vregfile pointer 
	gp_use[7] = 0;
    }

    // load virtual reg from vregfile into real register gp
    void load_virtual_register(ZAssembler &a, int r, int gp) {
	int offs = offsetof(vregfile_t, r) + r*sizeof(int64_t);
	a.mov(reg(gp), x86::ptr(a.zdi(), offs));
    }

    // save virtual register to vregfile (must be mapped)
    void save_virtual_register(ZAssembler &a, int r, int gp) {
	int offs = offsetof(vregfile_t, r) + r*sizeof(int64_t);
	a.mov(x86::ptr(a.zdi(), offs), reg(gp));
    }

    const char* format_native_reg(int gp) {
	fmtbuf.reset();
	Formatter::formatOperand(fmtbuf, FormatFlags::kNone, NULL,
				 Arch::kX64, reg(gp));	
	return fmtbuf.data();
    }
};

#endif
