/******************************************************************************
 * hvm/emulate.h
 * 
 * HVM instruction emulation. Used for MMIO and VMX real mode.
 * 
 * Copyright (c) 2008 Citrix Systems, Inc.
 * 
 * Authors:
 *    Keir Fraser <keir@xen.org>
 */

#ifndef __ASM_X86_HVM_EMULATE_H__
#define __ASM_X86_HVM_EMULATE_H__

#include <xen/config.h>
#include <asm/hvm/hvm.h>
#include <asm/x86_emulate.h>

struct hvm_emulate_ctxt {
    struct x86_emulate_ctxt ctxt;

    /* Cache of 16 bytes of instruction. */
    uint8_t insn_buf[16];
    unsigned long insn_buf_eip;
    unsigned int insn_buf_bytes;

    struct segment_register seg_reg[10];
    unsigned long seg_reg_accessed;
    unsigned long seg_reg_dirty;

    bool_t exn_pending;
    struct hvm_trap trap;

    uint32_t intr_shadow;

    bool_t set_context;
};

enum emul_kind {
    EMUL_KIND_NORMAL,
    EMUL_KIND_NOWRITE,
    EMUL_KIND_SET_CONTEXT
};

int hvm_emulate_one(
    struct hvm_emulate_ctxt *hvmemul_ctxt);
int hvm_emulate_one_no_write(
    struct hvm_emulate_ctxt *hvmemul_ctxt);
void hvm_mem_access_emulate_one(enum emul_kind kind,
    unsigned int trapnr,
    unsigned int errcode);
void hvm_emulate_prepare(
    struct hvm_emulate_ctxt *hvmemul_ctxt,
    struct cpu_user_regs *regs);
void hvm_emulate_writeback(
    struct hvm_emulate_ctxt *hvmemul_ctxt);
struct segment_register *hvmemul_get_seg_reg(
    enum x86_segment seg,
    struct hvm_emulate_ctxt *hvmemul_ctxt);
int hvm_emulate_one_mmio(unsigned long mfn, unsigned long gla);

int hvmemul_do_pio_buffer(uint16_t port,
                          unsigned int size,
                          uint8_t dir,
                          void *buffer);

void hvm_dump_emulation_state(const char *prefix,
                              struct hvm_emulate_ctxt *hvmemul_ctxt);

#endif /* __ASM_X86_HVM_EMULATE_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
