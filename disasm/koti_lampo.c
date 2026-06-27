/*
 * koti_lampo.c
 *
 * Hand-written C RECONSTRUCTION of the Valmet Kotilampo HVAC controller
 * firmware, reverse-engineered from the two 2 KB UV-EPROM dumps:
 *
 *     bin/3EF2H.bin  -> bank 0 (program 0x000-0x7FF)   "boot + I/O + control"
 *     bin/A98EH.bin  -> bank 1 (program 0x800-0xFFF)   "BCD math + state machine"
 *
 * CPU: Intel P8035L (MCS-48 family, ROM-less 8048). 8-bit Harvard core,
 *      12-bit program counter = 4 KB external program space split into two
 *      2 KB banks selected by `SEL MB0` / `SEL MB1`.
 *
 * ---------------------------------------------------------------------------
 * IMPORTANT - READ THIS FIRST
 * ---------------------------------------------------------------------------
 * This file is NOT compilable firmware and NOT a guaranteed-correct decompile.
 * It is a READING AID: the assembly was hand-translated to C so the logic is
 * easier to follow. Every function carries its ROM address range so you can
 * cross-check against disasm/3EF2H.d48 and disasm/A98EH.d48.
 *
 * Confidence levels used in comments:
 *   [SURE]   - mechanical translation, matches the opcodes 1:1.
 *   [LIKELY] - logic is clear, but the *name/purpose* is inferred.
 *   [GUESS]  - speculative interpretation of hardware/peripheral meaning.
 *
 * The 8048's quirks are modelled with these conventions:
 *   - A, R0..R7        -> locals; two register banks (RB0/RB1) noted inline.
 *   - F0, F1           -> bool flags carried across calls (F1 often = "error/
 *                         compare result", returned implicitly).
 *   - movx @r,a        -> write to EXTERNAL data space  -> XRAM[addr] = a
 *   - movx a,@r        -> read  from EXTERNAL data space -> a = XRAM[addr]
 *   - movd / anld / orld p4..p7 -> 4-bit nibble I/O to an 8243-style port
 *                         expander hung off P4-P7; P2 high nibble = chip/page
 *                         select strobe for that expander (the constants
 *                         0xEF/0xDF/0xCF/0xAF/0x9F/0x8F written to P2).
 * ---------------------------------------------------------------------------
 */

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t u8;

/* =========================================================================
 * Hardware model
 * ========================================================================= */

/* On-chip 8048 I/O ports */
extern u8 P1;                 /* control strobes (keypad/sensor mux, expander) */
extern u8 P2;                 /* low nibble = data to expander; high nibble =
                                 expander page/chip select strobe [LIKELY] */
extern u8 BUS;                /* external bus port (unused in normal flow)     */

/* 8243-style 4-bit port expander reached through P4..P7 (movd/anld/orld) */
u8 movd_p4(void);  void wr_p4(u8);  void or_p5(u8);  void and_p5(u8);
u8 movd_p5(void);  void wr_p5(u8);  void or_p6(u8);  void and_p6(u8);
u8 movd_p6(void);  void wr_p6(u8);  void or_p7(u8);  void and_p7(u8);
u8 movd_p7(void);  void wr_p7(u8);

/* MCS-48 timer/counter */
void timer_load(u8);  u8 timer_read(void);
void timer_start_count(void);   /* STRT CNT */
void timer_stop(void);          /* STOP TCNT */
bool timer_flag(void);          /* JTF */
void en_int(void);  void dis_int(void);  /* EN I / DIS I */

/*
 * External data space (movx). On this board it is a small RAM plus a few
 * memory-mapped peripheral registers in the 0xF4-0xF9 window.
 *
 * XRAM map (addresses are the @R0/@R1 pointers seen in the code) [LIKELY/GUESS]:
 *   0x00-0x0A  scratch BCD work area; 5-byte little-endian-by-address BCD
 *              numbers live at [0x05..0x01] and [0x0F..0x0B] and [0x0A..0x06].
 *   0x05       primary 5-digit BCD accumulator ("ACC")           [LIKELY]
 *   0x0F       secondary 5-digit BCD buffer ("TMP")              [LIKELY]
 *   0x14       BCD buffer                                        [GUESS]
 *   0x15       output-latch shadow for relays on P7 (bits set/cleared
 *              then pushed with movd p7)                          [LIKELY]
 *   0x16       run/aux countdown timer                           [GUESS]
 *   0x17-0x1A  event counters / one-shot flags                   [GUESS]
 *   0x1B       current menu / channel selection                  [GUESS]
 *   0x1C       display-scan state bitmask (drives the big switch) [LIKELY]
 *   0x1D       scan repeat counter                               [GUESS]
 *   0x1E       debounce / valid-press counter                    [GUESS]
 *   0x1F,0x21  ISR save area (ACC, P2) - used under RB1           [SURE]
 *   0x24-0x28  BCD operand B (5 bytes)                            [LIKELY]
 *   0x29-0x2D  BCD operand A (5 bytes)                            [LIKELY]
 *   0x2E-0x33  BCD operand / result                              [LIKELY]
 *   0x34-0x4D  parameter/setpoint table: 6 entries x 5 BCD bytes,
 *              seeded by sensor_table_init()                      [LIKELY]
 *   0x56,0x5B,0x60,0x65,0x6F  stored measured values per channel [GUESS]
 *   0xF4       8155-style command / mux register                 [GUESS]
 *   0xF7       packed status/flag bits (bit0..bit4 used heavily)  [LIKELY]
 *   0xF8       P6 expander output shadow                          [LIKELY]
 *   0xF9       retry / loop counter                              [GUESS]
 */
