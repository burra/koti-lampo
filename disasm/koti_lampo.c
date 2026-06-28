/*
 * koti_lampo.c
 *
 * MERGED, CORRECTED C reconstruction of the Valmet Kotilampo HVAC controller
 * firmware. Built by combining three views of the same code:
 *
 *   1. d48 disassembly      (disasm/3EF2H.d48, disasm/A98EH.d48)
 *        -> byte-exact instruction ground truth; per-bank addresses.
 *   2. Ghidra 8048 decompile (ghidra/out/combined_4k.ghidra.c)
 *        -> recovered control flow, resolved cross-bank SEL MB1 calls, and
 *           crucially the INTERNAL vs EXTERNAL memory-space split.
 *   3. Hand analysis
 *        -> inferred names and intent (the [LIKELY]/[GUESS] tags).
 *
 * Each function is anchored with its ROM address range (verify in *.d48) and,
 * where useful, the matching Ghidra symbol (FUN_CODE_xxxx).
 *
 * ---------------------------------------------------------------------------
 * CPU: Intel P8035L (MCS-48 / ROM-less 8048). 4 KB program space = two 2 KB
 * banks selected by SEL MB0 / SEL MB1:
 *     3EF2H.bin -> bank 0 (0x000-0x7FF)  boot, I/O, scan, regulation
 *     A98EH.bin -> bank 1 (0x800-0xFFF)  BCD math library + state machine
 *
 * ---------------------------------------------------------------------------
 * CORRECTION vs. the first reconstruction
 * ---------------------------------------------------------------------------
 * The 8048 has TWO distinct data memories, and the same numeric address can
 * live in either one:
 *   - INTERNAL RAM  (on-chip, accessed with `mov a,@r0` / `mov @r1,a`)
 *   - EXTERNAL RAM  (off-chip, accessed with `movx a,@r0` / `movx @r1,a`)
 * Ghidra correctly tags these as DAT_INTMEM_xx vs DAT_EXTMEM_xx; the first
 * hand pass wrongly merged them into one array. They are modelled here as two
 * separate arrays, IRAM[] and XRAM[]. In particular the BCD operands at
 * 0x24..0x3A used by the bank-1 math library are INTERNAL RAM, while the
 * 0x05/0x0A/0x0F working buffers and the 0xF4..0xF9 peripheral registers are
 * EXTERNAL.
 *
 * Confidence tags:  [SURE] 1:1 with opcodes | [LIKELY] logic clear, name
 * inferred | [GUESS] speculative hardware meaning.
 * ---------------------------------------------------------------------------
 *
 * This is a READING AID, not compilable firmware or a verified decompile.
 */

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t u8;

/* =========================================================================
 * Memory model
 * ========================================================================= */

/* INTERNAL on-chip RAM - `mov a,@r` / `mov @r,a`. Holds the bank-1 BCD
 * scratch/operand region (Ghidra: DAT_INTMEM_xx). [SURE - space confirmed by
 * both `mov` opcodes in d48 and Ghidra's INTMEM tagging] */
extern u8 IRAM[64];            /* 8048 has 64 bytes on-chip (R-banks + stack) */
#define I(a) IRAM[a]

/* EXTERNAL RAM + memory-mapped peripherals - `movx a,@r` / `movx @r,a`
 * (Ghidra: DAT_EXTMEM_xx). [SURE - space confirmed by `movx`]
 *
 * XRAM map [LIKELY/GUESS for purposes]:
 *   0x04-0x0A  working BCD buffers; primary accumulator at [0x05..0x01],
 *              staged operand at [0x06..0x0A], secondary buffer at [0x0F..].
 *   0x15       relay/output latch shadow (pushed to P7)            [LIKELY]
 *   0x16       timed-action countdown                              [GUESS]
 *   0x17-0x1A  event counters / one-shot flags                     [GUESS]
 *   0x1B       current menu / channel selection                    [GUESS]
 *   0x1C       display-scan state bitmask                           [LIKELY]
 *   0x1D       scan repeat counter                                  [GUESS]
 *   0x1E       debounce / valid-press counter                       [GUESS]
 *   0x1F,0x21  ISR save area (ACC, P2) under RB1                     [SURE]
 *   0x24-0x29  EXTERNAL parameter/measurement copies (distinct from the
 *              INTERNAL 0x24.. BCD scratch!)                         [SURE]
 *   0x34,0x4D,0x6D  parameter-table / schedule bytes                [LIKELY]
 *   0xF4       8155-style command / mux register                    [GUESS]
 *   0xF7       packed status/flag bits (bit0..bit4)                  [LIKELY]
 *   0xF8       P6 expander output shadow                             [LIKELY]
 *   0xF9       retry / anti-cycle counter                           [GUESS]
 */
