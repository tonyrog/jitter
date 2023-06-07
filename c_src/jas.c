/*
 *  Jitter assembler
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "jitter_types.h"
#include "jitter.h"

#define MAX_PROG_SIZE 1024
#define MAX_LINE_SIZE 1024
#define MAX_OPERANDS  8

// syntax:
// [<label>] [<opcode> [ <operand> [','<operand>]]]  ['//' <comment>]
// '/*' <char> + '\n' '*/'
//
// label = <symbol> ':'
// symbol = [A-Za-z_.$@][A-Za-z0-9_.$@]*
// opcode = <symbol>
// operand = <string>|['-']<symbol>|['-']<integer>
//

// : is added like :::
#define BLANK    " \t"
#define DIGIT    "0:9"
#define XDIGIT   "0:9A:Fa:f"
#define ALPHA    "A:Za:z"
#define ALNUM    "A:Za:z0:9"
// fixme %() should OPCHAR1 / OPCHAR1 ?
// OPCHAR1 "%()-*"
// OPCHAR  "%()-:::"
#define SYMCHAR1 "0:9A:Za:z_.$@%()-*"
#define SYMCHAR  "0:9A:Za:z_.$@%()-:::"

typedef uint64_t bitset64_t[2];

bitset64_t blank;
bitset64_t digit;
bitset64_t xdigit;
bitset64_t alpha;
bitset64_t alnum;
bitset64_t symchar1;
bitset64_t symchar;

#define IS_BLANK(c)  tst_bit(blank, (c))
#define IS_DIGIT(c)  tst_bit(digit, (c))
#define IS_XDIGIT(c) tst_bit(xdigit, (c))
#define IS_ALPHA(c)  tst_bit(alpha, (c))
#define IS_ALNUM(c)  tst_bit(alnum, (c))
#define IS_SYMCHAR1(c) tst_bit(symchar1, (c))
#define IS_SYMCHAR(c)  tst_bit(symchar, (c))

static inline void clr_bits(bitset64_t set)
{
    set[0] = set[1] = 0;
}

static inline void set_bit(bitset64_t set, int n)
{
    int bit = (n & 0x7f); // 0-127
    if (bit < 64)
	set[0] |= ((uint64_t)1 << bit);
    else
	set[1] |= ((uint64_t)1 << (bit-64));
}

static inline int tst_bit(bitset64_t set, int n)
{
    int bit = (n & 0x7f); // 0-127
    if (bit < 64)
	return (set[0] & ((uint64_t)1 << bit)) != 0;
    else
	return (set[1] & ((uint64_t)1 << (bit-64))) != 0;
}

void set_char(bitset64_t set, const char* spec)
{
    int c;
    
    clr_bits(set);
    while((c = *spec++)) {
	if (*spec == ':') {
	    int c2 = *++spec;
	    spec++;	    
	    while (c <= c2) {
		set_bit(set, c);
		c++;
	    }
	}
	else {
	    set_bit(set, c);
	}
    }
}

#ifdef DEBUG
void set_print(bitset64_t set)
{
    int i;
    for (i = 0; i < 128; i++) {
	if (tst_bit(set, i))
	    printf("%c", i);
    }
    printf("\n");
}

void set_debug()
{
    printf("blank = "); set_print(blank);
    printf("digit = "); set_print(digit);
    printf("xdigit = "); set_print(xdigit);
    printf("alpha = "); set_print(alpha);
    printf("alnum = "); set_print(alnum);
    printf("symchar1 = "); set_print(symchar1);
    printf("symchar = "); set_print(symchar);
}
#endif

void init()
{
    set_char(blank, BLANK);
    set_char(digit, DIGIT);
    set_char(xdigit, XDIGIT);
    set_char(alpha, ALPHA);
    set_char(alnum, ALNUM);
    set_char(symchar1, SYMCHAR1);
    set_char(symchar, SYMCHAR);
}

#define SYM(name, id) {(name),(id)}

typedef struct
{
    const char* name;
    int   id;
} symbol_t;

