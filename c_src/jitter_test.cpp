// Test of jitter

#include <asmjit/x86.h>
#include <iostream>

using namespace asmjit;

#include "jitter_types.h"
#include "jitter.h"
#include "jitter_asm.h"

// A simple error handler implementation, extend according to your needs.
class MyErrorHandler : public ErrorHandler {
public:
  void handleError(Error err, const char* message, BaseEmitter* origin) override {
    printf("AsmJit error: %s\n", message);
  }
};

extern void assemble(ZAssembler &a, const Environment &env,
		     uint32_t reg_mask,
		     x86::Mem save_ptr,
		     instr_t* code, size_t n);

extern void emulate(scalar0_t r[16], vscalar0_t v[16], instr_t* code,
		    size_t n, int* ret);

extern void sprint(FILE* f,uint8_t type, scalar0_t v);
extern int  scmp(uint8_t type, scalar0_t v1, scalar0_t v2);
extern void x86_info(void);

extern x86::Vec xreg(int i);
extern x86::Vec yreg(int i);
extern x86::Gp reg(int i);


extern void vprint(FILE* f, uint8_t type, vector_t v);
extern int  vcmp(uint8_t type, vector_t v1, vector_t v2);

extern void set_element_int64(jitter_type_t type, vector_t &r, int i, int64_t v);
extern void set_element_float64(jitter_type_t type, vector_t &r, int i, float64_t v);

#define OPdij(o,d,i,j) \
    {.op = (o),.type=INT64,.rd=(d),.ri=(i),.rj=(j),.pad=0}
#define OPdi(o,d,i) \
    {.op = (o),.type=INT64,.rd=(d),.ri=(i),.rj=0,.pad=0}
#define OPd(o,d) \
    {.op = (o),.type=INT64,.rd=(d),.ri=0,.rj=0,.pad=0}
#define OPdiimm8(o,d,i,imm)				\
    {.op = (o),.type=INT64,.rd=(d),.ri=(i),.imm8=(imm)}

#define OPimm12d(o,d,rel)				\
    {.op = (o),.type=INT64,.rd=(d),.imm12=(rel)}
#define OPimm12(o,rel)					\
    {.op = (o),.type=INT64,.rd=(0),.imm12=(rel)}


// convert type to integer type with the same size
static uint8_t int_type(uint8_t at)
{
    return ((at & ~BASE_TYPE_MASK) | INT);
}

static uint8_t uint_type(uint8_t at)
{
    return ((at & ~BASE_TYPE_MASK) | UINT);
}

static const char* asm_opname(uint8_t op)
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

static const char* asm_typename(uint8_t type)
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

// set all instructions to the same type!
void set_type(uint8_t type, instr_t* code, size_t n)
{
    while (n--) {
	code->type = type;
	code++;
    }
}

size_t code_size(instr_t* code)
{
    size_t len = 0;
    
    while((code->op != OP_RET) && (code->op != OP_VRET)) {
	len++;
	code++;
    }
    return len+1;
}

// pass pointer to area with register data, return fxsave64 area
typedef void* (*fun1_t)(void* reg_data);

int64_t value_u8_0[16] =
{ 0,1,2,5,10,20,50,70,127,128,130,150,180,253,254,255};
int64_t value_u8_1[16] =
{ 0,2,2,100,20,30,30,71,8,128,2,255,180,254,20,9};
int64_t value_u8_2[16] =
{ 0,3,4,200,10,150,200,2,3,128,170,3,5,8};

int64_t value_i8_0[16] =
{ 0,1,2,5,10,20,50,70,127,-128,-126,-106,-56,-3,-2,-1};
int64_t value_i8_1[16] =
{ 0,2,2,100,20,30,30,71,8,128,2,255,-76,-3,20,9};
int64_t value_i8_2[16] =
{ 0,3,4,-56,10,-106,-56,2,3,-128,-86,100,3,5,8};

int64_t value_u16_0[8] =
{ 0,2,50,32767,1300,180,253,65535};
int64_t value_u16_1[8] =
{ 2,1000,71,128,65535,100,20,9};
int64_t value_u16_2[8] =
{ 3,200,1500,2,128,100,5, 8};

int64_t value_i16_0[8] =
{ -32768,-1260,-106,-76,-56,-3,-2,-1};
int64_t value_i16_1[8] =
{ 0,2,2,1000,200,30,71,8 };
int64_t value_i16_2[8] =
{ 0,4,-56,100,-32768,3,5,8};

int64_t value_u32_0[4] =
{ 0,500,130,2530 };
int64_t value_u32_1[4] =
{ 100,12800,100,9};
int64_t value_u32_2[4] =
{ 200,2,1000, 8};

