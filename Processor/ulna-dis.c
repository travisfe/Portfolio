/*
Travis Earely hw4 te4@umbc.edu
https://stackoverflow.com/questions/3501338/c-read-file-line-by-line
https://stackoverflow.com/questions/10156409/convert-hex-string-char-to-int

BranchType: See Branch control unit in hw4.circ

ExtSel: 0 == PC + 1, 1 == PC + SignExtend(Imm11), 2 == PC + SignExtend(Imm8),
3 == PC, 4 == R[A] 

MemToReg: 0 == alu, 1 == shifter, 2 == Memory, 3 == direct from PCNext

Movis 0 == do nothing, 1 == left shift RegB Imm8 by 8

SetHalt when 1 sets adress 0xFFFF to 0xFFFF


1. 
    addi with the register you want to move and 0
    Rotate 0 bits
    Rotate 5 bits
    Arithmetic Shift 0 bits
    Logical Shift 0 bits


2. Every addition is signed two's complements, so doing an add (immediate) with the immediate being a negative number gives the same behaviour as sub (immediate)

*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

char * cmd[] = {"or", "add", "and", "stw", "br", "sub", "neg", "ldw", "sha",
		"addi", "shl", "stwi", "DNC", "DNC", "rot", "ldwi", "ori",
		"inci", "andi", "DNC", "movi", "cmp", "movis", "call", "b",
		"b.gt", "b.eq", "b.ge", "b.lt", "b.ne", "b.le", "halt"};


union u_init {
  uint64_t num;
  struct {
    unsigned rest: 11;
    unsigned type: 5;
  } bits;

};

union u_R {
  uint64_t num;
  struct {
    unsigned dnc: 2;
    unsigned RegB: 3;
    unsigned RegA: 3;
    unsigned RegW: 3;
    unsigned type: 5;
  } bits;

};

union u_D {
  uint64_t num;
  struct {
    signed Imm5: 5;
    unsigned RegA: 3;
    unsigned RegW: 3;
    unsigned type: 5;
  } bits;

};

union u_I {
  uint64_t num;
  struct {
    signed Imm8: 8;
    unsigned RegW: 3;
    unsigned type: 5;
  } bits;

};

union u_B {
  uint64_t num;
  struct {
    signed Imm11: 11;
    unsigned type: 5;
  } bits;

};

struct {
  char ALUBSrc[10];
  char ALUOp[10];
  char ShiftOP[10];
  char BranchType[10];
  char ExtSel[10];
  char MemRead[10];
  char MemToReg[10];
  char MemWrite[10];
  char RegA[10];
  char RegB[10];
  char RegW[10];
  char RegWrite[10];
  char UpdateCond[10];
  char SetHalt[10];
  char Movis[10];

} sigs[] = {
  {"RegB\0", "OR\0", "X\0", "0\0", "0\0", "0\0", "0\0", "0\0", "R[A]\0", "R[B]\0", "R[W]\0", "1\0", "0\0", "0\0", "0\0"},      //r
  {"RegB\0", "ADD\0", "X\0", "0\0", "0\0", "0\0", "0\0", "0\0", "R[A]\0", "R[B]\0", "R[W]\0", "1\0", "1\0", "0\0", "0\0"},      //add
  {"RegB\0", "AND\0", "X\0", "0\0", "0\0", "0\0", "0\0", "0\0", "R[A]\0", "R[B]\0", "R[W]\0", "1\0", "0\0", "0\0", "0\0"},      //and
  {"RegB\0", "ADD\0", "X\0", "0\0", "0\0", "0\0", "X\0", "1\0", "R[A]\0", "R[B]\0", "R[W]\0", "0\0", "0\0", "0\0", "0\0"},      //or
  {"RegB\0", "OR\0", "X\0", "0\0", "0\0", "0\0", "0\0", "0\0", "R[A]\0", "R[B]\0", "R[W]\0", "1\0", "0\0", "0\0", "0\0"},      //stw
  {"X\0", "X\0", "X\0", "7\0", "4\0", "0\0", "X\0", "0\0", "R[A]\0", "X\0", "X\0", "X\0", "0\0", "0\0", "0\0"},                //br
  {"RegB\0", "SUB\0", "X\0", "0\0", "0\0", "0\0", "0\0", "0\0", "R[A]\0", "R[B]\0", "R[W]\0", "1\0", "1\0", "0\0", "0\0"},      //sub
  {"RegB\0", "NOT\0", "X\0", "0\0", "0\0", "0\0", "0\0", "0\0", "R[A]\0", "R[B]\0", "R[W]\0", "1\0", "0\0", "0\0", "0\0"},      //neg
  {"RegB\0", "ADD\0", "X\0", "0\0", "0\0", "1\0", "2\0", "0\0", "R[A]\0", "R[B]\0", "R[W]\0", "1\0", "0\0", "0\0", "0\0"},      //ldw
  {"RegB\0", "X\0", "A-Shift\0", "0\0", "0\0", "0\0", "1\0", "0\0", "R[A]\0", "X\0", "R[W]\0", "1\0", "0\0", "0\0", "0\0"},      //sha
  {"Imm5\0", "ADD\0", "X\0", "0\0", "0\0", "0\0", "0\0", "0\0", "R[A]\0", "R[B]\0", "R[W]\0", "1\0", "1\0", "0\0", "0\0"},      //addi
  {"Imm5\0", "X\0", "L-Shift\0", "0\0", "0\0", "0\0", "1\0", "0\0", "R[A]\0", "X\0", "R[W]\0", "1\0", "0\0", "0\0", "0\0"},      //shl
  {"Imm5\0", "ADD\0", "X\0", "0\0", "0\0", "0\0", "X\0", "1\0", "R[A]\0", "X\0", "R[W]\0", "0\0", "0\0", "0\0", "0\0"},      //stwi
  {"X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0"},      //DNC
  {"X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0"},      //DNC
  {"Imm5\0", "X\0", "Rotate\0", "0\0", "0\0", "0\0", "1\0", "0\0", "R[A]\0", "X\0", "R[W]\0", "1\0", "0\0", "0\0", "0\0"},      //rot 
  {"Imm5\0", "ADD\0", "X\0", "0\0", "0\0", "1\0", "2\0", "0\0", "R[A]\0", "X\0", "R[W]\0", "1\0", "0\0", "0\0", "0\0"},      //ldwi
  {"Imm8\0", "OR\0", "X\0", "0\0", "0\0", "0\0", "0\0", "0\0", "R[A]\0", "X\0", "R[W]\0", "1\0", "0\0", "0\0", "0\0"},      //ori
  {"Imm5\0", "ADD\0", "X\0", "0\0", "0\0", "0\0", "0\0", "0\0", "R[A]\0", "X\0", "R[W]\0", "1\0", "1\0", "0\0", "0\0"},      //inci
  {"Imm8\0", "AND\0", "X\0", "0\0", "0\0", "0\0", "0\0", "0\0", "R[A]\0", "X\0", "R[W]\0", "1\0", "0\0", "0\0", "0\0"},      //andi
  {"X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0", "X\0"},      //DNC
  {"Imm8\0", "X\0", "X\0", "0\0", "0\0", "0\0", "X\0", "0\0", "0\0", "X\0", "R[W]\0", "1\0", "0\0", "0\0", "0\0"},      //movi
  {"Imm8\0", "SUB\0", "X\0", "0\0", "0\0", "0\0", "X\0", "0\0", "R[W]\0", "X\0", "X\0", "0\0", "1\0", "0\0", "0\0"},      //cmp
  {"Imm8\0", "OR\0", "X\0", "0\0", "0\0", "0\0", "0\0", "0\0", "R[W]\0", "X\0", "R[W]\0", "1\0", "0\0", "0\0", "1\0"},      //movis
  {"Imm11\0", "X\0", "X\0", "7\0", "2\0", "0\0", "3\0", "0\0", "X\0", "X\0", "R[W]\0", "1\0", "0\0", "0\0", "0\0"},      //call
  {"X\0", "X\0", "X\0", "7\0", "1\0", "0\0", "X\0", "0\0", "X\0", "X\0", "X\0", "0\0", "0\0", "0\0", "0\0"},      //b
  {"Imm8\0", "SUB\0", "X\0", "2\0", "1\0", "X\0", "0\0", "0\0", "X\0", "X\0", "X\0", "0\0", "0\0", "0\0", "0\0"},      //b.gt
  {"Imm8\0", "SUB\0", "X\0", "1\0", "1\0", "X\0", "0\0", "0\0", "X\0", "X\0", "X\0", "0\0", "0\0", "0\0", "0\0"},      //b.eq
  {"Imm8\0", "SUB\0", "X\0", "3\0", "1\0", "X\0", "0\0", "0\0", "X\0", "X\0", "X\0", "0\0", "0\0", "0\0", "0\0"},      //b.ge
  {"Imm8\0", "SUB\0", "X\0", "5\0", "1\0", "X\0", "0\0", "0\0", "X\0", "X\0", "X\0", "0\0", "0\0", "0\0", "0\0"},      //b.lt
  {"Imm8\0", "SUB\0", "X\0", "6\0", "1\0", "X\0", "0\0", "0\0", "X\0", "X\0", "X\0", "0\0", "0\0", "0\0", "0\0"},      //b.ne
  {"Imm8\0", "SUB\0", "X\0", "4\0", "1\0", "X\0", "0\0", "0\0", "X\0", "X\0", "X\0", "0\0", "0\0", "0\0", "0\0"},      //b.le
  {"X\0", "X\0", "X\0", "7\0", "3\0", "0\0", "X\0", "1\0", "X\0", "X\0", "X\0", "0\0", "0\0", "1\0", "0\0"},      //halt  
    };
int main(int argc, char *argv[])
{
  FILE * f;
  size_t n = 0;
  char ins_a[4];
  int ins = 0;
  if (argc < 2) {
    fprintf(stderr, "Usage: %s FILE.IMG\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  char * l = NULL;
  ssize_t c;
  f = fopen(argv[1], "r");
  //throwaway first line
  if (getline(&l, &n, f) == -1){
        return 1;
  }
  while ((c = getline(&l, &n, f)) != -1){
    int i = 0;
    for (i = 0; i <= 3; i++){
      ins_a[i] = l[i];
    }
    ins = (int)strtol(ins_a, NULL, 16);
    union u_init u1;
    u1.num = ins;
    printf("\n%04x %-10s", ins, cmd[u1.bits.type]);
    if (u1.bits.type <= 7){
      union u_R u2;
      u2.num = ins;
      printf("A = R%d, B = R%d, W = R%d\n", u2.bits.RegA, u2.bits.RegB, u2.bits.RegW);
    }
    else if (u1.bits.type <= 15){
      union u_D u2;
      u2.num = ins;
      printf("A = R%d, Imm5 = %d, W = R%d\n", u2.bits.RegA, u2.bits.Imm5, u2.bits.RegW);
    }
    else if (u1.bits.type <= 23){
      union u_I u2;
      u2.num = ins;
      printf("W = R%d, Imm8 = %d\n", u2.bits.RegW, u2.bits.Imm8);
    }
    else{
      union u_B u2;
      u2.num = ins;
      printf("Imm11 = %d\n", u2.bits.Imm11);
    }
    printf("ALUBSrc: %s\nALUOp: %s\nShiftOp: %s\nBranchType: %s\nExtSel: %s\nMemRead: %s\nMemToReg: %s\nMemWrite: %s\n", sigs[u1.bits.type].ALUBSrc, sigs[u1.bits.type].ALUOp,
	   sigs[u1.bits.type].ShiftOP, sigs[u1.bits.type].BranchType, sigs[u1.bits.type].ExtSel, sigs[u1.bits.type].MemRead, sigs[u1.bits.type].MemToReg, sigs[u1.bits.type].MemWrite);

    printf("RegA: %s\nRegB: %s\nRegW: %s\nRegWrite: %s\nUpdateCond: %s\nSetHalt: %s\nMovis: %s\n", sigs[u1.bits.type].RegA, sigs[u1.bits.type].RegB, sigs[u1.bits.type].RegW,
	   sigs[u1.bits.type].RegWrite, sigs[u1.bits.type].UpdateCond, sigs[u1.bits.type].SetHalt, sigs[u1.bits.type].Movis);
    
    
  }
  return 0;
}
