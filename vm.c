#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm_dbg.h"

#define NOPS (16)

#define OPC(i) ((i) >> 12)
#define DR(i) (((i) >> 9) & 0x7)
#define SR1(i) (((i) >> 6) & 0x7)
#define SR2(i) ((i) & 0x7)
#define FIMM(i) ((i >> 5) & 01)
#define IMM(i) ((i) & 0x1F)
#define SEXTIMM(i) sext(IMM(i), 5)
#define FCND(i) (((i) >> 9) & 0x7)
#define POFF(i) sext((i) & 0x3F, 6)
#define POFF9(i) sext((i) & 0x1FF, 9)
#define POFF11(i) sext((i) & 0x7FF, 11)
#define FL(i) (((i) >> 11) & 1)
#define BR(i) (((i) >> 6) & 0x7)
#define TRP(i) ((i) & 0xFF)

/* New OS declarations */

// OS bookkeeping constants
#define PAGE_SIZE       (4096)  // Page size in bytes
#define OS_MEM_SIZE     (2)     // OS Region size. Also the start of the page tables' page
#define Cur_Proc_ID     (0)     // id of the current process
#define Proc_Count      (1)     // total number of processes, including ones that finished executing.
#define OS_STATUS       (2)     // Bit 0 shows whether the PCB list is full or not
#define OS_FREE_BITMAP  (3)     // Bitmap for free pages

// Process list and PCB related constants
#define PCB_SIZE  (3)  // Number of fields in a PCB
#define PID_PCB   (0)  // Holds the pid for a process
#define PC_PCB    (1)  // Value of the program counter for the process
#define PTBR_PCB  (2)  // Page table base register for the process

#define CODE_SIZE       (2)  // Number of pages for the code segment
#define HEAP_INIT_SIZE  (2)  // Number of pages for the heap segment initially

bool running = true;

typedef void (*op_ex_f)(uint16_t i);
typedef void (*trp_ex_f)();

enum { trp_offset = 0x20 };
enum regist { R0 = 0, R1, R2, R3, R4, R5, R6, R7, RPC, RCND, PTBR, RCNT };
enum flags { FP = 1 << 0, FZ = 1 << 1, FN = 1 << 2 };

uint16_t mem[UINT16_MAX] = {0};
uint16_t reg[RCNT] = {0};
uint16_t PC_START = 0x3000;

void initOS();
int createProc(char *fname, char *hname);
void loadProc(uint16_t pid);
uint16_t allocMem(uint16_t ptbr, uint16_t vpn, uint16_t read, uint16_t write);  // Can use 'bool' instead
int freeMem(uint16_t ptr, uint16_t ptbr);
static inline uint16_t mr(uint16_t address);
static inline void mw(uint16_t address, uint16_t val);
static inline void tbrk();
static inline void thalt();
static inline void tyld();
static inline void trap(uint16_t i);

static inline uint16_t sext(uint16_t n, int b) { return ((n >> (b - 1)) & 1) ? (n | (0xFFFF << b)) : n; }
static inline void uf(enum regist r) {
    if (reg[r] == 0)
        reg[RCND] = FZ;
    else if (reg[r] >> 15)
        reg[RCND] = FN;
    else
        reg[RCND] = FP;
}
static inline void add(uint16_t i)  { reg[DR(i)] = reg[SR1(i)] + (FIMM(i) ? SEXTIMM(i) : reg[SR2(i)]); uf(DR(i)); }
static inline void and(uint16_t i)  { reg[DR(i)] = reg[SR1(i)] & (FIMM(i) ? SEXTIMM(i) : reg[SR2(i)]); uf(DR(i)); }
static inline void ldi(uint16_t i)  { reg[DR(i)] = mr(mr(reg[RPC]+POFF9(i))); uf(DR(i)); }
static inline void not(uint16_t i)  { reg[DR(i)]=~reg[SR1(i)]; uf(DR(i)); }
static inline void br(uint16_t i)   { if (reg[RCND] & FCND(i)) { reg[RPC] += POFF9(i); } }
static inline void jsr(uint16_t i)  { reg[R7] = reg[RPC]; reg[RPC] = (FL(i)) ? reg[RPC] + POFF11(i) : reg[BR(i)]; }
static inline void jmp(uint16_t i)  { reg[RPC] = reg[BR(i)]; }
static inline void ld(uint16_t i)   { reg[DR(i)] = mr(reg[RPC] + POFF9(i)); uf(DR(i)); }
static inline void ldr(uint16_t i)  { reg[DR(i)] = mr(reg[SR1(i)] + POFF(i)); uf(DR(i)); }
static inline void lea(uint16_t i)  { reg[DR(i)] =reg[RPC] + POFF9(i); uf(DR(i)); }
static inline void st(uint16_t i)   { mw(reg[RPC] + POFF9(i), reg[DR(i)]); }
static inline void sti(uint16_t i)  { mw(mr(reg[RPC] + POFF9(i)), reg[DR(i)]); }
static inline void str(uint16_t i)  { mw(reg[SR1(i)] + POFF(i), reg[DR(i)]); }
static inline void rti(uint16_t i)  {} // unused
static inline void res(uint16_t i)  {} // unused
static inline void tgetc()        { reg[R0] = getchar(); }
static inline void tout()         { fprintf(stdout, "%c", (char)reg[R0]); }
static inline void tputs() {
  uint16_t *p = mem + reg[R0];
  while(*p) {
    fprintf(stdout, "%c", (char) *p);
    p++;
  }
}
static inline void tin()      { reg[R0] = getchar(); fprintf(stdout, "%c", reg[R0]); }
static inline void tputsp()   { /* Not Implemented */ }
static inline void tinu16()   { fscanf(stdin, "%hu", &reg[R0]); }
static inline void toutu16()  { fprintf(stdout, "%hu\n", reg[R0]); }