int64_t value_i32_0[4] =
{ -106,-3,-2,-1};
int64_t value_i32_1[4] =
{ 20,30,71,8 };
int64_t value_i32_2[4] =
{ 0,-56,10,-32768};

int64_t value_u64_0[2] =
{ 130,2530 };
int64_t value_u64_1[2] =
{ 12800,100};
int64_t value_u64_2[2] =
{ 2,1000 };

int64_t value_i64_0[2] =
{ -106,-56};
int64_t value_i64_1[2] =
{ 20,30 };
int64_t value_i64_2[2] =
{ -56-32768};

    
int load_vreg(jitter_type_t type, int j, int jval,
	      vector_t vreg[16], int r0, int r1, int r2)
{
    int i;    
    switch(type) {
    case UINT8:
	for (i = 0; i < 16; i++) {
	    set_element_int64(type, vreg[r0], i, value_u8_0[i]);
	    set_element_int64(type, vreg[r1], i, value_u8_1[i]);
	    set_element_int64(type, vreg[r2], i, value_u8_2[i]);
	}
	break;
    case INT8:
	for (i = 0; i < 16; i++) {
	    set_element_int64(type, vreg[r0], i, value_i8_0[i]);
	    set_element_int64(type, vreg[r1], i, value_i8_1[i]);
	    set_element_int64(type, vreg[r2], i, value_i8_2[i]);
	}
	break;
    case UINT16:
	for (i = 0; i < 8; i++) {
	    set_element_int64(type, vreg[r0], i, value_u16_0[i]);
	    set_element_int64(type, vreg[r1], i, value_u16_1[i]);
	    set_element_int64(type, vreg[r2], i, value_u16_2[i]);
	}
	break;
    case INT16:
	for (i = 0; i < 8; i++) {
	    set_element_int64(type, vreg[r0], i, value_i16_0[i]);
	    set_element_int64(type, vreg[r1], i, value_i16_1[i]);
	    set_element_int64(type, vreg[r2], i, value_i16_2[i]);
	}
	break;
    case UINT32:
	for (i = 0; i < 4; i++) {
	    set_element_int64(type, vreg[r0], i, value_u32_0[i]);
	    set_element_int64(type, vreg[r1], i, value_u32_1[i]);
	    set_element_int64(type, vreg[r2], i, value_u32_2[i]);
	}
	break;
    case FLOAT32:
    case INT32:
	for (i = 0; i < 4; i++) {
	    set_element_int64(type, vreg[r0], i, value_i32_0[i]);
	    set_element_int64(type, vreg[r1], i, value_i32_1[i]);
	    set_element_int64(type, vreg[r2], i, value_i32_2[i]);
	}
	break;
    case UINT64:
	for (i = 0; i < 2; i++) {
	    set_element_int64(type, vreg[r0], i, value_u64_0[i]);
	    set_element_int64(type, vreg[r1], i, value_u64_1[i]);
	    set_element_int64(type, vreg[r2], i, value_u64_2[i]);
	}
	break;
    case INT64:
    case FLOAT64:
	for (i = 0; i < 2; i++) {
	    set_element_int64(type, vreg[r0], i, value_i64_0[i]);
	    set_element_int64(type, vreg[r1], i, value_i64_1[i]);
	    set_element_int64(type, vreg[r2], i, value_i64_2[i]);
	}
	break;
    default:
	break;
    }
    if (jval>=0) {
	set_element_int64(UINT64, vreg[j], 0, jval);
	set_element_int64(UINT64, vreg[j], 1, 0);
    }
    return 0;
}


int load_reg(jitter_type_t type, int i, int j, int jval,
	     scalar0_t reg[16], int r0, int r1, int r2)
{
    switch(type) {
    case UINT8:
	reg[r0].u8 = value_u8_0[i & 0xf];
	reg[r1].u8 = value_u8_1[i & 0xf];
	reg[r2].u8 = value_u8_2[i & 0xf];
	break;
    case INT8:
	reg[r0].i8 = value_i8_0[i & 0xf];
	reg[r1].i8 = value_i8_1[i & 0xf];
	reg[r2].i8 = value_i8_2[i & 0xf];
	break;
    case UINT16:
	reg[r0].u16 = value_u16_0[i & 0x7];
	reg[r1].u16 = value_u16_1[i & 0x7];
	reg[r2].u16 = value_u16_2[i & 0x7];
	break;
    case INT16:
	reg[r0].i16 = value_i16_0[i & 0x7];
	reg[r1].i16 = value_i16_1[i & 0x7];
	reg[r2].i16 = value_i16_2[i & 0x7];
	break;
    case UINT32:
	reg[r0].u32 = value_u32_0[i & 0x3];
	reg[r1].u32 = value_u32_1[i & 0x3];
	reg[r2].u32 = value_u32_2[i & 0x3];
	break;
    case INT32:
	reg[r0].i32 = value_i32_0[i & 0x3];
	reg[r1].i32 = value_i32_1[i & 0x3];
	reg[r2].i32 = value_i32_2[i & 0x3];
	break;
    case UINT64:
	reg[r0].u64 = value_u64_0[i & 0x1];
	reg[r1].u64 = value_u64_1[i & 0x1];
	reg[r2].u64 = value_u64_2[i & 0x1];
	break;
    case INT64:
	reg[r0].i64 = value_i64_0[i & 0x1];
	reg[r1].i64 = value_i64_1[i & 0x1];
	reg[r2].i64 = value_i64_2[i & 0x1];
	break;
    default:
	break;
    }
    if (jval>=0) {
	if (jval >= 0) reg[j].i64 = jval;
    }    
    return 0;
}


