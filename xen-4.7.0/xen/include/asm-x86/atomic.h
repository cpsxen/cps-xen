#ifndef __ARCH_X86_ATOMIC__
#define __ARCH_X86_ATOMIC__

#include <xen/config.h>
#include <asm/system.h>

#define build_read_atomic(name, size, type, reg, barrier) \
static inline type name(const volatile type *addr) \
{ type ret; asm volatile("mov" size " %1,%0":reg (ret) \
:"m" (*(volatile type *)addr) barrier); return ret; }

#define build_write_atomic(name, size, type, reg, barrier) \
static inline void name(volatile type *addr, type val) \
{ asm volatile("mov" size " %1,%0": "=m" (*(volatile type *)addr) \
:reg (val) barrier); }

#define build_add_sized(name, size, type, reg) \
    static inline void name(volatile type *addr, type val)              \
    {                                                                   \
        asm volatile("add" size " %1,%0"                                \
                     : "=m" (*addr)                                     \
                     : reg (val));                                      \
    }

build_read_atomic(read_u8_atomic, "b", uint8_t, "=q", )
build_read_atomic(read_u16_atomic, "w", uint16_t, "=r", )
build_read_atomic(read_u32_atomic, "l", uint32_t, "=r", )
build_read_atomic(read_u64_atomic, "q", uint64_t, "=r", )

build_write_atomic(write_u8_atomic, "b", uint8_t, "q", )
build_write_atomic(write_u16_atomic, "w", uint16_t, "r", )
build_write_atomic(write_u32_atomic, "l", uint32_t, "r", )
build_write_atomic(write_u64_atomic, "q", uint64_t, "r", )

build_add_sized(add_u8_sized, "b", uint8_t, "qi")
build_add_sized(add_u16_sized, "w", uint16_t, "ri")
build_add_sized(add_u32_sized, "l", uint32_t, "ri")
build_add_sized(add_u64_sized, "q", uint64_t, "ri")

#undef build_read_atomic
#undef build_write_atomic
#undef build_add_sized

void __bad_atomic_size(void);

#define read_atomic(p) ({                                 \
    unsigned long x_;                                     \
    switch ( sizeof(*(p)) ) {                             \
    case 1: x_ = read_u8_atomic((uint8_t *)(p)); break;   \
    case 2: x_ = read_u16_atomic((uint16_t *)(p)); break; \
    case 4: x_ = read_u32_atomic((uint32_t *)(p)); break; \
    case 8: x_ = read_u64_atomic((uint64_t *)(p)); break; \
    default: x_ = 0; __bad_atomic_size(); break;          \
    }                                                     \
    (typeof(*(p)))x_;                                     \
})

#define write_atomic(p, x) ({                             \
    typeof(*(p)) __x = (x);                               \
    unsigned long x_ = (unsigned long)__x;                \
    switch ( sizeof(*(p)) ) {                             \
    case 1: write_u8_atomic((uint8_t *)(p), x_); break;   \
    case 2: write_u16_atomic((uint16_t *)(p), x_); break; \
    case 4: write_u32_atomic((uint32_t *)(p), x_); break; \
    case 8: write_u64_atomic((uint64_t *)(p), x_); break; \
    default: __bad_atomic_size(); break;                  \
    }                                                     \
})

#define add_sized(p, x) ({                                \
    typeof(*(p)) x_ = (x);                                \
    switch ( sizeof(*(p)) )                               \
    {                                                     \
    case 1: add_u8_sized((uint8_t *)(p), x_); break;      \
    case 2: add_u16_sized((uint16_t *)(p), x_); break;    \
    case 4: add_u32_sized((uint32_t *)(p), x_); break;    \
    case 8: add_u64_sized((uint64_t *)(p), x_); break;    \
    default: __bad_atomic_size(); break;                  \
    }                                                     \
})

/*
 * NB. I've pushed the volatile qualifier into the operations. This allows
 * fast accessors such as _atomic_read() and _atomic_set() which don't give
 * the compiler a fit.
 */
typedef struct { int counter; } atomic_t;

#define ATOMIC_INIT(i) { (i) }

/**
 * atomic_read - read atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically reads the value of @v.
 */
static inline int atomic_read(atomic_t *v)
{
    return read_atomic(&v->counter);
}

/**
 * _atomic_read - read atomic variable non-atomically
 * @v atomic_t
 *
 * Non-atomically reads the value of @v
 */
static inline int _atomic_read(atomic_t v)
{
    return v.counter;
}


/**
 * atomic_set - set atomic variable
 * @v: pointer of type atomic_t
 * @i: required value
 *
 * Atomically sets the value of @v to @i.
 */
static inline void atomic_set(atomic_t *v, int i)
{
    write_atomic(&v->counter, i);
}

