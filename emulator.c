#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
typedef unsigned char byte;
typedef struct c_bits { // condition code bits
	uint8_t z:1; // zero bit, set when the result is zero
	uint8_t s:1; // sign bit, set when the sign of the result is negative
	uint8_t p:1; // parity bit, set when even number of 1s in result
	uint8_t cy:1; // carry bit, set when result includes a carry out
	uint8_t ac:1; // auxilary carry, set when result includes a carry out of bit 3
	uint8_t pad:3; // ???
} c_bits;
typedef struct hw_state { // state of the processor
	uint8_t a;
	uint8_t b;
	uint8_t c;
	uint8_t d;
	uint8_t e;
	uint8_t h;
	uint8_t l;
	uint16_t sp; // stack pointer
	uint16_t pc; // program counter
	uint8_t* memory; // main memory
	struct c_bits cc; // condition bits
	uint8_t int_enable; // ???
} hw_state;

// Count the number of ones in the binary representation of v, return 1 if even, 0 otherwise
int parity(uint8_t v) {
	int count = 0;
	for (int i=0; i < 8; i++) {
		count += i & 0x1; // add 1 to count if the LSB is 1
		v >> 1; // bitwise shift right by 1
	}
	return ((count % 2) == 1);
}

// Returns the 16 bit value stored in specified register pair
uint16_t get_reg_pair(hw_state* state, char reg) {
	switch(reg) {
		case 'B': return (state->b<<8) | (state->c);
		case 'D': return (state->d<<8) | (state->e);
		case 'H': return (state->h<<8) | (state->l);
		case 'S': return state->sp;
	}
}

// Sets the 16bit value stored in specified register pair to v
void set_reg_pair(hw_state* state, uint16_t v, char reg) {
	switch(reg) {
		case 'B': state->b = (v>>8) & 0xff; state->c = v & 0xff; break;
		case 'D': state->d = (v>>8) & 0xff; state->e = v & 0xff; break;
		case 'H': state->h = (v>>8) & 0xff; state->l = v & 0xff; break;
		case 'S': state->sp = v;
	}
}

// Returns the specified register
uint8_t get_reg(hw_state* state, char reg) {
	switch(reg) {
		case 'B': return state->b;
		case 'C': return state->c;
		case 'D': return state->d;
		case 'E': return state->e;
		case 'H': return state->h;
		case 'L': return state->l;
		case 'M': return state->memory[get_reg_pair(state,'H')];
		case 'A': return state->a;
	}
}

// Sets the specified regist to value v
void set_reg(hw_state* state, uint8_t v, char reg) {
	switch(reg) {
		case 'B': state->b = v; break;
		case 'C': state->c = v; break;
		case 'D': state->d = v; break;
		case 'E': state->e = v; break;
		case 'H': state->h = v; break;
		case 'L': state->l = v; break;
		case 'M': state->memory[get_reg_pair(state,'H')] = v; break;
		case 'A': state->a = v; break;
	}
}

void unimplemented(hw_state* state) {
	// state->pc -= 1;
	printf("Error: unimplemented instruction");
	exit(1);
}

// Load 16-bit immediate into register pair
void lxi(hw_state* state, byte* opcode, char reg) {
	switch (reg) {
		case 'B': state->b = opcode[2]; state->c = opcode[1]; break;
		case 'D': state->d = opcode[2]; state->e = opcode[1]; break;
		case 'H': state->h = opcode[2]; state->l = opcode[1]; break;
		case 'S': state->sp = (opcode[2] << 8) | opcode[1]; break;
	}
	state->pc += 2; // increment pc by 2 more than default (3 total)
}

/* --------------- JUMPS  ----------------*/

// Jump (change pc) to address contained in the two bytes following the opcode
void jmp(hw_state* state, byte* opcode) {
	state->pc = (opcode[2] << 8) | opcode[1]; // jump
}

void jump_if(hw_state* state, byte* opcode, int cond) {
	if (cond) {
		state->pc = (opcode[2] << 8) | opcode[1];
	} else {
		state->pc += 2; // account for size of instruction
	}
}

// Jump if zero bit is set
void jz(hw_state* state, byte* opcode) {
	jump_if(state, opcode, state->cc.z);
}

