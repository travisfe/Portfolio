/**
 * Assembler for the ULNAv1 instruction set.
 *
 * Generates an image file suitable to be read by Logisim.
 *
 * Copyright(c) 2018 Jason Tang <jtang@umbc.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int lineno;
static int memaddr;

/**
 * List of known instructions, sorted by instruction number.
 */
static const char *INSTRUCTIONS[] = {
  "or", "add", "and", "stw", "br", "sub", "neg", "ldw",
  "sha", "addi", "shl", "stwi", NULL, NULL, "rot", "ldwi",
  "ori", "inci", "andi", NULL, "movi", "cmp", "movis", "call",
  "b", "b.gt", "b.eq", "b.ge", "b.lt", "b.ne", "b.le", "halt"
};

/**
 * Parse a register name.
 *
 * A register name begins with an 'R' or 'r', followed by a single
 * digit from 0 through 7, inclusive.
 *
 * @return Register number, or -1 on syntax error.
 */
static int regparse(const char *p)
{
  if (p && *p && (*p == 'r' || *p == 'R')) {
    char c = *(p + 1);
    if (c >= '0' && c <= '7') {
      return c - '0';
    }
  }
  return -1;
}

/**
 * Parse an immediate value.
 *
 * If the immediate begins with "0X" or "0x" then treat it as a
 * hexadecimal. Else if it starts with "0" then treat it as an
 * octal. Otherwise, treat it as a decimal; a leading '#' would be
 * ignored.
 *
 * The number of allowed bits in the immediate is given by @a bits.
 *
 * @return Immediate value (as an unsigned), or -1 on syntax error or
 * if it does not fit within @a bits.
 */
static int immparse(const char *p, int bits)
{
  if (!p) {
    return -1;
  }
  int base = 0;
  if (*p == '#') {
    base = 10;
    p++;
  }
  char *end;
  long l = strtol(p, &end, base);
  if (*p == '\0' || *end != '\0') {
    return -1;
  }
  // check that immediate fits within bits
  long max = 1 << bits;
  if (l > max || l < (-1 * max)) {
    return -1;
  }
  int val = l;
  val &= (1 << bits) - 1;
  return val;
}

/**
 * Display the line number, an error message, then terminate the program.
 */
static void __attribute__ ((noreturn)) syntax_error(const char *fmt, ...)
{
  printf("Line %d: ", lineno);
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  exit(EXIT_FAILURE);
}

/**
 * Parse a R-Type instruction.
 */
static void assemble_rtype(FILE * outfd, int inst_num, const char *p1,
			   const char *p2, const char *p3)
{
  int rw = regparse(p1);
  int ra = regparse(p2);
  int rb = regparse(p3);
  if (inst_num == 4) {
    // br
    if (ra >= 0 || rb >= 0)
      syntax_error("Too many operands\n");
    if (rw < 0)
      syntax_error("Invalid register %s\n", p1);
    ra = rw;
    rw = rb = 0;
  } else if (inst_num == 6) {
    // neg
    if (rb >= 0)
      syntax_error("Too many operands\n");
    if (rw < 0 || ra < 0)
      syntax_error("Invalid register number\n");
    rb = 0;
  } else if (rw < 0 || ra < 0 || rb < 0) {
    syntax_error("Invalid register number\n");
  }
  uint16_t val = (inst_num << 11);
  val |= (rw << 8);
  val |= (ra << 5);
  val |= (rb << 2);
  fprintf(outfd, "%04x", val);
}

/**
 * Parse a D-type instruction.
 */
static void assemble_dtype(FILE * outfd, int inst_num, const char *p1,
			   const char *p2, const char *p3)
{
  int rw = regparse(p1);
  int ra = regparse(p2);
  int imm5 = immparse(p3, 5);
  if (rw < 0 || ra < 0) {
    syntax_error("Invalid register number\n");
  }
  if (imm5 < 0) {
    syntax_error("Invalid imm5 value %s\n", p3);
  }
  uint16_t val = (inst_num << 11);
  val |= (rw << 8);
  val |= (ra << 5);
  val |= imm5;
  fprintf(outfd, "%04x", val);
}

/**
 * Parse a I-type instruction.
 */
static void assemble_itype(FILE * outfd, int inst_num, const char *p1,
			   const char *p2)
{
  int rw = regparse(p1);
  int imm8 = immparse(p2, 8);
  if (rw < 0) {
    syntax_error("Invalid register number\n");
  }
  if (imm8 < 0) {
    syntax_error("Invalid imm8 value %s\n", p2);
  }
  uint16_t val = (inst_num << 11);
  val |= (rw << 8);
  val |= imm8;
  fprintf(outfd, "%04x", val);
}