extern u8 XRAM[256];
#define X(a) XRAM[a]

/* On-chip I/O ports */
extern u8 P1, P2, BUS;

/* 8243-style nibble expander on P4..P7 (movd/anld/orld). P2 high nibble is the
 * page/chip-select strobe (constants 0x8F/0x9F/0xAF/0xCF/0xDF/0xEF). [LIKELY] */
u8 movd_p4(void); void wr_p4(u8); u8 movd_p5(void); void wr_p5(u8);
u8 movd_p6(void); void wr_p6(u8); u8 movd_p7(void); void wr_p7(u8);
void or_p5(u8); void and_p5(u8); void or_p6(u8); void and_p6(u8);
void or_p7(u8); void and_p7(u8);

/* MCS-48 timer/counter + interrupts */
void timer_load(u8); u8 timer_read(void); void timer_start_count(void);
void timer_stop(void); bool timer_flag(void);
void en_int(void); void dis_int(void); void busy_delay(void);

/* The 8048 F1 software flag, threaded through helpers as a bool. Ghidra
 * surfaces it as the carry/return in several functions. [SURE it is F1] */
extern bool F1;

/* =========================================================================
 * BANK 0  (3EF2H.bin)  -  boot, I/O, scan, regulation
 * ========================================================================= */

/* @0x0000 reset vector (0x000=NOP, jmp X0005). Ghidra: RESET/FUN_CODE_0005. */
void reset(void)                                    /* X0005 @ 0x0005 */
{
    if (/* T0 pin low */ 0) seed_cold_boot();       /* jnt0 -> X002f */
    io_init();                                       /* X000d */
    main_scan_loop();                                /* X006f (never returns) */
}

/* @0x000d - park all expander ports in a known state. [SURE] */
void io_init(void)                                  /* X000d / FUN_CODE_000d */
{
    P2 = 0xFF;
    or_p7(0xFF); or_p6(0xFF); or_p5(0xFF);
    and_p7(0x00); and_p6(0x00);
    expander_kick(0x8F); expander_kick(0x9F); expander_kick(0xAF);
    P2 = 0xFF;
    (void)movd_p4();
}

/* @0x0029 - select expander page via P2, read back p4..p7. [SURE] */
void expander_kick(u8 p2_strobe)                    /* X0029 */
{
    P2 = p2_strobe;
    (void)movd_p4(); (void)movd_p5(); (void)movd_p6(); (void)movd_p7();
}

/*
 * @0x002f - cold-boot seed. Note Ghidra shows the wipe loop and the seeds all
 * target EXTERNAL RAM (movx). The 0xF8/0xF4 writes are peripheral regs. [SURE]
 */
void seed_cold_boot(void)                           /* X002f / FUN_CODE_002f */
{
    for (int i = 0xFF; i >= 0; --i) X(i) = 0x00;    /* movx wipe of XRAM */
    X(0x15) = 0xFE;                                  /* relay shadow default */
    X(0xF8) = 0xFF;                                  /* P6 expander shadow */
    P1 |= 0x20;
    seed_param(0x34); seed_param(0x48); seed_param(0x43);   /* X0067 */
    seed_param(0x3E); seed_param(0x39); seed_param(0x4D);
    X(0x6D) = 0x60;
    X(0xF4) = 0x20;                                  /* mux/command register */
}

/* @0x0067 - write default {0x80,0x01} at X[addr],X[addr+1] (external). [SURE] */
void seed_param(u8 addr)                            /* X0067 */
{
    X(addr) = 0x80; X(addr + 1) = 0x01;
}

/* ---- top-level control loop ----------------------------------------- */

/*
 * @0x006f - outer loop: a display/keypad scan state machine driven by the
 * bitmask in XRAM[0x1C], then the regulation pass at X0224. [LIKELY]
 */