// Jump if zero bit is not set
void jnz(hw_state* state, byte* opcode) {
	jump_if(state, opcode, !(state->cc.z));
}

// Jump if carry bit is set
void jc(hw_state* state, byte* opcode) {
	jump_if(state, opcode, state->cc.cy);
}

// Jump if carry bit is not set
void jnc(hw_state* state, byte* opcode) {
	jump_if(state, opcode, !(state->cc.cy));
}

// Jump if parity odd
void jpo(hw_state* state, byte* opcode) {
	jump_if(state, opcode, state->cc.p);
}

// Jump if parity even
void jpe(hw_state* state, byte* opcode) {
	jump_if(state, opcode, !(state->cc.p));
}

// Jump if sign minus
void jm(hw_state* state, byte* opcode) {
	jump_if(state, opcode, state->cc.s);
}

// Jump if sign plus
void jp(hw_state* state, byte* opcode) {
	jump_if(state, opcode, !(state->cc.s));
}


/* ----------- ARITHMETIC ------------- */

// Add v to accumulator, update condition bits
void add(hw_state* state, uint16_t v) {
	uint16_t a = (uint16_t) state->a;
	uint16_t answer = a + v; // keep 16 bit answer to determine carry
	uint8_t answer_8b = answer & 0xff; // convert to 8 bit
	state->cc.z = ((answer_8b) == 0); // set zero bit if answer is zero
	state->cc.s = ((answer_8b & 0x80) != 0); // 1 if bit 7 is 1 (answer is negative), 0 otherwise
	state->cc.p = parity(answer_8b); // check parity
	state->cc.cy = (answer > 0xff); // Set carry if overflow occured
	state->a = answer_8b; // update A register
}

// Add v plus carry bit to accumulator, update condition bits
void adc(hw_state* state, uint16_t v) {
	v += (uint16_t) state->cc.cy; // add carry bit to input value
	add(state, v); // perform standard addition with new value
}

// Subtract v from accumulator, update condition bits
void sub(hw_state* state, uint16_t v) {
	uint16_t a = (uint16_t) state->a;
	uint16_t v_tc = (uint16_t) ~v + 1; // two's complement of v
	uint16_t answer = a + v_tc; // keep 16 bit answer to determine carry
	uint8_t answer_8b = answer & 0xff; // convert to 8 bit
	state->cc.z = ((answer_8b) == 0); // set zero bit if answer is zero
	state->cc.s = ((answer_8b & 0x80) != 0); // 1 if bit 7 is 1 (answer is negative), 0 otherwise
	state->cc.p = parity(answer_8b); // check parity
	state->cc.cy = (answer < 0xff); // Set carry if overflow did NOT occur (i.e. borrow occured)
	state->a = answer_8b; // update A register
}

// Increment register pair by 1
void inx(hw_state* state, char reg) {
	uint16_t v = get_reg_pair(state, reg);
	v += 1;
	set_reg_pair(state, v, reg);
}

// Decrement register pair by 1
void dcx(hw_state* state, char reg) {
	uint16_t v = get_reg_pair(state,reg);
	v += ~1 + 1; // -1 in TC TODO: is this necessary?
	set_reg_pair(state, v, reg);
}

// Increment register by 1, does not affect carry
void inr(hw_state* state, char reg) {
	uint8_t v = get_reg(state,reg); // get value in register reg
	uint8_t answer = v + 1;
	state->cc.z = ((answer) == 0); // set zero bit if answer is zero
	state->cc.s = ((answer & 0x80) != 0); // 1 if bit 7 is 1 (answer is negative), 0 otherwise
	state->cc.p = parity(answer); // check parity
	set_reg(state,answer,reg); // update register with answer
}

// Decrement register by 1, does not affect carry
void dcr(hw_state* state, char reg) {
	uint8_t v = get_reg(state,reg); // get value in register reg
	uint8_t answer = v + (~1 + 1); // TODO: is this necessary?
	state->cc.z = ((answer) == 0); // set zero bit if answer is zero
	state->cc.s = ((answer & 0x80) != 0); // 1 if bit 7 is 1 (answer is negative), 0 otherwise
	state->cc.p = parity(answer); // check parity
	set_reg(state,answer,reg);
}