extern u8 XRAM[256];
#define X(addr)  XRAM[addr]

/* The 6-bit "F1" software flag is threaded through many helpers as a bool. */

/* =========================================================================
 * BANK 0  (3EF2H.bin)  -  boot, I/O, keypad/display scan, control logic
 * ========================================================================= */

/* ---- reset / cold start --------------------------------------------- */

/* @0x000 reset vector. 0x000=NOP, then jmp here.                        */
void reset(void)                                   /* X0005 @ 0x0005 */
{
    if (/* T0 pin low */ 0)                         /* jnt0 X0009 */
        clear_xram_and_seed_params();              /* call X002f */
    io_init();                                      /* call X000d */
    main_scan_loop();                               /* jmp X006f  (never returns) */
}

/* @0x000d - bring all expander ports to a known state. [SURE structure] */
void io_init(void)                                  /* X000d */
{
    P2 = 0xFF;
    or_p7(0xFF); or_p6(0xFF); or_p5(0xFF);          /* set all expander outputs */
    and_p7(0x00); and_p6(0x00);                     /* then clear p6/p7 */
    expander_kick(0x8F);                            /* call X0029 x3 with */
    expander_kick(0x9F);                            /* different P2 page strobes */
    expander_kick(0xAF);
    P2 = 0xFF;
    (void)movd_p4();
}

/* @0x0029 - select an expander page via P2, then read p4..p7 (latch).   */
void expander_kick(u8 p2_strobe)                    /* X0029 */
{
    P2 = p2_strobe;
    (void)movd_p4(); (void)movd_p5(); (void)movd_p6(); (void)movd_p7();
}

/* @0x002f - wipe all 256 bytes of XRAM, then seed peripheral regs and
 * the 6-entry parameter table. Called only on a true cold boot (T0 high). */
void clear_xram_and_seed_params(void)               /* X002f */
{
    for (int i = 0xFF; i >= 0; --i)                 /* movx loop r1=0xFF..0 */
        X(i) = 0x00;

    X(0x15) = 0xFE;                                 /* relay shadow default */
    X(0xF8) = 0xFF;                                 /* P6 expander shadow */
    P1 |= 0x20;

    seed_param(0x34); seed_param(0x48); seed_param(0x43);   /* X0067 each */
    seed_param(0x3E); seed_param(0x39); seed_param(0x4D);
    X(0x6D) = 0x60;
    X(0xF4) = 0x20;                                 /* mux/command register */
}

/* @0x0067 - write the 2-byte default {0x80, 0x01} at [r0], [r0+1].      */
void seed_param(u8 addr)                            /* X0067 */
{
    X(addr)     = 0x80;
    X(addr + 1) = 0x01;
}

/* ---- top-level control loop ----------------------------------------- */

/*
 * @0x006f - the controller's outer loop. It runs a small display/keypad
 * scanning state machine driven by the bitmask in XRAM[0x1C], then drops
 * into the regulation pass at X0224 (relay/valve control). [LIKELY]
 */
void main_scan_loop(void)                           /* X006f */
{
    lamp_test_sequence(0x0F);                       /* call X0635 */
    X(0x1A) = 0x01;
    X(0x1D) = 0x07;                                 /* scan repeat = 7 */
    X(0x1E) = 0x03;
    X(0x1C) = 0x00;

    for (;;) {                                       /* X0084 dispatch loop */
        u8 state = X(0x1C);
        if (state == 0)            { scan_state_idle();    continue; } /* X0097 */
        if (state & 0x01)          { scan_state(0x38,0x0B,0x02); continue; } /* X00a7 */
        if (state & 0x02)          { scan_state(0x3D,0x09,0x04); continue; } /* X00af */
        if (state & 0x04)          { scan_state(0x42,0x07,0x08); continue; } /* X00b7 */
        if (state & 0x08)          { scan_state(0x47,0x05,0x10); continue; } /* X00bf */
        if (state & 0x10)          { scan_state(0x4C,0x03,0x20); continue; } /* X00c7 */
        if (state & 0x20)          { scan_state(0x51,0x01,0x00); continue; } /* X00d1 */
        regulation_pass();                          /* X0110 -> X0224 */
    }
}