// The opcode is divided in op.type if .type is missing then
// machine size integer type is used (DEFAULT_TYPE_ID)

#if __SIZEOF_PTRDIFF_T__ == 8
#define DEFAULT_TYPE_ID INT64
#elif __SIZEOF_PTRDIFF_T__ == 4
#define DEFAULT_TYPE_ID INT32
#endif

static symbol_t type_id[] =
{
    SYM("u8", UINT8),
    SYM("u16", UINT16),
    SYM("u32", UINT32),
    SYM("u64", UINT64),
    SYM("i8", INT8),
    SYM("i16", UINT16),
    SYM("i32", UINT32),
    SYM("i64", UINT64),
    SYM("f32", FLOAT32),
    SYM("f64", FLOAT64),
};

static symbol_t symbol_id[] =
{
    SYM("nop", OP_NOP),
    SYM("jmp", OP_JMP),
    SYM("jnz.", OP_JNZ),
    SYM("jz.", OP_JZ),
    SYM("ret", OP_RET),
    SYM("neg.", OP_NEG),
    SYM("bnot.", OP_BNOT),
    SYM("inv.", OP_INV),

    SYM("mov.", OP_MOV),
    SYM("movi.", OP_MOVI),
	
    SYM("add.", OP_ADD),
    SYM("addi.", OP_ADDI),
    SYM("vadd.", OP_VADD),
    SYM("vaddi.", OP_VADDI),
	
    SYM("sub.", OP_SUB),
    SYM("subi.", OP_SUBI),
    SYM("vsub.", OP_VSUB),
    SYM("vsubi.", OP_VSUBI),

    SYM("rsub.", OP_RSUB),
    SYM("rsubi.", OP_RSUBI),
    SYM("vrsub.", OP_VRSUB),
    SYM("vrsubi.", OP_VRSUBI),

    SYM("mul.", OP_MUL),
    SYM("muli.", OP_MULI),
    SYM("vmul.", OP_VMUL),
    SYM("vmuli.", OP_VMULI),
	
    SYM("sll.", OP_SLL),
    SYM("slli.", OP_SLLI),
    SYM("vsll.", OP_VSLL),
    SYM("vslli.", OP_VSLLI),

    SYM("srl.", OP_SRL),
    SYM("srli.", OP_SRLI),
    SYM("vsrl.", OP_VSRL),
    SYM("vsrli.", OP_VSRLI),

    SYM("sra.", OP_SRA),
    SYM("srai.", OP_SRAI),
    SYM("vsra.", OP_VSRA),
    SYM("vsrai.", OP_VSRAI),

    SYM("band.", OP_BAND),
    SYM("bandi.", OP_BANDI),
    SYM("vband.", OP_VBAND),
    SYM("vbandi.", OP_VBANDI),
	
    SYM("bor.", OP_BOR),
    SYM("bori.", OP_BORI),
    SYM("vbor.", OP_VBOR),
    SYM("vbori.", OP_VBORI),
	
    SYM("bxor.", OP_BXOR),
    SYM("bxori.", OP_BXORI),
    SYM("vbxor.", OP_VBXOR),
    SYM("vbxori.", OP_VBXORI),
	
    SYM("cmplt.", OP_CMPLT),
    SYM("cmplti.", OP_CMPLTI),
    SYM("vcmplt.", OP_VCMPLT),
    SYM("vcmplti.", OP_VCMPLTI),
	
    SYM("cmple.", OP_CMPLE),
    SYM("cmplei.", OP_CMPLEI),
    SYM("vcmple.", OP_VCMPLE),
    SYM("vcmplei.", OP_VCMPLEI),

    SYM("cmpeq.", OP_CMPEQ),
    SYM("cmpeqi.", OP_CMPEQI),
    SYM("vcmpeq.", OP_VCMPEQ),
    SYM("vcmpeqi.", OP_VCMPEQI),

    SYM("cmpgt.", OP_CMPGT),
    SYM("cmpgti.", OP_CMPGTI),
    SYM("vcmpgt.", OP_VCMPGT),
    SYM("vcmpgti.", OP_VCMPGTI),
	
    SYM("cmpge.", OP_CMPGE),
    SYM("cmpgei.", OP_CMPGEI),
    SYM("vcmpge.", OP_VCMPGE),
    SYM("vcmpgei.", OP_VCMPGEI),
	
    SYM("cmpne.", OP_CMPNE),
    SYM("cmpnei.", OP_CMPNEI),
    SYM("vcmpne.", OP_VCMPNE),
    SYM("vcmpnei.", OP_VCMPNEI),

    SYM("vret", OP_VRET),
    SYM("vmov.", OP_VMOV),
    SYM("vmovi.", OP_VMOVI),
    SYM("vneg.", OP_VNEG),
    SYM("vbnot.", OP_VBNOT),
    SYM("vinv.", OP_VINV),

// registers
    SYM("%v0", 0),
    SYM("%v1", 1),
    SYM("%v2", 2),
    SYM("%v3", 3),
    SYM("%v4", 4),
    SYM("%v5", 5),
    SYM("%v6", 6),
    SYM("%v7", 7),
    SYM("%v8", 8),
    SYM("%v9", 9),
    SYM("%v10", 10),
    SYM("%v11", 11),
    SYM("%v12", 12),
    SYM("%v13", 13),
    SYM("%v14", 14),
    SYM("%v15", 15),

    SYM("%r0", 0),
    SYM("%r1", 1),
    SYM("%r2", 2),
    SYM("%r3", 3),
    SYM("%r4", 4),
    SYM("%r5", 5),
    SYM("%r6", 6),
    SYM("%r7", 7),
    SYM("%r8", 8),
    SYM("%r9", 9),
    SYM("%r10", 10),
    SYM("%r11", 11),
    SYM("%r12", 12),
    SYM("%r13", 13),
    SYM("%r14", 14),
    SYM("%r15", 15),
    
    SYM(NULL, -1),
};