void main_scan_loop(void)                           /* X006f */
{
    lamp_test_sequence(0x0F);                       /* X0635 */
    X(0x1A) = 0x01; X(0x1D) = 0x07; X(0x1E) = 0x03; X(0x1C) = 0x00;

    for (;;) {                                       /* X0084 */
        u8 state = X(0x1C);
        if (state == 0)   { scan_state_idle();              continue; }
        if (state & 0x01) { scan_state(0x38,0x0B,0x02);     continue; }
        if (state & 0x02) { scan_state(0x3D,0x09,0x04);     continue; }
        if (state & 0x04) { scan_state(0x42,0x07,0x08);     continue; }
        if (state & 0x08) { scan_state(0x47,0x05,0x10);     continue; }
        if (state & 0x10) { scan_state(0x4C,0x03,0x20);     continue; }
        if (state & 0x20) { scan_state(0x51,0x01,0x00);     continue; }
        regulation_pass();                           /* X0110 -> X0224 */
    }
}

void scan_state_idle(void)                          /* X0097 */
{
    set_display_group(0x0D, 0x01);                  /* X0117 */
    keypad_sample();                                /* X014b */
    bcd_copy5_x(0xF1, 0x05);                         /* X06e5 (external) */
    scan_tail();
}

void scan_state(u8 field_addr, u8 group, u8 chan_bit)   /* X00d7 */
{
    X(0x1B) = field_addr;
    set_display_group(group, chan_bit);             /* X0117 */
    for (;;) {                                       /* X00dc */
        keypad_sample();
        if (F1) { regulation_pass(); return; }       /* jf1 X00cf -> X0110 */
        update_channel();                            /* X0175 */
        debounce_step();                             /* X0121 */
        if (X(0x1E) != 0) { P2 = 0xFF; or_p5(0x01); continue; }
        X(0x1E) = 0x03;
        if (F1) break;                               /* jf1 X00f1 */
        bcd_copy5_x(X(0x1B), 0x05);
        break;
    }
    scan_tail();
}

void scan_tail(void)                                /* X0102 */
{
    F1 = false;
    if (X(0x1D) == 0) { regulation_pass(); return; }
    if (--X(0x1D) == 0) regulation_pass();
}

void set_display_group(u8 group, u8 chan_bit)       /* X0117 */
{
    P2 = 0xFF; wr_p5(group); X(0x1C) = chan_bit;
}

/*
 * @0x0121 - keypad debounce. Reads the column register at XRAM[0x02]; high
 * columns mark a press, low columns gate on XRAM[0x01] bit7; F1 set on a
 * stable valid press. [LIKELY] (Ghidra confirms the bit tests and the
 * decrement-to-zero of the 0x1E counter.)
 */
void debounce_step(void)                            /* X0121 */
{
    u8 cols = X(0x02);
    if (cols & 0xFC) goto pressed;
    if ((cols & 0x03) && (X(0x01) & 0x80)) goto pressed;
    X(0x1E) = 0x00; F1 = false; return;             /* X0134 */
pressed:                                            /* X0141 */
    F1 = (--X(0x1E) == 0);
}

/* ---- keypad / sensor sampling --------------------------------------- */

/*
 * @0x014b - one sample cycle. Reads the timing channel, and on success hands
 * a 5-byte field to bank-1 compare (X019c) and copies the result. [LIKELY]
 */
void keypad_sample(void)                            /* X014b */
{
    en_int(); dis_int();
    read_timing_channel();                          /* X01eb */
    if (F1) { en_int(); dis_int(); return; }

    en_int(); dis_int();
    bcd_copy5_x(0x0A, 0x05);
    P2 = 0xFF; and_p5(0x0E);
    read_timing_channel();
    if (F1) { en_int(); dis_int(); return; }

    bank1_compare_u(0x05, 0x0A);                    /* SEL MB1; call X099c */
    bcd_copy5_x(0xEC, 0x05);
    F1 = false; en_int(); dis_int();
}

/*
 * @0x0175 - acquire one channel: shuffles measured values through the bank-1
 * BCD library (compare/add) and seeds a BCD constant. The constant bytes are
 * written to EXTERNAL RAM 0x06..0x0A. [LIKELY]
 */
