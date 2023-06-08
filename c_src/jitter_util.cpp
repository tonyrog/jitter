#include <stdio.h>
#include "jitter_types.h"
#include "jitter.h"


const char* asm_opname(uint8_t op)
{
    switch(op) {
    case OP_NOP:   return "nop";
    case OP_JMP:   return "jmp";
    case OP_JNZ:   return "jnz";
    case OP_JZ:    return "jz";		
    case OP_RET:   return "ret";
    case OP_NEG:   return "neg";
    case OP_BNOT:  return "bnot";
    case OP_INV:   return "inv";	

    case OP_MOV:   return "mov";
    case OP_MOVI:  return "movi";
	
    case OP_ADD:   return "add";
    case OP_ADDI:  return "addi";
    case OP_VADD:  return "vadd";	
    case OP_VADDI:  return "vaddi";
	
    case OP_SUB:   return "sub";
    case OP_SUBI:  return "subi";
    case OP_VSUB:  return "vsub";	
    case OP_VSUBI:  return "vsubi";

    case OP_RSUB:   return "rsub";
    case OP_RSUBI:  return "rsubi";
    case OP_VRSUB:  return "vrsub";	
    case OP_VRSUBI:  return "vrsubi";		

    case OP_MUL:   return "mul";
    case OP_MULI:  return "muli";
    case OP_VMUL:  return "vmul";
    case OP_VMULI:  return "vmuli";
	
    case OP_SLL:   return "sll";
    case OP_SLLI:  return "slli";
    case OP_VSLL:  return "vsll";	
    case OP_VSLLI:  return "vslli";

    case OP_SRL:   return "srl";	
    case OP_SRLI:  return "srli";
    case OP_VSRL:  return "vsrl";	
    case OP_VSRLI:  return "vsrli";

    case OP_SRA:   return "sra";
    case OP_SRAI:  return "srai";
    case OP_VSRA:   return "vsra";	
    case OP_VSRAI:  return "vsrai";

    case OP_BAND:  return "band";
    case OP_BANDI:  return "bandi";	
    case OP_VBAND:  return "vband";
    case OP_VBANDI:  return "vbandi";

    case OP_BANDN:  return "bandn";
    case OP_BANDNI:  return "bandni";	
    case OP_VBANDN:  return "vbandn";
    case OP_VBANDNI:  return "vbandni";
	
    case OP_BOR:   return "bor";
    case OP_BORI:   return "bori";
    case OP_VBOR:  return "vbor";
    case OP_VBORI:  return "vbori";
	
    case OP_BXOR:  return "bxor";
    case OP_BXORI:  return "bxori";	
    case OP_VBXOR:  return "vbxor";
    case OP_VBXORI:  return "vbxori";
	
    case OP_CMPLT: return "cmplt";
    case OP_CMPLTI: return "cmplti";
    case OP_VCMPLT:  return "vcmplt";
    case OP_VCMPLTI:  return "vcmplti";
	
    case OP_CMPLE: return "cmple";
    case OP_CMPLEI: return "cmplei";
    case OP_VCMPLE:  return "vcmple";
    case OP_VCMPLEI:  return "vcmplei";

    case OP_CMPEQ: return "cmpeq";	
    case OP_CMPEQI: return "cmpeqi";
    case OP_VCMPEQ:  return "vcmpeq";
    case OP_VCMPEQI:  return "vcmpeqi";

    case OP_CMPGT: return "cmpgt";	
    case OP_CMPGTI: return "cmpgti";
    case OP_VCMPGT:  return "vcmpgt";
    case OP_VCMPGTI:  return "vcmpgti";	
	
    case OP_CMPGE: return "cmpge";
    case OP_CMPGEI: return "cmpgei";
    case OP_VCMPGE:  return "vcmpge";
    case OP_VCMPGEI:  return "vcmpgei";
	
    case OP_CMPNE: return "cmpne";
    case OP_CMPNEI: return "cmpnei";	
    case OP_VCMPNE:  return "vcmpne";
    case OP_VCMPNEI:  return "vcmpnei";		

    case OP_VRET:  return "vret";
    case OP_VMOV:  return "vmov";
    case OP_VMOVI:  return "vmovi";	
    case OP_VNEG:  return "vneg";	
    case OP_VBNOT:  return "vbnot";
    case OP_VINV:  return "vinv";

    default: return "?????";
    }
}

