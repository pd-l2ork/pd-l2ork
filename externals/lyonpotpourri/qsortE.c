/* Plug-compatible replacement for UNIX qsort.
   Copyright (C) 1989 Free Software Foundation, Inc.
   Written by Douglas C. Schmidt (schmidt@ics.uci.edu)

This file is part of GNU CC.

GNU QSORT is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU QSORT is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU QSORT; see the file COPYING.  If not, write to
the Free the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA. */

/* Synched up with: FSF 19.28. */

#ifdef sparc
#include <alloca.h>
#endif

#include <stdlib.h>

/* Invoke the comparison function, returns either 0, < 0, or > 0. */
#define CMP(A,B) ((*cmp)((A),(B)))

/* Byte-wise swap two items of size SIZE. */
#define SWAP(A,B,SIZE) do {int sz = (SIZE); char *a = (A); char *b = (B); \
    do { char _temp = *a;*a++ = *b;*b++ = _temp;} while (--sz);} while (0)

/* Copy SIZE bytes from item B to item A. */
#define COPY(A,B,SIZE) {int sz = (SIZE); do { *(A)++ = *(B)++; } while (--sz); }

/* This should be replaced by a standard ANSI macro. */
#define BYTES_PER_WORD 8

/* The next 4 #defines implement a very fast in-line stack abstraction. */
#define STACK_SIZE (BYTES_PER_WORD * sizeof (long))
#define PUSH(LOW,HIGH) do {top->lo = LOW;top++->hi = HIGH;} while (0)
#define POP(LOW,HIGH)  do {LOW = (--top)->lo;HIGH = top->hi;} while (0)
#define STACK_NOT_EMPTY (stack < top)                

/* Discontinue quicksort algorithm when partition gets below this size.
   This particular magic number was chosen to work best on a Sun 4/260. */
#define MAX_THRESH 4


/* requisite prototype */

int qsortE (char *base_ptr, int total_elems, int size, int (*cmp)(char*, char*));



/* Stack node declarations used to store unfulfilled partition obligations. */
typedef struct 
{
  char *lo;
  char *hi;
  
} stack_node;

/* Order size using quicksort.  This implementation incorporates
   four optimizations discussed in Sedgewick:
   
   1. Non-recursive, using an explicit stack of pointer that store the 
      next array partition to sort.  To save time, this maximum amount 
      of space required to store an array of MAX_INT is allocated on the 
      stack.  Assuming a 32-bit integer, this needs only 32 * 
      sizeof (stack_node) == 136 bits.  Pretty cheap, actually.

   2. Choose the pivot element using a median-of-three decision tree.
      This reduces the probability of selecting a bad pivot value and 
      eliminates certain extraneous comparisons.

   3. Only quicksorts TOTAL_ELEMS / MAX_THRESH partitions, leaving
      insertion sort to order the MAX_THRESH items within each partition.  
      This is a big win, since insertion sort is faster for small, mostly
      sorted array segments.
   
   4. The larger of the two sub-partitions is always pushed onto the
      stack first, with the algorithm then concentrating on the
      smaller partition.  This *guarantees* no more than log (n)
      stack size is needed (actually O(1) in this case)! */
      