void update_channel(void)                           /* X0175 */
{
    bank1_compare_u(0xEC, 0xF1);
    bank1_entry_0000(0x05, 0xF1);                   /* call X0800 == bank1 0x000 */
    X(0x0A)=0x00; X(0x09)=0x98; X(0x08)=0x58; X(0x07)=0x02; X(0x06)=0x00;
    bank1_add(0x05, 0x0A);                          /* call X0916 */
    bcd_copy5_x(0x05, 0x14);
    X(0x00)=X(0x01); X(0x01)=0x00;
    bank1_add(0x04, 0x04);
    stage_operand_x(0x01, 0x56);                    /* X06d5 */
    X(0x0A)=0x25; bank1_add(0x0A, 0x05); bcd_copy5_x(0x05, 0x0F);
    stage_operand_x(0x00, 0x98);
    X(0x0A)=0x75; bank1_add(0x0A, 0x14);
    bank1_compare_s(0x05, 0x0F);                    /* X099f */
}

/*
 * @0x01eb - precision timing measurement (dual-slope / time-to-count, almost
 * certainly a thermistor read). Runs under register bank 1, arms the counter,
 * waits on T-flag and P1 bit4, converts via bank-1 X0a12. [GUESS purpose;
 * SURE mechanics - Ghidra shows the STRT CNT / JTF / counter-read sequence.]
 */
void read_timing_channel(void)                      /* X01eb */
{
    if (X(0x1D) == 0) { F1 = true; return; }
    P1 |= 0x20;
    /* SEL RB1 */
    timer_load(0);
    u8 overflows = 0;
    busy_delay();
    timer_start_count();
    P1 &= 0xDF;
    for (;;) {                                       /* X0203 */
        if (timer_flag()) { overflows++; continue; }
        if (P1 & 0x10) break;
        P2 = 0xAF;
        if (movd_p7() & 0x04) { P2 &= 0x7F; for (;;) {} }  /* fault hang */
    }
    P1 |= 0x20;
    timer_stop();
    u8 count = timer_read();
    bank1_timer_to_bcd(count, overflows);           /* SEL MB1; call X0a12 */
    /* SEL RB0 / SEL MB0 */
}

/* ---- regulation (HVAC output decision) ------------------------------ */

/*
 * @0x0224 - regulation pass. Compares measured vs setpoint via the bank-1 BCD
 * library and drives the relay bits in the XRAM[0x15] shadow out to P7
 * (heat call / valve / pump), then arms the external interrupt and idles. [LIKELY]
 */
void regulation_pass(void)                          /* X0224 */
{
    if (X(0x16) != 0) {
        P2 = 0xEF;
        bank1_compare_u(0x3D, 0x47);
        stage_operand_x(0x00, 0x08);
        bank1_compare_u(0x05, 0x0A);
        if (!F1) { control_idle_no_action(); return; }
        if (--X(0x16) == 0) { relay_clear_and_idle(); return; }
        stage_operand_x(0x00, 0x10);
        bank1_compare_u(0x4C, 0x0A);
        if (F1) { relay_clear_and_idle(); return; }
        u8 relays = X(0x15) | 0x08; wr_p7(relays);
        bank1_busy_delay();                          /* call X0cff */
        relays &= 0xFB; X(0x15) = relays; wr_p7(relays);
        P2 = 0xEF; or_p5(0x01);
    } else {
        P2 = 0xEF; and_p5(0x0E);
        keypad_menu_dispatch();                      /* X0291 */
    }
    P2 = 0xDF; and_p6(0x00);
    en_int();
    for (;;) {}                                      /* X0287 idle */
}

void control_idle_no_action(void)                   /* X027a */
{ P2 = 0xEF; and_p5(0x0E); X(0x16) = 0x00; }

void relay_clear_and_idle(void)                     /* X0273 */
{ u8 r = X(0x15) | 0xFC; X(0x15) = r; wr_p7(r); }

/*
 * @0x0291 - decode the front-panel keypad (read via P4/P5 with P2 strobes)
 * into a parameter selection, map it to a setpoint slot (stride tables
 * {0x20,0x25,0x30,0x35,0x40,0x45} and {0x40,0x50,0x60,0x70}), then apply. [LIKELY]
 */
void keypad_menu_dispatch(void)                     /* X0291 */
{
    P2 = 0xAF;
    u8 sel = movd_p5() | ((movd_p4() & 0x01) << 4);
    (void)sel;
    /* key->slot ladders X02a9-X02e7 (mechanical) */
    apply_selection();                              /* X0402 */
}

/*
 * @0x0402 - apply selected setpoint: compares working values via bank-1 X099c
 * and sets/clears relay bits (XRAM[0x15]) and status bits (XRAM[0xF7]),
 * then tail-jumps into the bank-1 state machine at 0x240. [LIKELY]
 */