/*
 * @0x0097 - "idle" branch: refresh display segment group 0x0D, run the
 * keypad sample (X014b) and copy a 5-byte field to the ACC at 0x05. [LIKELY]
 */
void scan_state_idle(void)                          /* X0097 */
{
    set_display_group(/*r2=*/0x0D, /*r3=*/0x01);    /* call X0117 */
    keypad_sample();                                /* call X014b */
    bcd_copy5(/*from r1=*/0xF1, /*to r0=*/0x05);    /* call X06e5 */
    /* falls through to X0102 housekeeping */
    scan_tail();
}

/*
 * @0x00a7..0x00d7 - the 6 keypad/display channels share this body. Each
 * sets the segment field address (a), a display group selector (r2) and a
 * channel bit (r3), then scans. [SURE structure / LIKELY purpose]
 */
void scan_state(u8 field_addr, u8 group, u8 chan_bit)   /* X00d7 */
{
    X(0x1B) = field_addr;
    set_display_group(group, chan_bit);             /* X0117 */
    for (;;) {                                       /* X00dc */
        keypad_sample();                            /* X014b */
        if (/*F1 set*/ keypad_error) { regulation_pass(); return; } /* jf1 X00cf->X0110 */
        update_channel();                           /* X0175 */
        debounce_step();                            /* X0121 */
        if (X(0x1E) != 0) {                         /* still debouncing */
            P2 = 0xFF; or_p5(0x01);                 /* pulse strobe */
            continue;
        }
        X(0x1E) = 0x03;                             /* reload debounce */
        if (/*F1*/ keypad_error) break;             /* jf1 X00f1 */
        bcd_copy5(X(0x1B), 0x05);                   /* commit edited value */
        break;
    }
    scan_tail();
}

/* @0x0102 - shared loop tail: decrement scan-repeat counter at 0x1D.    */
void scan_tail(void)                                /* X0102 */
{
    keypad_error = false;                           /* clr f1 */
    if (X(0x1D) == 0) { regulation_pass(); return; }
    if (--X(0x1D) == 0) regulation_pass();          /* else loop continues */
}

/* @0x0117 - push display group selector to P5 and record channel bit.   */
void set_display_group(u8 group, u8 chan_bit)       /* X0117 */
{
    P2 = 0xFF;
    wr_p5(group);                                   /* movd p5,a */
    X(0x1C) = chan_bit;
}

/*
 * @0x0121 - read the keypad column register (XRAM[0x02]); any bit in the
 * high columns clears the debounce counter, low columns step it. Sets F1
 * when a stable, valid press is detected. [LIKELY]
 */
void debounce_step(void)                            /* X0121 */
{
    u8 cols = X(0x02);
    if (cols & 0xFC) goto pressed;                  /* jb7..jb2 X0141 */
    if (cols & 0x03) {                              /* jb1/jb0 X013b */
        if (X(0x01) & 0x80) goto pressed;
    }
    X(0x1E) = 0x00;                                 /* X0134: no press */
    keypad_error = false;
    return;
pressed:                                            /* X0141 */
    keypad_error = (--X(0x1E) == 0);
}

/* ---- keypad / sensor sampling --------------------------------------- */

/*
 * @0x014b - one keypad/sensor sample cycle. Enables ints briefly, reads
 * the analog/timer channel (X01eb), and on a valid result copies a small
 * field and calls a bank-1 routine. [LIKELY]
 */
void keypad_sample(void)                            /* X014b */
{
    en_int(); dis_int();
    read_timing_channel();                          /* X01eb */
    if (keypad_error) { en_int(); dis_int(); return; }

    en_int(); dis_int();
    bcd_copy5(0x0A, 0x05);                          /* X06e5 */
    P2 = 0xFF; and_p5(0x0E);
    read_timing_channel();                          /* X01eb again */
    if (keypad_error) { en_int(); dis_int(); return; }

    /* hand off to bank 1 BCD routine A98EH:X019c (compare/normalise) */
    bank1_X019c(/*r7=*/0x05, /*r6=*/0x0A);
    bcd_copy5(0xEC, 0x05);
    keypad_error = false;
    en_int(); dis_int();
}