trp_ex_f trp_ex[10] = {tgetc, tout, tputs, tin, tputsp, thalt, tinu16, toutu16, tyld, tbrk};
static inline void trap(uint16_t i) { trp_ex[TRP(i) - trp_offset](); }
op_ex_f op_ex[NOPS] = {/*0*/ br, add, ld, st, jsr, and, ldr, str, rti, not, ldi, sti, jmp, res, lea, trap};

/**
  * Load an image file into memory.
  * @param fname the name of the file to load
  * @param offsets the offsets into memory to load the file
  * @param size the size of the file to load
*/
void ld_img(char *fname, uint16_t *offsets, uint16_t size) {
    FILE *in = fopen(fname, "rb");
    if (NULL == in) {
        fprintf(stderr, "Cannot open file %s.\n", fname);
        exit(1);
    }

    for (uint16_t s = 0; s < size; s += PAGE_SIZE) {
        uint16_t *p = mem + offsets[s / PAGE_SIZE];
        uint16_t writeSize = (size - s) > PAGE_SIZE ? PAGE_SIZE : (size - s);
        fread(p, sizeof(uint16_t), (writeSize), in);
    }
    
    fclose(in);
}

void run(char *code, char *heap) {
  while (running) {
    uint16_t i = mr(reg[RPC]++);
    op_ex[OPC(i)](i);
  }
}

// YOUR CODE STARTS HERE
uint16_t countFreePages() {
    uint16_t freePages = 0;
    for (uint16_t pfn = 0; pfn < 32; pfn++) {
        if (mem[OS_FREE_BITMAP] & (1 << pfn)) {
            freePages++;
        }
    }
    return freePages;
}

void initOS() {
    mem[Cur_Proc_ID] = 0xFFFF;
    mem[Proc_Count] = 0;        
    mem[OS_STATUS] = 0x0000;    

    mem[3] = 0x1FFF;
    mem[4] = 0xFFFF;
}