void print_instr(FILE* f,instr_t* pc)
{
    if ((pc->op & OP_MASK) == OP_NOP) {
	fprintf(f, "%s", asm_opname(OP_NOP));
    }
    else if ((pc->op & OP_MASK) == OP_RET) {
	fprintf(f, "%s.%s %s",
		asm_opname(pc->op),
		asm_typename(pc->type),
		asm_regname(pc->op,pc->rd));
    }
    else if (pc->op == OP_JMP) {
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


static int verbose = 1;
static int debug   = 0;
static int exit_on_fail = 0;
static int debug_on_fail = 0;

int test_icode(jitter_type_t itype, jitter_type_t otype, int i, int jval,
	       instr_t* icode, size_t code_len)
{
    JitRuntime rt;           // Runtime designed for JIT code execution
    MyErrorHandler myErrorHandler;
    CodeHolder code;         // Holds code and relocation information
    FileLogger logger(stderr);
    Label save_label;
    Section* xmm_data;
    int res;
    
    // Initialize to the same arch as JIT runtime
    code.init(rt.environment(), rt.cpuFeatures());
    code.newSection(&xmm_data, ".data", 5, SectionFlags::kNone, 128);
    code.setErrorHandler(&myErrorHandler);
    
    ZAssembler a(&code, 1024);

    scalar0_t  rr[16], rr_save[16];  // regular registers
    vscalar0_t vr[16], vr_save[16];  // vector registers

    memset(rr, 0, sizeof(rr));
    memset(vr, 0, sizeof(vr));

    load_reg(itype, i, icode->rj, jval, rr, 0, 1, 2);
    load_vreg(itype, icode->rj, jval, (vector_t*)vr, 0, 1, 2);    

    if (verbose) {
	fprintf(stderr, "TEST ");
	icode[0].type = itype; // otherwise set by test_code!
	print_instr(stderr, icode);
	if (jval >= 0) fprintf(stderr, " jval=%d", jval);

	fprintf(stderr, "\nreg_data.r0 = "); sprint(stderr, itype, rr[0]);
	fprintf(stderr, "\nreg_data.r1 = "); sprint(stderr, itype, rr[1]);
	fprintf(stderr, "\nreg_data.r2 = "); sprint(stderr, itype, rr[2]);
	fprintf(stderr,"\n");		
    }

    if (debug) {
	a.setLogger(&logger);
    }

    // register must be passed in order! xmm 0...15 gp 0...15
    uint32_t reg_mask = (1 << 16) | (1 << 17) | (1 << 18);
    struct {
	scalar0_t r0;
	scalar0_t r1;
	scalar0_t r2;
    } reg_data;


    set_type(itype, icode, code_len);

    save_label = a.newLabel();

    a.disable_avx();
    
    assemble(a, rt.environment(),
	     reg_mask,  // r8,r9,r10
	     x86::ptr(save_label),
	     icode, code_len);

    a.section(xmm_data);
    a.bind(save_label);
    a.embedDataArray(TypeId::kUInt8, "\0", 1, 512);

    fun1_t fn;
    Error err = rt.add(&fn, &code);   // Add the generated code to the runtime.
    if (err) {
	fprintf(stderr, "rt.add ERROR\n");
	return -1;               // Handle a possible error case.
    }
    

    scalar0_t rexe, remu;
    memcpy(rr_save, rr, sizeof(rr));
    memcpy(vr_save, vr, sizeof(vr));

    emulate(rr, vr, icode, code_len, &i);
    memcpy(&remu, &rr[i], sizeof(scalar0_t));

    // restore state!
    memcpy(rr, rr_save, sizeof(rr));
    memcpy(vr, vr_save, sizeof(vr));

    reg_data.r0 = rr[0];
    reg_data.r1 = rr[1];    
    reg_data.r2 = rr[2];

    fxsave64_1_t* xmm_save = (fxsave64_1_t*) fn(&reg_data);
    memcpy(&rexe, &reg_data.r2, sizeof(scalar0_t));

    // print some xmm register direcly from fxsave64 output
    res = scmp(otype, remu, rexe);
    if (res != 0) {
	if (debug || debug_on_fail) {
	    fprintf(stderr, "\nxmm_save=%p\n", xmm_save);
	    for (i = 0; i < 16; i++) {
		fprintf(stderr, "xmm%d = ", i);
		vprint(stderr, itype, (vector_t)xmm_save->xmm[i]);
		fprintf(stderr,"\n");
	    }
	}
    }
    rt.release(fn);
    
    // compare output from emu and exe
    if (res != 0) {
	if (verbose)
	    fprintf(stderr, " FAIL\n");
	if (debug_on_fail) {
	    print_code(stderr, icode, code_len);
	    fprintf(stderr, "r0 = "); sprint(stderr,itype, rr[0]); fprintf(stderr,"\n");
	    fprintf(stderr,"r1 = "); sprint(stderr,itype, rr[1]); fprintf(stderr,"\n");
	    fprintf(stderr,"r2 = "); sprint(stderr,itype, rr[2]); fprintf(stderr,"\n");
	    fprintf(stderr, "emu:r = "); sprint(stderr,otype,remu); fprintf(stderr,"\n");
	    fprintf(stderr, "exe:r = "); sprint(stderr, otype, rexe); fprintf(stderr, "\n");
	    // reassemble with logging
	    CodeHolder code1;

	    code1.init(rt.environment(), rt.cpuFeatures());
	    code1.newSection(&xmm_data, ".data", 5, SectionFlags::kNone, 128);	    
	    ZAssembler b(&code1,1024);
	    
	    b.setLogger(&logger);

	    save_label = b.newLabel();

	    b.disable_avx();
	    assemble(b, rt.environment(),
		     reg_mask,  // r8,r9,r10
		     x86::ptr(save_label),
		     icode, code_len);
	    b.section(xmm_data);
	    b.bind(save_label);
	    b.embedDataArray(TypeId::kUInt8, "\0", 1, 512);
	}
	if (exit_on_fail)
	    exit(1);
	return -1;
    }
    else {
	if (debug) {
	    print_code(stderr, icode, code_len);
	    fprintf(stderr, "r0 = "); sprint(stderr,itype, rr[0]); fprintf(stderr,"\n");
	    fprintf(stderr,"r1 = "); sprint(stderr,itype, rr[1]); fprintf(stderr,"\n");
	    fprintf(stderr,"r2 = "); sprint(stderr,itype, rr[2]); fprintf(stderr,"\n");
	    fprintf(stderr, "emu:r = "); sprint(stderr,otype,remu); fprintf(stderr,"\n");
	    fprintf(stderr, "exe:r = "); sprint(stderr, otype, rexe); fprintf(stderr, "\n");	    
	}
	if (verbose) fprintf(stderr, " OK\n");
    }
    return 0;
}

int test_code(jitter_type_t itype, jitter_type_t otype, int jval, instr_t* icode, size_t code_len)
{
    int failed = 0;    
    size_t n = 16/(1 << get_scalar_exp_size(itype)); // 16,8,4,2
    int i;
    for (i = 0; i < (int)n; i++) {
	failed += test_icode(itype, otype, i, jval, icode, code_len);
    }
    return failed;
}

int test_vcode(jitter_type_t itype, jitter_type_t otype, int jval,
	       instr_t* icode, size_t code_len)
{
    JitRuntime rt;           // Runtime designed for JIT code execution
    MyErrorHandler myErrorHandler;    
    CodeHolder code;         // Holds code and relocation information
    int i, res;
    FileLogger logger(stderr);
    Label save_label;
    Section* xmm_data;
    // register must be passed in order! xmm 0...15 gp 0...15
    uint32_t reg_mask;
    struct {
	vscalar0_t v0;
	vscalar0_t v1;
	vscalar0_t v2;	
	scalar0_t  r0;
    } reg_data;

    if (jval >= 0)
	reg_mask = (1 << 0) | (1 << 1) | (1 << 2) | (1 << (16+icode->rj));
    else
	reg_mask = (1 << 0) | (1 << 1) | (1 << 2); 

    // Initialize to the same arch as JIT runtime
    code.init(rt.environment(), rt.cpuFeatures());
    code.newSection(&xmm_data, ".data", 5, SectionFlags::kNone, 128);
    code.setErrorHandler(&myErrorHandler);
    
    ZAssembler a(&code, 1024);  // Create and attach x86::Assembler to `code`

    if (verbose) {
	fprintf(stderr, "TEST ");
	icode[0].type = itype; // otherwise set by test_code!
	print_instr(stderr, icode);
	if (jval >= 0) fprintf(stderr, " jval=%d", jval);
    }

    if (debug)
	a.setLogger(&logger);

    set_type(itype, icode, code_len);

    save_label = a.newLabel();

    a.disable_avx();
    assemble(a, rt.environment(),
	     reg_mask,  // xmm0,xmm1,xmm2
	     x86::ptr(save_label),
	     icode, code_len);
    a.section(xmm_data);
    a.bind(save_label);
    a.embedDataArray(TypeId::kUInt8, "\0", 1, 512);
    
    fun1_t fn;
    Error err = rt.add(&fn, &code);   // Add the generated code to the runtime.
    if (err) {
	fprintf(stderr, "rt.add ERROR\n");
	return -1;               // Handle a possible error case.
    }

    // fprintf(stderr, "code is added %p\n", fn);

    scalar0_t  rr[16], rr_save[16];  // regular registers
    vscalar0_t vr[16], vr_save[16];  // vector registers

    memset(rr, 0, sizeof(rr));
    memset(vr, 0, sizeof(vr));

    load_reg(itype, 0, icode->rj, jval, rr, 0, 1, 2);
    load_vreg(itype, icode->rj, jval, (vector_t*)vr, 0, 1, 2);

    vector_t rexe, remu;

    // save for emu run
    memcpy(rr_save, rr, sizeof(rr));
    memcpy(vr_save, vr, sizeof(vr));
    
    emulate(rr, vr, icode, code_len, &i);  // i is the register returned
    memcpy(&remu, &vr[i], sizeof(vector_t));   // save result

    
    // fprintf(stderr, "\ncalling fn\n");
    // restore state!
    memcpy(rr, rr_save, sizeof(rr));
    memcpy(vr, vr_save, sizeof(vr));

    reg_data.v0 = vr[0];
    reg_data.v1 = vr[1];
    reg_data.v2 = vr[2];
    reg_data.r0 = rr[icode->rj];

    if (debug) {
	fprintf(stderr, "\nreg_data.v0 = "); vprint(stderr, itype, reg_data.v0.v);
	fprintf(stderr, "\nreg_data.v1 = "); vprint(stderr, itype, reg_data.v1.v);
	fprintf(stderr, "\nreg_data.v2 = "); vprint(stderr, itype, reg_data.v2.v);
	fprintf(stderr, "\nreg_data.r%d = ", icode->rj); sprint(stderr, itype, reg_data.r0);
	fprintf(stderr,"\n");
    }

    fxsave64_1_t* xmm_save = (fxsave64_1_t*) fn(&reg_data);

    memcpy(&rexe, &reg_data.v2, sizeof(vector_t));

    // compare output from emu and exe
    res = vcmp(otype, remu, rexe);

    if (res != 0) {
    // print some xmm register direcly from fxsave64 output
	if (debug_on_fail || debug) {
	    fprintf(stderr, "\nxmm_save=%p\n", xmm_save);
	    for (i = 0; i < 16; i++) {
		fprintf(stderr, "xmm%d = ", i);
		vprint(stderr, itype, (vector_t)xmm_save->xmm[i]);
		fprintf(stderr,"\n");
	    }

	    for (i = 0; i < 16; i++) {
		fprintf(stderr, "xmm%d = ", i);
		vprint(stderr, INT16, (vector_t)xmm_save->xmm[i]);
		fprintf(stderr,"\n");
	    }	    
	}
    }

    rt.release(fn);
    
    if (res != 0) {
	if (verbose)
	    printf(" FAIL\n");
	if (debug_on_fail) {
	    print_code(stderr, icode, code_len);
	    // itype = uint_type(itype);

	    fprintf(stderr, "\nreg_data.v0 = "); vprint(stderr, itype, reg_data.v0.v);
	    fprintf(stderr, "\nreg_data.v1 = "); vprint(stderr, itype, reg_data.v1.v);
	    fprintf(stderr, "\nreg_data.v2 = "); vprint(stderr, itype, reg_data.v2.v);
	    fprintf(stderr, "\nreg_data.r%d = ", icode->rj); sprint(stderr, itype, reg_data.r0);    

	    fprintf(stderr,"\nexe:r = "); vprint(stderr,otype, rexe);
	    fprintf(stderr,"\nemu:r = "); vprint(stderr,otype, remu);
	    fprintf(stderr,"\n");
	    
	    // reassemble with logging
	    CodeHolder code1;
	    
	    code1.init(rt.environment(), rt.cpuFeatures());
	    code1.newSection(&xmm_data, ".data", 5, SectionFlags::kNone, 128);
	    
	    ZAssembler b(&code1, 1024);
	    b.setLogger(&logger);

	    save_label = b.newLabel();

	    b.disable_avx();
	    assemble(b, rt.environment(),
		     reg_mask,  // xmm0,xmm1,xmm2
		     x86::ptr(save_label),
		     icode, code_len);
	    b.section(xmm_data);
	    b.bind(save_label);
	    b.embedDataArray(TypeId::kUInt8, "\0", 1, 512);
	}
	if (exit_on_fail)
	    exit(1);	
	return -1;
    }
    else {
	if (debug) {
	    print_code(stderr, icode, code_len);
	    fprintf(stderr, "v0 = "); vprint(stderr, itype, vr[0].v); fprintf(stderr,"\n");
	    fprintf(stderr, "v1 = "); vprint(stderr, itype, vr[1].v); fprintf(stderr,"\n");
	    fprintf(stderr, "v2 = "); vprint(stderr, itype, vr[2].v); fprintf(stderr,"\n");
	    fprintf(stderr,"exe:r = "); vprint(stderr,otype, rexe); fprintf(stderr,"\n");
	    fprintf(stderr,"emu:r = "); vprint(stderr,otype, remu); fprintf(stderr,"\n");
	}
	if (verbose) printf(" OK\n");
    }
    return 0;
}


#define CODE_LEN(code) (sizeof((code))/sizeof((code)[0]))

int test_ts_code(uint8_t* ts, int td, int vec, int jval,
		 instr_t* code, size_t code_len)
{
    int failed = 0;
    
    while(*ts != VOID) {
	uint8_t otype;
	switch(td) {
	case INT:  otype = int_type(*ts); break;
	case UINT: otype = uint_type(*ts); break;
	default:   otype = *ts; break;
	}

	if (vec) {
	    if (test_vcode(*ts, otype, jval, code, code_len) < 0)
		failed++;
	}
	else {
	    if (test_code(*ts, otype, jval, code, code_len) < 0)
		failed++;
	}
	ts++;
    }
    return failed;
}

int test_binary(uint8_t op, uint8_t* ts, uint8_t otype)
{
    instr_t code[2];
    int i, j;
    int failed = 0;
    int vec = (op & OP_VEC) != 0;

    printf("+------------------------------\n");
    printf("| %s\n", asm_opname(op));
    printf("+------------------------------\n");    
    
    code[0].op = op;
    code[1].op = vec ? OP_VRET : OP_RET;
    code[1].rd = 2;
    code[0].rd = 2;
    
    for (i = 0; i <= 2; i++) {
	code[0].ri = i;
	for (j = 0; j <= 2; j++) {
	    code[0].rj = j;
	    failed += test_ts_code(ts, otype, vec, -1, code, 2);
	}
    }
    return failed;
}

int test_unary(uint8_t op, uint8_t* ts, uint8_t otype)
{
    instr_t code[2];
    int i;
    int failed = 0;
    int vec = (op & OP_VEC) != 0;

    printf("+------------------------------\n");
    printf("| %s\n", asm_opname(op));
    printf("+------------------------------\n");
    
    code[0].op = op;
    code[1].op = vec ? OP_VRET : OP_RET;
    code[1].rd = 2;
    code[0].rd = 2;
    
    for (i = 0; i <= 2; i++) {
	code[0].ri = i;
	failed += test_ts_code(ts, otype, vec, -1, code, 2);
    }
    return failed;
}

int test_imm8(uint8_t op, uint8_t* ts, uint8_t otype)
{
    instr_t code[2];
    int i, imm;
    int failed = 0;
    int vec = (op & OP_VEC) != 0;

    printf("+------------------------------\n");
    printf("| %s\n", asm_opname(op));
    printf("+------------------------------\n");
        
    code[0].op = op;
    code[1].op = vec ? OP_VRET : OP_RET;
    code[1].rd = 2;
    code[0].rd = 2;

    for (i = 0; i <= 2; i++) {
	code[0].ri = i;
	for (imm = -128; imm < 128; imm += 27) {
	    code[0].imm8 = imm;
	    failed += test_ts_code(ts, otype, vec, -1,  code, 2);
	}
    }
    return failed;
}

int test_imm12(uint8_t op, uint8_t* ts, uint8_t otype)
{
    instr_t code[2];
    int imm;
    int failed = 0;
    int vec = (op & OP_VEC) != 0;

    printf("+------------------------------\n");
    printf("| %s\n", asm_opname(op));
    printf("+------------------------------\n");
    
    code[0].op = op;
    code[1].op = vec ? OP_VRET : OP_RET;
    code[1].rd = 2;
    code[0].rd = 2;

    for (imm = -2048; imm < 2048; imm += 511) {
	code[0].imm12 = imm;
	failed += test_ts_code(ts, otype, vec, -1, code, 2);
    }
    return failed;
}

int test_shift(uint8_t op, uint8_t* ts, uint8_t otype)
{
    instr_t code[2];
    int i;
    int failed = 0;
    int vec = (op & OP_VEC) != 0;

    printf("+------------------------------\n");
    printf("| %s\n", asm_opname(op));
    printf("+------------------------------\n");
    
    code[0].op = op;
    code[1].op = vec ? OP_VRET : OP_RET;
    code[1].rd = 2;
    code[0].rd = 2;

    for (i = 0; i <= 2; i++) {
	uint8_t* ts1 = ts;
	code[0].ri = i;
	while(*ts1 != VOID) {
	    int imm;
	    size_t n = (1 << get_scalar_exp_bit_size(*ts1));
	    for (imm = 0; imm < (int)n; imm++) {
		uint8_t tv[2];
		code[0].imm8 = imm;
		tv[0] = *ts1; tv[1] = VOID;
		failed += test_ts_code(tv, otype, vec, -1, code, 2); 
	    }
	    ts1++;
	}	    
    }
    return failed;
}

// test shift operations using registers
int test_bshift(uint8_t op, uint8_t* ts, uint8_t otype)
{
    instr_t code[2];
    int i, j;
    int failed = 0;
    int vec = (op & OP_VEC) != 0;

    printf("+------------------------------\n");
    printf("| %s\n", asm_opname(op));
    printf("+------------------------------\n");
    
    code[0].op = op;
    code[1].op = vec ? OP_VRET : OP_RET;
    code[1].rd = 2;
    code[0].rd = 2;

    for (i = 0; i <= 2; i++) {
	code[0].ri = i;	
	for (j = 0; j <= 2; j++) {
	    uint8_t* ts1 = ts;
	    code[0].rj = j;
	    // load shift value
	    while(*ts1 != VOID) {
		int imm;
		size_t n = (1 << get_scalar_exp_bit_size(*ts1));
		for (imm = 0; imm < (int)n; imm++) {
		    uint8_t tv[2];
		    tv[0] = *ts1; tv[1] = VOID;
		    failed += test_ts_code(tv, otype, vec, imm, code, 2);
		}
		ts1++;	    
	    }
	}
    }
    return failed;
}


int main()
{
    printf("VSIZE = %d\n", VSIZE);
    printf("sizeof(vector_t) = %ld\n", sizeof(vector_t));
    printf("sizeof(scalar0_t) = %ld\n", sizeof(scalar0_t));
    printf("sizeof(vscalar0_t) = %ld\n", sizeof(vscalar0_t));
    printf("sizeof(instr_t) = %ld\n", sizeof(instr_t));

    x86_info();

    int failed = 0;  // number of failed cases
    
    instr_t code_sum[] = {
	OPimm12d(OP_MOVI, 0, 0),     // SUM=0 // OPdij(OP_BXOR, 0, 0, 0),
	OPimm12d(OP_MOVI, 1, 13),    // I=13  OPdij(OP_BXOR, 0, 0, 0),
	OPdij(OP_ADD, 0, 1, 0),      // SUM += I
	OPimm12d(OP_SUBI, 1, 1),    // I -= 1
	OPimm12d(OP_JNZ, 1, -3),
	OPd(OP_RET, 0)
    };
    
    uint8_t int_types[] =
	{ UINT8, UINT16, UINT32, UINT64, INT8, INT16, INT32, INT64, VOID };
//    uint8_t float_types[] =
//	{ FLOAT32, FLOAT64, VOID };
    uint8_t all_types[] =
	{ UINT8, UINT16, UINT32, UINT64, INT8, INT16, INT32, INT64,
	  //FLOAT16
	  FLOAT32, FLOAT64, VOID };
    
    uint8_t int_type[] = { INT8, VOID };

//     debug = 1;
      exit_on_fail = 1;
      debug_on_fail = 1;
//    instr_t code1[] = { OPdij(OP_CMPLT,2,0,2), OPd(OP_RET, 2) };
//    code1[0].type = UINT64;
//    code1[1].type = UINT64;
//    test_code(UINT64, INT64, code1, 2);    
//    exit(0);
//////////////////

    failed += test_imm8(OP_VCMPLEI, all_types, INT);    

//////////////////     


    failed += test_unary(OP_NOP, int_types, VOID); 
    failed += test_imm12(OP_MOVI, int_types, INT);
    failed += test_unary(OP_MOV, int_types, VOID); // fixme: float registers!
    
    failed += test_unary(OP_NEG, int_types, VOID);
    failed += test_unary(OP_BNOT, int_types, INT);
    // failed += test_unary(OP_INV, int_types, INT);
    // OP_JMP/OP_JNZ/OP_JZ

    failed += test_binary(OP_ADD, int_types, VOID);
    failed += test_binary(OP_SUB, int_types, VOID);
    failed += test_binary(OP_RSUB, int_types, VOID);
    failed += test_binary(OP_MUL, int_types, VOID);
    failed += test_binary(OP_BAND, int_types, INT);    
    failed += test_binary(OP_BOR, int_types, INT);        
    failed += test_binary(OP_BXOR, int_types, INT);
    failed += test_bshift(OP_SLL, int_types, INT);
    failed += test_bshift(OP_SRL, int_types, INT);
    failed += test_bshift(OP_SRA, int_types, INT);        
    failed += test_binary(OP_CMPLT, int_types, INT);
    failed += test_binary(OP_CMPLE, int_types, INT);
    failed += test_binary(OP_CMPGT, int_types, INT);
    failed += test_binary(OP_CMPGE, int_types, INT);    
    failed += test_binary(OP_CMPEQ, int_types, INT);
    failed += test_binary(OP_CMPNE, int_types, INT);

    // imm8 argument
    failed += test_imm8(OP_ADDI, int_types, INT);
    failed += test_imm8(OP_SUBI, int_types, INT);
    failed += test_imm8(OP_RSUBI, int_types, INT);
    failed += test_imm8(OP_MULI, int_types, INT);
    failed += test_shift(OP_SLLI, int_types, INT);
    failed += test_shift(OP_SRLI, int_types, INT);
    failed += test_shift(OP_SRAI, int_types, INT);
    failed += test_imm8(OP_CMPLTI, int_types, INT);
    failed += test_imm8(OP_CMPLEI, int_types, INT);
    failed += test_imm8(OP_CMPGTI, int_types, INT);
    failed += test_imm8(OP_CMPGEI, int_types, INT);    
    failed += test_imm8(OP_CMPEQI, int_types, INT);
    failed += test_imm8(OP_CMPNEI, int_types, INT);    

    // vectors
    failed += test_unary(OP_VMOV, all_types, VOID);
    failed += test_imm12(OP_VMOVI, int_types, INT);
    failed += test_unary(OP_VNEG, all_types, VOID);    
    failed += test_unary(OP_VBNOT, all_types, INT);

    failed += test_binary(OP_VADD, all_types, VOID);
    failed += test_binary(OP_VSUB, all_types, VOID);
    failed += test_binary(OP_VRSUB, all_types, VOID);    
    failed += test_binary(OP_VMUL, all_types, VOID);
    failed += test_bshift(OP_VSLL, int_types, INT);
    failed += test_bshift(OP_VSRL, int_types, INT);
    failed += test_bshift(OP_VSRA, int_types, INT);
    failed += test_binary(OP_VBAND, all_types, INT);
    failed += test_binary(OP_VBOR, all_types, INT);
    failed += test_binary(OP_VBXOR, all_types, INT);
    failed += test_binary(OP_VCMPLT, all_types, INT);
    failed += test_binary(OP_VCMPLE, all_types, INT);    
    failed += test_binary(OP_VCMPEQ, all_types, INT);
    failed += test_binary(OP_VCMPGT, all_types, INT);
    failed += test_binary(OP_VCMPGE, all_types, INT);    
    failed += test_binary(OP_VCMPNE, all_types, INT);    

    failed += test_imm8(OP_VADDI, int_types, INT);
    failed += test_imm8(OP_VSUBI, int_types, INT);
    failed += test_imm8(OP_VRSUBI, int_types, INT);
    failed += test_imm8(OP_VMULI, int_types, INT);
    failed += test_shift(OP_VSLLI, int_types, INT);
    failed += test_shift(OP_VSRLI, int_types, INT);
    failed += test_shift(OP_VSRAI, int_types, INT);
    failed += test_imm8(OP_VBANDI, all_types, INT);
    failed += test_imm8(OP_VBORI, all_types, INT);
    failed += test_imm8(OP_VBXORI, all_types, INT);    
    failed += test_imm8(OP_VCMPLTI, all_types, INT);
    failed += test_imm8(OP_VCMPLEI, all_types, INT);    
    failed += test_imm8(OP_VCMPEQI, all_types, INT);
    failed += test_imm8(OP_VCMPGTI, all_types, INT);
    failed += test_imm8(OP_VCMPGEI, all_types, INT);    
    failed += test_imm8(OP_VCMPNEI, all_types, INT);    
    
    if (failed) {
	printf("ERROR: %d cases failed\n", failed);
	exit(1);
    }
    printf("OK\n");
    exit(0);
}