int qsortE (char *base_ptr, int total_elems, int size, int (*cmp)(char*, char*))
{
  /* Allocating SIZE bytes for a pivot buffer facilitates a better 
     algorithm below since we can do comparisons directly on the pivot. */
  char *pivot_buffer = (char *)  malloc(size);
  int   max_thresh   = MAX_THRESH * size;

  if (total_elems > MAX_THRESH)
    {
      char       *lo = base_ptr;
      char       *hi = lo + size * (total_elems - 1);
      stack_node stack[STACK_SIZE]; /* Largest size needed for 32-bit int!!! */
      stack_node *top = stack + 1;

      while (STACK_NOT_EMPTY)
        {
          char *left_ptr;
          char *right_ptr;
          {
            char *pivot = pivot_buffer;
            {
              /* Select median value from among LO, MID, and HI. Rearrange
                 LO and HI so the three values are sorted. This lowers the 
                 probability of picking a pathological pivot value and 
                 skips a comparison for both the LEFT_PTR and RIGHT_PTR. */

              char *mid = lo + size * ((hi - lo) / size >> 1);

              if (CMP (mid, lo) < 0)
                SWAP (mid, lo, size);
              if (CMP (hi, mid) < 0)
                SWAP (mid, hi, size);
              else 
                goto jump_over;
              if (CMP (mid, lo) < 0)
                SWAP (mid, lo, size);
            jump_over:
              COPY (pivot, mid, size);
              pivot = pivot_buffer;
            }
            left_ptr  = lo + size;
            right_ptr = hi - size; 

            /* Here's the famous ``collapse the walls'' section of quicksort.  
               Gotta like those tight inner loops!  They are the main reason 
               that this algorithm runs much faster than others. */
            do 
              {
                while (CMP (left_ptr, pivot) < 0)
                  left_ptr += size;

                while (CMP (pivot, right_ptr) < 0)
                  right_ptr -= size;

                if (left_ptr < right_ptr) 
                  {
                    SWAP (left_ptr, right_ptr, size);
                    left_ptr += size;
                    right_ptr -= size;
                  }
                else if (left_ptr == right_ptr) 
                  {
                    left_ptr += size;
                    right_ptr -= size;
                    break;
                  }
              } 
            while (left_ptr <= right_ptr);

          }

          /* Set up pointers for next iteration.  First determine whether
             left and right partitions are below the threshold size. If so, 
             ignore one or both.  Otherwise, push the larger partition's
             bounds on the stack and continue sorting the smaller one. */

          if ((right_ptr - lo) <= max_thresh)
            {
              if ((hi - left_ptr) <= max_thresh) /* Ignore both small partitions. */
                POP (lo, hi); 
              else              /* Ignore small left partition. */  
                lo = left_ptr;
            }
          else if ((hi - left_ptr) <= max_thresh) /* Ignore small right partition. */
            hi = right_ptr;
          else if ((right_ptr - lo) > (hi - left_ptr)) /* Push larger left partition indices. */
            {                   
              PUSH (lo, right_ptr);
              lo = left_ptr;
            }
          else                  /* Push larger right partition indices. */
            {                   
              PUSH (left_ptr, hi);
              hi = right_ptr;
            }
        }
    }

  /* Once the BASE_PTR array is partially sorted by quicksort the rest
     is completely sorted using insertion sort, since this is efficient 
     for partitions below MAX_THRESH size. BASE_PTR points to the beginning 
     of the array to sort, and END_PTR points at the very last element in
     the array (*not* one beyond it!). */

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

  {
    char *end_ptr = base_ptr + size * (total_elems - 1);
    char *run_ptr;
    char *tmp_ptr = base_ptr;
    char *thresh  = MIN (end_ptr, base_ptr + max_thresh);

    /* Find smallest element in first threshold and place it at the
       array's beginning.  This is the smallest array element,
       and the operation speeds up insertion sort's inner loop. */

    for (run_ptr = tmp_ptr + size; run_ptr <= thresh; run_ptr += size)
      if (CMP (run_ptr, tmp_ptr) < 0)
        tmp_ptr = run_ptr;

    if (tmp_ptr != base_ptr)
      SWAP (tmp_ptr, base_ptr, size);

    /* Insertion sort, running from left-hand-side up to `right-hand-side.' 
       Pretty much straight out of the original GNU qsort routine. */

    for (run_ptr = base_ptr + size; (tmp_ptr = run_ptr += size) <= end_ptr; )
      {

        while (CMP (run_ptr, tmp_ptr -= size) < 0)
          ;

        if ((tmp_ptr += size) != run_ptr)
          {
            char *trav;

            for (trav = run_ptr + size; --trav >= run_ptr;)
              {
                char c = *trav;
                char *hi, *lo;

                for (hi = lo = trav; (lo -= size) >= tmp_ptr; hi = lo)
                  *hi = *lo;
                *hi = c;
              }
          }

      }
  }
  return 1;
}