/*
 * @0x0175 - per-channel update: shuffles measured values between buffers
 * via the bank-1 BCD library (copy/normalise/add) and reads the sensor mux
 * with X0645. This is the "acquire one channel's value" step. [LIKELY]
 */
void update_channel(void)                           /* X0175 */
{
    bank1_X019c(0xEC, 0xF1);
    bank1_X0000(0x05, 0xF1);                        /* call X0800 == bank1 0x000 */
    /* seed a 5-byte BCD constant {0x00,0x02,0x58,0x98,0x00} at [0x0A..0x06] */
    X(0x0A) = 0x00; X(0x09) = 0x98; X(0x08) = 0x58;
    X(0x07) = 0x02; X(0x06) = 0x00;
    bank1_X0116(0x05, 0x0A);                        /* call X0916 */
    bcd_copy5(0x05, 0x14);

    X(0x00) = X(0x01); X(0x01) = 0x00;
    bank1_X0116(0x04, 0x04);
    bank1_X06d5(/*r4=*/0x01, /*r5=*/0x56);          /* stage operand */
    X(0x0A) = 0x25;
    bank1_X0116(0x0A, 0x05);
    bcd_copy5(0x05, 0x0F);
    bank1_X06d5(0x00, 0x98);
    X(0x0A) = 0x75;
    bank1_X0116(0x0A, 0x14);
    bank1_X019f(0x05, 0x0F);
}

/*
 * @0x01eb - the precision-timing measurement. Selects register bank 1,
 * arms the counter, waits on T-flag / P1 bit-4, and converts the elapsed
 * count to a BCD reading via bank-1 routine X0a12. This is almost certainly
 * a dual-slope / time-to-count temperature (thermistor) read. [GUESS]
 */
void read_timing_channel(void)                      /* X01eb */
{
    if (X(0x1D) == 0) { keypad_error = true; return; }   /* nothing to do */

    P1 |= 0x20;
    /* SEL RB1: separate register set for the tight timing loop */
    timer_load(0);
    u8 r4 = 0;                                       /* elapsed-overflow count */
    busy_delay();                                    /* djnz r2/r3 settle */
    timer_start_count();
    P1 &= 0xDF;                                      /* drop strobe -> start ramp */

    for (;;) {                                        /* X0203 */
        if (timer_flag()) { r4++; continue; }        /* JTF -> overflow tick */
        if (P1 & 0x10) break;                         /* comparator tripped */
        P2 = 0xAF;
        if (movd_p7() & 0x04) {                       /* abort condition */
            P2 &= 0x7F;                               /* hang (fault) */
            for (;;) {}
        }
    }
    P1 |= 0x20;
    timer_stop();
    u8 count = timer_read();
    /* SEL MB1 ; convert (r3=count, r4=overflows) to BCD reading */
    bank1_X0212(count, r4);                           /* call X0a12 */
    /* SEL RB0 / SEL MB0 on the way out */
}

/* ---- regulation (the actual HVAC control output) -------------------- */

/*
 * @0x0224 - the regulation pass. Reads the comparison result of the
 * measured value vs. setpoint (through bank-1 X019c on the 0x3D/0x47 and
 * 0x05/0x0A operand pairs) and drives the relay bits in the XRAM[0x15]
 * shadow out to P7 (heating call / valve / pump). After the decision it
 * enables interrupts and idles, waking on the external interrupt. [LIKELY]
 */
void regulation_pass(void)                          /* X0224 */
{
    if (X(0x16) != 0) {                              /* a timed action pending */
        P2 = 0xEF;
        bank1_X019c(0x3D, 0x47);                     /* compare setpoints */
        bank1_X06d5(0x00, 0x08);
        bank1_X019c(0x05, 0x0A);                     /* compare meas vs ref */
        if (!keypad_error) { control_idle_no_action(); return; }  /* X027a */

        if (--X(0x16) == 0) {                         /* action window expired */
            relay_clear_and_idle();                   /* X0273 */
            return;
        }
        bank1_X06d5(0x00, 0x10);
        bank1_X019c(0x4C, 0x0A);
        if (keypad_error) { relay_clear_and_idle(); return; }

        u8 relays = X(0x15) | 0x08;                   /* assert one relay */
        wr_p7(relays);
        bank1_X04ff();                                /* call X0cff (settle) */
        relays &= 0xFB;
        X(0x15) = relays; wr_p7(relays);
        P2 = 0xEF; or_p5(0x01);
    } else {                                          /* X0289: no pending action */
        P2 = 0xEF; and_p5(0x0E);
        keypad_menu_dispatch();                        /* X0291 */
    }

    /* X0281 / X0287: arm external interrupt and idle until woken */
    P2 = 0xDF; and_p6(0x00);
    en_int();
    for (;;) {}                                       /* jmp self  (HLT-ish) */
}