int createProc(char *fname, char *hname) {
    if (mem[OS_STATUS] & 1) {
        fprintf(stderr, "The OS memory region is full. Cannot create a new PCB.\n");
        return 0;
    }
    if (countFreePages() < CODE_SIZE) {
        fprintf(stderr, "Cannot create code segment.\n");
        return 0;
    }
    if (countFreePages() < HEAP_INIT_SIZE) {
        fprintf(stderr, "Cannot create heap segment.\n");
        return 0;
    }
    uint16_t pid = mem[Proc_Count];
    mem[Proc_Count]++;

    uint16_t pcb_base = 12 + pid * PCB_SIZE;
    mem[pcb_base + PID_PCB] = pid;
    mem[pcb_base + PC_PCB] = 0x3000;
    mem[pcb_base + PTBR_PCB] = 0;

    uint16_t ptbr = allocMem(12, 0, UINT16_MAX, UINT16_MAX);
    if (ptbr == 0) {
        mem[pcb_base + PID_PCB] = 0xFFFF;
        return 0;
    }
    mem[pcb_base + PTBR_PCB] = ptbr;
    uint16_t codePages[CODE_SIZE];
    for (uint16_t vpn = 0; vpn < CODE_SIZE; vpn++) {
        codePages[vpn] = allocMem(ptbr, vpn, UINT16_MAX, 0);
        if (codePages[vpn] == 0) {
            fprintf(stderr, "Cannot create code segment.\n");

            for (uint16_t i = 0; i < vpn; i++) {
                freeMem(i, ptbr);
            }
            freeMem(ptbr, 12);
            mem[pcb_base + PID_PCB] = 0xFFFF;
            return 0;
        }
    }
    ld_img(fname, codePages, CODE_SIZE * PAGE_SIZE);

    uint16_t heapPages[HEAP_INIT_SIZE];
    for (uint16_t vpn = CODE_SIZE; vpn < CODE_SIZE + HEAP_INIT_SIZE; vpn++) {
        heapPages[vpn - CODE_SIZE] = allocMem(ptbr, vpn, UINT16_MAX, UINT16_MAX);
        if (heapPages[vpn - CODE_SIZE] == 0) {
            fprintf(stderr, "Cannot create heap segment.\n");

            for (uint16_t i = 0; i < vpn; i++) {
                freeMem(i, ptbr);
            }
            freeMem(ptbr, 12);
            mem[pcb_base + PID_PCB] = 0xFFFF;
            return 0;
        }
    }
    ld_img(hname, heapPages, HEAP_INIT_SIZE * PAGE_SIZE);

    return 1;
}

void loadProc(uint16_t pid) {
    uint16_t pcb_base = 12 + pid * PCB_SIZE;

    if (mem[pcb_base + PID_PCB] == 0xFFFF) {
        return;
    }
    reg[RPC] = mem[pcb_base + PC_PCB];
    reg[PTBR] = mem[pcb_base + PTBR_PCB];
    mem[Cur_Proc_ID] = pid;
}


uint16_t findFirstFreeBit(uint16_t bitmap) {
    for (int16_t pfn = 15; pfn >= 0; pfn--) {
        if (bitmap & (1 << pfn)) {
            return pfn;                     
        }
    }
    return UINT16_MAX;
}

uint16_t allocMem(uint16_t ptbr, uint16_t vpn, uint16_t read, uint16_t write) {
    uint16_t *page_table = mem + ptbr;

    if (page_table[vpn] & (1 << 0)) {
        return 0;
    }

    uint16_t pfn = findFirstFreeBit(mem[OS_FREE_BITMAP]);
    if (pfn == UINT16_MAX) {
        return 0;
    }
    mem[OS_FREE_BITMAP] &= ~(1 << pfn);
    uint16_t pte = ((15-pfn) & 0x1F) << 11;
    pte |= (1 << 0);               
    if (read == UINT16_MAX) pte |= (1 << 1);      
    if (write ==UINT16_MAX) pte |= (1 << 2);      
    page_table[vpn] = pte;           

    return pte;
}


int freeMem(uint16_t vpn, uint16_t ptbr) {
    uint16_t *page_table = mem + ptbr;
    uint16_t pte = page_table[vpn];

    if (!(pte & (1 << 0))) {
        return 0;
    }
    uint16_t pfn = (pte >> 11) & 0x1F; 
    mem[OS_FREE_BITMAP] |= (1 << (15 - pfn));
    page_table[vpn] &= ~(1 << 0);
    return 1;
}

// Instructions to implement
static inline void tbrk() {
    uint16_t r0 = reg[R0];
    uint16_t vpn = (r0 >> 11) & 0x1F;
    bool allocation = (r0 & 1);      
    bool write = (r0 & (1 << 1)); 
    bool read = (r0 & (1 << 2));

    uint16_t ptbr = reg[PTBR];
    uint16_t *page_table = mem + ptbr;
    uint16_t pte = page_table[vpn];

    if(allocation){
        fprintf(stdout, "Heap increase requested by process %u.\n", reg[Cur_Proc_ID]);
        if(pte & 0x8000){
            return;
        }
        for (uint16_t pfn = 0; pfn < 32; pfn++) {
            if (mem[OS_FREE_BITMAP] & (1 << pfn)) {
                mem[OS_FREE_BITMAP] &= ~(1 << pfn);
                page_table[vpn] = (pfn) | 0x8000; 
                if (read) {
                    page_table[vpn] |= (1 << 1);
                }
                if (write) {
                    page_table[vpn] |= (1 << 2);
                }
                return;
            }
        }
        return;
    }
    else{
        fprintf(stdout, "Heap decrease requested by process %u.\n", reg[Cur_Proc_ID]);

        if (!(pte & 0x8000)) {
            return;
        }

        uint16_t pfn = pte & 0x1F;
        mem[OS_FREE_BITMAP] |= (1 << pfn);
        page_table[vpn] &= ~0x8000;
    }
}