#define EOL     0
#define SYMBOL  1
#define STRING  2
#define CHAR    3

#define SYM_NONE      0x0   // just a symbol
#define SYM_LABEL     0x1
#define SYM_STRING    0x2
#define SYM_INTEGER   0x3   // plain integer
#define SYM_REGISTER  0x4   // %rsp/%cl/%r1/%v1 ...
#define SYM_IMMEDIATE 0x5   // $123

typedef struct {
    char* name;   // The name / string
    int   len;    // name len
    int   type;   // SYM_xxx
    int   id;     // token id lookup value (0 == undefined)
}  token_t;

static inline const char* operand_flag(token_t* tp)
{
    switch(tp->type) {
    case SYM_LABEL: return ":lbl";
    case SYM_STRING: return ":str";
    case SYM_INTEGER: return ":int";
    case SYM_REGISTER: return ":reg";
    case SYM_IMMEDIATE: return ":imm";
    default: return "";
    }
}
//
// compare characters in str with name
// if name. and str. then this is extended (type) later,
// if name. and str  then use default extenstion
//
// add.  match add.u8
// add.  match add.f32
// add.  match add
// add   do NOT match add.64
//
static int eqstrn(const char* name, char* str, int len)
{
    int c;
    while(len-- && ((c = *name++) == *str++) && (c != '.'))
	;
    if (len < 0) {
	if (*name == '\0') return 1; // exact match
	if (*name == '.') return 1;  // partial
    }
    else if ((*name == '\0') && (c == '.')) {
	return 1;  // partial match "add."  and "add.u32"
    }
    return 0;
}

// fixme: hash lookup
static int lookup(symbol_t* tab, char* name, int len)
{
    int i = 0;
    while(tab[i].name) {
	if (eqstrn(tab[i].name, name, len)) {
	    return tab[i].id;
	}
	i++;
    }
    return -1;
}