// Subtract v plus carry bit from accumulator, update condition bits
void sbb(hw_state* state, uint16_t v) {
	v += (uint16_t) state->cc.cy; // add carry bit to input value
	sub(state, v); // perform standard subtraction with new value
}

// Adds the contents of register pair reg to HL register pair
void dad(hw_state* state, char reg) {
	uint32_t v = (uint32_t) get_reg_pair(state,reg); // get 16 bit value
	uint32_t answer = v + get_reg_pair(state,'H'); // add to contents of HL
	state->cc.cy = (answer < 0xffff); // update (16 bit) carry
	set_reg_pair(state,answer & 0xffff,'H'); // store answer in HL pair
}

// Executes next instruction for processor in state hw_state
void emulate(hw_state* state) {
	byte* opcode = &state->memory[state->pc]; // the address of the current instruction in memory
	int size = 1;
    // Each "register pair" is denoted by the first register. E.g. 'B' can refer to the pair B, C
	switch (*opcode) {
		case 0x00: printf("NOP\n"); break; // Do nothing
		case 0x01: printf("LXI B,#$%02x%02x\n", opcode[2], opcode[1]); lxi(state, opcode, 'B'); break; // LXI B,C
		case 0x02: printf("STAX B\n"); unimplemented(state); break; // Store accumulator
		case 0x03: printf("INX B\n"); inx(state,'B'); break; // Increment 16-bit value in register pair
		case 0x04: printf("INR B\n"); inr(state,'B'); break; // Increment register
		case 0x05: printf("DCR B\n"); dcr(state,'B'); break; // Decrement register
        case 0x06: printf("MVI B,#$%02x\n", opcode[1]); size = 2; unimplemented(state); break; // Load immediate into register
		case 0x07: printf("RLC\n"); unimplemented(state); break; // Rotate accumulator left
		case 0x08: printf("NOP\n"); break;
		case 0x09: printf("DAD B\n"); dad(state,'B'); break; // Add register pair to H and L registers
		case 0x0a: printf("LDAX B\n"); unimplemented(state); break; // Load accumulator from register pair
		case 0x0b: printf("DCX B\n"); dcx(state,'B'); break; // Decrement 16-bit value in register pair
		case 0x0c: printf("INR C\n"); inr(state,'C'); break;
		case 0x0d: printf("DCR C\n"); dcr(state,'C'); break;
        case 0x0e: printf("MVI C,#$%02x\n", opcode[1]); size = 2; unimplemented(state); break;
		case 0x0f: printf("RRC\n"); unimplemented(state); break; // Rotate accumulator right
		case 0x10: printf("NOP\n"); break;
        case 0x11: printf("LXI D,#$%02x%02x\n", opcode[2], opcode[1]); lxi(state, opcode, 'D'); break;
		case 0x12: printf("STAX D\n"); unimplemented(state); break;
		case 0x13: printf("INX D\n"); inx(state,'D'); break;
		case 0x14: printf("INR D\n"); inr(state,'D'); break;
		case 0x15: printf("DCR D\n"); dcr(state,'D'); break;
        case 0x16: printf("MVI D,#$%02x\n", opcode[1]); size = 2; unimplemented(state); break;
		case 0x17: printf("RAL\n"); unimplemented(state); break; // Rotate accumulator left through carry
		case 0x18: printf("NOP\n"); break;
		case 0x19: printf("DAD D\n"); dad(state,'D'); break;
		case 0x1a: printf("LDAX D\n"); unimplemented(state); break;
		case 0x1b: printf("DCX D\n"); dcx(state,'D'); break;
		case 0x1c: printf("INR E\n"); inr(state,'E'); break;
		case 0x1d: printf("DCR E\n"); dcr(state,'E'); break;
		case 0x1e: printf("MVI E,#$%02x\n", opcode[1]); size = 2; unimplemented(state); break;
		case 0x1f: printf("RAR\n"); unimplemented(state); break; // Rotate accumulator right through carry
		case 0x20: printf("NOP\n"); break;
		case 0x21: printf("LXI H,#$%02x%02x\n", opcode[2], opcode[1]); lxi(state,opcode,'H'); break;
        case 0x22: printf("SHLD $%X%X\n", opcode[2], opcode[1]); size = 3; unimplemented(state); break; // Contents of H and L stored at address
		case 0x23: printf("INX H\n"); inx(state,'H'); break;
		case 0x24: printf("INR H\n"); inr(state,'H'); break;
		case 0x25: printf("DCR H\n"); dcr(state,'H'); break;
		case 0x26: printf("MVI H,#$%02x\n", opcode[1]); size = 2; unimplemented(state); break;
		case 0x27: printf("DAA\n"); unimplemented(state); break; // Adjust 8 bit accumulator to form two four bit decimals
		case 0x28: printf("NOP\n"); break;
		case 0x29: printf("DAD H\n"); dad(state,'H'); break;
        case 0x2a: printf("LHLD $%X%X\n", opcode[2], opcode[1]); size = 3; unimplemented(state); break; // Load H and L with contents stored at address
		case 0x2b: printf("DCX H\n"); dcx(state,'H'); break;
		case 0x2c: printf("INR L\n"); inr(state,'L'); break;
		case 0x2d: printf("DCR L\n"); dcr(state,'L'); break;
		case 0x2e: printf("MVI L,#$%02x\n", opcode[1]); size = 2; unimplemented(state); break;
		case 0x2f: printf("CMA\n"); unimplemented(state); break; // Complement accumulator
		case 0x30: printf("NOP\n"); break;
		case 0x31: printf("LXI SP,#$%02x%02x\n", opcode[2], opcode[1]); lxi(state,opcode,'S'); break;
        case 0x32: printf("STA $%X%X\n", opcode[2], opcode[1]); size = 3; unimplemented(state); break; // Store data in accumulator at address
		case 0x33: printf("INX SP\n"); inx(state,'S'); break;
		case 0x34: printf("INR M\n"); inr(state,'M'); break;
		case 0x35: printf("DCR M\n"); dcr(state,'M'); break;
		case 0x36: printf("MVI M,#$%02x\n", opcode[1]); size = 2; unimplemented(state); break;
		case 0x37: printf("STC\n"); unimplemented(state); break;
		case 0x38: printf("NOP\n"); break;
		case 0x39: printf("DAD SP\n"); dad(state,'S'); break;
        case 0x3a: printf("LDA $%X%X\n", opcode[2], opcode[1]); size = 3; unimplemented(state); break;
		case 0x3b: printf("DCX SP\n"); dcx(state,'S'); break;
		case 0x3c: printf("INR A\n"); inr(state,'A'); break;
		case 0x3d: printf("DCR A\n"); dcr(state,'A'); break;
		case 0x3e: printf("MVI A,#$%02x\n", opcode[1]); size = 2; unimplemented(state); break;
        case 0x3f: printf("CMC\n"); unimplemented(state); break;
		case 0x40: printf("MOV B,B\n"); unimplemented(state); break;
		case 0x41: printf("MOV B,C\n"); unimplemented(state); break;
		case 0x42: printf("MOV B,D\n"); unimplemented(state); break;
		case 0x43: printf("MOV B,E\n"); unimplemented(state); break;
		case 0x44: printf("MOV B,H\n"); unimplemented(state); break;
		case 0x45: printf("MOV B,L\n"); unimplemented(state); break;
		case 0x46: printf("MOV B,M\n"); unimplemented(state); break;
		case 0x47: printf("MOV B,A\n"); unimplemented(state); break;
		case 0x48: printf("MOV C,B\n"); unimplemented(state); break;
		case 0x49: printf("MOV C,C\n"); unimplemented(state); break;
		case 0x4a: printf("MOV C,D\n"); unimplemented(state); break;
		case 0x4b: printf("MOV C,E\n"); unimplemented(state); break;
		case 0x4c: printf("MOV C,H\n"); unimplemented(state); break;
		case 0x4d: printf("MOV C,L\n"); unimplemented(state); break;
		case 0x4e: printf("MOV C,M\n"); unimplemented(state); break;
		case 0x4f: printf("MOV C,A\n"); unimplemented(state); break;
		case 0x50: printf("MOV D,B\n"); unimplemented(state); break;
		case 0x51: printf("MOV D,C\n"); unimplemented(state); break;
		case 0x52: printf("MOV D,D\n"); unimplemented(state); break;
		case 0x53: printf("MOV D,E\n"); unimplemented(state); break;
		case 0x54: printf("MOV D,H\n"); unimplemented(state); break;
		case 0x55: printf("MOV D,L\n"); unimplemented(state); break;
		case 0x56: printf("MOV D,M\n"); unimplemented(state); break;
		case 0x57: printf("MOV D,A\n"); unimplemented(state); break;
		case 0x58: printf("MOV E,B\n"); unimplemented(state); break;
		case 0x59: printf("MOV E,C\n"); unimplemented(state); break;
		case 0x5a: printf("MOV E,D\n"); unimplemented(state); break;
		case 0x5b: printf("MOV E,E\n"); unimplemented(state); break;
		case 0x5c: printf("MOV E,H\n"); unimplemented(state); break;
		case 0x5d: printf("MOV E,L\n"); unimplemented(state); break;
		case 0x5e: printf("MOV E,M\n"); unimplemented(state); break;
		case 0x5f: printf("MOV E,A\n"); unimplemented(state); break;
		case 0x60: printf("MOV H,B\n"); unimplemented(state); break;
		case 0x61: printf("MOV H,C\n"); unimplemented(state); break;
		case 0x62: printf("MOV H,D\n"); unimplemented(state); break;
		case 0x63: printf("MOV H,E\n"); unimplemented(state); break;
		case 0x64: printf("MOV H,H\n"); unimplemented(state); break;
		case 0x65: printf("MOV H,L\n"); unimplemented(state); break;
		case 0x66: printf("MOV H,M\n"); unimplemented(state); break;
		case 0x67: printf("MOV H,A\n"); unimplemented(state); break;
		case 0x68: printf("MOV L,B\n"); unimplemented(state); break;
		case 0x69: printf("MOV L,C\n"); unimplemented(state); break;
		case 0x6a: printf("MOV L,D\n"); unimplemented(state); break;
		case 0x6b: printf("MOV L,E\n"); unimplemented(state); break;
		case 0x6c: printf("MOV L,H\n"); unimplemented(state); break;
		case 0x6d: printf("MOV L,L\n"); unimplemented(state); break;
		case 0x6e: printf("MOV L,M\n"); unimplemented(state); break;
		case 0x6f: printf("MOV L,A\n"); unimplemented(state); break;
        case 0x70: printf("MOV M,B\n"); unimplemented(state); break;
        case 0x71: printf("MOV M,C\n"); unimplemented(state); break;
        case 0x72: printf("MOV M,D\n"); unimplemented(state); break;
        case 0x73: printf("MOV M,E\n"); unimplemented(state); break;
        case 0x74: printf("MOV M,H\n"); unimplemented(state); break;
        case 0x75: printf("MOV M,L\n"); unimplemented(state); break;
		case 0x76: printf("HLT\n"); unimplemented(state); break;
		case 0x77: printf("MOV M,A\n"); unimplemented(state); break;
		case 0x78: printf("MOV A,B\n"); unimplemented(state); break;
		case 0x79: printf("MOV A,C\n"); unimplemented(state); break;
		case 0x7a: printf("MOV A,D\n"); unimplemented(state); break;
		case 0x7b: printf("MOV A,E\n"); unimplemented(state); break;
		case 0x7c: printf("MOV A,H\n"); unimplemented(state); break;
		case 0x7d: printf("MOV A,L\n"); unimplemented(state); break;
		case 0x7e: printf("MOV A,M\n"); unimplemented(state); break;
		case 0x7f: printf("MOV A,A\n"); unimplemented(state); break;
		case 0x80: printf("ADD B\n"); add(state, state->b); break;
		case 0x81: printf("ADD C\n"); add(state, state->c); break;
		case 0x82: printf("ADD D\n"); add(state, state->d); break;
		case 0x83: printf("ADD E\n"); add(state, state->e); break;
		case 0x84: printf("ADD H\n"); add(state, state->h); break;
		case 0x85: printf("ADD L\n"); add(state, state->l); break;
		case 0x86: printf("ADD M\n"); add(state, state->memory[get_reg_pair(state,'H')]); break;
		case 0x87: printf("ADD A\n"); add(state, state->a); break;
		case 0x88: printf("ADC B\n"); add(state, state->b); break;
		case 0x89: printf("ADC C\n"); add(state, state->c); break;
		case 0x8a: printf("ADC D\n"); add(state, state->d); break;
		case 0x8b: printf("ADC E\n"); add(state, state->e); break;
		case 0x8c: printf("ADC H\n"); add(state, state->h); break;
		case 0x8d: printf("ADC L\n"); add(state, state->l); break;
		case 0x8e: printf("ADC M\n"); add(state, state->memory[get_reg_pair(state,'H')]); break;
		case 0x8f: printf("ADC A\n"); add(state, state->a); break;
		case 0x90: printf("SUB B\n"); sub(state, state->b); break; // Subtract register from accumulator
		case 0x91: printf("SUB C\n"); sub(state, state->c); break;
		case 0x92: printf("SUB D\n"); sub(state, state->d); break;
		case 0x93: printf("SUB E\n"); sub(state, state->e); break;
		case 0x94: printf("SUB H\n"); sub(state, state->h); break;
		case 0x95: printf("SUB L\n"); sub(state, state->l); break;
		case 0x96: printf("SUB M\n"); sub(state, state->memory[get_reg_pair(state,'H')]); break;
		case 0x97: printf("SUB A\n"); sub(state, state->a); break;
		case 0x98: printf("SBB B\n"); sbb(state, state->b); break; // Subtract register from accumulator with borrow
		case 0x99: printf("SBB C\n"); sbb(state, state->c); break;
		case 0x9a: printf("SBB D\n"); sbb(state, state->d); break;
		case 0x9b: printf("SBB E\n"); sbb(state, state->e); break;
		case 0x9c: printf("SBB H\n"); sbb(state, state->h); break;
		case 0x9d: printf("SBB L\n"); sbb(state, state->l); break;
		case 0x9e: printf("SBB M\n"); sbb(state, state->memory[get_reg_pair(state,'H')]); break;
		case 0x9f: printf("SBB A\n"); sbb(state, state->a); break;
		case 0xa0: printf("ANA B\n"); unimplemented(state); break; // Bitwise AND register with accumulator
		case 0xa1: printf("ANA C\n"); unimplemented(state); break;
		case 0xa2: printf("ANA D\n"); unimplemented(state); break;
		case 0xa3: printf("ANA E\n"); unimplemented(state); break;
		case 0xa4: printf("ANA H\n"); unimplemented(state); break;
		case 0xa5: printf("ANA L\n"); unimplemented(state); break;
		case 0xa6: printf("ANA M\n"); unimplemented(state); break;
		case 0xa7: printf("ANA A\n"); unimplemented(state); break;
		case 0xa8: printf("XRA B\n"); unimplemented(state); break; // Bitwise XOR register with accumulator
		case 0xa9: printf("XRA C\n"); unimplemented(state); break;
		case 0xaa: printf("XRA D\n"); unimplemented(state); break;
		case 0xab: printf("XRA E\n"); unimplemented(state); break;
		case 0xac: printf("XRA H\n"); unimplemented(state); break;
		case 0xad: printf("XRA L\n"); unimplemented(state); break;
		case 0xae: printf("XRA M\n"); unimplemented(state); break;
		case 0xaf: printf("XRA A\n"); unimplemented(state); break;
		case 0xb0: printf("ORA B\n"); unimplemented(state); break; // Bitwise OR register with accumulator
		case 0xb1: printf("ORA C\n"); unimplemented(state); break;
		case 0xb2: printf("ORA D\n"); unimplemented(state); break;
		case 0xb3: printf("ORA E\n"); unimplemented(state); break;
		case 0xb4: printf("ORA H\n"); unimplemented(state); break;
		case 0xb5: printf("ORA L\n"); unimplemented(state); break;
		case 0xb6: printf("ORA M\n"); unimplemented(state); break;
		case 0xb7: printf("ORA A\n"); unimplemented(state); break;
		case 0xb8: printf("CMP B\n"); unimplemented(state); break; // Set conditon bits based on register less than accumulator
		case 0xb9: printf("CMP C\n"); unimplemented(state); break;
		case 0xba: printf("CMP D\n"); unimplemented(state); break;
		case 0xbb: printf("CMP E\n"); unimplemented(state); break;
		case 0xbc: printf("CMP H\n"); unimplemented(state); break;
		case 0xbd: printf("CMP L\n"); unimplemented(state); break;
		case 0xbe: printf("CMP M\n"); unimplemented(state); break;
		case 0xbf: printf("CMP A\n"); unimplemented(state); break;
		case 0xc0: printf("RNZ\n"); unimplemented(state); break; // If zero bit is zero, jump to return address
		case 0xc1: printf("POP B\n"); unimplemented(state); break; // Pop stack to register pair
        case 0xc2: printf("JNZ $%X%X\n", opcode[2], opcode[1]); size = 3; jnz(state, opcode); break; // If zero bit is zero, jump to address
        case 0xc3: printf("JMP $%X%X\n", opcode[2], opcode[1]); size = 3; jmp(state, opcode); break; // Jump to address
        case 0xc4: printf("CNZ $%X%X\n", opcode[2], opcode[1]); size = 3; unimplemented(state); break; // TBD
		case 0xc5: printf("PUSH B\n"); unimplemented(state); break; // Push register pair onto stack
		case 0xc6: printf("ADI #$%02x\n", opcode[1]); add(state, opcode[1]); state->pc += 1; break; // Add immediate to accumulator
		case 0xc7: printf("RST 0\n"); unimplemented(state); break;
		case 0xc8: printf("RZ\n"); unimplemented(state); break; // If zero bit is one, return
		case 0xc9: printf("RET\n"); unimplemented(state); break; // Return to address at top of stack
        case 0xca: printf("JZ $%X%X\n", opcode[2], opcode[1]); size = 3; jz(state, opcode); break; // If zero bit is one, jump to address
		case 0xcb: printf("NOP\n"); break;
		case 0xcc: printf("CZ $%X%X\n", opcode[2], opcode[1]); unimplemented(state); break; // If zero bit is one, call address
		case 0xcd: printf("CALL $%X%X\n", opcode[2], opcode[1]); unimplemented(state); break; // Push PC to stack, jump to address
		case 0xce: printf("ACI #$%02x\n", opcode[1]); adc(state, opcode[1]); state->pc += 1; break; // Add immediate to accumulator with carry
		case 0xcf: printf("RST 1\n"); unimplemented(state); break; // Special call
		case 0xd0: printf("RNC\n"); unimplemented(state); break; // If not carry, return
		case 0xd1: printf("POP D\n"); unimplemented(state); break;
        case 0xd2: printf("JNC $%X%X\n", opcode[2], opcode[1]); size = 3; jnc(state, opcode); break; // If not carry, jump to address
		case 0xd3: printf("OUT #$%02x\n", opcode[1]); size = 2; unimplemented(state); break; // ???
        case 0xd4: printf("CNC $%X%X\n", opcode[2], opcode[1]); size = 3; unimplemented(state); break; // If not carry, call address
		case 0xd5: printf("PUSH D\n"); unimplemented(state); break;
        case 0xd6: printf("SUI #$%02x\n", opcode[1]); sub(state, opcode[1]); state->pc += 1; break; // Subtract immediate from accumulator
		case 0xd7: printf("RST 2\n"); unimplemented(state); break; // TBD
		case 0xd8: printf("RC\n"); unimplemented(state); break; // If carry, return
		case 0xd9: printf("NOP\n"); break;
        case 0xda: printf("JC $%X%X\n", opcode[2], opcode[1]); size = 3; jc(state, opcode); break; // If carry, jump to address
		case 0xdb: printf("IN #$%02x\n", opcode[1]); size = 2; unimplemented(state); break; // ???
        case 0xdc: printf("CC $%X%X\n", opcode[2], opcode[1]); size = 3; unimplemented(state); break; // If carry, call address
		case 0xdd: printf("NOP\n"); break;
		case 0xde: printf("SBI #$%02x\n", opcode[1]); sbb(state, opcode[1]); state->pc += 1; break; // Subtract immediate from accumulator with carry
		case 0xdf: printf("RST 3\n"); unimplemented(state); break; // TBD
		case 0xe0: printf("RPO\n"); unimplemented(state); break; // If parity bit zero, return
		case 0xe1: printf("POP H\n"); unimplemented(state); break;
        case 0xe2: printf("JPO $%X%X\n", opcode[2], opcode[1]); jpo(state, opcode); break; // If parity bit zero, jump to address
		case 0xe3: printf("XTHL\n"); unimplemented(state); break; // Exchange H and L registers with data at stack pointer
        case 0xe4: printf("CPO $%X%X\n", opcode[2], opcode[1]); size = 3; unimplemented(state); break; // If PO, call address
		case 0xe5: printf("PUSH H\n"); unimplemented(state); break;
		case 0xe6: printf("ANI %X\n", opcode[1]); size = 2; unimplemented(state); break; // Bitwise AND immediate with accumulator
		case 0xe7: printf("RST 4\n"); unimplemented(state); break;
		case 0xe8: printf("RPE\n"); unimplemented(state); break;
		case 0xe9: printf("PCHL\n"); unimplemented(state); break; // PC set to H and L
        case 0xea: printf("JPE $%X%X\n", opcode[2], opcode[1]); size = 3; jpe(state, opcode); break; // If parity bit one, jump to address
		case 0xeb: printf("XCHG\n"); unimplemented(state); break; // Exchange H and L registers with D and E registers
		case 0xec: printf("CPE $%X%X\n", opcode[2], opcode[1]); size = 3; unimplemented(state); break; // If parity bit one, call address
		case 0xed: printf("NOP\n"); break;
        case 0xee: printf("XRI %X\n", opcode[1]); size = 2; unimplemented(state); break; // Bitwise XOR immediate with accumulator
		case 0xef: printf("RST 5\n"); unimplemented(state); break;
		case 0xf0: printf("RP\n"); unimplemented(state); break; // If sign bit zero, return
		case 0xf1: printf("POP PSW\n"); unimplemented(state); break;
        case 0xf2: printf("JP $%X%X\n", opcode[2], opcode[1]); jp(state, opcode); break; // If sign bit zero, jump to address
		case 0xf3: printf("DI\n"); unimplemented(state); break;
        case 0xf4: printf("CP $%X%X\n", opcode[2], opcode[1]); size = 3; unimplemented(state); break; // If sign bit zero, call address
		case 0xf5: printf("PUSH PSW\n"); unimplemented(state); break;
        case 0xf6: printf("ORI #$%02x\n", opcode[1]); size = 2; unimplemented(state); break;
		case 0xf7: printf("RST 6\n"); unimplemented(state); break;
		case 0xf8: printf("RM\n"); unimplemented(state); break; // If sign bit one, return
		case 0xf9: printf("SPHL\n"); unimplemented(state); break; // H and L replace data at stack pointer
        case 0xfa: printf("JM $%X%X\n", opcode[2], opcode[1]); jm(state, opcode); break; // If sign bit one, jump to address
		case 0xfb: printf("EI\n"); unimplemented(state); break;
        case 0xfc: printf("CM $%X%X\n", opcode[2], opcode[1]); size = 3; unimplemented(state); break; // If sign bit one, call address
		case 0xfd: printf("NOP\n"); break;
        case 0xfe: printf("CPI #$%02x\n", opcode[1]); size = 2; unimplemented(state); break; // Compare immediate with accumulator
		case 0xff: printf("RST 7\n"); unimplemented(state); break;
	}
	state->pc += 1;
}

// Takes filename of binary as argument
int main(int argc, char** argv) {
	FILE* fp; // points to file
	byte* buffer;
	int position = 0; // position in file
	long numbytes; // number of bytes in file

	if (argc < 1) {
		printf("Please provide filename argument");
		return 1;
	}

	char* filename = argv[1]; // get argument from command line
	fp = fopen(filename, "r");

	if (fp == NULL) {
		printf("Could not open file %s", filename);
		return 1;
	}

	fseek(fp, 0L, SEEK_END); // TODO: SEEK_END reduces portability
	numbytes = ftell(fp); // TODO: not working
	fseek(fp, 0L, SEEK_SET); // reset to start of file
	buffer = calloc(10000, sizeof(byte)); // TODO: fix this

	fread(buffer, sizeof(byte), numbytes, fp); // read file into buffer
	fclose(fp);

	c_bits bits;
	hw_state state = {.cc = bits, .memory = buffer}; // initialize state, load program into memory
	int x = 0;
	while (x < 20) {
		printf("PC: %04X ", state.pc);
		printf("ACCUMULATOR: %d ", state.a);
		emulate(&state);
		x+=1;
	}
	return 0;
}