const char* asm_typename(uint8_t type)
{
    switch(type) {
    case UINT8:  return "u8"; 
    case UINT16: return "u16"; 
    case UINT32: return "u32"; 
    case UINT64: return "u64"; 
    case INT8:   return "i8"; 
    case INT16:  return "i16"; 
    case INT32:  return "i32";  
    case INT64:  return "i64";
    case FLOAT8: return "f8"; 	
    case FLOAT16: return "f16"; 
    case FLOAT32: return "f32"; 
    case FLOAT64: return "f64";
    default: return "??";
    }
}

static const char* asm_regname(uint8_t op, uint8_t r)
{
    if (op & OP_VEC) {
	switch(r) {
	case 0: return "v0";
	case 1: return "v1";
	case 2: return "v2";
	case 3: return "v3";
	case 4: return "v4";
	case 5: return "v5";
	case 6: return "v6";
	case 7: return "v7";
	case 8: return "v8";
	case 9: return "v9";
	case 10: return "v10";
	case 11: return "v11";
	case 12: return "v12";
	case 13: return "v13";
	case 14: return "v14";
	case 15: return "v15";
	default: return "v?";
	}
    }
    else {
	switch(r) {
	case 0: return "r0";
	case 1: return "r1";
	case 2: return "r2";
	case 3: return "r3";
	case 4: return "r4";
	case 5: return "r5";
	case 6: return "r6";
	case 7: return "r7";
	case 8: return "r8";
	case 9: return "r9";
	case 10: return "r10";
	case 11: return "r11";
	case 12: return "r12";
	case 13: return "r13";
	case 14: return "r14";
	case 15: return "r15";
	default: return "r?";
	}
    }
}

void print_instr(FILE* f,instr_t* pc)
{
    if (pc->op == OP_JMP) {
	fprintf(f, "%s %d", asm_opname(pc->op), pc->imm12);
    }
    else if ((pc->op == OP_JNZ) || (pc->op == OP_JZ)) {
	fprintf(f, "%s.%s %s, %d",
		asm_opname(pc->op),
		asm_typename(pc->type),
		asm_regname(pc->op,pc->rd), pc->imm12);
    }
    else if (pc->op & OP_BIN) {
	if (pc->op & OP_IMM) {
	    fprintf(f, "%s.%s, %s, %s, %d",
		    asm_opname(pc->op),
		    asm_typename(pc->type),
		    asm_regname(pc->op,pc->rd),
		    asm_regname(pc->op,pc->ri),
		    pc->imm8);
	}
	else {
	    fprintf(f, "%s.%s, %s, %s, %s",
		    asm_opname(pc->op),
		    asm_typename(pc->type),
		    asm_regname(pc->op,pc->rd),
		    asm_regname(pc->op,pc->ri),
		    asm_regname(pc->op,pc->rj));
	}
    }
    else if ((pc->op == OP_MOVI)||(pc->op == OP_VMOVI)) {
	fprintf(f, "%s.%s %s, %d",
		asm_opname(pc->op),
		asm_typename(pc->type),
		asm_regname(pc->op,pc->rd), pc->imm12);
    }
    else {
	if (pc->op & OP_IMM) {
	    fprintf(f, "%s.%s, %s, %d",
		    asm_opname(pc->op),
		    asm_typename(pc->type),
		    asm_regname(pc->op,pc->rd),
		    pc->imm8);
	}
	else if ((pc->op & OP_MASK) == OP_NOP) {
	    fprintf(f, "%s", asm_opname(OP_NOP));
	}
	else if ((pc->op & OP_MASK) == OP_RET) {
	    fprintf(f, "%s.%s %s",
		    asm_opname(pc->op),
		    asm_typename(pc->type),
		    asm_regname(pc->op,pc->rd));
	}
	else {    
	    fprintf(f, "%s.%s, %s, %s",
		    asm_opname(pc->op),
		    asm_typename(pc->type),
		    asm_regname(pc->op,pc->rd),
		    asm_regname(pc->op,pc->ri));
	}
    }
}

void print_code(FILE* f, instr_t* code, size_t len)
{
    int i;

    for (i = 0; i < (int)len; i++) {
	print_instr(f, &code[i]);
	fprintf(f, "\n");
    }
}

void set_vuint8(vuint8_t &r, int i, uint8_t v) { r[i] = v; }
void set_vuint16(vuint16_t &r, int i, uint16_t v) { r[i] = v; }
void set_vuint32(vuint32_t &r, int i, uint32_t v) { r[i] = v; }
void set_vuint64(vuint64_t &r, int i, uint64_t v) { r[i] = v; }
void set_vint8(vint8_t &r, int i, int8_t v) { r[i] = v; }
void set_vint16(vint16_t &r, int i, int16_t v) { r[i] = v; }
void set_vint32(vint32_t &r, int i, int32_t v) { r[i] = v; }
void set_vint64(vint64_t &r, int i, int64_t v) { r[i] = v; }
void set_vfloat32(vfloat32_t &r, int i, float32_t v) { r[i] = v; }
void set_vfloat64(vfloat64_t &r, int i, float64_t v) { r[i] = v; }

