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
#include "dynarec_native.h"

#include "arm64_printer.h"
#include "dynarec_arm64_private.h"
#include "dynarec_arm64_helper.h"
#include "dynarec_arm64_functions.h"

#define GETG        gd = ((nextop&0x38)>>3)+(rex.r<<3)

uintptr_t dynarec64_64(dynarec_arm_t* dyn, uintptr_t addr, uintptr_t ip, int ninst, rex_t rex, int rep, int seg, int* ok, int* need_epilog)
{
    (void)ip; (void)rep; (void)need_epilog;

    uint8_t opcode = F8;
    uint8_t nextop;
    uint8_t u8;
    uint8_t gd, ed, eb1, eb2, gb1, gb2;
    uint8_t wback, wb1, wb2, wb;
    int64_t i64, j64;
    int v0, v1;
    int q0;
    int d0;
    int64_t fixedaddress;
    MAYUSE(eb1);
    MAYUSE(eb2);
    MAYUSE(wb1);
    MAYUSE(wb2);
    MAYUSE(gb1);
    MAYUSE(gb2);
    MAYUSE(j64);
    MAYUSE(d0);
    MAYUSE(q0);
    MAYUSE(v0);
    MAYUSE(v1);

    while((opcode==0xF2) || (opcode==0xF3)) {
        rep = opcode-0xF1;
        opcode = F8;
    }
    // REX prefix before the F0 are ignored
    rex.rex = 0;
    while(opcode>=0x40 && opcode<=0x4f) {
        rex.rex = opcode;
        opcode = F8;
    }

    switch(opcode) {

        case 0x03:
            INST_NAME("ADD Gd, Seg:Ed");
            SETFLAGS(X_ALL, SF_SET_PENDING);
            grab_segdata(dyn, addr, ninst, x4, seg);
            nextop = F8;
            GETGD;
            GETEDO(x4, 0);
            emit_add32(dyn, ninst, rex, gd, ed, x3, x4);
            break;

        case 0x0F:
            opcode = F8;
            switch(opcode) {

                case 0x10:
                    switch(rep) {
                        case 1:
                            INST_NAME("MOVSD Gx, Ex");
                            nextop = F8;
                            GETG;
                            if(MODREG) {
                                ed = (nextop&7)+ (rex.b<<3);
                                v0 = sse_get_reg(dyn, ninst, x1, gd);
                                d0 = sse_get_reg(dyn, ninst, x1, ed);
                                VMOVeD(v0, 0, d0, 0);
                            } else {
                                grab_segdata(dyn, addr, ninst, x4, seg);
                                v0 = sse_get_reg_empty(dyn, ninst, x1, gd);
                                addr = geted(dyn, addr, ninst, nextop, &ed, x1, &fixedaddress, 0xfff<<3, 7, rex, 0, 0);
                                ADDx_REG(x4, x4, ed);
                                VLDR64_U12(v0, x4, fixedaddress); // upper part reseted
                            }
                            break;
                        case 2:
                            INST_NAME("MOVSS Gx, Ex");
                            nextop = F8;
                            GETG;
                            if(MODREG) {
                                v0 = sse_get_reg(dyn, ninst, x1, gd);
                                q0 = sse_get_reg(dyn, ninst, x1, (nextop&7) + (rex.b<<3));
                                VMOVeS(v0, 0, q0, 0);
                            } else {
                                grab_segdata(dyn, addr, ninst, x4, seg);
                                v0 = sse_get_reg_empty(dyn, ninst, x1, gd);
                                addr = geted(dyn, addr, ninst, nextop, &ed, x1, &fixedaddress, 0xfff<<2, 3, rex, 0, 0);
                                ADDx_REG(x4, x4, ed);
                                VLDR32_U12(v0, x4, fixedaddress);
                            }
                            break;
                        default:
                            DEFAULT;
                    }
                    break;
                case 0x11:
                    switch(rep) {
                        case 0:
                            INST_NAME("MOVUPS Ex,Gx");
                            nextop = F8;
                            GETG;
                            v0 = sse_get_reg(dyn, ninst, x1, gd);
                            if(MODREG) {
                                ed = (nextop&7)+(rex.b<<3);
                                v1 = sse_get_reg_empty(dyn, ninst, x1, ed);
                                VMOVQ(v1, v0);
                            } else {
                                grab_segdata(dyn, addr, ninst, x4, seg);
                                addr = geted(dyn, addr, ninst, nextop, &ed, x1, &fixedaddress, 0xfff<<4, 15, rex, 0, 0);
                                ADDx_REG(x4, x4, ed);
                                VSTR128_U12(v0, x4, fixedaddress);
                            }
                            break;
                        case 1:
                            INST_NAME("MOVSD Ex, Gx");
                            nextop = F8;
                            GETG;
                            v0 = sse_get_reg(dyn, ninst, x1, gd);
                            if(MODREG) {
                                ed = (nextop&7)+ (rex.b<<3);
                                d0 = sse_get_reg(dyn, ninst, x1, ed);
                                VMOVeD(d0, 0, v0, 0);
                            } else {
                                grab_segdata(dyn, addr, ninst, x4, seg);
                                addr = geted(dyn, addr, ninst, nextop, &ed, x1, &fixedaddress, 0xfff<<3, 7, rex, 0, 0);
                                ADDx_REG(x4, x4, ed);
                                VSTR64_U12(v0, x4, fixedaddress);
                            }
                            break;
                        case 2:
                            INST_NAME("MOVSS Ex, Gx");
                            nextop = F8;
                            GETG;
                            v0 = sse_get_reg(dyn, ninst, x1, gd);
                            if(MODREG) {
                                q0 = sse_get_reg(dyn, ninst, x1, (nextop&7) + (rex.b<<3));
                                VMOVeS(q0, 0, v0, 0);
                            } else {
                                grab_segdata(dyn, addr, ninst, x4, seg);
                                addr = geted(dyn, addr, ninst, nextop, &ed, x1, &fixedaddress, 0xfff<<2, 3, rex, 0, 0);
                                ADDx_REG(x4, x4, ed);
                                VSTR32_U12(v0, x4, fixedaddress);
                            }
                            break;
                        default:
                            DEFAULT;
                    }
                    break;

                case 0x6F:
                    switch(rep) {
                        case 2:
                            INST_NAME("MOVDQU Gx,Ex");// no alignment constraint on NEON here, so same as MOVDQA
                            nextop = F8;
                            GETG;
                            v0 = sse_get_reg_empty(dyn, ninst, x1, gd);
                            if(MODREG) {
                                v1 = sse_get_reg(dyn, ninst, x1, (nextop&7)+(rex.b<<3));
                                VMOVQ(v0, v1);
                            } else {
                                grab_segdata(dyn, addr, ninst, x4, seg);
                                addr = geted(dyn, addr, ninst, nextop, &ed, x1, &fixedaddress, 0xfff<<4, 15, rex, 0, 0);
                                ADDx_REG(x4, x4, ed);
                                VLDR128_U12(v0, ed, fixedaddress);
                            }
                            break;
                        default:
                            DEFAULT;
                    }
                    break;


                case 0xAF:
                    INST_NAME("IMUL Gd, Ed");
                    SETFLAGS(X_ALL, SF_PENDING);
                    nextop = F8;
                    grab_segdata(dyn, addr, ninst, x4, seg);
                    GETGD;
                    GETEDO(x4, 0);
                    if(rex.w) {
                        // 64bits imul
                        UFLAG_IF {
                            SMULH(x3, gd, ed);
                            MULx(gd, gd, ed);
                            UFLAG_OP1(x3);
                            UFLAG_RES(gd);
                            UFLAG_DF(x3, d_imul64);
                        } else {
                            MULxw(gd, gd, ed);
                        }
                    } else {
                        // 32bits imul
                        UFLAG_IF {
                            SMULL(gd, gd, ed);
                            UFLAG_RES(gd);
                            LSRx(x3, gd, 32);
                            UFLAG_OP1(x3);
                            UFLAG_DF(x3, d_imul32);
                            MOVw_REG(gd, gd);
                        } else {
                            MULxw(gd, gd, ed);
                        }
                    }
                    break;

                case 0xB6:
                    INST_NAME("MOVZX Gd, Eb");
                    nextop = F8;
                    grab_segdata(dyn, addr, ninst, x4, seg);
                    GETGD;
                    if(MODREG) {
                        if(rex.rex) {
                            eb1 = xRAX+(nextop&7)+(rex.b<<3);
                            eb2 = 0;                \
                        } else {
                            ed = (nextop&7);
                            eb1 = xRAX+(ed&3);  // Ax, Cx, Dx or Bx
                            eb2 = (ed&4)>>2;    // L or H
                        }
                        UBFXxw(gd, eb1, eb2*8, 8);
                    } else {
                        addr = geted(dyn, addr, ninst, nextop, &ed, x2, &fixedaddress, 0, 0, rex, 0, 0);
                        LDRB_REG(gd, ed, x4);
                    }
                    break;

                default:
                    DEFAULT;
            }
            break;

        case 0x33:
            INST_NAME("XOR Gd, Seg:Ed");
            SETFLAGS(X_ALL, SF_SET_PENDING);
            grab_segdata(dyn, addr, ninst, x4, seg);
            nextop = F8;
            GETGD;
            GETEDO(x4, 0);
            emit_xor32(dyn, ninst, rex, gd, ed, x3, x4);
            break;
                    
        case 0x39:
            INST_NAME("CMP Seg:Ed, Gd");
            SETFLAGS(X_ALL, SF_SET_PENDING);
            grab_segdata(dyn, addr, ninst, x4, seg);
            nextop = F8;
            GETGD;
            GETEDO(x4, 0);
            emit_cmp32(dyn, ninst, rex, ed, gd, x3, x4, x5);
            break;

        case 0x3B:
            INST_NAME("CMP Gd, Seg:Ed");
            SETFLAGS(X_ALL, SF_SET_PENDING);
            grab_segdata(dyn, addr, ninst, x4, seg);
            nextop = F8;
            GETGD;
            GETEDO(x4, 0);
            emit_cmp32(dyn, ninst, rex, gd, ed, x3, x4, x5);
            break;

        case 0x63:
            INST_NAME("MOVSXD Gd, Ed");
            nextop = F8;
            GETGD;
            if(rex.w) {
                if(MODREG) {   // reg <= reg
                    SXTWx(gd, xRAX+(nextop&7)+(rex.b<<3));
                } else {                    // mem <= reg
                    grab_segdata(dyn, addr, ninst, x4, seg);
                    addr = geted(dyn, addr, ninst, nextop, &ed, x2, &fixedaddress, 0, 0, rex, 0, 0);
                    LDRSW_REG(gd, ed, x4);
                }
            } else {
                if(MODREG) {   // reg <= reg
                    MOVw_REG(gd, xRAX+(nextop&7)+(rex.b<<3));
                } else {                    // mem <= reg
                    grab_segdata(dyn, addr, ninst, x4, seg);
                    addr = geted(dyn, addr, ninst, nextop, &ed, x2, &fixedaddress, 0, 0, rex, 0, 0);
                    LDRw_REG(gd, ed, x4);
                }
            }
            break;

        case 0x66:
            addr = dynarec64_6664(dyn, addr, ip, ninst, rex, rep, ok, need_epilog);
            break;

        case 0x80:
            nextop = F8;
            switch((nextop>>3)&7) {
                case 0: //ADD
                    INST_NAME("ADD Eb, Ib");
                    grab_segdata(dyn, addr, ninst, x1, seg);
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEBO(x1, 1);
                    u8 = F8;
                    emit_add8c(dyn, ninst, x1, u8, x2, x4);
                    EBBACK;
                    break;
                case 1: //OR
                    INST_NAME("OR Eb, Ib");
                    grab_segdata(dyn, addr, ninst, x1, seg);
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEBO(x1, 1);
                    u8 = F8;
                    emit_or8c(dyn, ninst, x1, u8, x2, x4);
                    EBBACK;
                    break;
                case 2: //ADC
                    INST_NAME("ADC Eb, Ib");
                    grab_segdata(dyn, addr, ninst, x1, seg);
                    READFLAGS(X_CF);
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEBO(x1, 1);
                    u8 = F8;
                    emit_adc8c(dyn, ninst, x1, u8, x2, x4, x5);
                    EBBACK;
                    break;
                case 3: //SBB
                    INST_NAME("SBB Eb, Ib");
                    grab_segdata(dyn, addr, ninst, x1, seg);
                    READFLAGS(X_CF);
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEBO(x1, 1);
                    u8 = F8;
                    emit_sbb8c(dyn, ninst, x1, u8, x2, x4, x5);
                    EBBACK;
                    break;
                case 4: //AND
                    INST_NAME("AND Eb, Ib");
                    grab_segdata(dyn, addr, ninst, x1, seg);
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEBO(x1, 1);
                    u8 = F8;
                    emit_and8c(dyn, ninst, x1, u8, x2, x4);
                    EBBACK;
                    break;
                case 5: //SUB
                    INST_NAME("SUB Eb, Ib");
                    grab_segdata(dyn, addr, ninst, x1, seg);
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEBO(x1, 1);
                    u8 = F8;
                    emit_sub8c(dyn, ninst, x1, u8, x2, x4, x5);
                    EBBACK;
                    break;
                case 6: //XOR
                    INST_NAME("XOR Eb, Ib");
                    grab_segdata(dyn, addr, ninst, x1, seg);
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEBO(x1, 1);
                    u8 = F8;
                    emit_xor8c(dyn, ninst, x1, u8, x2, x4);
                    EBBACK;
                    break;
                case 7: //CMP
                    INST_NAME("CMP Eb, Ib");
                    grab_segdata(dyn, addr, ninst, x1, seg);
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEBO(x1, 1);
                    u8 = F8;
                    if(u8) {
                        MOV32w(x2, u8);
                        emit_cmp8(dyn, ninst, x1, x2, x3, x4, x5);
                    } else {
                        emit_cmp8_0(dyn, ninst, x1, x3, x4);
                    }
                    break;
                default:
                    DEFAULT;
            }
            break;
        case 0x81:
        case 0x83:
            nextop = F8;
            grab_segdata(dyn, addr, ninst, x6, seg);
            switch((nextop>>3)&7) {
                case 0: //ADD
                    if(opcode==0x81) {INST_NAME("ADD Ed, Id");} else {INST_NAME("ADD Ed, Ib");}
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEDO(x6, (opcode==0x81)?4:1);
                    if(opcode==0x81) i64 = F32S; else i64 = F8S;
                    emit_add32c(dyn, ninst, rex, ed, i64, x3, x4, x5);
                    WBACKO(x6);
                    break;
                case 1: //OR
                    if(opcode==0x81) {INST_NAME("OR Ed, Id");} else {INST_NAME("OR Ed, Ib");}
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEDO(x6, (opcode==0x81)?4:1);
                    if(opcode==0x81) i64 = F32S; else i64 = F8S;
                    emit_or32c(dyn, ninst, rex, ed, i64, x3, x4);
                    WBACKO(x6);
                    break;
                case 2: //ADC
                    if(opcode==0x81) {INST_NAME("ADC Ed, Id");} else {INST_NAME("ADC Ed, Ib");}
                    READFLAGS(X_CF);
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEDO(x6, (opcode==0x81)?4:1);
                    if(opcode==0x81) i64 = F32S; else i64 = F8S;
                    MOV64xw(x5, i64);
                    emit_adc32(dyn, ninst, rex, ed, x5, x3, x4);
                    WBACKO(x6);
                    break;
                case 3: //SBB
                    if(opcode==0x81) {INST_NAME("SBB Ed, Id");} else {INST_NAME("SBB Ed, Ib");}
                    READFLAGS(X_CF);
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEDO(x6, (opcode==0x81)?4:1);
                    if(opcode==0x81) i64 = F32S; else i64 = F8S;
                    MOV64xw(x5, i64);
                    emit_sbb32(dyn, ninst, rex, ed, x5, x3, x4);
                    WBACKO(x6);
                    break;
                case 4: //AND
                    if(opcode==0x81) {INST_NAME("AND Ed, Id");} else {INST_NAME("AND Ed, Ib");}
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEDO(x6, (opcode==0x81)?4:1);
                    if(opcode==0x81) i64 = F32S; else i64 = F8S;
                    emit_and32c(dyn, ninst, rex, ed, i64, x3, x4);
                    WBACKO(x6);
                    break;
                case 5: //SUB
                    if(opcode==0x81) {INST_NAME("SUB Ed, Id");} else {INST_NAME("SUB Ed, Ib");}
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEDO(x6, (opcode==0x81)?4:1);
                    if(opcode==0x81) i64 = F32S; else i64 = F8S;
                    emit_sub32c(dyn, ninst, rex, ed, i64, x3, x4, x5);
                    WBACKO(x6);
                    break;
                case 6: //XOR
                    if(opcode==0x81) {INST_NAME("XOR Ed, Id");} else {INST_NAME("XOR Ed, Ib");}
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEDO(x6, (opcode==0x81)?4:1);
                    if(opcode==0x81) i64 = F32S; else i64 = F8S;
                    emit_xor32c(dyn, ninst, rex, ed, i64, x3, x4);
                    WBACKO(x6);
                    break;
                case 7: //CMP
                    if(opcode==0x81) {INST_NAME("CMP Ed, Id");} else {INST_NAME("CMP Ed, Ib");}
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEDO(x6, (opcode==0x81)?4:1);
                    if(opcode==0x81) i64 = F32S; else i64 = F8S;
                    if(i64) {
                        MOV64xw(x2, i64);
                        emit_cmp32(dyn, ninst, rex, ed, x2, x3, x4, x5);
                    } else
                        emit_cmp32_0(dyn, ninst, rex, ed, x3, x4);
                    break;
            }
            break;
        case 0x8A:
            INST_NAME("MOV Gb, Eb");
            nextop = F8;
            if(rex.rex) {
                gb1 = gd = xRAX+((nextop&0x38)>>3)+(rex.r<<3);
                gb2=0;
            } else {
                gd = (nextop&0x38)>>3;
                gb1 = xRAX+(gd&3);
                gb2 = ((gd&4)>>2);
            }
            if(MODREG) {
                if(rex.rex) {
                    wback = xRAX+(nextop&7)+(rex.b<<3);
                    wb2 = 0;
                } else {
                    wback = (nextop&7);
                    wb2 = (wback>>2);
                    wback = xRAX+(wback&3);
                }
                if(wb2) {
                    UBFXw(x4, wback, wb2*8, 8);
                    ed = x4;
                } else {
                    ed = wback;
                }
            } else {
                grab_segdata(dyn, addr, ninst, x4, seg);
                addr = geted(dyn, addr, ninst, nextop, &wback, x3, &fixedaddress, 0, 0, rex, 0, 0);
                LDRB_REG(x4, wback, x4);
                ed = x4;
            }
            BFIx(gb1, ed, gb2*8, 8);
            break;
        case 0x89:
            INST_NAME("MOV Seg:Ed, Gd");
            grab_segdata(dyn, addr, ninst, x4, seg);
            nextop=F8;
            GETGD;
            if(MODREG) {   // reg <= reg
                MOVxw_REG(xRAX+(nextop&7)+(rex.b<<3), gd);
            } else {                    // mem <= reg
                addr = geted(dyn, addr, ninst, nextop, &ed, x2, &fixedaddress, 0, 0, rex, 0, 0);
                STRxw_REG(gd, ed, x4);
            }
            break;

        case 0x8B:
            INST_NAME("MOV Gd, Seg:Ed");
            grab_segdata(dyn, addr, ninst, x4, seg);
            nextop=F8;
            GETGD;
            if(MODREG) {   // reg <= reg
                MOVxw_REG(gd, xRAX+(nextop&7)+(rex.b<<3));
            } else {                    // mem <= reg
                addr = geted(dyn, addr, ninst, nextop, &ed, x2, &fixedaddress, 0, 0, rex, 0, 0);
                LDRxw_REG(gd, ed, x4);
            }
            break;

        case 0xC6:
            INST_NAME("MOV Seg:Eb, Ib");
            grab_segdata(dyn, addr, ninst, x4, seg);
            nextop=F8;
            if(MODREG) {   // reg <= u8
                u8 = F8;
                if(!rex.rex) {
                    ed = (nextop&7);
                    eb1 = xRAX+(ed&3);  // Ax, Cx, Dx or Bx
                    eb2 = (ed&4)>>2;    // L or H
                } else {
                    eb1 = xRAX+(nextop&7)+(rex.b<<3);
                    eb2 = 0;            
                }
                MOV32w(x3, u8);
                BFIx(eb1, x3, eb2*8, 8);
            } else {                    // mem <= u8
                addr = geted(dyn, addr, ninst, nextop, &ed, x1, &fixedaddress, 0, 0, rex, 0, 1);
                u8 = F8;
                MOV32w(x3, u8);
                STRB_REG(x3, ed, x4);
            }
            break;
        case 0xC7:
            INST_NAME("MOV Seg:Ed, Id");
            grab_segdata(dyn, addr, ninst, x4, seg);
            nextop=F8;
            if(MODREG) {   // reg <= i32
                i64 = F32S;
                ed = xRAX+(nextop&7)+(rex.b<<3);
                MOV64xw(ed, i64);
            } else {                    // mem <= i32
                addr = geted(dyn, addr, ninst, nextop, &ed, x2, &fixedaddress, 0, 0, rex, 0, 4);
                i64 = F32S;
                MOV64xw(x3, i64);
                STRxw_REG(x3, ed, x4);
            }
            break;

        case 0xD1:
            nextop = F8;
            grab_segdata(dyn, addr, ninst, x6, seg);
            switch((nextop>>3)&7) {
                case 0:
                    INST_NAME("ROL Ed, 1");
                    SETFLAGS(X_OF|X_CF, SF_SUBSET);
                    GETEDO(x6, 0);
                    emit_rol32c(dyn, ninst, rex, ed, 1, x3, x4);
                    WBACKO(x6);
                    break;
                case 1:
                    INST_NAME("ROR Ed, 1");
                    SETFLAGS(X_OF|X_CF, SF_SUBSET);
                    GETEDO(x6, 0);
                    emit_ror32c(dyn, ninst, rex, ed, 1, x3, x4);
                    WBACKO(x6);
                    break;
                case 2:
                    INST_NAME("RCL Ed, 1");
                    MESSAGE(LOG_DUMP, "Need Optimization\n");
                    READFLAGS(X_CF);
                    SETFLAGS(X_OF|X_CF, SF_SET);
                    MOV32w(x2, 1);
                    GETEDO(x6, 0);
                    if(wback) {ADDx_REG(x6, x6, wback); wback=x6;}
                    if(ed!=x1) {MOVxw_REG(x1, ed);}
                    CALL_(rcl32, ed, x6);
                    WBACK;
                    break;
                case 3:
                    INST_NAME("RCR Ed, 1");
                    MESSAGE(LOG_DUMP, "Need Optimization\n");
                    READFLAGS(X_CF);
                    SETFLAGS(X_OF|X_CF, SF_SET);
                    MOV32w(x2, 1);
                    GETEDO(x6, 0);
                    if(wback) {ADDx_REG(x6, x6, wback); wback=x6;}
                    if(ed!=x1) {MOVxw_REG(x1, ed);}
                    CALL_(rcr32, ed, x6);
                    WBACK;
                    break;
                case 4:
                case 6:
                    INST_NAME("SHL Ed, 1");
                    SETFLAGS(X_ALL, SF_SET_PENDING);    // some flags are left undefined
                    GETEDO(x6, 0);
                    emit_shl32c(dyn, ninst, rex, ed, 1, x3, x4);
                    WBACKO(x6);
                    break;
                case 5:
                    INST_NAME("SHR Ed, 1");
                    SETFLAGS(X_ALL, SF_SET_PENDING);    // some flags are left undefined
                    GETEDO(x6, 0);
                    emit_shr32c(dyn, ninst, rex, ed, 1, x3, x4);
                    WBACKO(x6);
                    break;
                case 7:
                    INST_NAME("SAR Ed, 1");
                    SETFLAGS(X_ALL, SF_SET_PENDING);    // some flags are left undefined
                    GETEDO(x6, 0);
                    emit_sar32c(dyn, ninst, rex, ed, 1, x3, x4);
                    WBACKO(x6);
                    break;
            }
            break;
        case 0xD3:
            nextop = F8;
            grab_segdata(dyn, addr, ninst, x6, seg);
            switch((nextop>>3)&7) {
                case 0:
                    INST_NAME("ROL Ed, CL");
                    SETFLAGS(X_OF|X_CF, SF_SUBSET);
                    if(rex.w) {
                        ANDSx_mask(x3, xRCX, 1, 0, 0b00101);  //mask=0x000000000000003f
                    } else {
                        ANDSw_mask(x3, xRCX, 0, 0b00100);  //mask=0x00000001f
                    }
                    MOV64xw(x4, (rex.w?64:32));
                    SUBx_REG(x3, x4, x3);
                    GETEDO(x6, 0);
                    if(!rex.w && MODREG) {MOVw_REG(ed, ed);}
                    B_NEXT(cEQ);
                    RORxw_REG(ed, ed, x3);
                    WBACKO(x6);
                    UFLAG_IF {  // calculate flags directly
                        CMPSw_U12(x3, rex.w?63:31);
                        B_MARK(cNE);
                            LSRxw(x4, ed, rex.w?63:31);
                            ADDxw_REG(x4, x4, ed);
                            BFIw(xFlags, x4, F_OF, 1);
                        MARK;
                        BFIw(xFlags, ed, F_CF, 1);
                        UFLAG_DF(x2, d_none);
                    }
                    break;
                case 1:
                    INST_NAME("ROR Ed, CL");
                    SETFLAGS(X_OF|X_CF, SF_SUBSET);
                    if(rex.w) {
                        ANDSx_mask(x3, xRCX, 1, 0, 0b00101);  //mask=0x000000000000003f
                    } else {
                        ANDSw_mask(x3, xRCX, 0, 0b00100);  //mask=0x00000001f
                    }
                    GETEDO(x6, 0);
                    if(!rex.w && MODREG) {MOVw_REG(ed, ed);}
                    B_NEXT(cEQ);
                    RORxw_REG(ed, ed, x3);
                    WBACKO(x6);
                    UFLAG_IF {  // calculate flags directly
                        CMPSw_U12(x3, 1);
                        B_MARK(cNE);
                            LSRxw(x2, ed, rex.w?62:30); // x2 = d>>30
                            EORw_REG_LSR(x2, x2, x2, 1); // x2 = ((d>>30) ^ ((d>>30)>>1))
                            BFIw(xFlags, x2, F_OF, 1);
                        MARK;
                        LSRxw(x2, ed, rex.w?63:31);
                        BFIw(xFlags, x2, F_CF, 1);
                        UFLAG_DF(x2, d_none);
                    }
                    break;
                case 2:
                    INST_NAME("RCL Ed, CL");
                    MESSAGE(LOG_DUMP, "Need Optimization\n");
                    READFLAGS(X_CF);
                    SETFLAGS(X_OF|X_CF, SF_SET);
                    if(rex.w) {
                        ANDSx_mask(x2, xRCX, 1, 0, 0b00101);  //mask=0x000000000000003f
                    } else {
                        ANDSw_mask(x2, xRCX, 0, 0b00100);  //mask=0x00000001f
                    }
                    GETEDO(x6, 0);
                    if(wback) {ADDx_REG(x6, x6, wback); wback=x6;}
                    if(!rex.w && MODREG) {MOVw_REG(ed, ed);}
                    B_NEXT(cEQ);
                    CALL_(rex.w?((void*)rcl64):((void*)rcl32), ed, x6);
                    WBACK;
                    break;
                case 3:
                    INST_NAME("RCR Ed, CL");
                    MESSAGE(LOG_DUMP, "Need Optimization\n");
                    READFLAGS(X_CF);
                    SETFLAGS(X_OF|X_CF, SF_SET);
                    if(rex.w) {
                        ANDSx_mask(x2, xRCX, 1, 0, 0b00101);  //mask=0x000000000000003f
                    } else {
                        ANDSw_mask(x2, xRCX, 0, 0b00100);  //mask=0x00000001f
                    }
                    GETEDO(x6, 0);
                    if(wback) {ADDx_REG(x6, x6, wback); wback=x6;}
                    if(!rex.w && MODREG) {MOVw_REG(ed, ed);}
                    B_NEXT(cEQ);
                    CALL_(rex.w?((void*)rcr64):((void*)rcr32), ed, x6);
                    WBACK;
                    break;
                case 4:
                case 6:
                    INST_NAME("SHL Ed, CL");
                    SETFLAGS(X_ALL, SF_SET_PENDING);    // some flags are left undefined
                    if(rex.w) {
                        ANDSx_mask(x3, xRCX, 1, 0, 0b00101);  //mask=0x000000000000003f
                    } else {
                        ANDSw_mask(x3, xRCX, 0, 0b00100);  //mask=0x00000001f
                    }
                    GETEDO(x6, 0);
                    if(!rex.w && MODREG) {MOVw_REG(ed, ed);}
                    B_NEXT(cEQ);
                    emit_shl32(dyn, ninst, rex, ed, x3, x5, x4);
                    WBACKO(x6);
                    break;
                case 5:
                    INST_NAME("SHR Ed, CL");
                    SETFLAGS(X_ALL, SF_SET_PENDING);    // some flags are left undefined
                    if(rex.w) {
                        ANDSx_mask(x3, xRCX, 1, 0, 0b00101);  //mask=0x000000000000003f
                    } else {
                        ANDSw_mask(x3, xRCX, 0, 0b00100);  //mask=0x00000001f
                    }
                    GETEDO(x6, 0);
                    if(!rex.w && MODREG) {MOVw_REG(ed, ed);}
                    B_NEXT(cEQ);
                    emit_shr32(dyn, ninst, rex, ed, x3, x5, x4);
                    WBACKO(x6);
                    break;
                case 7:
                    INST_NAME("SAR Ed, CL");
                    SETFLAGS(X_ALL, SF_PENDING);
                    if(rex.w) {
                        ANDSx_mask(x3, xRCX, 1, 0, 0b00101);  //mask=0x000000000000003f
                    } else {
                        ANDSw_mask(x3, xRCX, 0, 0b00100);  //mask=0x00000001f
                    }
                    GETEDO(x6, 0);
                    if(!rex.w && MODREG) {MOVw_REG(ed, ed);}
                    B_NEXT(cEQ);
                    UFLAG_OP12(ed, x3);
                    ASRxw_REG(ed, ed, x3);
                    WBACKO(x6);
                    UFLAG_RES(ed);
                    UFLAG_DF(x3, rex.w?d_sar64:d_sar32);
                    break;
            }
            break;
            
        case 0xF7:
            nextop = F8;
            switch((nextop>>3)&7) {
                case 0:
                case 1:
                    INST_NAME("TEST Ed, Id");
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEDO(x6, 4);
                    i64 = F32S;
                    MOV64xw(x2, i64);
                    emit_test32(dyn, ninst, rex, ed, x2, x3, x4);
                    break;
                case 2:
                    INST_NAME("NOT Ed");
                    GETEDO(x6, 4);
                    MVNxw_REG(ed, ed);
                    WBACKO(x6);
                    break;
                case 3:
                    INST_NAME("NEG Ed");
                    SETFLAGS(X_ALL, SF_SET_PENDING);
                    GETEDO(x6, 0);
                    emit_neg32(dyn, ninst, rex, ed, x3, x4);
                    WBACKO(x6);
                    break;
                case 4:
                    INST_NAME("MUL EAX, Ed");
                    SETFLAGS(X_ALL, SF_PENDING);
                    UFLAG_DF(x2, rex.w?d_mul64:d_mul32);
                    GETEDO(x6, 0);
                    if(rex.w) {
                        if(ed==xRDX) gd=x3; else gd=xRDX;
                        UMULH(gd, xRAX, ed);
                        MULx(xRAX, xRAX, ed);
                        if(gd!=xRDX) {MOVx_REG(xRDX, gd);}
                    } else {
                        UMULL(xRDX, xRAX, ed);  //64 <- 32x32
                        MOVw_REG(xRAX, xRDX);
                        LSRx(xRDX, xRDX, 32);
                    }
                    UFLAG_RES(xRAX);
                    UFLAG_OP1(xRDX);
                    break;
                case 5:
                    INST_NAME("IMUL EAX, Ed");
                    SETFLAGS(X_ALL, SF_PENDING);
                    UFLAG_DF(x2, rex.w?d_imul64:d_imul32);
                    GETEDO(x6, 0);
                    if(rex.w) {
                        if(ed==xRDX) gd=x3; else gd=xRDX;
                        SMULH(gd, xRAX, ed);
                        MULx(xRAX, xRAX, ed);
                        if(gd!=xRDX) {MOVx_REG(xRDX, gd);}
                    } else {
                        SMULL(xRDX, xRAX, ed);  //64 <- 32x32
                        MOVw_REG(xRAX, xRDX);
                        LSRx(xRDX, xRDX, 32);
                    }
                    UFLAG_RES(xRAX);
                    UFLAG_OP1(xRDX);
                    break;
                case 6:
                    INST_NAME("DIV Ed");
                    SETFLAGS(X_ALL, SF_SET);
                    if(!rex.w) {
                        SET_DFNONE(x2);
                        GETEDO(x6, 0);
                        MOVw_REG(x3, xRAX);
                        ORRx_REG_LSL(x3, x3, xRDX, 32);
                        if(MODREG) {
                            MOVw_REG(x4, ed);
                            ed = x4;
                        }
                        UDIVx(x2, x3, ed);
                        MSUBx(x4, x2, ed, xRAX);
                        MOVw_REG(xRAX, x2);
                        MOVw_REG(xRDX, x4);
                    } else {
                        if(ninst
                           && dyn->insts[ninst-1].x64.addr 
                           && *(uint8_t*)(dyn->insts[ninst-1].x64.addr)==0x31 
                           && *(uint8_t*)(dyn->insts[ninst-1].x64.addr+1)==0xD2) {
                            SET_DFNONE(x2);
                            GETEDO(x6, 0);
                            UDIVx(x2, xRAX, ed);
                            MSUBx(xRDX, x2, ed, xRAX);
                            MOVx_REG(xRAX, x2);
                        } else {
                            GETEDO(x6, 0);
                            CBZxw_MARK(xRDX);
                            if(ed!=x1) {MOVx_REG(x1, ed);}
                            CALL(div64, -1);
                            B_NEXT_nocond;
                            MARK;
                            UDIVx(x2, xRAX, ed);
                            MSUBx(xRDX, x2, ed, xRAX);
                            MOVx_REG(xRAX, x2);
                            SET_DFNONE(x2);
                        }
                    }
                    break;
                case 7:
                    INST_NAME("IDIV Ed");
                    SETFLAGS(X_ALL, SF_SET);
                    if(!rex.w) {
                        SET_DFNONE(x2)
                        GETSEDOw(x6, 0);
                        MOVw_REG(x3, xRAX);
                        ORRx_REG_LSL(x3, x3, xRDX, 32);
                        SDIVx(x2, x3, wb);
                        MSUBx(x4, x2, wb, x3);
                        MOVw_REG(xRAX, x2);
                        MOVw_REG(xRDX, x4);
                    } else {
                        if(ninst && dyn->insts
                           &&  dyn->insts[ninst-1].x64.addr 
                           && *(uint8_t*)(dyn->insts[ninst-1].x64.addr)==0x48
                           && *(uint8_t*)(dyn->insts[ninst-1].x64.addr+1)==0x99) {
                            SET_DFNONE(x2)
                            GETEDO(x6, 0);
                            SDIVx(x2, xRAX, ed);
                            MSUBx(xRDX, x2, ed, xRAX);
                            MOVx_REG(xRAX, x2);
                        } else {
                            GETEDO(x6, 0);
                            CBZxw_MARK(xRDX);
                            MVNx_REG(x2, xRDX);
                            CBZxw_MARK(x2);
                            if(ed!=x1) {MOVx_REG(x1, ed);}
                            CALL((void*)idiv64, -1);
                            B_NEXT_nocond;
                            MARK;
                            SDIVx(x2, xRAX, ed);
                            MSUBx(xRDX, x2, ed, xRAX);
                            MOVx_REG(xRAX, x2);
                            SET_DFNONE(x2)
                        }
                    }
                    break;
            }
            break;
            
        case 0xFF:
            nextop = F8;
            grab_segdata(dyn, addr, ninst, x6, seg);
            switch((nextop>>3)&7) {
                case 0: // INC Ed
                    INST_NAME("INC Ed");
                    SETFLAGS(X_ALL&~X_CF, SF_SUBSET);
                    GETEDO(x6, 0);
                    emit_inc32(dyn, ninst, rex, ed, x3, x4);
                    WBACKO(x6);
                    break;
                case 1: //DEC Ed
                    INST_NAME("DEC Ed");
                    SETFLAGS(X_ALL&~X_CF, SF_SUBSET);
                    GETEDO(x6, 0);
                    emit_dec32(dyn, ninst, rex, ed, x3, x4);
                    WBACKO(x6);
                    break;
                case 2: // CALL Ed
                    INST_NAME("CALL Ed");
                    PASS2IF(((ninst && dyn->insts[ninst-1].x64.set_flags)
                        || ((ninst>1) && dyn->insts[ninst-2].x64.set_flags)), 1)
                    {
                        READFLAGS(X_PEND);          // that's suspicious
                    } else {
                        SETFLAGS(X_ALL, SF_SET);    //Hack to put flag in "don't care" state
                    }
                    GETEDOx(x6, 0);
                    BARRIER(1);
                    BARRIER_NEXT(1);
                    if(!dyn->insts || ninst==dyn->size-1) {
                        *need_epilog = 0;
                        *ok = 0;
                    }
                    GETIP(addr);
                    PUSH1(xRIP);
                    jump_to_next(dyn, 0, ed, ninst);
                    break;
                case 4: // JMP Ed
                    INST_NAME("JMP Ed");
                    BARRIER(1);
                    GETEDOx(x6, 0);
                    jump_to_next(dyn, 0, ed, ninst);
                    *need_epilog = 0;
                    *ok = 0;
                    break;
                case 6: // Push Ed
                    INST_NAME("PUSH Ed");
                    GETEDOx(x6, 0);
                    PUSH1(ed);
                    break;

                default:
                    DEFAULT;
            }
            break;

        default:
            DEFAULT;
    }
    return addr;
}