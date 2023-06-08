#include <asmjit/x86.h>
#include <iostream>
#include <assert.h>

using namespace asmjit;

#include "jitter_types.h"
#include "jitter.h"
#include "jitter_asm.h"

#define CMP_EQ    0
#define CMP_LT    1
#define CMP_LE    2
#define CMP_UNORD 3
#define CMP_NEQ   4
#define CMP_NLT   5
#define CMP_GE    5
#define CMP_NLE   6
#define CMP_GT    6
#define CMP_ORD   7

// xreg/reg to id
static int regno(x86::Reg reg)
{
    return reg.id();
}

// reg id to register	
x86::Gp reg(int i)
{
    return x86::Gpq(i);
}

// id to register
x86::Xmm xreg(int i)
{
    return x86::Xmm(i);
}

#ifdef not_used
x86::Ymm yreg(int i)
{
    return x86::Ymm(i);
}
#endif

// print debug info
void x86_info()
{
    fprintf(stderr, "sizeof(x86_fxsave_t) = %ld\n",
	    sizeof(fxsave32_t));
    fprintf(stderr, "sizeof(x86_64_fxsave0_t) = %ld\n",
	    sizeof(fxsave64_0_t));
    fprintf(stderr, "sizeof(x86_64_fxsave1_t) = %ld\n",
	    sizeof(fxsave64_1_t));        
}

void crash(const char* filename, int line, int code)
{
    fprintf(stderr, "%s:%d: CRASH code=%d\n", filename, line, code);
    assert(0);
}

x86::Xmm alloc_xmm(ZAssembler &a)
{
    int r;
    if ((r = a.xreg_alloc()) >= 0) {
	x86::Xmm xr = xreg(r);
	a.add_dirty_reg(xr);
	return xr;
    }
    crash(__FILE__, __LINE__, -1);
    return x86::regs::xmm0;
}

void release_xmm(ZAssembler &a, x86::Xmm rr)
{
    int r = regno(rr);
    a.xreg_release(r);
}

x86::Gp alloc_gp(ZAssembler &a)
{
    int r;
    if ((r = a.reg_alloc()) >= 0) {
	x86::Gp rr = reg(r);
	a.add_dirty_reg(rr);
	return rr;
    }
    crash(__FILE__, __LINE__, -1);
    return x86::regs::rax;
}

void release_gp(ZAssembler &a, x86::Gp rr)
{
    int r = regno(rr);
    a.reg_release(r);
}
    
static void emit_save(ZAssembler &a)
{
    a.push(x86::regs::rax);
    a.push(x86::regs::rbx);
    a.push(x86::regs::rcx);
    a.push(x86::regs::rdx);
    a.push(x86::regs::rbp);
    a.push(x86::regs::rsi);
    a.push(x86::regs::rdi);
    // r8-r15
}

static void emit_restore(ZAssembler &a)
{
    // r15-r18
    a.pop(x86::regs::rdi);
    a.pop(x86::regs::rsi);
    a.pop(x86::regs::rbp);
    a.pop(x86::regs::rdx);
    a.pop(x86::regs::rcx);
    a.pop(x86::regs::rbx);
    a.pop(x86::regs::rax);
}

#ifdef unused
static void emit_inc(ZAssembler &a, uint8_t type, int dst)
{
    switch(type) {
    case UINT8:
    case INT8:    a.inc(reg(dst).r8()); break;
    case UINT16:
    case INT16:   a.inc(reg(dst).r16()); break;
    case UINT32:
    case INT32:   a.inc(reg(dst).r32()); break;
    case UINT64:
    case INT64:   a.inc(reg(dst).r64()); break;
    default: crash(__FILE__, __LINE__, type); break;	
    }    
}
#endif

static void emit_dec(ZAssembler &a, uint8_t type, int dst)
{
    switch(type) {
    case UINT8:
    case INT8:    a.dec(reg(dst).r8()); break;
    case UINT16:
    case INT16:   a.dec(reg(dst).r16()); break;
    case UINT32:
    case INT32:   a.dec(reg(dst).r32()); break;
    case UINT64:
    case INT64:   a.dec(reg(dst).r64()); break;
    default: crash(__FILE__, __LINE__, type); break;
    }    
}

static void emit_movi(ZAssembler &a, uint8_t type, int dst, int16_t imm)
{
    switch(type) {
    case UINT8:
    case INT8:    a.mov(reg(dst).r8(), imm); break;
    case UINT16:
    case INT16:   a.mov(reg(dst).r16(), imm); break;
    case UINT32:
    case INT32:   a.mov(reg(dst).r32(), imm); break;
    case UINT64:
    case INT64:   a.rex().mov(reg(dst).r64(), imm); break;
    case FLOAT32: {
	x86::Gp t = alloc_gp(a);
	a.mov(t.r32(), imm);
	a.cvtsi2ss(xreg(dst), t.r32());
	release_gp(a, t);
	break;
    }
    case FLOAT64:  {
	x86::Gp t = alloc_gp(a);
	a.mov(t.r32(), imm);
	a.cvtsi2sd(xreg(dst), t.r32());
	release_gp(a, t);
	break;
    }
    default: crash(__FILE__, __LINE__, type); break;
    }    
}

static void emit_slli(ZAssembler &a, uint8_t type, int dst, int src, int8_t imm)
{
    if (src != dst)
	a.mov(reg(dst), reg(src));
    switch(type) {
    case UINT8:
    case INT8:    a.shl(reg(dst).r8(), imm); break;
    case UINT16:
    case INT16:   a.shl(reg(dst).r16(), imm); break;
    case UINT32:
    case INT32:   a.shl(reg(dst).r32(), imm); break;
    case UINT64:
    case INT64:   a.shl(reg(dst).r64(), imm); break;
    default: crash(__FILE__, __LINE__, type); break;
    }    
}

static void emit_srli(ZAssembler &a, uint8_t type, int dst, int src, int8_t imm)
{
    if (src != dst)
	a.mov(reg(dst), reg(src));    
    switch(type) {
    case UINT8:
    case INT8:    a.shr(reg(dst).r8(), imm); break;
    case UINT16:
    case INT16:   a.shr(reg(dst).r16(), imm); break;
    case UINT32:
    case INT32:   a.shr(reg(dst).r32(), imm); break;
    case UINT64:
    case INT64:   a.shr(reg(dst).r64(), imm); break;
    default: crash(__FILE__, __LINE__, type); break;
    }    
}

static void emit_zero(ZAssembler &a, uint8_t type, int dst)
{
    switch(type) {
    case UINT8:
    case INT8:    a.xor_(reg(dst).r8(), reg(dst).r8()); break;
    case UINT16:
    case INT16:   a.xor_(reg(dst).r16(), reg(dst).r16()); break;
    case UINT32:
    case INT32:   a.xor_(reg(dst).r32(), reg(dst).r32()); break;
    case UINT64:
    case INT64:   a.xor_(reg(dst).r64(), reg(dst).r64()); break;
    default: crash(__FILE__, __LINE__, type); break;
    }    
}

#ifdef unused
static void emit_one(ZAssembler &a, uint8_t type, int dst)
{
    emit_movi(a, type, dst, 1);
}
#endif