void apply_selection(void)                          /* X0402 */
{
    if (X(0x34) & 0x80) goto out;
    stage_operand_x(0x35, 0x00);
    bank1_compare_u(0x38, 0x0A);
    if (F1) goto out;
    /* heat/valve/pump decision ladder X041b-X04f4: toggles XRAM[0x15] relay
     * bits and XRAM[0xF7] status bits. [LIKELY] */
out:                                                /* X04f6 */
    bcd_copy5_x(0x38, 0xF6);
    bank1_state_0240();                             /* SEL MB1; jmp X0a40 */
}

/*
 * @0x04ff - external interrupt service routine. Ghidra (EXTIRQ) confirms:
 * SEL RB1, save P2 -> XRAM[0x21] and ACC -> XRAM[0x1F], then dispatch on the
 * P6/P7 panel button bits, RETR. [SURE structure / LIKELY button names]
 */
void ext_interrupt(void)                            /* X04ff / EXTIRQ */
{
    /* SEL RB1 */
    X(0x1F) = /*ACC*/ 0;
    X(0x21) = P2;
    for (;;) {                                       /* X0509 */
        P2 = 0xAF;
        u8 p6 = movd_p6(), p7 = movd_p7();
        u8 btn = (p6 << 4) | p7;
        if (p7 & 0x04) { for (;;) { P2 &= 0x7F; } }  /* fault hang */
        if (p7 & 0x01) { btn_inc(0x01, 0x19); X(0x17)=0; continue; }
        if (p7 & 0x02) { btn_inc(0x02, 0x18);          continue; }
        if (p7 & 0x08) { btn_program();                continue; }
        if (p6 & 0x02) { btn_clock_adjust();           continue; } /* X054f */
        if (p6 & 0x04) { btn_mode();                   continue; } /* X053e */
        if (p6 & 0x08) { btn_start();                  continue; } /* X056d */
        break;                                        /* X062c */
    }
    P2 = X(0x21);
    /* ACC = X(0x1F); SEL RB0; RETR */
}

/* ---- low-level panel / display I/O ---------------------------------- */

void lamp_test_sequence(u8 pattern)                 /* X0635 */
{
    display_blank(); wr_p4(0x00); read_sensor_mux();
    display_refresh(); F1 = false; output_flags();
    (void)pattern;
}

/*
 * @0x0645 - read 4-nibble sensor mux by strobing P1 bits 6/7. Ghidra
 * (FUN_CODE_0645) confirms the exact P1 masking and the final `& 3 | 0xF0`. [SURE]
 */
void read_sensor_mux(void)                          /* X0645 / FUN_CODE_0645 */
{
    P1 &= 0x3F; u8 n0 = P1 & 0x0F;
    P1 |= 0x40; u8 n1 = (P1 & 0x07) << 4;  u8 r7 = n0 | n1;
    P1 &= 0x3F; P1 |= 0x80; u8 r6 = (P1 << 4) | 0x0F;
    P1 |= 0xC0; u8 r5 = (P1 & 0x03) | 0xF0;
    (void)r7; (void)r6; (void)r5;
}

/* @0x0665 - push display buffer (R5:R6:R7) to P4..P7 in two strobed passes.
 * Ghidra confirms the nibble split (& 0xf / >> 4). [SURE] */
void display_refresh(void)                          /* X0665 / FUN_CODE_0665 */
{
    P2 = 0xCF; /* p5=r7&0xf; p4=r7>>4; p7=r6&0xf; p6=r6>>4 */
    P2 = 0xDF; /* p5=r5&0xf; p4=r5>>4 */
}

void display_blank(void) { P2 = 0xDF; }             /* X067e */
void output_flags(void)  { P2 = 0xEF; }             /* X06c7 (sets p5 bit by F1) */

/* @0x06e5 - copy 5 EXTERNAL bytes downward: X[dst-i] = X[src-i]. [SURE] */
void bcd_copy5_x(u8 src, u8 dst)                    /* X06e5 */
{ for (int i = 0; i < 5; ++i) X(dst - i) = X(src - i); }

/* @0x06d5 - stage a 16-bit value as a zero-padded operand at EXTERNAL
 * X[0x06..0x0A]. [SURE] */