static inline void tyld() {
    uint16_t current_pid = reg[Cur_Proc_ID];
    uint16_t pcb_base_addr = OS_MEM_SIZE + current_pid * PCB_SIZE;
    mem[pcb_base_addr + PC_PCB] = reg[RPC];
    mem[pcb_base_addr + PTBR_PCB] = reg[PTBR];

    uint16_t process_count = reg[Proc_Count];
    uint16_t next_pid = (current_pid + 1 ) % process_count;

    for(uint16_t i = 0; i < process_count; i++){
        uint16_t candidate_pid = (next_pid + i) % process_count;
        uint16_t candidate_base = OS_MEM_SIZE + candidate_pid * PCB_SIZE;

        if(mem[candidate_base + PID_PCB] != 0xffff){
            reg[Cur_Proc_ID] = candidate_pid;
            reg[RPC] = mem[candidate_base + PC_PCB];
            reg[PTBR] = mem[candidate_base + PTBR_PCB];

            fprintf(stdout, "We are switching from process %u to %u.\n", current_pid, candidate_pid);
            return;
        }
    }
    fprintf(stdout, "We are switching from process %u to %u.\n", current_pid, current_pid);
}

static inline void thalt() {
    uint16_t current_pid = reg[Cur_Proc_ID]; 
    uint16_t pcb_address = 12 + (current_pid * PCB_SIZE);
    uint16_t ptbr = mem[pcb_address + PTBR_PCB];

    for(uint16_t vpn = 0; vpn < 32; vpn++){
        freeMem(vpn, ptbr);
    }

    uint16_t pfn =(ptbr >> 11);
    mem[OS_FREE_BITMAP] |= (1 << pfn);

    mem[pcb_address + PID_PCB] = 0xffff;

    uint16_t total_processes = reg[Proc_Count];
    bool found_runnable = false;
    for (uint16_t pid = 0; pid < total_processes; pid++) {
        uint16_t pcb_addr = 12 + pid * PCB_SIZE;
        if (mem[pcb_addr + PID_PCB] != 0xFFFF) { 
            found_runnable = true;
            break;
        }
    }
    if (!found_runnable) {
        running = false;
        return;
    }
    tyld();

}

static inline uint16_t mr(uint16_t address) {
    uint16_t vpn =(address >> 11);
    uint16_t offset = (address & 0x7ff);

    if(vpn < 12){
        fprintf(stderr, "Segmentation fault.\n");
        exit(EXIT_FAILURE);
    }

    uint16_t ptbr = reg[PTBR];
    uint16_t *page_table = mem + ptbr;
    uint16_t pte = page_table[vpn];

    if (!(pte & (1 << 0))) {            
        fprintf(stderr, "Segmentation fault inside free space.\n");
        exit(EXIT_FAILURE);
    }
    if (!(pte & (1 << 1))) {          
        fprintf(stderr, "Cannot write to a read-only page.\n");
        return 0;
    }
    uint16_t pfn = (pte & 0x1f);
    uint16_t physical_address = (pfn << 11) | offset;

    return mem[physical_address];
}
static inline void mw(uint16_t address, uint16_t val) {
    uint16_t vpn = (address >> 10);        
    uint16_t offset = (address & 0x7FF);
    

    if(vpn < 12){
        fprintf(stderr, "Segmentation fault.\n");
        exit(EXIT_FAILURE);
    }

    uint16_t ptbr = reg[PTBR];           
    uint16_t *page_table = mem + ptbr;     
    uint16_t pte = page_table[vpn];

    if (!(pte & (1 << 2))) {             
        fprintf(stderr, "Cannot write to a read-only page.\n");
        exit(EXIT_FAILURE);;
    }

    if (!(pte & (1 << 0))) {                
        fprintf(stderr, "Segmentation fault inside free space.\n");
        exit(EXIT_FAILURE);
    }

    uint16_t pfn = (pte & 0x1F);          
    uint16_t physical_address = (pfn << 11) | offset;

    mem[physical_address] = val;   
}

// YOUR CODE ENDS HERE