/**
 * _atomic_set - set atomic variable non-atomically
 * @v: pointer of type atomic_t
 * @i: required value
 *
 * Non-atomically sets the value of @v to @i.
 */
static inline void _atomic_set(atomic_t *v, int i)
{
    v->counter = i;
}

static inline int atomic_cmpxchg(atomic_t *v, int old, int new)
{
    return cmpxchg(&v->counter, old, new);
}

/**
 * atomic_add - add integer to atomic variable
 * @i: integer value to add
 * @v: pointer of type atomic_t
 * 
 * Atomically adds @i to @v.
 */
static inline void atomic_add(int i, atomic_t *v)
{
    asm volatile (
        "lock; addl %1,%0"
        : "=m" (*(volatile int *)&v->counter)
        : "ir" (i), "m" (*(volatile int *)&v->counter) );
}

/**
 * atomic_add_return - add integer and return
 * @i: integer value to add
 * @v: pointer of type atomic_t
 *
 * Atomically adds @i to @v and returns @i + @v
 */
static inline int atomic_add_return(int i, atomic_t *v)
{
    return i + arch_fetch_and_add(&v->counter, i);
}

/**
 * atomic_sub - subtract the atomic variable
 * @i: integer value to subtract
 * @v: pointer of type atomic_t
 * 
 * Atomically subtracts @i from @v.
 */
static inline void atomic_sub(int i, atomic_t *v)
{
    asm volatile (
        "lock; subl %1,%0"
        : "=m" (*(volatile int *)&v->counter)
        : "ir" (i), "m" (*(volatile int *)&v->counter) );
}

/**
 * atomic_sub_and_test - subtract value from variable and test result
 * @i: integer value to subtract
 * @v: pointer of type atomic_t
 * 
 * Atomically subtracts @i from @v and returns
 * true if the result is zero, or false for all
 * other cases.
 */
static inline int atomic_sub_and_test(int i, atomic_t *v)
{
    unsigned char c;

    asm volatile (
        "lock; subl %2,%0; sete %1"
        : "=m" (*(volatile int *)&v->counter), "=qm" (c)
        : "ir" (i), "m" (*(volatile int *)&v->counter) : "memory" );
    return c;
}

/**
 * atomic_inc - increment atomic variable
 * @v: pointer of type atomic_t
 * 
 * Atomically increments @v by 1.
 */ 
static inline void atomic_inc(atomic_t *v)
{
    asm volatile (
        "lock; incl %0"
        : "=m" (*(volatile int *)&v->counter)
        : "m" (*(volatile int *)&v->counter) );
}

/**
 * atomic_dec - decrement atomic variable
 * @v: pointer of type atomic_t
 * 
 * Atomically decrements @v by 1.
 */ 
static inline void atomic_dec(atomic_t *v)
{
    asm volatile (
        "lock; decl %0"
        : "=m" (*(volatile int *)&v->counter)
        : "m" (*(volatile int *)&v->counter) );
}

/**
 * atomic_dec_and_test - decrement and test
 * @v: pointer of type atomic_t
 * 
 * Atomically decrements @v by 1 and
 * returns true if the result is 0, or false for all other
 * cases.
 */ 
static inline int atomic_dec_and_test(atomic_t *v)
{
    unsigned char c;

    asm volatile (
        "lock; decl %0; sete %1"
        : "=m" (*(volatile int *)&v->counter), "=qm" (c)
        : "m" (*(volatile int *)&v->counter) : "memory" );
    return c != 0;
}

/**
 * atomic_inc_and_test - increment and test 
 * @v: pointer of type atomic_t
 * 
 * Atomically increments @v by 1
 * and returns true if the result is zero, or false for all
 * other cases.
 */ 
static inline int atomic_inc_and_test(atomic_t *v)
{
    unsigned char c;

    asm volatile (
        "lock; incl %0; sete %1"
        : "=m" (*(volatile int *)&v->counter), "=qm" (c)
        : "m" (*(volatile int *)&v->counter) : "memory" );
    return c != 0;
}

/**
 * atomic_add_negative - add and test if negative
 * @v: pointer of type atomic_t
 * @i: integer value to add
 * 
 * Atomically adds @i to @v and returns true
 * if the result is negative, or false when
 * result is greater than or equal to zero.
 */ 
static inline int atomic_add_negative(int i, atomic_t *v)
{
    unsigned char c;

    asm volatile (
        "lock; addl %2,%0; sets %1"
        : "=m" (*(volatile int *)&v->counter), "=qm" (c)
        : "ir" (i), "m" (*(volatile int *)&v->counter) : "memory" );
    return c;
}

#endif /* __ARCH_X86_ATOMIC__ */