void stage_operand_x(u8 hi, u8 lo)                  /* X06d5 / FUN_CODE_06d5 */
{ X(0x0A)=0x00; X(0x09)=lo; X(0x08)=hi; X(0x07)=0x00; X(0x06)=0x00; }

/* =========================================================================
 * BANK 1  (A98EH.bin)  -  multi-byte packed-BCD math + state machine
 *
 * IMPORTANT: the operand/scratch region 0x24..0x3A here is INTERNAL RAM
 * (mov @r), NOT the external 0x24.. used by bank 0. Modelled as I(...).
 * The 5-byte working buffers at 0x05/0x0A/0x0F are EXTERNAL (movx) = X(...).
 * ========================================================================= */

/* @bank1 0x000 (X0800) - BCD compare/normalise entry. [LIKELY] */
void bank1_entry_0000(u8 a_ptr, u8 b_ptr);

/* @0x00a2 - copy n INTERNAL bytes downward I[dst-i]=I[src-i]. [SURE] */
void iram_copy_down(u8 src, u8 dst, u8 n)           /* X00a2 */
{ for (int i = 0; i < n; ++i) I(dst - i) = I(src - i); }

/* @0x00a9 - rotate a packed-BCD field right one digit (÷10). [SURE] */
void bcd_shift_right_digit(u8 ptr, u8 n);           /* X00a9 */

/* @0x00c2 - zero n INTERNAL bytes downward from r1. [SURE] */
void iram_clear_down(u8 ptr, u8 n)                  /* X00c2 */
{ for (int i = 0; i < n; ++i) I(ptr - i) = 0; }

/*
 * @0x00c8 - 6-digit packed-BCD subtract / comparator. Forms the 10's
 * complement of the INTERNAL operand at I[0x24..0x20] into I[0x2E..0x2A],
 * then BCD-adds to I[0x29..0x25] with DA, threading carry; returns carry
 * (= "A >= B"). This is the core comparator. Ghidra (FUN_CODE_08c8) confirms
 * the full DA-add expansion on DAT_INTMEM_24/29/2e. [SURE]
 */
bool bcd_subtract(void)                             /* X00c8 / FUN_CODE_08c8 */
{
    for (int i = 0; i < 6; ++i)
        I(0x2E - i) = (u8)(~I(0x24 - i) + 0x9A);    /* tens-complement */
    bool carry = true;                               /* start +1 for 10's comp */
    for (int i = 0; i < 5; ++i) {
        /* I[0x2E-i] = bcd_add(I[0x29-i], I[0x2E-i], carry); DA; update carry */
    }
    return carry;                                    /* C set -> A >= B */
}

/* @0x00f8 - copy 4 EXTERNAL bytes downward (movx). @0x0101 - store 4 acc
 * bytes into EXTERNAL X[5..2]. @0x010c - clear INTERNAL 0x20..0x3A. [SURE] */
void xram_copy4(u8 src, u8 dst);                    /* X00f8 */
void acc_store4(u8 src);                            /* X0101 */
void iram_scratch_clear(void)                       /* X010c */
{ for (int i = 0; i < 0x1B; ++i) I(0x3A - i) = 0; }

/*
 * @0x0116 / @0x019c / @0x019f - the public BCD ops used throughout bank 0:
 *   bank1_add        (X0116): dst += src
 *   bank1_compare_u  (X019c): unsigned compare (clr F0)
 *   bank1_compare_s  (X019f): signed compare   (set F0)
 * Each loads operands, runs bcd_subtract(), returns the relation in F1. [SURE
 * these are the BCD entry points; names [LIKELY]]
 */
void bank1_add(u8 a_ptr, u8 b_ptr);                 /* X0116 / FUN_CODE_0916 */
void bank1_compare_u(u8 a_ptr, u8 b_ptr);           /* X019c / FUN_CODE_099c */
void bank1_compare_s(u8 a_ptr, u8 b_ptr);           /* X019f / FUN_CODE_099f */

/* @0x0168 - 8-digit BCD add I[0x27..]+I[0x33..] -> I[0x33..] with DA. [SURE] */
void bcd_add8(void);                                /* X0168 */

/* @0x01f9 - stage (hi,lo) as padded EXTERNAL operand at X[0x06..0x0A]
 * (bank-1 twin of bank-0 stage_operand_x). [SURE] */
void stage_operand_b1(u8 hi, u8 lo)                 /* X01f9 */
{ X(0x0A)=0x00; X(0x09)=lo; X(0x08)=hi; X(0x07)=0x00; X(0x06)=0x00; }

