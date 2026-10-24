#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

/* 17.14 fixed-point real arithmetic, used by the 4.4BSD scheduler
   because the kernel cannot use floating point.  A fixed_t holds a
   real number X as the integer X * 2^14.  N is an integer, X and Y
   are fixed-point values. */
typedef int fixed_t;

#define FP_F (1 << 14)

#define INT_TO_FP(N) ((N) * FP_F)
#define FP_TO_INT_ZERO(X) ((X) / FP_F)
#define FP_TO_INT_NEAREST(X) ((X) >= 0 ? ((X) + FP_F / 2) / FP_F \
                                       : ((X) - FP_F / 2) / FP_F)

#define FP_ADD(X, Y) ((X) + (Y))
#define FP_SUB(X, Y) ((X) - (Y))
#define FP_ADD_INT(X, N) ((X) + (N) * FP_F)
#define FP_SUB_INT(X, N) ((X) - (N) * FP_F)

/* Widen to 64 bits so the intermediate product cannot overflow. */
#define FP_MUL(X, Y) ((fixed_t) (((int64_t) (X)) * (Y) / FP_F))
#define FP_DIV(X, Y) ((fixed_t) (((int64_t) (X)) * FP_F / (Y)))
#define FP_MUL_INT(X, N) ((X) * (N))
#define FP_DIV_INT(X, N) ((X) / (N))

#endif /* threads/fixed-point.h */