void control_idle_no_action(void)                   /* X027a */
{
    P2 = 0xEF; and_p5(0x0E);
    X(0x16) = 0x00;
    /* falls into X0281 idle in the original */
}

void relay_clear_and_idle(void)                     /* X0273 */
{
    u8 relays = X(0x15) | 0xFC;
    X(0x15) = relays; wr_p7(relays);
    /* X027a */
}

/*
 * @0x0291 - decode the front-panel keypad (read through P4/P5/P6/P7 with
 * the various P2 page strobes) into a menu/parameter selection, look up the
 * matching setpoint slot (0x20..0x70 stride table) and store it. [LIKELY]
 * The long jb0..jb4 ladders are just "which key was pressed". The body
 * eventually jumps to X0402 (apply selection) or X0356/X0402.
 */
void keypad_menu_dispatch(void)                     /* X0291 */
{
    /* read two key groups, OR into a selector */
    P2 = 0xAF;
    u8 sel = movd_p5();
    sel |= (movd_p4() & 0x01) << 4;
    static const u8 slot1[] = {0x20,0x25,0x30,0x35,0x40,0x45};  /* X02a9.. */
    /* pick slot1[index] by first set bit, stage it, scale via X06d5/X06e5 */
    /* ... (mechanical key->slot mapping, see X02a9-X02bf) ...            */

    P2 = 0xFF;
    u8 key2 = movd_p4();
    static const u8 slot2[] = {0x40,0x50,0x60,0x70};            /* X02d7.. */
    /* pick slot2[index] similarly (X02d7-X02e7)                          */
    (void)slot1; (void)slot2; (void)key2; (void)sel;

    apply_selection();                              /* X0402 (via X02e7 path) */
}

/*
 * @0x0402 - apply the selected setpoint: compares the working value at
 * 0x34/0x35/0x38 groups using bank-1 X099c, then sets or clears relay bits
 * in XRAM[0x15] and the status flags in XRAM[0xF7] accordingly, finally
 * tail-calls into bank 1 at 0x240 (X0a40). This is the heart of the
 * heat/cool decision. [LIKELY]
 */
void apply_selection(void)                          /* X0402 */
{
    if (X(0x34) & 0x80) { goto out; }               /* sign / disabled */
    bank1_X06d5(/*r4=*/0x35, /*r5=*/0x00);
    bank1_X099c(0x38, 0x0A);
    if (keypad_error) goto out;

    /* large branch tree comparing 0x38 against 0xF6/0x2E/0x33 references
     * and toggling relay bits (P7 shadow 0x15) plus status bits in 0xF7.
     * Net effect: decide heat call (bit0), valve (bit1), pump (bit2) and
     * latch them. [LIKELY] (full ladder: X041b-X04f4)                     */

out:                                                /* X04f6 */
    bcd_copy5(0x38, 0xF6);
    /* SEL MB1 ; jmp X0a40  -> continue in bank 1 state machine */
    bank1_X0240();
}

/* ---- external interrupt (mains zero-cross / panel) ------------------- */

/*
 * @0x04ff - external interrupt service routine. Saves ACC and P2 under
 * register bank 1, reads the panel button matrix (P6/P7), and dispatches
 * to the per-button handlers (set clock, change channel, start, etc.).
 * Returns with RETR. [LIKELY]
 */
void ext_interrupt(void)                            /* X04ff */
{
    /* SEL RB1 */
    X(0x1F) = /*A*/ 0;                               /* save ACC */
    X(0x21) = P2;                                    /* save P2 */
    for (;;) {                                        /* X0509 */
        P2 = 0xAF;
        u8 btn = (movd_p6() << 4) | movd_p7();
        if (btn & 0x04) { P2 &= 0x7F; for(;;){} }    /* X0522 fault hang */
        if (btn & 0x01) { btn_inc(0x01, 0x19); X(0x17)=0; continue; } /* X0526 */
        if (btn & 0x02) { btn_inc(0x02, 0x18);        continue; }     /* X0534 */
        if (btn & 0x08) { btn_program();              continue; }     /* X057e */
        if (btn & 0x20) { btn_clock_adjust();         continue; }     /* X054f */
        if (btn & 0x40) { btn_mode();                 continue; }     /* X053e */
        if (btn & 0x80) { btn_start();                continue; }     /* X056d */
        break;                                        /* X062c: none -> restore */
    }
    P2 = X(0x21);                                     /* restore P2 */
    /*A = X(0x1F);*/                                  /* restore ACC */
    /* SEL RB0 ; RETR */
}

/* ---- low-level panel / display I/O ---------------------------------- */