void set_element_uint64(jitter_type_t type, vector_t &r, int i, uint64_t v)
{
    switch(type) {
    case UINT8: set_vuint8((vuint8_t&)r, i, (uint8_t) v); break;
    case UINT16: set_vuint16((vuint16_t&)r, i, (uint16_t) v); break;
    case UINT32: set_vuint32((vuint32_t&)r, i, (uint32_t) v); break;
    case UINT64: set_vuint64((vuint64_t&)r, i, (uint64_t) v); break;
    case INT8: set_vint8((vint8_t&)r, i, (int8_t) v); break;
    case INT16: set_vint16((vint16_t&)r, i, (int16_t) v); break;
    case INT32: set_vint32((vint32_t&)r, i, (int32_t) v); break;
    case INT64: set_vint64((vint64_t&)r, i, (int64_t) v); break;
    case FLOAT32: set_vfloat32((vfloat32_t&)r, i, (float32_t) v); break;
    case FLOAT64: set_vfloat64((vfloat64_t&)r, i, (float64_t) v); break;
    default: break;
    }
}

void set_element_int64(jitter_type_t type, vector_t &r, int i, int64_t v)
{
    switch(type) {
    case UINT8:   set_vuint8((vuint8_t&)r,   i, (uint8_t) v); break;
    case UINT16:  set_vuint16((vuint16_t&)r, i, (uint16_t) v); break;
    case UINT32:  set_vuint32((vuint32_t&)r, i, (uint32_t) v); break;
    case UINT64:  set_vuint64((vuint64_t&)r, i, (uint64_t) v); break;
    case INT8:    set_vint8((vint8_t&)r,     i, (int8_t) v); break;
    case INT16:   set_vint16((vint16_t&)r,   i, (int16_t) v); break;
    case INT32:   set_vint32((vint32_t&)r,   i, (int32_t) v); break;
    case INT64:   set_vint64((vint64_t&)r,   i, (int64_t) v); break;
    case FLOAT32: set_vfloat32((vfloat32_t&)r, i, (float32_t) v); break;
    case FLOAT64: set_vfloat64((vfloat64_t&)r, i, (float64_t) v); break;
    default: break;
    }
}

void set_element_float64(jitter_type_t type, vector_t &r, int i, float64_t v)
{
    switch(type) {
    case UINT8: set_vuint8((vuint8_t&)r, i, (uint8_t) v); break;
    case UINT16: set_vuint16((vuint16_t&)r, i, (uint16_t) v); break;
    case UINT32: set_vuint32((vuint32_t&)r, i, (uint32_t) v); break;
    case UINT64: set_vuint64((vuint64_t&)r, i, (uint64_t) v); break;
    case INT8: set_vint8((vint8_t&)r, i, (int8_t) v); break;
    case INT16: set_vint16((vint16_t&)r, i, (int16_t) v); break;
    case INT32: set_vint32((vint32_t&)r, i, (int32_t) v); break;
    case INT64: set_vint64((vint64_t&)r, i, (int64_t) v); break;
    case FLOAT32: set_vfloat32((vfloat32_t&)r, i, (float32_t) v); break;
    case FLOAT64: set_vfloat64((vfloat64_t&)r, i, (float64_t) v); break;
    default: break;
    }
}

uint8_t   get_vuint8(vuint8_t r, int i)     { return r[i]; }
uint16_t  get_vuint16(vuint16_t r, int i)   { return r[i]; }
uint32_t  get_vuint32(vuint32_t r, int i)   { return r[i]; }
uint64_t  get_vuint64(vuint64_t r, int i)   { return r[i]; }
int8_t    get_vint8(vint8_t r, int i)       { return r[i]; }
int16_t   get_vint16(vint16_t r, int i)     { return r[i]; }
int32_t   get_vint32(vint32_t r, int i)     { return r[i]; }
int64_t   get_vint64(vint64_t r, int i)     { return r[i]; }
float32_t get_vfloat32(vfloat32_t r, int i) { return r[i]; }
float64_t get_vfloat64(vfloat64_t r, int i) { return r[i]; }

