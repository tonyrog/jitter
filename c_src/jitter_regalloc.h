#ifndef __JITTER_REGALLOC_H__
#define __JITTER_REGALLOC_H__

#include <asmjit/core.h>
#include <string.h>
#include <memory.h>
#include <stddef.h>

using namespace asmjit;

#include "jitter_asm.h"
#include "jitter_types.h"

class RegAlloc {
    size_t num_virtual_regs_;
    size_t num_native_regs_;
    char fmtbuf[16];

public:
    int tick_;   // use counter

    // r_map: virtual register number to gp register number
    //   r_map[r] => gp  | -1 = unmapped
    int* r_map;
    // gp_map: map real register number to gp register number
    //   gp_map[gp] => r | -1 = unmapped
    int* gp_map;
    // -1 = never mapped, 0 = fixed, k=tick when last mapped
    int* gp_use;

    RegAlloc(size_t num_virtual_regs, size_t num_native_regs) {
	tick_ = 0;
	num_virtual_regs_ = num_virtual_regs;
	num_native_regs_ = num_native_regs;	

	r_map = new int[num_virtual_regs];
	gp_map = new int[num_native_regs];
	gp_use = new int[num_native_regs];
	
	memset(r_map, 0xff, sizeof(int)*num_virtual_regs);
	memset(gp_map, 0xff, sizeof(int)*num_native_regs);
	memset(gp_use, 0xff, sizeof(int)*num_native_regs);
    }

    ~RegAlloc() {
	delete gp_map;
	delete gp_use;
    }

    void dump()
    {
	int i;
	printf("| REGMAP |     R->GP   |    GP->R  |  U  |\n");
	for (i = 0; i < NUM_SCALAR_REGISTERS; i++) {
	    printf("| %-6d |", i);
	    if (r_map[i] >= 0)
		printf(" %s%%r%-2d=%5s |", (gp_use[r_map[i]]==0? "!" : " "), i,
		       format_native_reg(r_map[i]));
	    else
		printf("             |");
	    if ((i < (int)num_native_regs_) && (gp_map[i] >= 0))
		printf("%5s=%%r%-2d |%5d|", format_native_reg(i), gp_map[i],
		       gp_use[i]);
	    else
		printf("           |     |");
	    printf("\n");	
	}
	printf("\n");
    }

    virtual const char* format_virtual_reg(int r) {
	sprintf(fmtbuf, "%%r%d", r);
	return fmtbuf;
    }    

    virtual const char* format_native_reg(int gp) {
	sprintf(fmtbuf, "gp%d", gp);
	return fmtbuf;
    }
    
    virtual void load_virtual_register(ZAssembler &a, int r, int gp) {
	(void) a;
	printf("mov %%r%d, mem[%d]\n", r, gp);
    }
    virtual void save_virtual_register(ZAssembler &a, int r, int gp) {
	(void) a;
	printf("mov mem[%d], %%r%d\n", gp, r);
    }

    // map r -> gp 
    void map_virtual_reg(int r, int gp) {
	r_map[r] = gp;
	gp_map[gp] = r;
	gp_use[gp] = tick_++;
    }

    // unmap r
    void unmap_virtual_reg(int r) {
	int gp = r_map[r];
	if (gp >= 0) {
	    gp_map[gp] = -1;
	    r_map[r]   = -1;
	    gp_use[gp] = -1;
	}
    }

    // return free native register or "least" used register
    int find_native_register() {
	int fgp = -1;  // first free (from HIGH TO LOW)
	int lgp = -1;  // least used gp
	int tick = -1;
	int i;
    
	for (i = num_native_regs_-1; i >= 0; i--) {
	    if (gp_use[i] == -1) {
		if (fgp == -1) fgp = i;
	    }
	    else if (gp_use[i] > 0) { // not using 0! == FIXED
		if ((tick == -1) || (gp_use[i] < tick)) {
		    lgp = i;
		    tick = gp_use[i];
		}
	    }
	}
	if (fgp == -1)
	    return lgp;
	return fgp;
    }

    void flush_and_unmap_native(ZAssembler &a, int gp) {
	int r;
	if ((r = gp_map[gp]) >= 0) {
	    save_virtual_register(a, r, gp);
	    unmap_virtual_reg(r);
	}
    }

    // ensure that virtual register r is mapped to real register
    void ensure_mapped(ZAssembler &a, int r) {
	int gp;
	if ((gp = r_map[r]) < 0) {
	    gp = find_native_register();
	    flush_and_unmap_native(a, gp);
	    map_virtual_reg(r, gp);
	}
	else if (gp_use[gp] != 0)
	    gp_use[gp] = tick_++;
    }

    // same as ensure mapped but also load
    void ensure_loaded(ZAssembler &a, int r) {
	int gp;
	if ((gp = r_map[r]) < 0) {
	    gp = find_native_register();
	    flush_and_unmap_native(a, gp);
	    map_virtual_reg(r, gp);
	    load_virtual_register(a, r, gp);
	}
	else if (gp_use[gp] != 0)
	    gp_use[gp] = tick_++;
    }

    // allocate a temporary native register
    int alloc_native_register(ZAssembler &a) {
	int gp = find_native_register();
	flush_and_unmap_native(a, gp);
	gp_use[gp] = 0;  // make sure it stays fixed until release
	return gp;
    }

    void release_native_register(int gp) {
	if (gp_use[gp] == 0)
	    gp_use[gp] = -1; // now it can be used again
    }
};

// fixme: make one for each type
class TmpAlloc {
    int reg_;
    RegAlloc* alloc_;
public:
    TmpAlloc(ZAssembler &a, RegAlloc &alloc) {
	alloc_ = &alloc;
	reg_ = alloc_->alloc_native_register(a);
	printf("tmp alloc %d\n", reg_);
    }
    ~TmpAlloc() {
	printf("tmp release %d\n", reg_);
	alloc_->release_native_register(reg_);
    }
    int reg() { return reg_; }
};


#endif