/* @0x0635 - light every segment for a moment (lamp test) then clear.    */
void lamp_test_sequence(u8 pattern)                 /* X0635 */
{
    display_blank();                                /* X067e */
    wr_p4(0x00);                                    /* X0678 */
    read_sensor_mux();                              /* X0645 */
    display_refresh();                              /* X0665 */
    keypad_error = false;
    output_flags();                                 /* X06c7 */
}

/*
 * @0x0645 - read the 4-nibble sensor multiplexer. Strobes P1 bits 6/7 to
 * select each nibble in turn and assembles the 4 reads into R5:R6:R7. [SURE]
 */
void read_sensor_mux(void)                          /* X0645 */
{
    P1 &= 0x3F;             u8 n0 = P1 & 0x0F;
    P1 |= 0x40;             u8 n1 = (P1 & 0x07) << 4;
    u8 r7 = n0 | n1;
    P1 &= 0x3F; P1 |= 0x80; u8 r6 = (P1 << 4) | 0x0F;
    P1 |= 0xC0;             u8 r5 = (P1 & 0x03) | 0xF0;
    (void)r7; (void)r6; (void)r5;                   /* -> regs for caller */
}

/* @0x0665 - push the R5:R6:R7 display buffer back out to P4..P7 in two
 * P2-strobed passes (segment data + digit select). [SURE]               */
void display_refresh(void)                          /* X0665 */
{
    P2 = 0xCF; /* group A */ /* wr_p5(r7); wr_p4(r7>>4); wr_p7(r6); wr_p6(r6>>4) */
    P2 = 0xDF; /* group B */ /* wr_p5(r5); wr_p4(r5>>4) */
}

void display_blank(void)        { P2 = 0xDF; /* wr_p7(r3) */ }            /* X067e */
void output_flags(void)         { P2 = 0xEF; /* set/clr p5 bit1 by F1 */ }/* X06c7 */

/* @0x06e5 - copy 5 bytes downward: XRAM[r1-i] = XRAM[r0-i], i=0..4.     */
void bcd_copy5(u8 src, u8 dst)                      /* X06e5 */
{
    for (int i = 0; i < 5; ++i) X(dst - i) = X(src - i);
}

/* @0x06d5 - stage a 16-bit value (r4,r5) as a zero-padded operand at
 * XRAM[0x06..0x0A] for the bank-1 BCD multiply/add. [SURE]              */
void bank1_X06d5(u8 hi, u8 lo)                      /* X06d5 (lives in bank0) */
{
    X(0x0A) = 0x00; X(0x09) = lo; X(0x08) = hi; X(0x07) = 0x00; X(0x06) = 0x00;
}

/* =========================================================================
 * BANK 1  (A98EH.bin)  -  multi-byte BCD math library + controller logic
 *
 * Everything below is reached from bank 0 with `SEL MB1`. The 5-byte BCD
 * numbers are stored most-significant-byte-at-lowest-address and processed
 * with the 8048's DA (decimal adjust) instruction. [SURE for the math]
 * ========================================================================= */

/* @bank1 0x000 (X0800 from bank0) - BCD compare/normalise entry. Copies
 * two operands, compares them and returns the relation in F1. [LIKELY]  */
void bank1_X0000(u8 a_ptr, u8 b_ptr);

/* @0x00a2 - copy r2 bytes downward XRAM[r1-i]=XRAM[r0-i]. [SURE]        */
void mem_copy_down(u8 src, u8 dst, u8 n)            /* X00a2 */
{
    for (int i = 0; i < n; ++i) X(dst - i) = X(src - i);
}

/* @0x00a9 - nibble-rotate a BCD field right one digit (divide by 10 in a
 * packed-BCD string). [SURE]                                            */
void bcd_shift_right_digit(u8 ptr, u8 n);           /* X00a9 */

/* @0x00c2 - zero r2 bytes downward from r1. [SURE]                      */
void mem_clear_down(u8 ptr, u8 n)                   /* X00c2 */
{
    for (int i = 0; i < n; ++i) X(ptr - i) = 0;
}

/*
 * @0x00c8 - 6-digit BCD subtract: tens-complement [0x24..] then add to
 * [0x29..] with DA, producing [0x2E..]; carry out is returned in C and
 * used by callers as "A >= B". This is the core comparator. [SURE]
 */
bool bcd_subtract(void)                             /* X00c8 -> returns carry */
{
    /* form 9's/10's complement of operand at 0x24..0x20 into 0x2E..0x2A */
    /* then BCD add to 0x29..0x25 with PSW carry threaded, DA each step  */
    /* return !borrow                                                    */
    return false; /* see disasm; mechanical */
}

/* @0x00f8 - copy 4 bytes (external) downward. @0x0101 - copy 4 bytes
 * (acc) into XRAM[5..2]. @0x010c - clear the 0x20..0x3A scratch block. */