int64_t get_element_int64(jitter_type_t type, vector_t r, int i)
{
    switch(type) {
    case UINT8: return   get_vuint8((vuint8_t)r, i); 
    case UINT16: return  get_vuint16((vuint16_t)r, i);
    case UINT32: return  get_vuint32((vuint32_t)r, i);
    case UINT64: return  get_vuint64((vuint64_t)r, i);
    case INT8: return    get_vint8((vint8_t)r, i);
    case INT16: return   get_vint16((vint16_t)r, i);
    case INT32: return   get_vint32((vint32_t)r, i);
    case INT64: return   get_vint64((vint64_t)r, i);
    case FLOAT32: return get_vfloat32((vfloat32_t)r, i);
    case FLOAT64: return get_vfloat64((vfloat64_t)r, i);
    default: return 0;
    }
}

uint64_t get_element_uint64(jitter_type_t type, vector_t r, int i)
{
    switch(type) {
    case UINT8: return get_vuint8((vuint8_t)r, i); 
    case UINT16: return get_vuint16((vuint16_t)r, i);
    case UINT32: return  get_vuint32((vuint32_t)r, i);
    case UINT64: return get_vuint64((vuint64_t)r, i);
    case INT8: return get_vint8((vint8_t)r, i);
    case INT16: return get_vint16((vint16_t)r, i);
    case INT32: return get_vint32((vint32_t)r, i);
    case INT64: return get_vint64((vint64_t)r, i);
    case FLOAT32: return get_vfloat32((vfloat32_t)r, i);
    case FLOAT64: return get_vfloat64((vfloat64_t)r, i);
    default: return 0;	
    }
}

float64_t get_elemen_float64(jitter_type_t type, vector_t r, int i)
{
    switch(type) {
    case UINT8: return get_vuint8((vuint8_t)r, i); 
    case UINT16: return get_vuint16((vuint16_t)r, i);
    case UINT32: return  get_vuint32((vuint32_t)r, i);
    case UINT64: return get_vuint64((vuint64_t)r, i);
    case INT8: return get_vint8((vint8_t)r, i);
    case INT16: return get_vint16((vint16_t)r, i);
    case INT32: return get_vint32((vint32_t)r, i);
    case INT64: return get_vint64((vint64_t)r, i);
    case FLOAT32: return get_vfloat32((vfloat32_t)r, i);
    case FLOAT64: return get_vfloat64((vfloat64_t)r, i);
    default: return 0;
    }
}

void print_vuint8(FILE* f,vuint8_t r)
{
    fprintf(f,"{0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x}",
	   r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7],
	   r[8], r[9], r[10], r[11], r[12], r[13], r[14], r[15]);
}

void print_vuint16(FILE* f,vuint16_t r)
{
    fprintf(f,"{0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x}",
	   r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
}

void print_vuint32(FILE* f,vuint32_t r)
{
    fprintf(f,"{0x%x,0x%x,0x%x,0x%x}",
	   r[0], r[1], r[2], r[3]);
}

void print_vuint64(FILE* f,vuint64_t r)
{
    fprintf(f,"{0x%lx,0x%lx}",
	   r[0], r[1]);
}

void print_vint8(FILE* f,vint8_t r)
{
    fprintf(f,"{%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d}",
	   r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7],
	   r[8], r[9], r[10], r[11], r[12], r[13], r[14], r[15]);
}

void print_vint16(FILE* f,vint16_t r)
{
    fprintf(f,"{%d,%d,%d,%d,%d,%d,%d,%d}",
	   r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
}

void print_vint32(FILE* f,vint32_t r)
{
    fprintf(f,"{%d,%d,%d,%d}",
	   r[0], r[1], r[2], r[3]);
}

void print_vint64(FILE* f,vint64_t r)
{
    fprintf(f,"{%ld,%ld}",
	   r[0], r[1]);
}

void print_vfloat32(FILE* f,vfloat32_t r)
{
    fprintf(f,"{%f,%f,%f,%f}",
	   r[0], r[1], r[2], r[3]);
}

void print_vfloat64(FILE* f,vfloat64_t r)
{
    fprintf(f,"{%f,%f}",
	   r[0], r[1]);
}

void vprint(FILE* f, uint8_t type, vector_t v)
{
    switch(type) {
    case UINT8: print_vuint8(f,(vuint8_t) v); break;
    case UINT16: print_vuint16(f,(vuint16_t) v); break;
    case UINT32: print_vuint32(f,(vuint32_t) v); break;
    case UINT64: print_vuint64(f,(vuint64_t) v); break;	
    case INT8: print_vint8(f,(vint8_t) v); break;
    case INT16: print_vint16(f,(vint16_t) v); break;
    case INT32: print_vint32(f,(vint32_t) v); break;
    case INT64: print_vint64(f,(vint64_t) v); break;
    case FLOAT32: print_vfloat32(f,(vfloat32_t) v); break;
    case FLOAT64: print_vfloat64(f,(vfloat64_t) v); break;
    default: break;
    }
}

void sprint(FILE* f, uint8_t type, scalar0_t v)
{
    switch(type) {
    case UINT8: fprintf(f, "0x%02x", v.u8); break;
    case UINT16: fprintf(f, "0x%04x", v.u16); break;
    case UINT32: fprintf(f, "0x%08x", v.u32); break;
    case UINT64: fprintf(f, "0x%016lx", v.u64); break;
    case INT8: fprintf(f, "%d", v.i8); break;
    case INT16: fprintf(f, "%d", v.i16); break;
    case INT32: fprintf(f, "%d", v.i32); break;
    case INT64: fprintf(f, "%ld", v.i64); break;
    case FLOAT8: fprintf(f, "%u", v.f8);break;
    case FLOAT16: fprintf(f, "%u", v.f16);break;		
    case FLOAT32: fprintf(f, "%f", v.f32);break;
    case FLOAT64: fprintf(f, "%f", v.f64);break;
    default: break;
    }
}

#define VCMP_BODY(v1,v2) do { \
	unsigned k;				\
	for (k=0; k<VSIZE/sizeof(v1[0]); k++) {	\
	    if (v1[k] < v2[k]) return -1;	\
	    else if (v1[k] > v2[k]) return 1;	\
	}					\
    } while(0)

