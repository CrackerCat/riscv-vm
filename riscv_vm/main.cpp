#include <cstring>
#include <ctime>
#include <memory>

#include "elf.h"
#include "file.h"
#include "memory.h"

#include "../riscv_core/riscv.h"
#include "state.h"


// enable program trace mode
bool g_arg_trace = false;
// enable compliance mode
bool g_arg_compliance = false;
// target executable
const char *g_arg_program = "a.out";
// show MIPS
bool g_arg_show_mips = false;

// main syscall handler
void syscall_handler(struct riscv_t *);
// arg parsing functions
void print_usage(const char *filename);
bool parse_args(int argc, char **args);

namespace {

riscv_word_t imp_mem_ifetch(struct riscv_t *rv, riscv_word_t addr) {
  state_t *s = (state_t*)rv_userdata(rv);
  return s->mem.read_ifetch(addr);
}

riscv_word_t imp_mem_read_w(struct riscv_t *rv, riscv_word_t addr) {
  state_t *s = (state_t*)rv_userdata(rv);
  return s->mem.read_w(addr);
}

riscv_half_t imp_mem_read_s(struct riscv_t *rv, riscv_word_t addr) {
  state_t *s = (state_t*)rv_userdata(rv);
  return s->mem.read_s(addr);
}

riscv_byte_t imp_mem_read_b(struct riscv_t *rv, riscv_word_t addr) {
  state_t *s = (state_t*)rv_userdata(rv);
  return s->mem.read_b(addr);
}

void imp_mem_write_w(struct riscv_t *rv, riscv_word_t addr, riscv_word_t data) {
  state_t *s = (state_t*)rv_userdata(rv);
  s->mem.write(addr, (uint8_t*)&data, sizeof(data));
}

void imp_mem_write_s(struct riscv_t *rv, riscv_word_t addr, riscv_half_t data) {
  state_t *s = (state_t*)rv_userdata(rv);
  s->mem.write(addr, (uint8_t*)&data, sizeof(data));
}

void imp_mem_write_b(struct riscv_t *rv, riscv_word_t addr, riscv_byte_t  data) {
  state_t *s = (state_t*)rv_userdata(rv);
  s->mem.write(addr, (uint8_t*)&data, sizeof(data));
}

void imp_on_ecall(struct riscv_t *rv, riscv_word_t addr, uint32_t inst) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  // in compliance testing it seems any `ecall` should abort
  if (g_arg_compliance) {
    s->done = true;
    return;
  }
  // pass to the syscall handler
  syscall_handler(rv);
}

void imp_on_ebreak(struct riscv_t *rv, riscv_word_t addr, uint32_t inst) {
  state_t *s = (state_t*)rv_userdata(rv);
  s->done = true;
}

// run the core - printing out an instruction trace
void run_and_trace(riscv_t *rv, state_t *state, elf_t &elf) {
  static const uint32_t cycles_per_step = 1;
  // run until we see the flag that we are done
  for (; !state->done;) {
    // trace execution
    uint32_t pc = rv_get_pc(rv);
    const char *sym = elf.find_symbol(pc);
    printf("%08x  %s\n", pc, (sym ? sym : ""));
    // step instructions
    rv_step(rv, cycles_per_step);
  }
}

// run the core - showing MIPS throughput
void run_and_show_mips(riscv_t *rv, state_t *state, elf_t &elf) {
  static const uint32_t cycles_per_step = 500;
  clock_t start = clock();
  uint32_t clocks = 0;
  // run until we see the flag that we are done
  for (; !state->done;) {
    // track instruction MIPS
    if ((clock() - start) >= CLOCKS_PER_SEC) {
      start += CLOCKS_PER_SEC;
      printf("%d IPS\n", int(clocks));
      clocks = 0;
    }
    // step instructions
    rv_step(rv, cycles_per_step);
    clocks += cycles_per_step;
  }
}

// run the core
void run(riscv_t *rv, state_t *state, elf_t &elf) {
  static const uint32_t cycles_per_step = 100;
  // run until we see the flag that we are done
  for (; !state->done;) {
    // step instructions
    rv_step(rv, cycles_per_step);
  }
}

} // namespace {}


int main(int argc, char **args) {

  // parse the program arguments
  if (!parse_args(argc, args)) {
    print_usage(args[0]);
    return 1;
  }

  // load the ELF file from disk
  elf_t elf;
  if (!elf.load(g_arg_program)) {
    fprintf(stderr, "Unable to load ELF file '%s'\n", g_arg_program);
    return 1;
  }

  // setup the IO handlers for the VM
  const riscv_io_t io = {
    imp_mem_ifetch,
    imp_mem_read_w,
    imp_mem_read_s,
    imp_mem_read_b,
    imp_mem_write_w,
    imp_mem_write_s,
    imp_mem_write_b,
    imp_on_ecall,
    imp_on_ebreak,
  };

  auto state = std::make_unique<state_t>();
  state->break_addr = 0;
  state->fd_map[0] = stdin;
  state->fd_map[1] = stdout;
  state->fd_map[2] = stderr;

  // find the start of the heap
  if (const ELF::Elf32_Sym *end = elf.get_symbol("_end")) {
    state->break_addr = end->st_value;
  }

  // create the VM
  riscv_t *rv = rv_create(&io, state.get());
  if (!rv) {
    fprintf(stderr, "Unable to create riscv emulator\n");
    return 1;
  }

  // upload the ELF file into our memory abstraction
  if (!elf.upload(rv, state->mem)) {
    fprintf(stderr, "Unable to upload ELF file '%s'\n", args[1]);
    return 1;
  }

  // run based on the chosen mode
  if (g_arg_trace) {
    run_and_trace(rv, state.get(), elf);
  }
  else if (g_arg_show_mips) {
    run_and_show_mips(rv, state.get(), elf);
  }
  else {
    run(rv, state.get(), elf);
  }

  // print execution signature
  if (g_arg_compliance) {
    uint32_t start = 0, end = 0;
    if (elf.get_data_section_range(start, end)) {
      // try and access the exact signature start
      if (const ELF::Elf32_Sym *sym = elf.get_symbol("begin_signature")) {
        start = sym->st_value;
      }
      // dump the data
      uint32_t value = 0;
      for (uint32_t i = start; i <= end; i += 4) {
        state->mem.read((uint8_t*)&value, i, 4);
        printf("%08x\n", value);
      }
    }
  }

  // delete the VM
  rv_delete(rv);
  return 0;
}