/**
 * Parse a B-type instruction.
 */
static void assemble_btype(FILE * outfd, int inst_num, const char *p1)
{
  int imm11 = immparse(p1, 11);
  if (inst_num == 31) {
    // halt
    if (imm11 >= 0) {
      syntax_error("No imm11 allowed\n");;
    }
    imm11 = 0;
  } else if (imm11 < 0) {
    syntax_error("Invalid imm11 value %s\n", p1);
  }
  uint16_t val = (inst_num << 11);
  val |= imm11;
  fprintf(outfd, "%04x", val);
}

/**
 * Write the parsed instruction to the output file.
 */
static void assemble(FILE * outfd, const char *inst, const char *p1,
		     const char *p2, const char *p3)
{
  if (!inst) {
    return;
  }
  int inst_num;
  for (inst_num = 0; inst_num < 32; inst_num++) {
    if (!INSTRUCTIONS[inst_num]) {
      continue;
    }
    if (strcasecmp(inst, INSTRUCTIONS[inst_num]) == 0) {
      break;
    }
  }
  if (inst_num >= 32) {
    syntax_error("Invalid instruction %s\n", inst);
  }
  if (inst_num < 8) {
    assemble_rtype(outfd, inst_num, p1, p2, p3);
  } else if (inst_num >= 8 && inst_num < 16) {
    assemble_dtype(outfd, inst_num, p1, p2, p3);
  } else if (inst_num >= 16 && inst_num < 24 && !p3) {
    assemble_itype(outfd, inst_num, p1, p2);
  } else if (inst_num >= 24 && inst_num < 32 && !p2 && !p3) {
    assemble_btype(outfd, inst_num, p1);
  } else {
    syntax_error
      ("Incorrect number of operands for instruction %s\n", inst);
  }
  fprintf(outfd, "   # %4d (%03xh):  %-5s", lineno, memaddr, inst);
  if (p1) {
    fprintf(outfd, "  %s", p1);
  }
  if (p2) {
    fprintf(outfd, ", %s", p2);
  }
  if (p3) {
    fprintf(outfd, ", %s", p3);
  }
  fprintf(outfd, "\n");
  memaddr++;
}

/**
 * Tokenize a line, ignoring any comments. A comment is defined as
 * "//" to the end of line.
 */
static char *strsplit(char **stringp)
{
  char *s = *stringp;

  // find start of token
  while (1) {
    if (!s || *s == '\0' || (*s == '/' && *(s + 1) == '/')) {
      // end of line
      *stringp = NULL;
      return NULL;
    }
    if (!isspace(*s) && *s != ',') {
      // found start of token
      break;
    }
    s++;
  }

  char *retval = s;

  // find end of token
  s++;
  while (1) {
    if (*s == '\0' || (*s == '/' && *(s + 1) == '/')) {
      // end of line or comment
      *s = '\0';
      *stringp = NULL;
      break;
    }
    if (isspace(*s) || *s == ',') {
      // end of token
      *s = '\0';
      *stringp = s + 1;
      break;
    }
    s++;
  }
  return retval;
}

/**
 * Parse a line from the input file. If the line contains an
 * instruction, try to assemble it.
 */
static void parse(FILE * infd, FILE * outfd)
{
  char line[255];
  while (fgets(line, sizeof(line), infd) != NULL) {
    lineno++;
    char *s = line;
    char *inst = strsplit(&s);
    char *p1 = strsplit(&s);
    char *p2 = strsplit(&s);
    char *p3 = strsplit(&s);
    assemble(outfd, inst, p1, p2, p3);
  }
  if (ferror(infd)) {
    perror("Error reading input file");
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char *argv[])
{
  if (argc < 3) {
    fprintf(stderr, "Usage: %s INPUT.S OUTPUT.IMG\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  FILE *infd = fopen(argv[1], "r");
  if (!infd) {
    perror("Could not open input file");
    exit(EXIT_FAILURE);
  }

  FILE *outfd = fopen(argv[2], "w");
  if (!outfd) {
    perror("Could not open output file");
    exit(EXIT_FAILURE);
  }

  fprintf(outfd, "v2.0 raw\n");
  parse(infd, outfd);

  exit(EXIT_SUCCESS);
}
