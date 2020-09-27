#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// riscv register file enums
enum {
  rv_reg_zero = 0,
  rv_reg_ra,
  rv_reg_sp,
  rv_reg_gp,
  rv_reg_tp,
  rv_reg_t0,
  rv_reg_t1,
  rv_reg_t2,
  rv_reg_s0,
  rv_reg_s1,
  rv_reg_a0,
  rv_reg_a1,
  rv_reg_a2,
  rv_reg_a3,
  rv_reg_a4,
  rv_reg_a5,
  rv_reg_a6,
  rv_reg_a7,
  rv_reg_s2,
  rv_reg_s3,
  rv_reg_s4,
  rv_reg_s5,
  rv_reg_s6,
  rv_reg_s7,
  rv_reg_s8,
  rv_reg_s9,
  rv_reg_s10,
  rv_reg_s11,
  rv_reg_t3,
  rv_reg_t4,
  rv_reg_t5,
  rv_reg_t6,
};

struct riscv_t;
typedef void *riscv_user_t;

typedef uint32_t riscv_word_t;
typedef uint16_t riscv_half_t;
typedef uint8_t  riscv_byte_t;

// memory read handlers
typedef riscv_word_t (*riscv_mem_read_w)(struct riscv_t *rv, riscv_word_t addr);
typedef riscv_half_t (*riscv_mem_read_s)(struct riscv_t *rv, riscv_word_t addr);
typedef riscv_byte_t (*riscv_mem_read_b)(struct riscv_t *rv, riscv_word_t addr);

// memory write handlers
typedef void (*riscv_mem_write_w)(struct riscv_t *rv, riscv_word_t addr, riscv_word_t data);
typedef void (*riscv_mem_write_s)(struct riscv_t *rv, riscv_word_t addr, riscv_half_t data);
typedef void (*riscv_mem_write_b)(struct riscv_t *rv, riscv_word_t addr, riscv_byte_t data);

// system instruction handlers
typedef void (*riscv_on_ecall )(struct riscv_t *rv, riscv_word_t addr, uint32_t inst);
typedef void (*riscv_on_ebreak)(struct riscv_t *rv, riscv_word_t addr, uint32_t inst);

// riscv emulator io interface
struct riscv_io_t {
  // memory read interface
  riscv_mem_read_w mem_read_w;
  riscv_mem_read_s mem_read_s;
  riscv_mem_read_b mem_read_b;
  // memory write interface
  riscv_mem_write_w mem_write_w;
  riscv_mem_write_s mem_write_s;
  riscv_mem_write_b mem_write_b;
  // system commands
  riscv_on_ecall on_ecall;
  riscv_on_ebreak on_ebreak;
};

// create a riscv emulator
struct riscv_t *rv_create(const struct riscv_io_t *io, riscv_user_t user_data);

// delete a riscv emulator
void rv_delete(struct riscv_t *);

// reset the riscv processor
void rv_reset(struct riscv_t *, riscv_word_t pc);

// single step the riscv emulator
void rv_step(struct riscv_t *);

// get riscv user data bound to an emulator
riscv_user_t rv_userdata(struct riscv_t *);

// set the program counter of a riscv emulator
void rv_set_pc(struct riscv_t *rv, riscv_word_t pc);

// get the program counter of a riscv emulator
riscv_word_t rv_get_pc(struct riscv_t *rv);

// set a register of the riscv emulator
void rv_set_reg(struct riscv_t *, uint32_t reg, riscv_word_t in);

// get a register of the riscv emulator
riscv_word_t rv_get_reg(struct riscv_t *, uint32_t reg);

#ifdef __cplusplus
};  // ifdef __cplusplus
#endif