int cmp_vuint8(vuint8_t v1, vuint8_t v2)    { VCMP_BODY(v1,v2); return 0; }
int cmp_vuint16(vuint16_t v1, vuint16_t v2) { VCMP_BODY(v1,v2); return 0; }
int cmp_vuint32(vuint32_t v1, vuint32_t v2) { VCMP_BODY(v1,v2); return 0; }
int cmp_vuint64(vuint64_t v1, vuint64_t v2) { VCMP_BODY(v1,v2); return 0; }
int cmp_vint8(vint8_t v1, vint8_t v2)       { VCMP_BODY(v1,v2); return 0; }
int cmp_vint16(vint16_t v1, vint16_t v2)    { VCMP_BODY(v1,v2); return 0; }
int cmp_vint32(vint32_t v1, vint32_t v2)    { VCMP_BODY(v1,v2); return 0; }
int cmp_vint64(vint64_t v1, vint64_t v2)    { VCMP_BODY(v1,v2); return 0; }
int cmp_vfloat32(vfloat32_t v1, vfloat32_t v2) { VCMP_BODY(v1,v2); return 0; }
int cmp_vfloat64(vfloat64_t v1, vfloat64_t v2) { VCMP_BODY(v1,v2); return 0; }

int vcmp(uint8_t type, vector_t v1, vector_t v2)
{
    switch(type) {
    case UINT8:  return cmp_vuint8((vuint8_t) v1, (vuint8_t) v2);
    case UINT16: return cmp_vuint16((vuint16_t) v1, (vuint16_t) v2);
    case UINT32: return cmp_vuint32((vuint32_t) v1, (vuint32_t) v2);
    case UINT64: return cmp_vuint64((vuint64_t) v1, (vuint64_t) v2);
    case INT8:   return cmp_vint8((vint8_t) v1, (vint8_t) v2);
    case INT16:  return cmp_vint16((vint16_t) v1, (vint16_t) v2);
    case INT32:  return cmp_vint32((vint32_t) v1, (vint32_t) v2);
    case INT64:  return cmp_vint64((vint64_t) v1, (vint64_t) v2);
    case FLOAT32: return cmp_vfloat32((vfloat32_t) v1, (vfloat32_t) v2);
    case FLOAT64: return cmp_vfloat64((vfloat64_t) v1, (vfloat64_t) v2);
    default: break;
    }
    return -1;
}

#define CMP(a,b) (((a)<(b)) ? -1 : ( ((a)>(b)) ? 1 : 0))

int scmp(uint8_t type, scalar0_t v1, scalar0_t v2)
{
    switch(type) {
    case UINT8:  return CMP(v1.u8, v2.u8);
    case UINT16: return CMP(v1.u16, v2.u16);
    case UINT32: return CMP(v1.u32, v2.u32);
    case UINT64: return CMP(v1.u64, v2.u64); 
    case INT8:   return CMP(v1.i8, v2.i8);
    case INT16:  return CMP(v1.i16, v2.i16);
    case INT32:  return CMP(v1.i32, v2.i32);
    case INT64:  return CMP(v1.i64, v2.i64); 
    case FLOAT32: return CMP(v1.f32, v2.f32); 
    case FLOAT64: return CMP(v1.f64, v2.f64); 
    default: break;
    }
    return -1;
}

