#ifndef W25Q64_H
#define W25Q64_H
#include <stdint.h>
#include <stddef.h>
/** @brief Read JEDEC and reject mismatched identity before any mutation. */
int w25q_init(void);
uint32_t w25q_id(void);
/** @brief Read a fresh 0x9f transaction (not the cached boot identity). */
int w25q_probe(uint32_t *id);
/** @brief Bounded SPI transactions; every public operation acquires arbiter. */
int w25q_read(uint32_t address,void *data,size_t length);
/** @brief Program1..256 bytes within ONE page; require erased bytes and verify. */
int w25q_program(uint32_t address,const void *data,size_t length);
/** @brief Aligned4KB erase, busy timeout, readback verify; no ISR use. */
int w25q_erase(uint32_t address);
/** @brief Successful erases since MCU boot; not a lifetime endurance counter. */
uint32_t w25q_erases(void);
#endif