static inline char* scan_token(char* ptr, int* twp, token_t* tp)
{
    tp->type = 0;
    tp->id   = -1;
    while(IS_BLANK(*ptr)) ptr++;
    
    if (IS_SYMCHAR1(*ptr)) {
	int dcount = IS_DIGIT(*ptr);
	int len    = 1;
	tp->name = ptr++;
	while(IS_SYMCHAR(*ptr)) {
	    dcount += IS_DIGIT(*ptr);
	    len++;
	    ptr++;
	}
	tp->len = len;
	*twp = SYMBOL;
	tp->type = SYM_NONE;
	tp->id = lookup(symbol_id,tp->name, tp->len);
	if (dcount == len) // all chars are digits
	    tp->type = SYM_INTEGER;
	else if (tp->name[len-1] == ':') {
	    tp->type = SYM_LABEL;
	}
	else if (tp->name[0] == '%') {
	    tp->type = SYM_REGISTER;
	}
	else if (dcount == len-1) {  // all but one are digits
	    if (tp->name[0] == '-') tp->type = SYM_INTEGER; // negative number
	    if (tp->name[0] == '$') tp->type = SYM_IMMEDIATE;
	}
	else if (dcount == len-2) {
	    if ((tp->name[0] == '$') && (tp->name[1] == '-'))
		tp->type = SYM_IMMEDIATE;
	}
    }
    else if (*ptr == '"') {
	tp->name = ptr+1;
	tp->type = SYM_STRING;
	ptr++;
	while((*ptr != '"') && (*ptr != '\n') && (*ptr != '\0')) ptr++;
	tp->len = ptr - tp->name;
	*twp = STRING;
    }
    else if ((*ptr == '/') && (*(ptr+1) == '/')) {
	tp->name = ptr;
	tp->len = 2;
	*twp  = EOL;
    }
    else if ((*ptr == '/') && (*(ptr+1) == '*')) {
	tp->name = ptr;
	tp->len = 2;	
	*twp  = EOL;
    }
    else if (*ptr == '\n') {
	tp->name = ptr;
	tp->len = 1;
	*twp = EOL;
    }
    else if (*ptr == '\0') {
	tp->name = ptr;
	tp->len = 0;	
	*twp = EOL;
    }
    else {
	tp->name = ptr++;
	tp->len = 1;
	*twp = CHAR;
    }
    return ptr;
}

int pp = 0;
instr_t prog[MAX_PROG_SIZE];

void emit_instruction(int line, token_t label, token_t instruction,
		      token_t* operand, size_t n)
{
    int i;
    FILE* fout = stdout;
    int opcode = -1;
    
    fprintf(fout, "%4d| ", line);
    if (label.name)
	fprintf(fout, "%s:@%d ", label.name, pp);
    else
	fprintf(fout, "    ");
    if (instruction.name) {
	if ((opcode = instruction.id) >= 0) {
	    char* ptr;
	    prog[pp].op = opcode;
	    prog[pp].type = DEFAULT_TYPE_ID;
	    if ((ptr = strchr(instruction.name, '.')) != NULL) {
		int len = strlen(++ptr);
		int t;
		if ((t = lookup(type_id, ptr, len)) < 0)
		    opcode = -1;
		else
		    prog[pp].type = t;
	    }
	}
	fprintf(fout, "{%s:%d:%d} ",
		instruction.name, prog[pp].op, prog[pp].type);
    }

    if (n == 1) {
	if ((operand[0].type == SYM_REGISTER))        // ret %ri
	    prog[pp].rd = operand[0].id;
	else if (operand[0].type == SYM_IMMEDIATE)
	    prog[pp].imm12 = atoi(operand[0].name+1);  // jmp L
    }
    else if (n == 2) {
	if ((operand[0].type == SYM_REGISTER) &&
	    (operand[1].type == SYM_REGISTER)) {    // neg, mov, vmov
	    prog[pp].rd = operand[0].id;
	    prog[pp].ri = operand[1].id;
	}
	else if ((operand[0].type == SYM_REGISTER) &&  // movi, vmovi
		 (operand[1].type == SYM_IMMEDIATE)) {
	    prog[pp].rd = operand[0].id;
	    prog[pp].imm12 = atoi(operand[1].name+1);
	}
    }
    else if (n == 3) {
	if ((operand[0].type == SYM_REGISTER) &&
	    (operand[1].type == SYM_REGISTER) &&
	    (operand[2].type == SYM_REGISTER)) {

	    prog[pp].rd = operand[0].id;
	    prog[pp].ri = operand[1].id;
	    prog[pp].rj = operand[2].id;
	}
	else if ((operand[0].type == SYM_REGISTER) &&
		 (operand[1].type == SYM_REGISTER) &&
		 (operand[2].type == SYM_IMMEDIATE)) {
	    prog[pp].rd = operand[0].id;
	    prog[pp].ri = operand[1].id;
	    prog[pp].imm8 = atoi(operand[2].name+1);
	}
    }
    
    for (i = 0; i < (int)n; i++) {
	if (operand[i].type == SYM_STRING)
	    fprintf(fout, "\"%s\" ", operand[i].name);
	else
	    fprintf(fout, "[%s%s:%d] ", operand[i].name,
		    operand_flag(&operand[i]),
		    operand[i].id);
    }
    fprintf(fout, "\n");

    if (opcode >= 0) pp++;
}