void mem_copy4(u8 src, u8 dst);                     /* X00f8 */
void acc_store4(u8 src);                            /* X0101 */
void scratch_clear(void)                            /* X010c */
{
    for (int i = 0; i < 0x1B; ++i) X(0x3A - i) = 0;
}

/*
 * @0x0116 / @0x019c / @0x019f - the three public arithmetic entry points
 * used all over bank 0:
 *   X0116  : BCD ADD     dst += src        [LIKELY]
 *   X019c  : BCD compare (clr F0)          [LIKELY]
 *   X019f  : BCD compare (set F0 = signed) [LIKELY]
 * They share the load/compare/add machinery at X012d/X0168/X01a1. F1 is
 * returned as the relation/overflow flag. [SURE these are the BCD ops]
 */
void bank1_X0116(u8 a_ptr, u8 b_ptr);   /* add */
void bank1_X019c(u8 a_ptr, u8 b_ptr);   /* compare, unsigned */
void bank1_X019f(u8 a_ptr, u8 b_ptr);   /* compare, signed   */

/* @0x0168 - 8-digit BCD add of [0x27..]+[0x33..] -> [0x33..] with DA.   */
void bcd_add8(void);                                /* X0168 */

/* @0x017d - BCD->binary-ish increment helper used when formatting. [SURE]*/
void bcd_increment_field(void);                     /* X017d */

/* @0x01f9 - stage (r4,r5) as a padded BCD operand at 0x06..0x0A, like the
 * bank-0 X06d5 but for bank-1 callers. [SURE]                           */
void bank1_stage_operand(u8 hi, u8 lo)              /* X01f9 */
{
    X(0x0A) = 0x00; X(0x09) = lo; X(0x08) = hi; X(0x07) = 0x00; X(0x06) = 0x00;
}

/* @0x0209 - 5-byte external copy downward (bank-1 twin of X06e5). [SURE]*/
void bank1_copy5(u8 src, u8 dst)                    /* X0209 */
{
    for (int i = 0; i < 5; ++i) X(dst - i) = X(src - i);
}

/* ---- bank-1 controller state machine -------------------------------- */

/*
 * @0x0212 (X0a12 from bank0) - convert a raw timer reading (r3=count,
 * r4=overflows) into a 5-digit BCD value in the accumulator and store it.
 * This is the back half of read_timing_channel(). [LIKELY]
 */
void bank1_X0212(u8 count, u8 overflows);

/*
 * @0x0240 (X0a40 from bank0) - top of the bank-1 control state machine.
 * Looks at the two run flags XRAM[0x18] / XRAM[0x19]; if either is set it
 * runs the heating/curve computation (calls X019c, X02f2 "ramp", X031f
 * "step") storing results at 0x60/0x65; otherwise jumps to X03d5. [LIKELY]
 */
void bank1_X0240(void)                              /* X0240 */
{
    if (X(0x18) == 0 && X(0x19) == 0) { state_X03d5(); return; }

    /* channel 1 */
    bank1_X019c(0x42, 0x47); bank1_copy5(0x05, 0x60);
    bank1_X019c(0x4C, 0x47); bank1_copy5(0x05, 0x65);
    if (X(0x18)) {                                   /* X0267 */
        capture_pair();                             /* X03c4 */
        bank1_stage_operand(0x45, 0x00);
        /* compute curve point -> 0x24/0x25, ramp via X02f2 */
        ramp_to(0xC4, 0xD8);                        /* X02f2 with operands */
        ramp_to(0xBA, 0xCE);
        X(0x18) = step_curve(0x18);                 /* X031f */
    }
    if (X(0x19)) {                                   /* X028e */
        /* channel 2: same shape, results to 0x65 */
        capture_pair();
        ramp_to(0xC4, 0xD8);
        ramp_to(0xBF, 0xD3);
        X(0x19) = step_curve(0x19);
    }
    state_X03d5();
}

/*
 * @0x02f2 - "ramp": repeatedly nudges the working value toward a target
 * (X0394 step generator, X03aa apply, X0358 dwell delay) until X0394
 * signals done via F1. Drives the modulating output smoothly. [GUESS]
 */
void ramp_to(u8 lo_target, u8 hi_target)            /* X02f2 */
{
    stage_ramp_window();                            /* X0383 */
    /* r6 = XRAM[0x25]; compare via X019f; copy to 0x0F */
    for (;;) {
        if (ramp_step_done()) break;                /* X0394 -> F1 */
        bank1_copy5(0x05, 0x0F);
        u8 step = X(0x24);
        apply_ramp_step(step);                      /* X03aa */
        dwell_delay();                              /* X0358 */
    }
    (void)lo_target; (void)hi_target;
}

