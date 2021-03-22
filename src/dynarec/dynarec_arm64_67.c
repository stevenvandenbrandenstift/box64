#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>
#include <errno.h>

#include "debug.h"
#include "box64context.h"
#include "dynarec.h"
#include "emu/x64emu_private.h"
#include "emu/x64run_private.h"
#include "x64run.h"
#include "x64emu.h"
#include "box64stack.h"
#include "callback.h"
#include "emu/x64run_private.h"
#include "x64trace.h"
#include "dynarec_arm64.h"
#include "dynarec_arm64_private.h"
#include "arm64_printer.h"

#include "dynarec_arm64_helper.h"
#include "dynarec_arm64_functions.h"


uintptr_t dynarec64_67(dynarec_arm_t* dyn, uintptr_t addr, uintptr_t ip, int ninst, rex_t rex, int rep, int* ok, int* need_epilog)
{
    uint8_t opcode = F8;
    uint8_t nextop, u8;
    int8_t  i8;
    uint32_t u32;
    int32_t i32, j32;
    int16_t i16;
    uint16_t u16;
    uint8_t gd, ed;
    uint8_t wback, wb1;
    int fixedaddress;
    MAYUSE(u16);
    MAYUSE(u8);
    MAYUSE(j32);

    // REX prefix before the 67 are ignored
    rex.rex = 0;
    while(opcode>=0x40 && opcode<=0x4f) {
        rex.rex = opcode;
        opcode = F8;
    }

    switch(opcode) {

        #define GO(NO, YES)   \
            BARRIER(2); \
            JUMP(addr+i8);\
            if(dyn->insts) {    \
                if(dyn->insts[ninst].x64.jmp_insts==-1) {   \
                    /* out of the block */                  \
                    i32 = dyn->insts[ninst+1].address-(dyn->arm_size); \
                    Bcond(NO, i32);     \
                    jump_to_next(dyn, addr+i8, 0, ninst); \
                } else {    \
                    /* inside the block */  \
                    i32 = dyn->insts[dyn->insts[ninst].x64.jmp_insts].address-(dyn->arm_size);    \
                    Bcond(YES, i32);    \
                }   \
            }
        case 0xE0:
            INST_NAME("LOOPNZ (32bits)");
            READFLAGS(X_ZF);
            i8 = F8S;
            MOVw_REG(x1, xRCX);
            SUBSw_U12(x1, x1, 1);
            BFIx(xRCX, x1, 0, 32);
            B_NEXT(cEQ);    // ECX is 0, no LOOP
            TSTw_mask(xFlags, 0b011010, 0); //mask=0x40
            GO(cNE, cEQ);
            break;
        case 0xE1:
            INST_NAME("LOOPZ (32bits)");
            READFLAGS(X_ZF);
            i8 = F8S;
            MOVw_REG(x1, xRCX);
            SUBSw_U12(x1, x1, 1);
            BFIx(xRCX, x1, 0, 32);
            B_NEXT(cEQ);    // ECX is 0, no LOOP
            TSTw_mask(xFlags, 0b011010, 0); //mask=0x40
            GO(cEQ, cNE);
            break;
        case 0xE2:
            INST_NAME("LOOP (32bits)");
            i8 = F8S;
            MOVw_REG(x1, xRCX);
            SUBSw_U12(x1, x1, 1);
            BFIx(xRCX, x1, 0, 32);
            GO(cEQ, cNE);
            break;
        case 0xE3:
            INST_NAME("JECXZ");
            i8 = F8S;
            MOVw_REG(x1, xRCX);
            TSTw_REG(x1, x1);
            GO(cNE, cEQ);
            break;
        #undef GO

        default:
            DEFAULT;
    }
    return addr;
}