/* @0x0209 - copy 5 EXTERNAL bytes downward (bank-1 twin of X06e5). [SURE] */
void bcd_copy5_b1(u8 src, u8 dst)                   /* X0209 */
{ for (int i = 0; i < 5; ++i) X(dst - i) = X(src - i); }

/* ---- bank-1 control state machine ----------------------------------- */

/* @0x0212 (X0a12) - convert raw timer (count, overflows) to BCD reading. [LIKELY] */
void bank1_timer_to_bcd(u8 count, u8 overflows);

/*
 * @0x0240 (X0a40) - state-machine head. Reads run flags XRAM[0x18]/[0x19];
 * if set, runs the heat-curve computation (compare/ramp/step) per channel,
 * else jumps to state_X03d5. [LIKELY]
 */
void bank1_state_0240(void)                         /* X0240 */
{
    if (X(0x18) == 0 && X(0x19) == 0) { state_X03d5(); return; }
    if (X(0x18)) { /* channel 1: compare 0x42/0x4C vs 0x47, ramp, step */ }
    if (X(0x19)) { /* channel 2 */ }
    state_X03d5();
}

/*
 * @0x0358 - dwell/settling delay that pulses the modulating valve on P6 (mask
 * 0x0B/0x0D chosen by INTERNAL I[0x24] bit6) with two busy-waits. Ghidra shows
 * the P6 writes and the nested djnz delays. [LIKELY]
 */
void dwell_delay(void)                              /* X0358 */
{
    P2 = 0xEF;
    u8 mask = (I(0x24) & 0x40) ? 0x0D : 0x0B;
    wr_p6(X(0xF8) & mask);
    busy_delay();
    wr_p6(X(0xF8) | (u8)~mask);
    busy_delay();
}

/*
 * @0x03d5 - secondary regulation: compares the computed demand at 0x3D vs
 * thresholds and sets/clears relay outputs (status bits XRAM[0xF7], P6/P7),
 * with anti-cycle counters XRAM[0x27]/[0xF9]. Tail to state_X0508. [LIKELY]
 */
void state_X03d5(void)                              /* X03d5 */
{
    stage_operand_b1(0x00, 0x50);
    bank1_compare_s(0x3D, 0x0A);
    if (F1) { state_X0508(); return; }
    X(0x07) = 0x02;
    bank1_compare_u(0x3D, 0x0A);
    if (F1) { state_X0508(); return; }
    /* threshold ladder X03f8-X04f4: toggle XRAM[0xF7] status + P6/P7 relays */
    state_X0508();
}

/*
 * @0x0508 - program/clock scheduler. Reads schedule flags XRAM[0x4D], walks
 * the time program (panel reads X06fa/X06f3) and sets the demand. [GUESS -
 * weekly/daily timer]. Ends at the final output stage.
 */
void state_X0508(void)                              /* X0508 */
{
    if (X(0x4D) & 0x80) { final_state(); return; }
    /* scheduling sub-states X0542/X05a2/X05e6/X061e stage operands + set demand */
    final_state();
}

/*
 * @0x072d - final output/idle: reads the sensor mux once more (SEL MB0; X0645),
 * updates the display latch and relay shadow XRAM[0x15], enables interrupts,
 * idles at X07d8 until the next external-interrupt tick. [LIKELY]
 */
void final_state(void)                              /* X072d */
{
    P2 = 0xDF; and_p6(0x03);
    /* stage + compare + display/relay update ladder X0741-X07c4 */
    en_int();
    for (;;) {}                                      /* X07d8 */
}

/* @0x06f3 - read 4 panel time/mode switches on P4 low nibble. [SURE] */
u8 read_panel_switches(void)                        /* X06f3 */
{ P2 = 0xAF; return movd_p4() & 0x0E; }

/* ---- mechanical helpers (see *.d48 / Ghidra for full bodies) -------- */
void btn_inc(u8 mask, u8 ctr);     /* X0526/X0534 */
void btn_program(void);            /* X057e */
void btn_clock_adjust(void);       /* X054f */
void btn_mode(void);               /* X053e */
void btn_start(void);              /* X056d */
void bank1_busy_delay(void);       /* X04ff (~0x80*256 cycles) */
void bank1_timer_to_bcd(u8, u8);   /* X0212 */
void bank1_state_0240(void);
