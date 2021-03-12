/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_ATOMIC_H_
#define _RTE_ATOMIC_H_

/**
 * @file
 * Atomic Operations
 *
 * This file defines a generic API for atomic operations.
 */

#include <stdint.h>
#include "rte_common.h"

#ifdef __DOXYGEN__

/** @name Memory Barrier
 */
///@{
/**
 * General memory barrier.
 *
 * Guarantees that the LOAD and STORE operations generated before the
 * barrier occur before the LOAD and STORE operations generated after.
 */
static inline void
rte_mb(void);

/**
 * Write memory barrier.
 *
 * Guarantees that the STORE operations generated before the barrier
 * occur before the STORE operations generated after.
 */
static inline void
rte_wmb(void);

/**
 * Read memory barrier.
 *
 * Guarantees that the LOAD operations generated before the barrier
 * occur before the LOAD operations generated after.
 */
static inline void
rte_rmb(void);
///@}

/** @name SMP Memory Barrier
 */
///@{
/**
 * General memory barrier between lcores
 *
 * Guarantees that the LOAD and STORE operations that precede the
 * rte_smp_mb() call are globally visible across the lcores
 * before the LOAD and STORE operations that follows it.
 */
static inline void
rte_smp_mb(void);

/**
 * Write memory barrier between lcores
 *
 * Guarantees that the STORE operations that precede the
 * rte_smp_wmb() call are globally visible across the lcores
 * before the STORE operations that follows it.
 */
static inline void
rte_smp_wmb(void);

/**
 * Read memory barrier between lcores
 *
 * Guarantees that the LOAD operations that precede the
 * rte_smp_rmb() call are globally visible across the lcores
 * before the LOAD operations that follows it.
 */
static inline void
rte_smp_rmb(void);
///@}

/** @name I/O Memory Barrier
 */
///@{
/**
 * General memory barrier for I/O device
 *
 * Guarantees that the LOAD and STORE operations that precede the
 * rte_io_mb() call are visible to I/O device or CPU before the
 * LOAD and STORE operations that follow it.
 */
static inline void
rte_io_mb(void);

/**
 * Write memory barrier for I/O device
 *
 * Guarantees that the STORE operations that precede the
 * rte_io_wmb() call are visible to I/O device before the STORE
 * operations that follow it.
 */
static inline void
rte_io_wmb(void);

/**
 * Read memory barrier for IO device
 *
 * Guarantees that the LOAD operations on I/O device that precede the
 * rte_io_rmb() call are visible to CPU before the LOAD
 * operations that follow it.
 */
static inline void
rte_io_rmb(void);
///@}

/** @name Coherent I/O Memory Barrier
 *
 * Coherent I/O memory barrier is a lightweight version of I/O memory
 * barriers which are system-wide data synchronization barriers. This
 * is for only coherent memory domain between lcore and I/O device but
 * it is same as the I/O memory barriers in most of architectures.
 * However, some architecture provides even lighter barriers which are
 * somewhere in between I/O memory barriers and SMP memory barriers.
 * For example, in case of ARMv8, DMB(data memory barrier) instruction
 * can have different shareability domains - inner-shareable and
 * outer-shareable. And inner-shareable DMB fits for SMP memory
 * barriers and outer-shareable DMB for coherent I/O memory barriers,
 * which acts on coherent memory.
 *
 * In most cases, I/O memory barriers are safer but if operations are
 * on coherent memory instead of incoherent MMIO region of a device,
 * then coherent I/O memory barriers can be used and this could bring
 * performance gain depending on architectures.
 */
///@{
/**
 * Write memory barrier for coherent memory between lcore and I/O device
 *
 * Guarantees that the STORE operations on coherent memory that
 * precede the rte_cio_wmb() call are visible to I/O device before the
 * STORE operations that follow it.
 */
static inline void
rte_cio_wmb(void);

/**
 * Read memory barrier for coherent memory between lcore and I/O device
 *
 * Guarantees that the LOAD operations on coherent memory updated by
 * I/O device that precede the rte_cio_rmb() call are visible to CPU
 * before the LOAD operations that follow it.
 */
static inline void
rte_cio_rmb(void);
///@}

#endif /* __DOXYGEN__ */

/**
 * Compiler barrier.
 *
 * Guarantees that operation reordering does not occur at compile time
 * for operations directly before and after the barrier.
 */
#define rte_compiler_barrier()                   \
        do {                                     \
                asm volatile("" : : : "memory"); \
        } while (0)

/**
 * The atomic counter structure.
 */
typedef struct {
        volatile int16_t cnt; /**< An internal counter value. */
} rte_atomic16_t;

/**
 * Static initializer for an atomic counter.
 */
#define RTE_ATOMIC16_INIT(val) \
        { (val) }

#endif /* _RTE_ATOMIC_H_ */

// onvm - default byte order
#define RTE_BIG_ENDIAN 1
#define RTE_BYTE_ORDER RTE_BIG_ENDIAN