/*
 * @0x0358 - dwell/settling delay that also pulses the modulating valve
 * output on P6 (bit pattern 0x0B/0x0D chosen by XRAM[0x24] bit6), with two
 * software busy-wait loops in between. [LIKELY]
 */
void dwell_delay(void)                              /* X0358 */
{
    P2 = 0xEF;
    u8 mask = (X(0x24) & 0x40) ? 0x0D : 0x0B;
    wr_p6(X(0xF8) & mask);
    busy_delay();                                   /* ~0x60 * 256 cycles */
    wr_p6(X(0xF8) | (u8)~mask);
    busy_delay();
}

/* @0x031f - one discrete step of the heat curve: snapshot, add increment,
 * normalise, store. Returns the updated channel byte. [LIKELY]          */
u8 step_curve(u8 chan_addr);                        /* X031f */

/* @0x03c4 - capture/save a pair of working bytes (swap ACC <-> buffers).*/
void capture_pair(void);                            /* X03c4 */

/*
 * @0x03d5 (state) - secondary regulation state: compares the computed
 * demand at 0x3D against thresholds and decides the on/off relay outputs
 * (status bits in XRAM[0xF7], P6/P7 expander), with anti-cycle counters in
 * XRAM[0x27]/[0xF9]. Tail-jumps to X0508. [LIKELY]
 */
void state_X03d5(void)                              /* X03d5 */
{
    bank1_stage_operand(0x00, 0x50);
    bank1_X019f(0x3D, 0x0A);
    if (keypad_error) { state_X0508(); return; }    /* jf1 X03f6 */
    X(0x07) = 0x02;
    bank1_X019c(0x3D, 0x0A);
    if (keypad_error) { state_X0508(); return; }
    /* threshold ladder X03f8..X04f4: set/clear status bits in 0xF7 and the
     * P6/P7 relay nibbles, with debounce via X04ff busy delay. [LIKELY]  */
    state_X0508();
}

/*
 * @0x0508 - the program/clock scheduler state. Reads the schedule flags at
 * XRAM[0x4D], walks the time program (X06fa/X06f3 read panel time), and
 * sets the demand for the regulation states above. Ends at X06f1->X072d
 * or idles at X07d8. [GUESS - this is the weekly/daily timer logic]
 */
void state_X0508(void)                              /* X0508 / X050a */
{
    if (X(0x4D) & 0x80) { final_state(); return; }  /* schedule disabled */
    /* compare program slot, branch by panel reads (X06fa/X06f3) into the
     * several scheduling sub-states X0542 / X05a2 / X05e6 / X061e, each of
     * which stages BCD operands and writes the demand. [LIKELY]          */
    final_state();
}

/*
 * @0x072d - the final output/idle stage: reads the sensor mux one more
 * time (bank-0 X0645 via SEL MB0), updates the display latch on P6/P7 and
 * the relay shadow XRAM[0x15], then enables interrupts and idles at
 * X07d8 waiting for the next external-interrupt tick. [LIKELY]
 */
void final_state(void)                              /* X072d */
{
    P2 = 0xDF; and_p6(0x03);
    bank1_X09f9(0x00, 0x05);                         /* stage operand */
    bank1_X019c(0x4C, 0x0A);
    /* ... display/relay update ladder X0741..X07c4 ... */
    /* X07d6: */
    en_int();
    for (;;) {}                                      /* X07d8 idle */
}

/* @0x06f3 - read the 4 panel time/mode switches on P4 (low nibble). [SURE]*/
u8 read_panel_switches(void)                        /* X06f3 */
{
    P2 = 0xAF;
    return movd_p4() & 0x0E;
}

/* bank-1 helpers referenced above (mechanical; see A98EH.d48) */
void stage_ramp_window(void);     /* X0383 */
bool ramp_step_done(void);        /* X0394 -> F1 */
void apply_ramp_step(u8 step);    /* X03aa */
void bank1_X09f9(u8 hi, u8 lo);   /* X09f9 stage */
void bank1_X099c(u8 a, u8 b);     /* alias of compare path */
void bank1_X04ff(void);           /* X04ff busy delay (~0x80*256) */
void bank1_X0240(void);
void bank1_X0212(u8, u8);
void btn_inc(u8 mask, u8 ctr);    /* X0526/X0534 */
void btn_program(void);           /* X057e */
void btn_clock_adjust(void);      /* X054f */
void btn_mode(void);              /* X053e */
void btn_start(void);             /* X056d */
void busy_delay(void);
void apply_selection(void);
void regulation_pass(void);

/* threaded software flag (8048 F1) */
extern bool keypad_error;