char* assemble(FILE* fin, int* linenop)
{
    char* ptr;
    char  input[MAX_LINE_SIZE];
    int   line = 1;
    int   comment = 0;
    int   i;

    while((ptr = fgets(input, sizeof(input), fin)) != NULL) {
	token_t label;
	token_t instruction;
	token_t operand[MAX_OPERANDS];
	token_t tok;
	int tw;

	label.name = NULL;
	instruction.name = NULL;

	if (comment) {
	    while(*ptr) {
		if ((*ptr == '*') && (*(ptr+1) == '/')) {
		    comment = 0;
		    ptr += 2;
		    break;
		}
		ptr++;
	    }
	    line++;
	    continue;
	}
	ptr = scan_token(ptr, &tw, &tok);
	if ((tw == SYMBOL) && (tok.type == SYM_LABEL)) {
	    label = tok;
	    *(ptr-1) = '\0'; // poke ':'
	    ptr = scan_token(ptr, &tw, &tok);  
	}
	if (tw == SYMBOL) {  // instruction
	    instruction = tok;
	    if (IS_BLANK(*ptr)) 
		*ptr++ = '\0';
	}
	// scan for operands
	i = 0;
	while((i < MAX_OPERANDS) && (tw != EOL)) {
	    ptr = scan_token(ptr, &tw, &tok);
	    switch(tw) {
	    case STRING:
		if (*ptr == '"') *ptr++ = '\0';
		operand[i++] = tok;
		break;
	    case SYMBOL:
		operand[i++] = tok;
		if (IS_BLANK(*ptr)) *ptr++ = '\0';
		break;
	    case EOL:
		if (i != 0) goto syntax_error;
		break;
	    default:
		goto syntax_error;
	    }
	    ptr = scan_token(ptr, &tw, &tok);
	    if ((tw == CHAR) && (tok.name[0] == ',')) {
		tok.name[0] = '\0';
		continue;
	    }
	    if ((tw == EOL) && (tok.name[0] == '\n'))
		tok.name[0] = '\0';
	    goto line_done;
	}
    line_done:
	emit_instruction(line, label, instruction, operand, i);
	if ((tw == EOL) && (tok.name[0]=='/') && (tok.name[1]=='*'))
	    comment = 1;
	line++;
    }
    *linenop = line;    
    return NULL;
syntax_error:
    *linenop = line;
    return ptr;
}


int main(int argc, char** argv)
{
    FILE* fin = stdin;
    char* filename = "*stdin*";
    char* errptr;
    int line;
    
    if (argc > 1) {
	filename = argv[1];
	if ((fin = fopen(argv[1], "r")) == NULL) {
	    fprintf(stderr, "unable to open %s\n", argv[1]);
	    exit(1);
	}
    }
    init();
    // set_debug()

    if ((errptr = assemble(fin, &line)) != NULL) {
	fprintf(stderr, "%s:%d: error: %s\n", filename, line, errptr);
	exit(1);
    }
    exit(0);
}