static void emit_neg_dst(ZAssembler &a, uint8_t type, int dst)
{
    switch(type) {
    case UINT8:	
    case INT8:       a.neg(reg(dst).r8()); break;
    case UINT16:	
    case INT16:      a.neg(reg(dst).r16()); break;
    case UINT32:	
    case INT32:      a.neg(reg(dst).r32()); break;
    case UINT64:	
    case INT64:      a.neg(reg(dst).r64()); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
}

// move a full register (64 bit..)
static void emit_mov(ZAssembler &a, uint8_t type, int dst, int src)
{
    if (dst != src) {
	switch(type) {
	case FLOAT32: a.movss(xreg(dst),xreg(src)); break;
	case FLOAT64: a.movsd(xreg(dst),xreg(src)); break;	    
	case UINT8:
	case INT8:
	case UINT16:
	case INT16:
	case UINT32:
	case INT32:
	case UINT64:
	case INT64:
	    a.mov(reg(dst),reg(src)); break;
	default: crash(__FILE__, __LINE__, type); break;
	}
    }
}

static void emit_movr(ZAssembler &a, uint8_t type, int dst, int src)
{
    if (dst != src) {
	switch(type) {
	case UINT8:
	case INT8:    a.mov(reg(dst).r8(), reg(src).r8()); break;
	case UINT16:
	case INT16:   a.mov(reg(dst).r16(), reg(src).r16()); break;
	case UINT32:
	case INT32:   a.mov(reg(dst).r32(), reg(src).r32()); break;
	case UINT64:
	case INT64:   a.mov(reg(dst).r64(), reg(src).r64()); break;
	case FLOAT32: a.movss(xreg(dst),xreg(src)); break;
	case FLOAT64: a.movsd(xreg(dst),xreg(src)); break;
	default: crash(__FILE__, __LINE__, type); break;
	}
    }
}

// move src to dst if needed
static void emit_src(ZAssembler &a, int dst, int src)
{
    if (src != dst) {
	a.mov(reg(dst), reg(src));
    }
}

// reduce two sources into one! operation does not depend on order!
static int emit_one_src(ZAssembler &a, uint8_t type, int dst,
			int src1, int src2)
{
    if ((dst == src1) && (dst == src2)) // dst = dst + dst : dst += dst
	return dst;
    else if (src1 == dst) // dst = dst + src2 : dst += src2
	return src2;
    else if (src2 == dst) // dst = src1 + dst : dst += src1
	return src1;
    else {
	emit_movr(a, type, dst, src1); // dst = src1;
	return src2;
    }
}

// dst = src (maybe)
static void emit_vmov(ZAssembler &a, uint8_t type, int dst, int src)
{
    if (src != dst) {    
	switch(type) {
	case FLOAT32: a.movaps(xreg(dst), xreg(src)); break;
	case FLOAT64: a.movapd(xreg(dst), xreg(src)); break;
	default: a.movdqa(xreg(dst), xreg(src)); break;
	}
    }
}

static int emit_one_vsrc(ZAssembler &a, uint8_t type, int dst,
			 int src1, int src2)
{
    if ((dst == src1) && (dst == src2)) // dst = dst + dst : dst += dst
	return dst;
    else if (src1 == dst) // dst = dst + src2 : dst += src2
	return src2;
    else if (src2 == dst) // dst = src1 + dst : dst += src1
	return src1;
    else {
	emit_vmov(a, type, dst, src1); // dst = src1;
	return src2;
    }
}

// ordered src  (use temporary register if needed)
static int emit_one_ord_vsrc(ZAssembler &a, uint8_t type, int dst,
			     int src1, int src2)
{
    if ((dst == src1) && (dst == src2)) // dst = dst + dst : dst += dst
	return dst;
    else if (src1 == dst) // dst = dst + src2 : dst += src2
	return src2;
    else if (src2 == dst) { // dst = src1 + dst
	x86::Xmm tmp = alloc_xmm(a);
	int t = regno(tmp);
	emit_vmov(a, type, t, dst);    // T=dst
	emit_vmov(a, type, dst, src1); // dst=src1
	return t;
    }
    else {
	emit_vmov(a, type, dst, src1); // dst = src1;
	return src2;
    }
}


// move src to dst if condition code (matching cmp) is set
// else set dst=0
static void emit_movecc(ZAssembler &a, int cmp, uint8_t type, int dst)
{
    Label Skip = a.newLabel();
    emit_movi(a, type, dst, 0);
    switch(cmp) {
    case CMP_EQ:
	a.jne(Skip); break;
    case CMP_NEQ:
	a.je(Skip); break;
    case CMP_LT:
	if (get_base_type(type) == UINT)
	    a.jae(Skip);
	else
	    a.jge(Skip);
	break;
    case CMP_LE:
	if (get_base_type(type) == UINT)
	    a.ja(Skip);
	else
	    a.jg(Skip);
	break;
    case CMP_GT:
	if (get_base_type(type) == UINT)
	    a.jbe(Skip);
	else
	    a.jle(Skip);
	break;
    case CMP_GE:
	if (get_base_type(type) == UINT)
	    a.jb(Skip);
	else
	    a.jl(Skip);
	break;
    default: crash(__FILE__, __LINE__, type); break;
    }
    emit_dec(a, type, dst);
    a.bind(Skip);
}

// above without jump
#ifdef not_used
static void emit_movecc_dst(ZAssembler &a, int cmp, uint8_t type, int dst)
{
    if ((type != UINT8) && (type != INT8))
	emit_movi(a, type, dst, 0); // clear if dst is 16,32,64
    // set byte 0|1
    switch(cmp) {
    case CMP_EQ: a.seteq(reg(dst).r8()); break;	
    case CMP_NEQ: a.setne(reg(dst).r8()); break;
    case CMP_LT: a.setl(reg(dst).r8()); break;
    case CMP_LE: a.setle(reg(dst).r8()); break;
    case CMP_GT: a.setg(reg(dst).r8()); break;
    case CMP_GE: a.setge(reg(dst).r8()); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
    // negate to set all bits (optimise before conditional jump)
    emit_neg_dst(a, type, dst);    
}
#endif

// set dst = 0
static void vzero_avx(ZAssembler &a, x86::Xmm dst)
{
    a.vpxor(dst, dst, dst);
}

static void vzero_sse2(ZAssembler &a, x86::Xmm dst)
{
    a.pxor(dst, dst);
}

static void vzero(ZAssembler &a, x86::Xmm dst)
{
    if (a.use_avx())
	vzero_avx(a, dst);
    else if (a.use_sse2())
	vzero_sse2(a, dst);
    // what about not a
}

static void emit_vzero(ZAssembler &a, int dst)
{
    vzero(a, xreg(dst));
}

static void emit_vone(ZAssembler &a, int dst)
{
    a.pcmpeqb(xreg(dst), xreg(dst));
}


// dst = -src  = 0 - src
static void emit_neg(ZAssembler &a, uint8_t type, int dst, int src)
{
    emit_src(a, dst, src);
    emit_neg_dst(a, type, dst);
}

// dst = ~src 
static void emit_bnot(ZAssembler &a, uint8_t type, int dst, int src)
{
    emit_src(a, dst, src);
    switch(type) {
    case UINT8:	
    case INT8:       a.not_(reg(dst).r8()); break;
    case UINT16:	
    case INT16:      a.not_(reg(dst).r16()); break;
    case UINT32:	
    case INT32:      a.not_(reg(dst).r32()); break;
    case UINT64:	
    case INT64:      a.not_(reg(dst).r64()); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
}

static void emit_add(ZAssembler &a, uint8_t type, int dst, int src1, int src2)
{
    int src = emit_one_src(a, type, dst, src1, src2);
    switch(type) {
    case UINT8:
    case INT8:       a.add(reg(dst).r8(), reg(src).r8()); break;
    case UINT16:
    case INT16:      a.add(reg(dst).r16(), reg(src).r16()); break;
    case UINT32:
    case INT32:      a.add(reg(dst).r32(), reg(src).r32()); break;
    case UINT64:
    case INT64:      a.add(reg(dst).r64(), reg(src).r64()); break;
	// fixme: preserve content of xmm registers?
    case FLOAT32:    a.addss(xreg(dst), xreg(src)); break;
    case FLOAT64:    a.addsd(xreg(dst), xreg(src)); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
}

static void emit_addi(ZAssembler &a, uint8_t type, int dst, int src,
		      int imm8)
{
    emit_src(a, dst, src);  // move src to dst (unless src == dst)
    switch(type) {
    case UINT8:
    case INT8:    a.add(reg(dst).r8(), imm8); break;
    case UINT16:
    case INT16:   a.add(reg(dst).r16(), imm8); break;
    case UINT32:
    case INT32:   a.add(reg(dst).r32(), imm8); break;
    case UINT64:
    case INT64:   a.add(reg(dst).r64(), imm8); break;
    default: crash(__FILE__, __LINE__, type); break;
    }    
}


static void emit_sub(ZAssembler &a, uint8_t type,
		     int dst, int src1, int src2)
{
    int src;
    if ((dst == src1) && (dst == src2)) { // dst = dst - dst : dst = 0
	emit_zero(a, type, dst);
	return;
    }
    else if (src1 == dst) {   // dst = dst - src2 : dst -= src2
	src = src2;
    }
    else if (src2 == dst) { // dst = src - dst; dst = src1 + (0 - dst)
	emit_neg(a, type, dst, dst);
	emit_add(a, type, dst, src1, dst);
	return;
    }
    else {
	emit_movr(a, type, dst, src1);
	src = src2;
    }
    switch(type) {
    case UINT8:	
    case INT8:       a.sub(reg(dst).r8(), reg(src).r8()); break;
    case UINT16:	
    case INT16:      a.sub(reg(dst).r16(), reg(src).r16()); break;
    case UINT32:	
    case INT32:      a.sub(reg(dst).r32(), reg(src).r32()); break;
    case UINT64:	
    case INT64:      a.sub(reg(dst).r64(), reg(src).r64()); break;
	// fixme: preserve content of xmm registers?
    case FLOAT32:    a.subss(xreg(dst), xreg(src)); break;
    case FLOAT64:    a.subsd(xreg(dst), xreg(src)); break;	
    default: crash(__FILE__, __LINE__, type); break;
    }
}

// dst = src - imm 
static void emit_subi(ZAssembler &a, uint8_t type, int dst, int src, int8_t imm)
{
    emit_src(a, dst, src);  // move src to dst (unless src == dst)    
    switch(type) {
    case UINT8:
    case INT8:    a.sub(reg(dst).r8(), imm); break;
    case UINT16:
    case INT16:   a.sub(reg(dst).r16(), imm); break;
    case UINT32:
    case INT32:   a.sub(reg(dst).r32(), imm); break;
    case UINT64:
    case INT64:   a.sub(reg(dst).r64(), imm); break;
    default: crash(__FILE__, __LINE__, type); break;
    }    
}

static void emit_rsub(ZAssembler &a, uint8_t type,
		      int dst, int src1, int src2)
{
    emit_sub(a, type, dst, src2, src1);
}

// dst = imm - src
static void emit_rsubi(ZAssembler &a, uint8_t type, int dst, int src, int8_t imm)
{
    emit_neg(a, type, dst, src);
    emit_addi(a, type, dst, dst, imm);
}



static void emit_mul(ZAssembler &a, uint8_t type, int dst, int src1, int src2)
{
    int src = emit_one_src(a, type, dst, src1, src2);
    switch(type) {
    case UINT8:	
    case INT8: // FIXME: make code that do not affect high byte!
	a.and_(reg(dst).r16(), 0x00ff);
	a.imul(reg(dst).r16(), reg(src).r16());
	break;
    case UINT16:	
    case INT16:    a.imul(reg(dst).r16(), reg(src).r16()); break;
    case UINT32:	
    case INT32:    a.imul(reg(dst).r32(), reg(src).r32()); break;
    case UINT64:	
    case INT64:    a.imul(reg(dst).r64(), reg(src).r64()); break;
    case FLOAT32:  a.mulss(xreg(dst), xreg(src)); break;
    case FLOAT64:  a.mulsd(xreg(dst), xreg(src)); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
}

static void emit_muli(ZAssembler &a, uint8_t type, int dst, int src, int imm8)
{
    emit_src(a, dst, src);  // move src to dst (unless src == dst)        
    switch(type) {
    case UINT8:	
    case INT8: // FIXME: make code that do not affect high byte!
	a.and_(reg(dst).r16(), 0x00ff);
	a.imul(reg(dst).r16(), imm8);
	break;
    case UINT16:	
    case INT16:      a.imul(reg(dst).r16(), imm8); break; // max imm16
    case UINT32:	
    case INT32:      a.imul(reg(dst).r32(), imm8); break; // max imm32
    case UINT64:	
    case INT64:      a.imul(reg(dst).r64(), imm8); break; // max imm32
    case FLOAT32:
	emit_movi(a, type, dst, imm8);
	a.mulss(xreg(dst), xreg(src));
	break;
    case FLOAT64:
	emit_movi(a, type, dst, imm8);
	a.mulsd(xreg(dst), xreg(src));
	break;
    default: crash(__FILE__, __LINE__, type); break;		
    }
}

// dst = src1 << src2
//
// t0 = rcx     // save cx
// t1 = src1
// rcx = src2   // load count value
// t1 <<= cl    // shift src1
// dst = t1   
// rcx = t0
//

static void emit_sll(ZAssembler &a, uint8_t type, int dst, int src1, int src2)
{
    x86::Gp t0 = alloc_gp(a);
    x86::Gp t1 = alloc_gp(a);    
    a.mov(t0, x86::regs::rcx);                        // save rcx
    emit_movr(a, type, regno(t1), src1);              // t1 = src1    
    emit_movr(a, type, regno(x86::regs::rcx), src2);  // load count

    switch(type) {
    case UINT8:
    case INT8:    a.shl(t1.r8(), x86::regs::cl); break;
    case UINT16:
    case INT16:   a.shl(t1.r16(), x86::regs::cl); break;
    case UINT32:
    case INT32:   a.shl(t1.r32(), x86::regs::cl); break;
    case UINT64:
    case INT64:   a.shl(t1.r64(), x86::regs::cl); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
    emit_movr(a, type, dst, regno(t1));
    a.mov(x86::regs::rcx, t0);          // restore rcx
    release_gp(a, t1);
    release_gp(a, t0);
}

static void emit_srl(ZAssembler &a, uint8_t type, int dst, int src1, int src2)
{
    x86::Gp t0 = alloc_gp(a);
    x86::Gp t1 = alloc_gp(a);    
    a.mov(t0, x86::regs::rcx);                        // save rcx
    emit_movr(a, type, regno(t1), src1);              // t1 = src1    
    emit_movr(a, type, regno(x86::regs::rcx), src2);  // load count

    switch(type) {
    case UINT8:
    case INT8:    a.shr(t1.r8(), x86::regs::cl); break;
    case UINT16:
    case INT16:   a.shr(t1.r16(), x86::regs::cl); break;
    case UINT32:
    case INT32:   a.shr(t1.r32(), x86::regs::cl); break;
    case UINT64:
    case INT64:   a.shr(t1.r64(), x86::regs::cl); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
    emit_movr(a, type, dst, regno(t1));
    a.mov(x86::regs::rcx, t0);          // restore rcx
    release_gp(a, t1);
    release_gp(a, t0);    
}

static void emit_sra_(ZAssembler &a, uint8_t type,
		      int dst, x86::Gp shift)
{
    switch(type) {
    case UINT8:
    case INT8:    a.sar(reg(dst).r8(), shift); break;
    case UINT16:
    case INT16:   a.sar(reg(dst).r16(), shift); break;
    case UINT32:
    case INT32:   a.sar(reg(dst).r32(), shift); break;
    case UINT64:
    case INT64:   a.sar(reg(dst).r64(), shift); break;
    default: crash(__FILE__, __LINE__, type); break;
    }        
}

static void emit_sra(ZAssembler &a, uint8_t type, int dst, int src1, int src2)
{
    x86::Gp t0 = alloc_gp(a);
    x86::Gp t1 = alloc_gp(a);    
    a.mov(t0, x86::regs::rcx);                        // save rcx
    emit_movr(a, type, regno(t1), src1);              // t1 = src1    
    emit_movr(a, type, regno(x86::regs::rcx), src2);  // load count
    emit_sra_(a, type, regno(t1), x86::regs::cl);

    emit_movr(a, type, dst, regno(t1));
    a.mov(x86::regs::rcx, t0);          // restore rcx
    release_gp(a, t1);
    release_gp(a, t0);    
}

static void emit_srai(ZAssembler &a, uint8_t type, int dst, int src, int8_t imm)
{
    emit_src(a, dst, src);    
    switch(type) {
    case UINT8:
    case INT8:    a.sar(reg(dst).r8(), imm); break;
    case UINT16:
    case INT16:   a.sar(reg(dst).r16(), imm); break;
    case UINT32:
    case INT32:   a.sar(reg(dst).r32(), imm); break;
    case UINT64:
    case INT64:   a.sar(reg(dst).r64(), imm); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
}

static void emit_band(ZAssembler &a, uint8_t type,
		      int dst, int src1, int src2)
{
    int src = emit_one_src(a, type, dst, src1, src2);
    switch(type) {
    case UINT8:	
    case INT8:       a.and_(reg(dst).r8(), reg(src).r8()); break;
    case UINT16:	
    case INT16:      a.and_(reg(dst).r16(), reg(src).r16()); break;
    case UINT32:	
    case INT32:      a.and_(reg(dst).r32(), reg(src).r32()); break;
    case UINT64:	
    case INT64:      a.and_(reg(dst).r64(), reg(src).r64()); break;
    case FLOAT32:    a.andps(xreg(dst), xreg(src)); break;
    case FLOAT64:    a.andpd(xreg(dst), xreg(src)); break;
    default: crash(__FILE__, __LINE__, type); break;	
    }    
}

// dst = src & imm => dst = imm; dst = src & src;
// dst = dst & imm
static void emit_bandi(ZAssembler &a, uint8_t type,
		       int dst, int src, int8_t imm)
{
    switch(type) {
    case UINT8:	
    case INT8:
	emit_src(a, dst, src);
	a.and_(reg(dst).r8(), imm); break;
    case UINT16:	
    case INT16:
	emit_src(a, dst, src);
	a.and_(reg(dst).r16(), imm); break;
    case UINT32:
    case INT32:
	emit_src(a, dst, src);	
	a.and_(reg(dst).r32(), imm); break;
    case UINT64:
    case INT64:
	emit_src(a, dst, src);
	a.and_(reg(dst).r64(), imm); break;
    case FLOAT32: {
	x86::Xmm t1 = alloc_xmm(a);
	emit_movi(a, uint_type(type), regno(t1), imm);
	a.andps(xreg(dst), t1);
	release_xmm(a, t1);
	break;
    }
    case FLOAT64: {
	x86::Xmm t1 = alloc_xmm(a);
	emit_movi(a, uint_type(type), regno(t1), imm);	
	a.andpd(xreg(dst), t1);
	release_xmm(a, t1);
	break;
    }
    default: crash(__FILE__, __LINE__, type); break;	
    }    
}

// dst = ~dst & dst    => dst = 0
// dst = ~dst & src2   => dst = ~dst; dst &= src2
// dst = ~src1 & dst   => dst &= ~src1
// dst = ~src1 & src2  => dst &= ~src1; dst &= src1; 

static void emit_bandn(ZAssembler &a, uint8_t type,
		       int dst, int src1, int src2)
{
    if ((dst == src1) && (dst == src2)) { // dst = ~dst & dst
	emit_zero(a, type, dst);
    }
    else {
	x86::Gp t = alloc_gp(a);
	emit_mov(a, type, regno(t), src1);        // t = src1
	emit_bnot(a, type, regno(t), regno(t));  // t = ~src1
	emit_band(a, type, dst, regno(t), src2);
	release_gp(a, t);
    }
}

// dst = ~src & imm
static void emit_bandni(ZAssembler &a, uint8_t type,
			int dst, int src, int8_t imm)
{
    x86::Gp t = alloc_gp(a);
    emit_mov(a, type, regno(t), src);        // t = src
    emit_bnot(a, type, regno(t), regno(t));   // t = ~src
    emit_bandi(a, type, dst, regno(t), imm);
    release_gp(a, t);
}

static void emit_bor(ZAssembler &a, uint8_t type,
		      int dst, int src1, int src2)
{    
    int src = emit_one_src(a, type, dst, src1, src2);
    switch(type) {
    case UINT8:	
    case INT8:       a.or_(reg(dst).r8(), reg(src).r8()); break;
    case UINT16:	
    case INT16:      a.or_(reg(dst).r16(), reg(src).r16()); break;
    case UINT32:	
    case INT32:      a.or_(reg(dst).r32(), reg(src).r32()); break;
    case UINT64:	
    case INT64:      a.or_(reg(dst).r64(), reg(src).r64()); break;
    case FLOAT32:    a.orps(xreg(dst), xreg(src)); break;
    case FLOAT64:    a.orpd(xreg(dst), xreg(src)); break;
    default: crash(__FILE__, __LINE__, type); break;
    }        
}

// dst = src | imm => dst = imm; dst = dst | src;
// dst = dst | imm
static void emit_bori(ZAssembler &a, uint8_t type,
		       int dst, int src, int8_t imm)
{
    switch(type) {
    case UINT8:	
    case INT8:
	emit_src(a, dst, src);
	a.or_(reg(dst).r8(), imm);
	break;
    case UINT16:	
    case INT16:
	emit_src(a, dst, src);
	a.or_(reg(dst).r16(), imm);
	break;
    case UINT32:	
    case INT32:
	emit_src(a, dst, src);
	a.or_(reg(dst).r32(), imm);
	break;
    case UINT64:	
    case INT64:
	emit_src(a, dst, src);	
	a.or_(reg(dst).r64(), imm);
	break;
    case FLOAT32: {
	x86::Xmm t1 = alloc_xmm(a);
	emit_movi(a, uint_type(type), regno(t1), imm);
	a.orps(xreg(dst), t1);
	release_xmm(a, t1);
	break;
    }
    case FLOAT64: {
	x86::Xmm t1 = alloc_xmm(a);
	emit_movi(a, uint_type(type), regno(t1), imm);	
	a.orpd(xreg(dst), t1);
	release_xmm(a, t1);
	break;
    }	
    default: crash(__FILE__, __LINE__, type); break;	
    }    
}

static void emit_bxor(ZAssembler &a, uint8_t type,
		       int dst, int src1, int src2)
{
    int src = emit_one_src(a, type, dst, src1, src2);    
    switch(type) {
    case UINT8:	
    case INT8:       a.xor_(reg(dst).r8(), reg(src).r8()); break;
    case UINT16:	
    case INT16:      a.xor_(reg(dst).r16(), reg(src).r16()); break;
    case UINT32:	
    case INT32:      a.xor_(reg(dst).r32(), reg(src).r32()); break;
    case UINT64:	
    case INT64:      a.xor_(reg(dst).r64(), reg(src).r64()); break;
    case FLOAT32:    a.xorps(xreg(dst), xreg(src)); break;
    case FLOAT64:    a.xorpd(xreg(dst), xreg(src)); break;	
    default: crash(__FILE__, __LINE__, type); break;
    }
}

static void emit_bxori(ZAssembler &a, uint8_t type,
		       int dst, int src, int8_t imm)
{
    switch(type) {
    case UINT8:	
    case INT8:
	emit_src(a, dst, src);		
	a.xor_(reg(dst).r8(), imm);
	break;
    case UINT16:	
    case INT16:
	emit_src(a, dst, src);	
	a.xor_(reg(dst).r16(), imm);
	break;
    case UINT32:
    case INT32:
	emit_src(a, dst, src);	
	a.xor_(reg(dst).r32(), imm);
	break;
    case UINT64:	
    case INT64:
	emit_src(a, dst, src);	
	a.xor_(reg(dst).r64(), imm);
	break;
    case FLOAT32: {
	x86::Xmm t1 = alloc_xmm(a);
	emit_movi(a, uint_type(type), regno(t1), imm);
	a.xorps(xreg(dst), t1);
	release_xmm(a, t1);
	break;
    }
    case FLOAT64: {
	x86::Xmm t1 = alloc_xmm(a);
	emit_movi(a, uint_type(type), regno(t1), imm);	
	a.xorpd(xreg(dst), t1);
	release_xmm(a, t1);
	break;
    }	
    default: crash(__FILE__, __LINE__, type); break;	
    }
}


static void emit_cmpi(ZAssembler &a, uint8_t type, int src1, int imm)
{
    switch(type) {
    case INT8:
    case UINT8:	a.cmp(reg(src1).r8(), imm); break;
    case INT16:
    case UINT16: a.cmp(reg(src1).r16(), imm); break;
    case INT32:	    
    case UINT32: a.cmp(reg(src1).r32(), imm); break;
    case INT64:	    
    case UINT64: a.cmp(reg(src1).r64(), imm); break;
    case FLOAT32: {
	x86::Gp t = alloc_gp(a);
	x86::Xmm t1 = alloc_xmm(a);
	a.mov(t.r32(), imm);
	a.cvtsi2ss(t1, t.r32());
	a.comiss(xreg(src1), t1);
	release_gp(a, t);
	release_xmm(a, t1);
	break;
    }
    case FLOAT64: {
	x86::Gp t = alloc_gp(a);
	x86::Xmm t1 = alloc_xmm(a);	
	a.mov(t.r32(), imm);
	a.cvtsi2sd(t1,t.r32());
	a.comisd(xreg(src1), t1);
	release_gp(a, t);
	release_xmm(a, t1);
	break;
    }
    default: crash(__FILE__, __LINE__, type); break;
    }	
}

static void emit_cmp(ZAssembler &a, uint8_t type, int src1, int src2)
{
    switch(type) {
    case INT8:
    case UINT8:	a.cmp(reg(src1).r8(), reg(src2).r8()); break;
    case INT16:
    case UINT16: a.cmp(reg(src1).r16(), reg(src2).r16()); break;
    case INT32:
    case UINT32: a.cmp(reg(src1).r32(), reg(src2).r32()); break;
    case INT64:
    case UINT64: a.cmp(reg(src1).r64(), reg(src2).r64()); break;
    case FLOAT32: a.comiss(xreg(src1), xreg(src2)); break;
    case FLOAT64: a.comisd(xreg(src1), xreg(src2)); break;
    default: crash(__FILE__, __LINE__, type); break;	
    }	
}

static void emit_cmpeq(ZAssembler &a, uint8_t type,
		       int dst, int src1, int src2)
{
    emit_cmp(a, type, src1, src2);
    emit_movecc(a, CMP_EQ, type, dst);
}

static void emit_cmpeqi(ZAssembler &a, uint8_t type,
			int dst, int src1, int8_t imm)
{
    emit_cmpi(a, type, src1, imm);
    emit_movecc(a, CMP_EQ, type, dst);
}


// emit dst = src1 > src2
static void emit_cmpgt(ZAssembler &a, uint8_t type,
		       int dst, int src1, int src2)
{
    emit_cmp(a, type, src1, src2);
    emit_movecc(a, CMP_GT, type, dst);
}

static void emit_cmpgti(ZAssembler &a, uint8_t type,
		       int dst, int src1, int8_t imm)
{
    emit_cmpi(a, type, src1, imm);
    emit_movecc(a, CMP_GT, type, dst);
}


// emit dst = src1 >= src2
static void emit_cmpge(ZAssembler &a, uint8_t type,
		       int dst, int src1, int src2)
{
    emit_cmp(a, type, src1, src2);
    emit_movecc(a, CMP_GE, type, dst);
}

static void emit_cmpgei(ZAssembler &a, uint8_t type,
		       int dst, int src1, int8_t imm)
{
    emit_cmpi(a, type, src1, imm);
    emit_movecc(a, CMP_GE, type, dst);
}

static void emit_cmplt(ZAssembler &a, uint8_t type,
		       int dst, int src1, int src2)
{
    emit_cmp(a, type, src1, src2);
    emit_movecc(a, CMP_LT, type, dst);
}

static void emit_cmplti(ZAssembler &a, uint8_t type,
		       int dst, int src1, int8_t imm)
{
    emit_cmpi(a, type, src1, imm);
    emit_movecc(a, CMP_LT, type, dst);
}

static void emit_cmple(ZAssembler &a, uint8_t type,
		       int dst, int src1, int src2)
{
    emit_cmp(a, type, src1, src2);
    emit_movecc(a, CMP_LE, type, dst);
}

static void emit_cmplei(ZAssembler &a, uint8_t type,
		       int dst, int src1, int8_t imm)
{
    emit_cmpi(a, type, src1, imm);
    emit_movecc(a, CMP_LE, type, dst);
}


static void emit_cmpne(ZAssembler &a, uint8_t type,
		       int dst, int src1, int src2)
{
    emit_cmp(a, type, src1, src2);
    emit_movecc(a, CMP_NEQ, type, dst);
}

static void emit_cmpnei(ZAssembler &a, uint8_t type,
		       int dst, int src1, int8_t imm)
{
    emit_cmpi(a, type, src1, imm);
    emit_movecc(a, CMP_NEQ, type, dst);
}

#define DST xreg(dst)
#define SRC xreg(src)
#define SRC1 xreg(src1)
#define SRC2 xreg(src2)

static void emit_vneg_avx(ZAssembler &a, uint8_t type, int dst, int src)
{
    x86::Xmm t0 = alloc_xmm(a);
    vzero_avx(a, t0);
    switch(type) {  // dst = -src; dst = 0 - src
    case INT8:
    case UINT8:   a.vpsubb(DST, t0, SRC); break;
    case INT16:	    
    case UINT16:  a.vpsubw(DST, t0, SRC); break;
    case INT32:
    case UINT32:  a.vpsubd(DST, t0, SRC); break;
    case INT64:
    case UINT64:  a.vpsubq(DST, t0, SRC); break;
    case FLOAT32: a.vsubps(DST, t0, SRC); break;
    case FLOAT64: a.vsubpd(DST, t0, SRC); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
    release_xmm(a, t0);
}

static void emit_vneg_sse2(ZAssembler &a, uint8_t type, int dst, int src)
{
    int src0 = src;
    if (src == dst) { // dst = -dst;
	x86::Xmm t0 = alloc_xmm(a);
	src = regno(t0);
	emit_vmov(a, type, src, dst);
    }
    vzero_sse2(a, DST);
    switch(type) {  // dst = src - dst
    case INT8:
    case UINT8:   a.psubb(DST, SRC); break;
    case INT16:	    
    case UINT16:  a.psubw(DST, SRC); break;
    case INT32:
    case UINT32:  a.psubd(DST, SRC); break;
    case INT64:
    case UINT64:  a.psubq(DST, SRC); break;
    case FLOAT32: a.subps(DST, SRC); break;
    case FLOAT64: a.subpd(DST, SRC); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
    if (src != src0)
	release_xmm(a, xreg(src));
}

// dst = -src  = 0 - src
static void emit_vneg(ZAssembler &a, uint8_t type, int dst, int src)
{
    if (a.use_avx())
	emit_vneg_avx(a, type, dst, src);
    else if (a.use_sse2())
	emit_vneg_sse2(a, type, dst, src);
    else
	emit_neg(a, type, dst, src);
}

// broadcast integer value (upto) imm12 into element
static void emit_vmovi(ZAssembler &a, uint8_t type, int dst, int16_t imm12)
{
    switch(type) {
    case INT8:
    case UINT8: {
	x86::Gp t = alloc_gp(a);
	a.mov(t.r32(), imm12);
	a.movd(DST, t.r32());
	a.punpcklbw(DST,DST);
	a.punpcklwd(DST,DST);
	a.pshufd(DST,DST,0);
	release_gp(a, t);
	break;
    }
    case INT16:
    case UINT16: {
	x86::Gp t = alloc_gp(a);
	a.mov(t.r32(), imm12);
	a.movd(DST, t.r32());
	a.punpcklwd(DST,DST);
	a.pshufd(DST,DST,0);
	release_gp(a, t);
	break;
    }
    case INT32:
    case UINT32: {
	x86::Gp t = alloc_gp(a);	
	a.mov(t.r32(), imm12);
	a.movd(DST, t.r32());
	a.pshufd(DST,DST,0);
	release_gp(a, t);
	break;
    }
    case INT64:
    case UINT64: {
	x86::Gp t = alloc_gp(a);
	a.rex().mov(t.r64(), imm12);
	a.movq(DST, t.r64());
	a.punpcklqdq(DST, DST);
	release_gp(a, t);	
	break;
    }
    case FLOAT32: {
	x86::Gp t = alloc_gp(a);
	a.mov(t.r32(), imm12);
	a.cvtsi2ss(DST, t.r32());	
	a.pshufd(DST,DST,0);
	release_gp(a, t);
	break;
    }
    case FLOAT64: {
	x86::Gp t = alloc_gp(a);
	a.rex().mov(t.r64(), imm12);
	a.cvtsi2sd(DST, t.r32());
	a.punpcklqdq(DST, DST);
	release_gp(a, t);
	break;
    }
    default:
	break;
    }
}

static void emit_vslli_sse2(ZAssembler &a, uint8_t type,
			    int dst, int src, int8_t imm8)
{
    emit_vmov(a, type, dst, src);
    switch(type) {
    case UINT8:
    case INT8: {
	x86::Xmm t0 = alloc_xmm(a);
	x86::Xmm t2 = alloc_xmm(a);	

	// LOW PART
	vzero_sse2(a, t0);
	a.punpcklbw(t0, DST);   // |76543210|00000000|
	a.psllw(t0, imm8);      // |54321000|00000000|
	a.psrlw(t0, 8);	        // |00000000|54321000|	

	// HIGH
	vzero_sse2(a, t2);
	a.punpckhbw(t2, DST);    // |FEDCBA98|00000000|
	a.psllw(t2, imm8);
	a.psrlw(t2, 8);          // |00000000|DCBA9800|

	a.movdqa(DST, t0);
	a.packuswb(DST, t2);     // combine
	release_xmm(a,t2);
	release_xmm(a,t0);	
	break;
    }
    case UINT16:
    case INT16:   a.psllw(DST, imm8); break;
    case UINT32:
    case INT32:   a.pslld(DST, imm8); break;
    case UINT64:
    case INT64:   a.psllq(DST, imm8); break;
    default: crash(__FILE__, __LINE__, type); break;
    }    
}

static void emit_vslli_avx(ZAssembler &a, uint8_t type,
			   int dst, int src, int8_t imm8)
{
    switch(type) {
    case UINT8:
    case INT8: {
	x86::Xmm t0 = alloc_xmm(a);
	x86::Xmm t2 = alloc_xmm(a);
	
	// LOW PART
	vzero_avx(a, t0);
	a.punpcklbw(t0, SRC);     // |76543210|00000000|
	a.vpsllw(t0, t0, imm8);   // |54321000|00000000|
	a.vpsrlw(t0, t0, 8);	  // |00000000|54321000|	

	// HIGH
	vzero_avx(a, t2);
	a.punpckhbw(t2, SRC);     // |FEDCBA98|00000000|
	a.vpsllw(t2, t2, imm8);
	a.vpsrlw(t2, t2, 8);      // |00000000|DCBA9800|

	a.movdqa(DST, t0);
	a.packuswb(DST, t2);      // combine
	release_xmm(a,t2);
	release_xmm(a,t0);	
	break;
    }
    case UINT16:
    case INT16:   a.vpsllw(DST, SRC, imm8); break;
    case UINT32:
    case INT32:   a.vpslld(DST, SRC, imm8); break;
    case UINT64:
    case INT64:   a.vpsllq(DST, SRC, imm8); break;
    default: crash(__FILE__, __LINE__, type); break;
    }    
}

static void emit_vslli(ZAssembler &a, uint8_t type,
		       int dst, int src, int8_t imm8)
{
    if (a.use_avx()) 
	emit_vslli_avx(a, type, dst, src, imm8);
    else if (a.use_sse2())
	emit_vslli_sse2(a, type, dst, src, imm8);
    else
	emit_slli(a, type, dst, src, imm8);
}

static void emit_vsrli(ZAssembler &a, uint8_t type,
		       int dst, int src, int8_t imm8)
{
    emit_vmov(a, type, dst, src);    
    switch(type) {
    case UINT8:
    case INT8: {
	x86::Xmm t0 = alloc_xmm(a);
	x86::Xmm t2 = alloc_xmm(a);
	
	// LOW PART (example shift=2)
	vzero(a, t0);
	a.punpcklbw(t0, DST);   // |76543210|00000000|	
	a.psrlw(t0, 8+imm8);    // |00000000|00765432|

	// HIGH PART
	vzero(a, t2);
	a.punpckhbw(t2, DST);   // |FEDCBA98|00000000|
	a.psrlw(t2, 8+imm8);
	
	a.movdqa(DST, t0);
	a.packuswb(DST, t2);
	break;
    }
    case UINT16:
    case INT16:   a.psrlw(DST, imm8); break;
    case UINT32:
    case INT32:   a.psrld(DST, imm8); break;
    case UINT64:
    case INT64:   a.psrlq(DST, imm8); break;
    default: crash(__FILE__, __LINE__, type); break;
    }    
}

static void emit_vsrai(ZAssembler &a, uint8_t type,
		       int dst, int src, int8_t imm8)
{
    emit_vmov(a, type, dst, src);
    switch(type) {
    case UINT8: 
    case INT8: {
	x86::Xmm t0 = alloc_xmm(a);
	x86::Xmm t2 = alloc_xmm(a);	
	// LOW PART (example shift=2)	
	vzero(a, t0);
	a.punpcklbw(t0, DST);     // |76543210|00000000|		    
	a.psraw(t0, imm8);        // |00765432|00000000|	
	a.psrlw(t0, 8);           // |00000000|00765432|
	
	// HIGH PART
	vzero(a, t2);
	a.punpckhbw(t2, DST);     // |FEDCBA98|00000000|
	a.psraw(t2, imm8);        // |FFFEDCBA|98000000|	
	a.psrlw(t2, 8);           // |00000000|FFFEDCBA|
	
	a.movdqa(DST, t0);
	a.packuswb(DST, t2);
	release_xmm(a,t2);
	release_xmm(a,t0);
	break;
    }
    case UINT16:
    case INT16:   a.psraw(DST, imm8); break;
    case UINT32:
    case INT32:   a.psrad(DST, imm8); break;
    case UINT64:
    case INT64: {
	x86::Gp t = alloc_gp(a);
	x86::Xmm t0 = alloc_xmm(a);
	a.movdqa(t0, DST);
	// shift low
	a.movq(t.r64(), DST);
	a.sar(t.r64(), imm8);
	a.movq(DST, t.r64());
	// shift high
	a.movhlps(t0, t0); // shift left 64
	a.movq(t.r64(), t0);
	a.sar(t.r64(), imm8);
	a.movq(t0, t.r64());
	a.punpcklqdq(DST, t0);
	release_gp(a, t);
	release_xmm(a, t0);	
	break;
    }
    default: crash(__FILE__, __LINE__, type); break;
    }
}

// [V]PADDW dst,src  : dst = dst + src
// dst = src1 + src2  |  dst=src1; dst += src2;
// dst = dst + src2   |  dst += src2;
// dst = src1 + dst   |  dst += src1;
// dst = dst + dst    |  dst += dst;   == dst = 2*dst == dst = (dst<<1)

static void emit_vadd_avx(ZAssembler &a, uint8_t type,
			  int dst, int src1, int src2)
{
    switch(type) {
    case INT8:
    case UINT8: a.vpaddb(DST, SRC1, SRC2); break;
    case INT16:
    case UINT16:  a.vpaddw(DST, SRC1, SRC2); break;
    case INT32:	    
    case UINT32:  a.vpaddd(DST, SRC1, SRC2); break;
    case INT64:	    
    case UINT64:  a.vpaddq(DST, SRC1, SRC2); break;
    case FLOAT32: a.vaddps(DST, SRC1, SRC2); break;
    case FLOAT64: a.vaddpd(DST, SRC1, SRC2); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
}

static void emit_vadd_sse2(ZAssembler &a, uint8_t type,
			   int dst, int src1, int src2)
{
    int src = emit_one_vsrc(a, type, dst, src1, src2);
    switch(type) {
    case INT8:
    case UINT8:  a.paddb(DST, SRC); break;
    case INT16:	    
    case UINT16:  a.paddw(DST, SRC); break;
    case INT32:	    
    case UINT32:  a.paddd(DST, SRC); break;
    case INT64:	    
    case UINT64:  a.paddq(DST, SRC); break;
    case FLOAT32: a.addps(DST, SRC); break;
    case FLOAT64: a.addpd(DST, SRC); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
}

static void emit_vadd(ZAssembler &a, uint8_t type,
		      int dst, int src1, int src2)
{
    if (a.use_avx()) 
	emit_vadd_avx(a, type, dst, src1, src2);
    else if (a.use_sse2())
	emit_vadd_sse2(a, type, dst, src1, src2);
    else
	emit_add(a, type, dst, src1, src2);
}

static void emit_vaddi(ZAssembler &a, uint8_t type,
		       int dst, int src, int8_t imm)
{
    x86::Xmm t1 = alloc_xmm(a);    
    emit_vmovi(a, type, regno(t1), imm);
    emit_vadd(a, type, dst, src, regno(t1));
    release_xmm(a, t1);    
}

// SUB r0, r1, r2   (r2 = r0 - r1 )
// dst = src1 - src2 
// PADDW dst,src  : dst = src - dst ???

static void emit_vsub_avx(ZAssembler &a, uint8_t type,
			  int dst, int src1, int src2)
{
    switch(type) {
    case INT8:
    case UINT8: a.vpsubb(DST, SRC1, SRC2); break;
    case INT16:
    case UINT16:  a.vpsubw(DST, SRC1, SRC2); break;
    case INT32:	    
    case UINT32:  a.vpsubd(DST, SRC1, SRC2); break;
    case INT64:	    
    case UINT64:  a.vpsubq(DST, SRC1, SRC2); break;
    case FLOAT32: a.vsubps(DST, SRC1, SRC2); break;
    case FLOAT64: a.vsubpd(DST, SRC1, SRC2); break;
    default: crash(__FILE__, __LINE__, type); break;
    }    
}

static void emit_vsub_sse2(ZAssembler &a, uint8_t type,
			  int dst, int src1, int src2)
{
    int src;
    if ((dst == src1) &&
	(dst == src2)) { // dst = dst - dst : dst = 0
	emit_vzero(a, dst);
	return;
    }
    else if (src1 == dst) {   // dst = dst - src2 : dst -= src2
	src = src2;
    }
    else if (src2 == dst) { // dst = src - dst; dst = src1 + (0 - dst)
	emit_vneg_sse2(a, type, dst, dst);
	emit_vadd_sse2(a, type, dst, src1, dst);
	return;
    }
    else {
	emit_vmov(a, type, dst, src1);  // dst != src1
	src = src2;
    }
    switch(type) {
    case INT8:
    case UINT8:   a.psubb(DST, SRC); break;
    case INT16:	    
    case UINT16:  a.psubw(DST, SRC); break;
    case INT32:
    case UINT32:  a.psubd(DST, SRC); break;
    case INT64:	    
    case UINT64:  a.psubq(DST, SRC); break;
    case FLOAT32: a.subps(DST, SRC); break;
    case FLOAT64: a.subpd(DST, SRC); break;
    default: crash(__FILE__, __LINE__, type); break;
    }    
}

static void emit_vsub(ZAssembler &a, uint8_t type,
		      int dst, int src1, int src2)
{
    if (a.use_avx())
	emit_vsub_avx(a, type, dst, src1,src2);
    else if (a.use_sse2())
	emit_vsub_sse2(a, type, dst, src1,src2);	
    else
	emit_sub(a, type, dst, src1, src2);
}

static void emit_vsubi(ZAssembler &a, uint8_t type,
		       int dst, int src, int8_t imm)
{
    x86::Xmm t1 = alloc_xmm(a);
    emit_vmovi(a, type, regno(t1), imm);
    emit_vsub(a, type, dst, src, regno(t1));
    release_xmm(a, t1);    
}

static void emit_vrsub(ZAssembler &a, uint8_t type,
		       int dst, int src1, int src2)
{
    emit_vsub(a, type, dst, src2, src1);
}

static void emit_vrsubi(ZAssembler &a, uint8_t type,
			int dst, int src, int8_t imm)
{
    emit_vneg(a, type, dst, src);
    emit_vaddi(a, type, dst, dst, imm);
}

static void emit_vmul_sse2(ZAssembler &a, uint8_t type,
			   int dst, int src1, int src2)
{
    int src = emit_one_vsrc(a, type, dst, src1, src2);    

    switch(type) {
    case INT8:
    case UINT8: {
	x86::Xmm t0 = alloc_xmm(a);
	x86::Xmm t2 = alloc_xmm(a); 	

	a.movdqa(t2, DST);
	a.movdqa(t0, SRC);
	a.pmullw(t2, t0);
	a.psllw(t2, 8);
	a.psrlw(t2, 8);
    
	a.psrlw(DST, 8);
	a.psrlw(t0, 8);
	a.pmullw(DST, t0);
	a.psllw(DST, 8);
	a.por(DST, t2);
	release_xmm(a, t0);
	release_xmm(a, t2);
	break;
    }
    case INT16:
    case UINT16: a.pmullw(DST, SRC); break;
    case INT32:	    
    case UINT32: a.pmulld(DST, SRC); break;

    case INT64:
    case UINT64: {
	x86::Xmm t0 = alloc_xmm(a);
	x86::Xmm t2 = alloc_xmm(a);	
	a.movdqa(t0, DST);     // T0=DST
	a.pmuludq(t0, SRC);    // T0=L(DST)*L(SRC)
	a.movdqa(t2, SRC);     // T2=SRC
	a.psrlq(t2, 32);       // T2=H(SRC)
	a.pmuludq(t2, DST);    // T2=H(SRC)*L(DST)
	a.psllq(t2,32);	       // T2=H(SRC)*L(DST)<<32
	a.paddq(t0, t2);       // T0+=H(SRC)*L(DST)<<32

	a.movdqa(t2, DST);     // T2=DST
	a.psrlq(t2, 32);       // T2=H(DST)
	a.pmuludq(t2, SRC);    // T2=H(DST)*L(SRC)
	a.psllq(t2,32);	       // T2=H(DST)*L(SRC)<<32
	a.paddq(t0, t2);       // T0+=H(DST)*L(SRC)<<32	

	a.movdqa(DST, t0);     // T0=DST
	release_xmm(a, t0);
	release_xmm(a, t2);	
	break;
    }

    case FLOAT32: a.mulps(DST, SRC); break;	    
    case FLOAT64: a.mulpd(DST, SRC); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
}

static void emit_vmul_avx(ZAssembler &a, uint8_t type,
			  int dst, int src1, int src2)
{
    switch(type) {
    case INT8:
    case UINT8: {
	int src = emit_one_vsrc(a, type, dst, src1, src2);
	x86::Xmm t0 = alloc_xmm(a);
	x86::Xmm t2 = alloc_xmm(a);	

	a.movdqa(t2, DST);
	a.movdqa(t0, SRC);
	a.vpmullw(t2, t2, t0);
	a.vpsllw(t2, t2, 8);
	a.vpsrlw(t2, t2, 8);
    
	a.vpsrlw(DST, DST, 8);
	a.vpsrlw(t0, t0, 8);
	a.vpmullw(DST, DST, t0);
	a.vpsllw(DST, DST, 8);
	a.vpor(DST, DST, t2);
	release_xmm(a, t2);
	release_xmm(a, t0);	
	break;
    }
    case INT16:
    case UINT16: a.vpmullw(DST, SRC1, SRC2); break;
    case INT32:	    
    case UINT32: a.vpmulld(DST, SRC1, SRC2); break;

    case INT64:
    case UINT64: {
	x86::Xmm t0 = alloc_xmm(a);
	x86::Xmm t2 = alloc_xmm(a);	
	
	a.vpmuludq(t0, SRC1, SRC2); // T0=L(SRC1)*L(SRC2)

	a.vpsrlq(t2, SRC1, 32);    // T2=H(SRC1)
	a.vpmuludq(t2, t2, SRC2);  // T2=H(SRC1)*L(SRC2)
	a.vpsllq(t2, t2, 32);     // T2=H(SRC1)*L(SRC2)<<32
	a.vpaddq(t0, t0, t2);     // T0+=H(SRC1)*L(SRC2)<<32

	a.vpsrlq(t2, SRC2, 32);
	a.vpmuludq(t2, t2, SRC1);   // T2=H(SRC2)*L(SRC1)
	a.vpsllq(t2, t2,32);	    // T2=H(SRC2)*L(SRC1)<<32
	a.vpaddq(DST, t0, t2);      // DST=T0+H(DST)*L(SRC)<<32
	release_xmm(a, t2);
	release_xmm(a, t0);	
	break;
    }
	
    case FLOAT32: a.vmulps(DST, SRC1, SRC2); break;	    
    case FLOAT64: a.vmulpd(DST, SRC1, SRC2); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
}

// dst = src1*src2
static void emit_vmul(ZAssembler &a, uint8_t type,
		      int dst, int src1, int src2)
{
    if (a.use_avx())
	emit_vmul_avx(a, type, dst, src1, src2);
    else if (a.use_sse2())
	emit_vmul_sse2(a, type, dst, src1, src2);
    else
	emit_mul(a, type, dst, src1, src2);
}

static void emit_vsll_sse2(ZAssembler &a, uint8_t type,
			   int dst, int src1, int src2)
{
    x86::Xmm t1 = alloc_xmm(a);    

    vzero_sse2(a, t1);  // needed?
    a.movq(t1, reg(src2));

    emit_vmov(a, type, dst, src1);
    
    switch(type) {
    case UINT8:
    case INT8: {
	x86::Xmm t0 = alloc_xmm(a);
	x86::Xmm t2 = alloc_xmm(a);
	// LOW PART (example shift=2)
	vzero_sse2(a, t0);
	a.punpcklbw(t0, DST);   // |76543210|00000000|
	a.psllw(t0, t1);        // |54321000|00000000|
	a.psrlw(t0, 8);	        // |00000000|54321000|

	// HIGH PART
	vzero_sse2(a, t2);
	a.punpckhbw(t2, DST);    // |FEDCBA98|00000000|
	a.psllw(t2, t1);
	a.psrlw(t2, 8);          // |00000000|DCBA9800|

	a.movdqa(DST, t0);
	a.packuswb(DST, t2);     // combine
	release_xmm(a, t2);
	release_xmm(a, t0);
	break;
    }
    case UINT16:
    case INT16:   a.psllw(DST, t1); break;
    case UINT32:
    case INT32:   a.pslld(DST, t1); break;
    case UINT64:
    case INT64:   a.psllq(DST, t1); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
    release_xmm(a, t1);
}

static void emit_vsll_avx(ZAssembler &a, uint8_t type,
			  int dst, int src1, int src2)
{
    x86::Xmm t1 = alloc_xmm(a);
    vzero_avx(a, t1);  // needed? only 64-bits used...
    a.movq(t1, reg(src2));

    switch(type) {
    case UINT8:
    case INT8: {
	x86::Xmm t0 = alloc_xmm(a);
	x86::Xmm t2 = alloc_xmm(a);	
	// LOW PART (example shift=2)
	vzero(a, t0);
	a.punpcklbw(t0, SRC1);    // |76543210|00000000|
	a.vpsllw(t0, t0, t1);    // |54321000|00000000|
	a.vpsrlw(t0, t0, 8);	 // |00000000|54321000|
	
	// HIGH PART
	vzero(a, t2);
	a.punpckhbw(t2, SRC1);    // |FEDCBA98|00000000|
	a.vpsllw(t2, t2, t1);
	a.vpsrlw(t2, t2, 8);     // |00000000|DCBA9800|

	a.movdqa(DST, t0);
	a.packuswb(DST, t2);     // combine
	release_xmm(a, t2);
	release_xmm(a, t0);	
	break;
    }
    case UINT16:
    case INT16:   a.vpsllw(DST, SRC1, t1); break;
    case UINT32:
    case INT32:   a.vpslld(DST, SRC1, t1); break;
    case UINT64:
    case INT64:   a.vpsllq(DST, SRC1, t1); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
    release_xmm(a, t1);
}

static void emit_vsll(ZAssembler &a, uint8_t type,
		      int dst, int src1, int src2)
{
    if (a.use_avx())
	emit_vsll_avx(a, type, dst, src1, src2);
    else if (a.use_sse2())
	emit_vsll_sse2(a, type, dst, src1, src2);
    else
	emit_sll(a, type, dst, src1, src2);
}

static void emit_vsrl(ZAssembler &a, uint8_t type,
		      int dst, int src1, int src2)
{
    x86::Xmm t1 = alloc_xmm(a);
    vzero(a, t1);  // needed? only 64-bits used...
    a.movq(t1, reg(src2));
    
    emit_vmov(a, type, dst, src1);
    
    switch(type) {
    case UINT8:
    case INT8: {
	x86::Xmm t0 = alloc_xmm(a);
	x86::Xmm t2 = alloc_xmm(a);	
	// LOW PART (example shift=2)
	vzero(a, t0);
	a.punpcklbw(t0, DST);   // |76543210|00000000|	
 	a.psrlw(t0, t1);        // |00765432|10000000|
	a.psrlw(t0, 8);         // |00000000|00765432|

	// HIGH PART
	vzero(a, t2);
	a.punpckhbw(t2, DST);   // |FEDCBA98|00000000|
	a.psrlw(t2, t1);
	a.psrlw(t2, 8);
	
	a.movdqa(DST, t0);	
	a.packuswb(DST, t2);
	release_xmm(a, t2);
	release_xmm(a, t0);	
	break;
    }
    case UINT16:
    case INT16:   a.psrlw(DST, t1); break;
    case UINT32:
    case INT32:   a.psrld(DST, t1); break;
    case UINT64:
    case INT64:   a.psrlq(DST, t1); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
    release_xmm(a, t1);
}

static void emit_vsra(ZAssembler &a, uint8_t type,
		      int dst, int src1, int src2)
{
    if ((type == UINT64) || (type == INT64)) {
	x86::Gp t = alloc_gp(a);
	x86::Gp tc = alloc_gp(a);    
	x86::Xmm t0 = alloc_xmm(a);

	a.mov(tc, x86::regs::rcx);  // save rcx	
	emit_vmov(a, type, dst, src1);
	
	a.movdqa(t0, DST);
	a.add_dirty_reg(x86::regs::cl);
	a.mov(x86::regs::cl, reg(src2).r8()); // setup shift value	
	
	// shift low
	a.movq(t.r64(), DST);
	emit_sra_(a, type, regno(t), x86::regs::cl);
	a.movq(DST, t.r64());
	
	// shift high
	a.movhlps(t0, t0); // shift left 64 bit (in 128)
	a.movq(t.r64(), t0);
	emit_sra_(a, type, regno(t), x86::regs::cl);
	a.movq(t0, t.r64());
	a.punpcklqdq(DST, t0);
	a.mov(x86::regs::rcx, tc);          // restore rcx
	
	release_xmm(a, t0);
	release_gp(a, tc);	
	release_gp(a, t);
    }
    else {
	x86::Xmm t1 = alloc_xmm(a); 
	vzero(a, t1);
	a.movq(t1, reg(src2));
	emit_vmov(a, type, dst, src1);

	switch(type) {
	case UINT8:
	case INT8: {
	    x86::Xmm t0 = alloc_xmm(a);
	    x86::Xmm t2 = alloc_xmm(a);
	    // LOW PART (example shift=2)	
	    vzero(a, t0);
	    a.punpcklbw(t0, DST);     // |76543210|00000000|		    
	    a.psraw(t0, t1);          // |00765432|00000000|	
	    a.psrlw(t0, 8);           // |00000000|00765432|

	    // HIGH PART
	    vzero(a, t2);
	    a.punpckhbw(t2, DST);     // |FEDCBA98|00000000|
	    a.psraw(t2, t1);          // |FFFEDCBA|98000000|	
	    a.psrlw(t2, 8);           // |00000000|FFFEDCBA|
	    
	    a.movdqa(DST, t0);
	    a.packuswb(DST, t2);
	    release_xmm(a, t0);
	    release_xmm(a, t2);
	    break;
	}
	case UINT16:
	case INT16:   a.psraw(DST, t1); break;
	case UINT32:
	case INT32:   a.psrad(DST, t1); break;
	default: crash(__FILE__, __LINE__, type); break;
	}
	release_xmm(a, t1);
    }
}

static void emit_vmuli(ZAssembler &a, uint8_t type,
		       int dst, int src, int8_t imm)
{
    x86::Xmm t1 = alloc_xmm(a);
    emit_vmovi(a, type, regno(t1), imm);
    emit_vmul(a, type, dst, src, regno(t1)); // FIXME: wont work INT64!
    release_xmm(a, t1);
}

static void emit_vbor_sse2(ZAssembler &a, uint8_t type,
			   int dst, int src1, int src2)
{
    (void) type;
    int src = emit_one_vsrc(a, type, dst, src1, src2);    
    switch(type) {
    case FLOAT32:
	a.orps(DST, SRC);
	break;	
    case FLOAT64:
	a.orpd(DST, SRC);
	break;	
    default:
	a.por(DST, SRC);
	break;
    }
}

static void emit_vbor_avx(ZAssembler &a, uint8_t type,
			  int dst, int src1, int src2)
{
    switch(type) {
    case FLOAT32:
	a.vorps(DST, SRC1, SRC2);
	break;	
    case FLOAT64:
	a.vorpd(DST, SRC1, SRC2);
	break;	
    default:
	a.vpor(DST, SRC1, SRC2);
	break;
    }
}

static void emit_vbor(ZAssembler &a, uint8_t type,
		      int dst, int src1, int src2)
{
    if (a.use_avx())
	emit_vbor_avx(a, type, dst, src1, src2);
    else if (a.use_sse2())
	emit_vbor_sse2(a, type, dst, src1, src2);
    else
	emit_bor(a, type, dst, src1, src2);
}

static void emit_vbori(ZAssembler &a, uint8_t type,
			int dst, int src, int8_t imm)
{
    x86::Xmm t1 = alloc_xmm(a);
    emit_vmovi(a, uint_type(type), regno(t1), imm);    
    emit_vbor(a, type, dst, src, regno(t1));
    release_xmm(a, t1);
}


static void emit_vbxor_sse2(ZAssembler &a, uint8_t type,
		       int dst, int src1, int src2)
{
    (void) type;
    int src = emit_one_vsrc(a, type, dst, src1, src2);        
    switch(type) {
    case FLOAT32:
	a.xorps(DST, SRC);
	break;
    case FLOAT64:
	a.xorpd(DST, SRC);
	break;
    default:
	a.pxor(DST, SRC);
	break;
    }
}

static void emit_vbxor_avx(ZAssembler &a, uint8_t type,
			  int dst, int src1, int src2)
{
    switch(type) {
    case FLOAT32:
	a.vxorps(DST, SRC1, SRC2);
	break;	
    case FLOAT64:
	a.vxorpd(DST, SRC1, SRC2);
	break;	
    default:
	a.vpxor(DST, SRC1, SRC2);
	break;
    }
}

static void emit_vbxor(ZAssembler &a, uint8_t type,
		      int dst, int src1, int src2)
{
    if (a.use_avx())
	emit_vbxor_avx(a, type, dst, src1, src2);
    else if (a.use_sse2())
	emit_vbxor_sse2(a, type, dst, src1, src2);
    else
	emit_bxor(a, type, dst, src1, src2);
}

static void emit_vbxori(ZAssembler &a, uint8_t type,
			int dst, int src, int8_t imm)
{
    x86::Xmm t1 = alloc_xmm(a);    
    emit_vmovi(a, uint_type(type), regno(t1), imm);
    emit_vbxor(a, type, dst, src, regno(t1));
    release_xmm(a, t1);
}


static void emit_vbnot(ZAssembler &a, uint8_t type, int dst, int src)
{
    x86::Xmm t1 = alloc_xmm(a);
    emit_vmov(a, type, dst, src);  // dst = src
    emit_vone(a, regno(t1));
    emit_vbxor(a, type, dst, dst, regno(t1));
    release_xmm(a, t1);
}

static void emit_vband_sse2(ZAssembler &a, uint8_t type,
			   int dst, int src1, int src2)
{
    int src = emit_one_vsrc(a, type, dst, src1, src2);
    switch(type) {
    case FLOAT32:
	a.andps(DST, SRC);
	break;	
    case FLOAT64:
	a.andpd(DST, SRC);
	break;
    default:
	a.pand(DST, SRC);
	break;
    }
}

static void emit_vband_avx(ZAssembler &a, uint8_t type,
			   int dst, int src1, int src2)
{
    switch(type) {
    case FLOAT32:
	a.vandps(DST, SRC1, SRC2);
	break;	
    case FLOAT64:
	a.vandpd(DST, SRC1, SRC2);
	break;
    default:
	a.vpand(DST, SRC1, SRC2);
	break;
    }    
}

static void emit_vband(ZAssembler &a, uint8_t type,
		      int dst, int src1, int src2)
{
    if (a.use_avx())
	emit_vband_avx(a, type, dst, src1, src2);
    else if (a.use_sse2())
	emit_vband_sse2(a, type, dst, src1, src2);
    else
	emit_band(a, type, dst, src1, src2);
}

static void emit_vbandi(ZAssembler &a, uint8_t type,
			int dst, int src, int8_t imm)
{
    x86::Xmm t1 = alloc_xmm(a);    
    emit_vmovi(a, uint_type(type), regno(t1), imm);
    emit_vband(a, type, dst, src, regno(t1));
    release_xmm(a, t1);
}

// dst = ~src1 & src2
static void emit_vbandn(ZAssembler &a, uint8_t type,
			int dst, int src1, int src2)
{
    x86::Xmm t1 = alloc_xmm(a);    
    emit_vbnot(a, type, regno(t1), src1);
    emit_vband(a, type, dst, regno(t1), src2);
    release_xmm(a, t1);    
}

// dst = ~src & imm
static void emit_vbandni(ZAssembler &a, uint8_t type,
			 int dst, int src, int8_t imm)
{
    x86::Xmm t1 = alloc_xmm(a);
    emit_vmovi(a, uint_type(type), regno(t1), imm);
    emit_vbandn(a, type, dst, src, regno(t1));
    release_xmm(a, t1);
}

// dst = dst == src
static void emit_vcmpeq1(ZAssembler &a, uint8_t type, int dst, int src)
{
    switch(type) {
    case INT8:
    case UINT8:   a.pcmpeqb(DST, SRC); break;
    case INT16:	    
    case UINT16:  a.pcmpeqw(DST, SRC); break;
    case INT32:	    
    case UINT32:  a.pcmpeqd(DST, SRC); break;
    case INT64:	    
    case UINT64:  a.pcmpeqq(DST, SRC); break;  // FIXME: check for SSE4_1 !!!
    case FLOAT32: a.cmpps(DST, SRC, CMP_EQ); break;
    case FLOAT64: a.cmppd(DST, SRC, CMP_EQ); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
}

// dst = dst != src
static void emit_vcmpne1(ZAssembler &a, uint8_t type, int dst, int src)
{
    switch(type) {
    case INT8:
    case UINT8:
	a.pcmpeqb(DST, SRC);
	emit_vbnot(a, type, dst, dst);
	break;
    case INT16:
    case UINT16:
	a.pcmpeqw(DST, SRC);
	emit_vbnot(a, type, dst, dst);
	break;
    case INT32:	    
    case UINT32:
	a.pcmpeqd(DST, SRC);
	emit_vbnot(a, type, dst, dst);	
	break;
    case INT64:	    
    case UINT64:
	a.pcmpeqq(DST, SRC);
	emit_vbnot(a, type, dst, dst);
	break;
    case FLOAT32: a.cmpps(DST, SRC, CMP_NEQ); break;
    case FLOAT64: a.cmppd(DST, SRC, CMP_NEQ); break;
    default: crash(__FILE__, __LINE__, type); break;
    }
    
}

// dst = dst > src
static void emit_vcmpgt1(ZAssembler &a, uint8_t type,
			 int dst, int src)
{
    switch(type) {
    case INT8:
    case UINT8:   a.pcmpgtb(DST, SRC); break;
    case INT16:
    case UINT16:  a.pcmpgtw(DST, SRC); break;
    case INT32:	    
    case UINT32:  a.pcmpgtd(DST, SRC); break;
    case INT64:	    
    case UINT64:  a.pcmpgtq(DST, SRC); break;
    case FLOAT32: a.cmpps(DST, SRC, CMP_GT); break;
    case FLOAT64: a.cmppd(DST, SRC, CMP_GT); break;	
    default: crash(__FILE__, __LINE__, type); break;
    }
}

// dst = dst < src | dst = src > dst |
static void emit_vcmplt1(ZAssembler &a, uint8_t type,
			 int dst, int src)
{
    switch(type) {
    case INT8:
    case UINT8: {
	x86::Xmm t1 = alloc_xmm(a);
	emit_vmov(a, type, regno(t1), dst);
	emit_vmov(a, type, dst, src);
	a.pcmpgtb(DST, t1);  // SSE2
	release_xmm(a, t1);	
	break;
    }
    case INT16:
    case UINT16: {
	x86::Xmm t1 = alloc_xmm(a);
	emit_vmov(a, type, regno(t1), dst);
	emit_vmov(a, type, dst, src);	
	a.pcmpgtw(DST, t1);  // SSE2
	release_xmm(a, t1);	
	break;
    }
    case INT32:	    
    case UINT32: {
	x86::Xmm t1 = alloc_xmm(a);
	emit_vmov(a, type, regno(t1), dst);
	emit_vmov(a, type, dst, src);	
	a.pcmpgtd(DST, t1); // SSE2
	release_xmm(a, t1);		
	break;
    }
    case INT64:	    
    case UINT64: {
	x86::Xmm t1 = alloc_xmm(a);
	emit_vmov(a, type, regno(t1), dst);
	emit_vmov(a, type, dst, src);
	a.pcmpgtq(DST, t1); // SSE4.2 (fixme!, implement when not sse4.2)
	release_xmm(a, t1);
	break;
    }
    case FLOAT32: a.cmpps(DST, SRC, CMP_LT); return;
    case FLOAT64: a.cmppd(DST, SRC, CMP_LT); return;
    default: crash(__FILE__, __LINE__, type); break;
    }
}

// dst = dst <= src // dst = !(dst > src) = !(src < dst)
static void emit_vcmple1(ZAssembler &a, uint8_t type,
			 int dst, int src)
{
    // assert dst != src
    switch(type) {
    case INT8:
    case UINT8: {
	x86::Xmm t1 = alloc_xmm(a);	
	emit_vmov(a, type, regno(t1), dst);
	a.pcmpgtb(t1, SRC);
	emit_vbnot(a, type, dst, regno(t1));
	release_xmm(a, t1);	
	break;
    }
    case INT16:
    case UINT16: {
	x86::Xmm t1 = alloc_xmm(a);
	emit_vmov(a, type, regno(t1), dst);
	a.pcmpgtw(t1, SRC);
	emit_vbnot(a, type, dst, regno(t1));
	release_xmm(a, t1);
	break;
    }
    case INT32:	    
    case UINT32: {
	x86::Xmm t1 = alloc_xmm(a);
	emit_vmov(a, type, regno(t1), dst);
	a.pcmpgtd(t1, SRC);
	emit_vbnot(a, type, dst, regno(t1));
	release_xmm(a, t1);	
	break;
    }
    case INT64:	    
    case UINT64: {
	x86::Xmm t1 = alloc_xmm(a);
	emit_vmov(a, type, regno(t1), dst);
	a.pcmpgtq(t1, SRC);
	emit_vbnot(a, type, dst, regno(t1));
	release_xmm(a, t1);
	break;
    }
    case FLOAT32: a.cmpps(DST, SRC, CMP_LE); break;
    case FLOAT64: a.cmppd(DST, SRC, CMP_LE); break;	
    default: crash(__FILE__, __LINE__, type); break;
    }
}

static void emit_vcmpeq(ZAssembler &a, uint8_t type,
			int dst, int src1, int src2)
{
    int src = emit_one_vsrc(a, type, dst, src1, src2);
    if (src1 == src2)
	emit_vone(a, dst);
    else
	emit_vcmpeq1(a, type, dst, src);
}

static void emit_vcmpeqi(ZAssembler &a, uint8_t type,
			 int dst, int src, int8_t imm)
{
    x86::Xmm t0 = alloc_xmm(a);    
    emit_vmovi(a, type, regno(t0), imm);
    emit_vcmpeq(a, type, dst, src, regno(t0));
    release_xmm(a, t0);
}

static void emit_vcmpne(ZAssembler &a, uint8_t type,
			int dst, int src1, int src2)
{
    int src = emit_one_vsrc(a, type, dst, src1, src2);
    if (src1 == src2)
	emit_vzero(a, dst);
    else
	emit_vcmpne1(a, type, dst, src);
}

static void emit_vcmpnei(ZAssembler &a, uint8_t type,
			 int dst, int src, int8_t imm)
{
    x86::Xmm t0 = alloc_xmm(a);    
    emit_vmovi(a, type, regno(t0), imm);
    emit_vcmpne(a, type, dst, src, regno(t0));
    release_xmm(a, t0);    
}

// emit dst = src1 > src2
static void emit_vcmpgt(ZAssembler &a, uint8_t type,
			int dst, int src1, int src2)
{
    if ((dst == src1) && (dst == src2)) { // dst = dst > dst
	emit_vzero(a, dst);
	return;
    }
    else if (src1 == dst) { // DST = DST > SRC2
	emit_vcmpgt1(a, type, dst, src2);
    }
    else if (src2 == dst) {  // DST = SRC1 > DST => DST = DST < SRC1
	emit_vcmplt1(a, type, dst, src1);
	return;
    }
    else {
	// DST = SRC1 > SRC2 : DST=SRC1,  DST = DST > SRC2
	emit_vmov(a, type, dst, src1);
	emit_vcmpgt1(a, type, dst, src2);
    }
}

static void emit_vcmpgti(ZAssembler &a, uint8_t type,
			 int dst, int src, int8_t imm)
{
    x86::Xmm t1 = alloc_xmm(a);    
    emit_vmovi(a, type, regno(t1), imm);
    emit_vcmpgt(a, type, dst, src, regno(t1));
    release_xmm(a, t1);    
}

// emit dst = src1 >= src2
static void emit_vcmpge(ZAssembler &a, uint8_t type,
			int dst, int src1, int src2)
{
    int src;

    if (IS_FLOAT_TYPE(type)) {
	int cmp = CMP_GE;
	if ((dst == src1) && (dst == src2)) { // dst = dst >= dst (TRUE!)
	    emit_vone(a, dst);
	    return;
	}
	else if (src1 == dst)   // dst = dst >= src2
	    src = src2;
	else if (src2 == dst) { // dst = src1 >= dst => dst = dst <= src1
	    cmp = CMP_LE;
	    src = src1;
	}
	else {
	    emit_vmov(a, type, dst, src1); // dst = (src1 >= src2)
	    src = src2;
	}
	if (type == FLOAT32)
	    a.cmpps(DST, SRC, cmp);
	else 
	    a.cmppd(DST, SRC, cmp);
    }
    else {
	if ((dst == src1) && (dst == src2)) {
            // DST = DST >= DST (TRUE)
	    emit_vone(a, dst);
	}
	else if (src1 == dst) { 
            // DST = DST >= SRC2 => DST == !(DST < SRC2)
	    emit_vcmplt1(a, type, dst, src2);
	    emit_vbnot(a, type, dst, dst);
	}
	else if (src2 == dst) {
	    // DST = SRC1 >= DST; DST = (DST <= SRC1)
	    emit_vcmple1(a, type, dst, src1);
	}
	else {
	    // DST = (SRC1 >= SRC2); DST = !(SRC1 < SRC2); DST = !(SRC2 > SRC1)
	    emit_vmov(a, type, dst, src2);     // DST = SRC2
	    emit_vcmpgt1(a, type, dst, src1);  // DST = (SRC2 > SRC1)
	    emit_vbnot(a, type, dst, dst);     // DST = !(SRC2 > SRC1)
	}
    }
}

static void emit_vcmpgei(ZAssembler &a, uint8_t type,
			 int dst, int src, int8_t imm)
{
    x86::Xmm t0 = alloc_xmm(a);    
    emit_vmovi(a, type, regno(t0), imm);
    emit_vcmpge(a, type, dst, src, regno(t0));
    release_xmm(a, t0);
}


static void emit_vcmplt(ZAssembler &a, uint8_t type,
			int dst, int src1, int src2)
{
    emit_vcmpgt(a, type, dst, src2, src1);
}

static void emit_vcmplti(ZAssembler &a, uint8_t type,
			 int dst, int src, int8_t imm)
{
    x86::Xmm t0 = alloc_xmm(a);    
    emit_vmovi(a, type, regno(t0), imm);
    emit_vcmplt(a, type, dst, src, regno(t0));
    release_xmm(a, t0);    
}

static void emit_vcmple(ZAssembler &a, uint8_t type,
			int dst, int src1, int src2)
{
    emit_vcmpge(a, type, dst, src2, src1);
}

static void emit_vcmplei(ZAssembler &a, uint8_t type,
			 int dst, int src, int8_t imm)
{
    x86::Xmm t0 = alloc_xmm(a);        
    emit_vmovi(a, type, regno(t0), imm);
    emit_vcmple(a, type, dst, src, regno(t0));
    release_xmm(a, t0);        
}

// Helper function to generate instructions based on type and operation
void emit_instruction(ZAssembler &a, instr_t* p, uint32_t reg_mask, x86::Gp rfp)
{
    int i;

    a.reg_alloc_reset();  // reset allocation for every instruction
    
    switch(p->op) {
    case OP_NOP: a.nop(); break;
    case OP_VNOP: a.nop(); break;
    case OP_RET:
	i = p->rd;
	if (reg_mask & (1 << (i+16))) {
	    int offs = offsetof(vregfile_t, r) + i*sizeof(scalar0_t);
	    a.mov(x86::ptr(rfp, offs), reg(i));
	}	
	break;
    case OP_VRET:
	i = p->rd;
	if (reg_mask & (1 << i)) {
	    int offs = offsetof(vregfile_t, v) + i*sizeof(vector_t);  
	    a.movdqu(x86::ptr(rfp, offs), xreg(i));
	}
	break;
    case OP_MOV: emit_movr(a, p->type, p->rd, p->ri); break;
    case OP_MOVI: emit_movi(a, p->type, p->rd, p->imm12); break;
    case OP_VMOV: emit_vmov(a, p->type, p->rd, p->ri); break;
    case OP_VMOVI: emit_vmovi(a, p->type, p->rd, p->imm12); break;
	
    case OP_NEG: emit_neg(a, p->type, p->rd,p->ri); break;
    case OP_VNEG: emit_vneg(a, p->type, p->rd, p->ri); break;

    case OP_BNOT: emit_bnot(a, p->type, p->rd, p->ri); break;
    case OP_VBNOT: emit_vbnot(a, p->type, p->rd, p->ri); break;		
	
    case OP_ADD: emit_add(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_ADDI: emit_addi(a, p->type, p->rd, p->ri, p->imm8); break;
    case OP_VADD: emit_vadd(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_VADDI: emit_vaddi(a, p->type, p->rd, p->ri, p->imm8); break;
	
    case OP_SUB: emit_sub(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_SUBI: emit_subi(a, p->type, p->rd, p->ri, p->imm8); break;
    case OP_VSUB: emit_vsub(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_VSUBI: emit_vsubi(a, p->type, p->rd, p->ri, p->imm8); break;

    case OP_RSUB: emit_rsub(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_RSUBI: emit_rsubi(a, p->type, p->rd, p->ri, p->imm8); break;
    case OP_VRSUB: emit_vrsub(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_VRSUBI: emit_vrsubi(a, p->type, p->rd, p->ri, p->imm8); break;	
	
    case OP_MUL: emit_mul(a, p->type, p->rd, p->ri, p->rj); break;	
    case OP_MULI: emit_muli(a, p->type, p->rd, p->ri, p->imm8); break;
    case OP_VMUL: emit_vmul(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_VMULI: emit_vmuli(a, p->type, p->rd, p->ri, p->imm8); break;	
	
    case OP_SLL: emit_sll(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_SLLI: emit_slli(a, p->type, p->rd, p->ri, p->imm8); break;
    case OP_VSLL: emit_vsll(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_VSLLI: emit_vslli(a, p->type, p->rd, p->ri, p->imm8); break;
	
    case OP_SRL: emit_srl(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_SRLI: emit_srli(a, p->type, p->rd, p->ri, p->imm8); break;
    case OP_VSRL: emit_vsrl(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_VSRLI: emit_vsrli(a, p->type, p->rd, p->ri, p->imm8); break;
	
    case OP_SRA: emit_sra(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_SRAI: emit_srai(a, p->type, p->rd, p->ri, p->imm8); break;
    case OP_VSRA: emit_vsra(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_VSRAI: emit_vsrai(a, p->type, p->rd, p->ri, p->imm8); break;
	
    case OP_BAND: emit_band(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_BANDI: emit_bandi(a, p->type, p->rd, p->ri, p->imm8); break;	
    case OP_VBAND: emit_vband(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_VBANDI: emit_vbandi(a, p->type, p->rd, p->ri, p->imm8); break;

    case OP_BANDN: emit_bandn(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_BANDNI: emit_bandni(a, p->type, p->rd, p->ri, p->imm8); break;	
    case OP_VBANDN: emit_vbandn(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_VBANDNI: emit_vbandni(a, p->type, p->rd, p->ri, p->imm8); break;	
	
    case OP_BOR:  emit_bor(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_BORI: emit_bori(a, p->type, p->rd, p->ri, p->imm8); break;		
    case OP_VBOR: emit_vbor(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_VBORI: emit_vbori(a, p->type, p->rd, p->ri, p->imm8); break;

    case OP_BXOR: emit_bxor(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_BXORI: emit_bxori(a, p->type, p->rd, p->ri, p->imm8); break;	
    case OP_VBXOR: emit_vbxor(a,p->type,p->rd,p->ri,p->rj); break;
    case OP_VBXORI: emit_vbxori(a,p->type,p->rd,p->ri,p->imm8); break;	
	
    case OP_CMPLT: emit_cmplt(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_CMPLTI: emit_cmplti(a, p->type, p->rd, p->ri, p->imm8); break;	
    case OP_VCMPLT: emit_vcmplt(a,p->type,p->rd,p->ri,p->rj); break;
    case OP_VCMPLTI: emit_vcmplti(a,p->type,p->rd,p->ri,p->imm8); break;	
	
    case OP_CMPLE: emit_cmple(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_CMPLEI: emit_cmplei(a, p->type, p->rd, p->ri, p->imm8); break;	
    case OP_VCMPLE: emit_vcmple(a,p->type,p->rd,p->ri,p->rj); break;
    case OP_VCMPLEI: emit_vcmplei(a,p->type,p->rd,p->ri,p->imm8); break;	
	
    case OP_CMPEQ: emit_cmpeq(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_CMPEQI: emit_cmpeqi(a, p->type, p->rd, p->ri, p->imm8); break;
    case OP_VCMPEQ: emit_vcmpeq(a,p->type,p->rd,p->ri,p->rj); break;
    case OP_VCMPEQI: emit_vcmpeqi(a,p->type,p->rd,p->ri,p->imm8); break;
	
    case OP_CMPGT: emit_cmpgt(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_CMPGTI: emit_cmpgti(a, p->type, p->rd, p->ri, p->imm8); break;	
    case OP_VCMPGT: emit_vcmpgt(a,p->type,p->rd,p->ri,p->rj); break;
    case OP_VCMPGTI: emit_vcmpgti(a,p->type,p->rd,p->ri,p->imm8); break;	

    case OP_CMPGE: emit_cmpge(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_CMPGEI: emit_cmpgei(a, p->type, p->rd, p->ri, p->imm8); break;	
    case OP_VCMPGE: emit_vcmpge(a,p->type,p->rd,p->ri,p->rj); break;
    case OP_VCMPGEI: emit_vcmpgei(a,p->type,p->rd,p->ri,p->imm8); break;
	
    case OP_CMPNE:  emit_cmpne(a, p->type, p->rd, p->ri, p->rj); break;
    case OP_CMPNEI: emit_cmpnei(a, p->type, p->rd, p->ri, p->imm8); break;
    case OP_VCMPNE: emit_vcmpne(a,p->type,p->rd,p->ri,p->rj); break;
    case OP_VCMPNEI: emit_vcmpnei(a, p->type, p->rd, p->ri, p->imm8); break;	
	
    default: crash(__FILE__, __LINE__, p->type); break;
    }
}

void add_dirty_regs(ZAssembler &a, instr_t* code, size_t n)
{
    while (n--) {
	if ((code->op == OP_JMP) ||
	    (code->op == OP_NOP) ||
	    (code->op == OP_JNZ) ||
	    (code->op == OP_JZ)) {
	}
	if (code->op & OP_VEC) {
	    a.add_dirty_reg(xreg(code->rd));
	}
	else {
	    a.add_dirty_reg(reg(code->rd));
	}
	code++;
    }
}

//
//  save_ptr points to 512 bytes (128bit aligned ) memory area
//   that can hold data fro fxsave64
//  reg_mask: (16 gp registers | 16 xmm registers)
//  reg_data: (128bit aligned)
//            xmm0
//            xmm1
//            xmm2
//           
//
void assemble(ZAssembler &a, const Environment &env,
	      uint32_t reg_mask,
	      x86::Mem save_ptr,
	      instr_t* code, size_t n)
{
    FuncDetail func;
    FuncFrame frame;
    x86::Gp rfp = a.zdi();
    int i;
    
    func.init(FuncSignatureT<void*, void*>(CallConvId::kHost), env);
    frame.init(func);
    a.set_func_frame(&frame);
    frame.addDirtyRegs(rfp);
    // FIXME: how do we get this info before generating code?
    for (i = 0; i < 16; i++) {
	if (R_FREE_MASK & (1 << i))
	    frame.addDirtyRegs(reg(i));
    }
    
    add_dirty_regs(a, code, n);

    FuncArgsAssignment args(&func);   // Create arguments assignment context.
    args.assignAll(rfp);             // Assign our registers to arguments.
    args.updateFuncFrame(frame);      // Reflect our args in FuncFrame.
    frame.finalize();                 // Finalize the FuncFrame (updates it).

    a.emitProlog(frame);              // Emit function prolog.
    a.emitArgsAssignment(frame, args);// Assign arguments to registers.

    // load vector registers
    for (i = 0; i < 16; i++) {
	if (reg_mask & (1 << i)) {
	    int offs = offsetof(vregfile_t, v) + i*sizeof(vector_t);  
	    a.movdqu(xreg(i), x86::ptr(rfp, offs));
	}
    }

    // load gp registers reversed
    for (i = 15; i >= 0; i--) {
	if (reg_mask & (1 << (i+16))) {
	    int offs = offsetof(vregfile_t, r) + i*sizeof(scalar0_t);
	    a.mov(reg(i), x86::ptr(rfp, offs));
	}
    }
    
    // Setup all labels
    Label lbl[n];    // potential landing positions

    for (i = 0; i < (int) n; i++)
	lbl[i].reset();

    for (i = 0; i < (int) n; i++) {
	if ((code[i].op == OP_JMP) ||
	    (code[i].op == OP_JZ) ||
	    (code[i].op == OP_JNZ)) {
	    int j = (i+1)+code[i].imm12;
	    if (lbl[j].id() == Globals::kInvalidId) //?
		lbl[j] = a.newLabel();
	}
    }
    
    // assemble all code
    for (i = 0; i < (int)n; i++) {
	if (lbl[i].id() != Globals::kInvalidId)
	    a.bind(lbl[i]);
	if (code[i].op == OP_JMP) {
	    int j = (i+1)+code[i].imm12;
	    a.jmp(lbl[j]);
	}
	else if (code[i].op == OP_JNZ) {
	    int j = (i+1)+code[i].imm12;
	    switch(code[i].type) {
	    case INT8:
	    case UINT8:	a.cmp(reg(code[i].rd).r8(), 0); break;
	    case INT16:
	    case UINT16: a.cmp(reg(code[i].rd).r16(), 0); break;
	    case INT32:	    
	    case UINT32: a.cmp(reg(code[i].rd).r32(), 0); break;
	    case INT64:	    
	    case UINT64: a.cmp(reg(code[i].rd).r64(), 0); break;
#ifdef FIXME
	    case FLOAT32:
		a.cmpps(reg(code[i].rd),0.0); break;
	    case FLOAT64:
		a.cmppd(reg(code[i].rd), 0.0); break;
#endif
	    default: crash(__FILE__, __LINE__, code[i].type); break;
	    }
	    a.jnz(lbl[j]);
	}
	else if (code[i].op == OP_JZ) {
	    int j = (i+1)+code[i].imm12;
	    switch(code[i].type) {
	    case INT8:
	    case UINT8:	a.cmp(reg(code[i].rd).r8(), 0); break;
	    case INT16:
	    case UINT16: a.cmp(reg(code[i].rd).r16(), 0); break;
	    case INT32:	    
	    case UINT32: a.cmp(reg(code[i].rd).r32(), 0); break;
	    case INT64:	    
	    case UINT64: a.cmp(reg(code[i].rd).r64(), 0); break;
#ifdef FIXME
	    case FLOAT32:
		a.cmpps(reg(code[i].rd),0.0); break;
	    case FLOAT64:
		a.cmppd(reg(code[i].rd), 0.0); break;
#endif
	    default: crash(__FILE__, __LINE__, code[i].type); break;
	    }
	    a.jz(lbl[j]);
	}	
	else {
	    emit_instruction(a, &code[i], reg_mask, rfp);
	}
    }
    // dump register so we can have a look
    if (a.cpuFeatures().x86().hasFXSR()) {
	// fprintf(stderr, "has fxsave\n");
	Error err;
	err = a.rex_w().fxsave64(save_ptr);
	if (err) {
	    fprintf(stderr, "a.fxsave64 ERROR\n");
	}
    }
    a.lea(x86::regs::rax, save_ptr);
    a.emitEpilog(frame);              // Emit function epilog and return.
}
