// Implement the shift-adder multiplier, version 3
// This only works for unsigned integers, and does not handle overflow.

// Instructions:
//  1. Reset simulation
//  2. Load this program into instruction memory
//  3. Set tick frequency to medium speed (say, 8 Hz)
//  4. Enable simulation
//  5. Enable ticks
//  6. Wait until system halts
//  7. Stop simulation, then examine data memory 0xfffe for result

// First, load data memory with operands
// (These steps can be skipped if data memory were
// pre-loaded prior to execution.)
movi R0, #0
movi R1, 0x9b   // multiplicand = 411
movis R1, #1
stwi R1, R0, 0
movi R1, #42    // multiplier = 42
stwi R1, R0, 1

// Fetch operands
movi R0, #0
ldwi R2, R0, #0 // R2 = multiplicand
ldwi R1, R0, #1 // R1 = bottom half, multiplier
movi R0, #0     // R0 = top half, multiplier
movi R3, #16    // R3 = counter

// top of loop
and  R4, R1, R1 // R4 = tmp
andi R4, #1     // R4 = LSB of bottom half
cmp R4, 1
b.ne 2
add R0, R0, R2  // add multiplicand to top half
and R4, R0, R0
andi R4, #1     // R4 = LSB of top half
shl R0, R0, 1   // shift top half by 1
shl R1, R1, 1   // shift bottom half by 1
rot R4, R4, -15
or R1, R1, R4   // add former LSB of top half to MSB of bottom half
inci R3, -1
b.ne -12        // repeat

neg R3, R3      // set R3 to 0xffff
stwi R1, R3, -1  // store product to address 0xfffe
halt
