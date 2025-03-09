/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <cpu/cpu.h>
#include <cpu/decode.h>
#include <cpu/difftest.h>
#include <locale.h>

/* The assembly code of instructions executed is only output to the screen
 * when the number of instructions executed is less than this value.
 * This is useful when you use the `si' command.
 * You can modify this value as you want.
 */
#define MAX_INST_TO_PRINT 10

CPU_state cpu = {};
uint64_t g_nr_guest_inst = 0;
static uint64_t g_timer = 0; // unit: us
static bool g_print_step = false;

void device_update();

/**
 * Trace and perform differential testing for the current instruction.
 *
 * This method logs the execution trace of the current instruction if tracing is enabled
 * and performs differential testing if the DIFFTEST feature is enabled. The method
 * checks the condition for instruction tracing (CONFIG_ITRACE_COND) and logs the
 * instruction trace to the log buffer if the condition is met. Additionally, if the
 * global flag `g_print_step` is set, the method prints the log buffer to the standard
 * output. Finally, if the DIFFTEST feature is enabled, the method invokes the
 * differential testing step with the current program counter (pc) and the next program
 * counter (dnpc) as arguments.
 *
 * @param _this Pointer to the Decode structure containing the current instruction's details.
 * @param dnpc The next program counter value after executing the current instruction.
 */
static void trace_and_difftest(Decode *_this, vaddr_t dnpc) {
#ifdef CONFIG_ITRACE_COND
  if (ITRACE_COND) { log_write("%s\n", _this->logbuf); }
#endif
  if (g_print_step) { IFDEF(CONFIG_ITRACE, puts(_this->logbuf)); }
  IFDEF(CONFIG_DIFFTEST, difftest_step(_this->pc, dnpc));
}

static void exec_once(Decode *s, vaddr_t pc) {
  s->pc = pc;
  s->snpc = pc;
  isa_exec_once(s);
  cpu.pc = s->dnpc;
#ifdef CONFIG_ITRACE
  char *p = s->logbuf;
  p += snprintf(p, sizeof(s->logbuf), FMT_WORD ":", s->pc);
  int ilen = s->snpc - s->pc;
  int i;
  uint8_t *inst = (uint8_t *)&s->isa.inst;
#ifdef CONFIG_ISA_x86
  for (i = 0; i < ilen; i ++) {
#else
  for (i = ilen - 1; i >= 0; i --) {
#endif
    p += snprintf(p, 4, " %02x", inst[i]);
  }
  int ilen_max = MUXDEF(CONFIG_ISA_x86, 8, 4);
  int space_len = ilen_max - ilen;
  if (space_len < 0) space_len = 0;
  space_len = space_len * 3 + 1;
  memset(p, ' ', space_len);
  p += space_len;

  void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
  disassemble(p, s->logbuf + sizeof(s->logbuf) - p,
      MUXDEF(CONFIG_ISA_x86, s->snpc, s->pc), (uint8_t *)&s->isa.inst, ilen);
#endif
}

/**
 * Executes a specified number of guest instructions on the CPU.
 *
 * This method iterates over the given number of instructions (`n`), decoding and executing each one.
 * It updates the guest instruction count and performs tracing and difftesting after each execution.
 * The loop terminates early if the NEMU state is no longer `NEMU_RUNNING`. Additionally, if the
 * `CONFIG_DEVICE` macro is defined, it updates the device state after each instruction execution.
 *
 * @param n The number of guest instructions to execute. If `n` is zero, the method does nothing.
 */
static void execute(uint64_t n) {
  Decode s;
  for (;n > 0; n --) {
    exec_once(&s, cpu.pc);
    g_nr_guest_inst ++;
    trace_and_difftest(&s, cpu.pc);
    if (nemu_state.state != NEMU_RUNNING) break;
    IFDEF(CONFIG_DEVICE, device_update());
  }
}

/**
 * The `statistic` method logs various performance metrics related to the simulation.
 * It first sets the locale for numeric formatting based on the configuration target.
 * The method then logs the following information:
 * 1. The total host time spent in microseconds (`g_timer`).
 * 2. The total number of guest instructions executed (`g_nr_guest_inst`).
 * 3. The simulation frequency in instructions per second, calculated as 
 *    `(g_nr_guest_inst * 1000000) / g_timer`. If the host time (`g_timer`) is less than or 
 *    equal to 0, it logs a message indicating that the simulation finished in less than 1 
 *    microsecond and the frequency cannot be calculated.
 * The numeric values are formatted according to the locale and configuration settings.
 */
static void statistic() {
  IFNDEF(CONFIG_TARGET_AM, setlocale(LC_NUMERIC, ""));
#define NUMBERIC_FMT MUXDEF(CONFIG_TARGET_AM, "%", "%'") PRIu64
  Log("host time spent = " NUMBERIC_FMT " us", g_timer);
  Log("total guest instructions = " NUMBERIC_FMT, g_nr_guest_inst);
  if (g_timer > 0) Log("simulation frequency = " NUMBERIC_FMT " inst/s", g_nr_guest_inst * 1000000 / g_timer);
  else Log("Finish running in less than 1 us and can not calculate the simulation frequency");
}

/**
 * @brief Handles a failure assertion by displaying the current state of ISA registers
 *        and logging relevant statistics.
 *
 * This function is invoked when an assertion fails. It performs two main actions:
 * 1. Calls `isa_reg_display()` to display the current state of the ISA (Instruction Set Architecture)
 *    registers, providing insight into the system's state at the time of the failure.
 * 2. Calls `statistic()` to log or display relevant statistics, which may include performance
 *    metrics, error counts, or other diagnostic information useful for debugging.
 *
 * This function is typically used in debugging scenarios to gather critical information
 * about the system's state when an unexpected condition occurs.
 */
void assert_fail_msg() {
  isa_reg_display();
  statistic();
}

/* Simulate how the CPU works. */
void cpu_exec(uint64_t n) {
  g_print_step = (n < MAX_INST_TO_PRINT);
  switch (nemu_state.state) {
    case NEMU_END: case NEMU_ABORT: case NEMU_QUIT:
      printf("Program execution has ended. To restart the program, exit NEMU and run again.\n");
      return;
    default: nemu_state.state = NEMU_RUNNING;
  }

  uint64_t timer_start = get_time();

  execute(n);

  uint64_t timer_end = get_time();
  g_timer += timer_end - timer_start;

  switch (nemu_state.state) {
    case NEMU_RUNNING: nemu_state.state = NEMU_STOP; break;

    case NEMU_END: case NEMU_ABORT:
      Log("nemu: %s at pc = " FMT_WORD,
          (nemu_state.state == NEMU_ABORT ? ANSI_FMT("ABORT", ANSI_FG_RED) :
           (nemu_state.halt_ret == 0 ? ANSI_FMT("HIT GOOD TRAP", ANSI_FG_GREEN) :
            ANSI_FMT("HIT BAD TRAP", ANSI_FG_RED))),
          nemu_state.halt_pc);
      // fall through
    case NEMU_QUIT: statistic();
  }
}
