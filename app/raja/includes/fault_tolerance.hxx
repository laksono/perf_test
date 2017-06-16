// This work was performed under the auspices of the U.S. Department of Energy by
// Lawrence Livermore National Laboratory under Contract DE-AC52-07NA27344.?

/*!
 ******************************************************************************
 *
 * \file
 *
 * \brief   Header file containing RAJA Fault Tolerance macros.
 *          RAJA Fault Tolerance only works when all the lambda
 *          functions passed to RAJA are in idempotent form,
 *          meaning there are no persistent variables in the
 *          lambda that have read-write semantics. In other words,
 *          persistent lambda function variables must be consistently
 *          used as read-only or write-only within the lambda scope.
 *
 *          These macros are designed to cooperate with an external
 *          signal handler that sets a global variable, fault_type,
 *          when a fault occurs. fult_type must be initialized to zero.
 *
 * \author  Rich Hornung, Center for Applied Scientific Computing, LLNL
 * \author  Jeff Keasler, Applications, Simulations And Quality, LLNL
 *
 ******************************************************************************
 */

#ifndef RAJA_fault_tolerance_HXX
#define RAJA_fault_tolerance_HXX

#ifdef RAJA_USE_FT

#ifdef RAJA_REPORT_FT
#include "cycle.h"
#include <stdio.h>

#define RAJA_FT_BEGIN \
   extern volatile int fault_type ; \
   bool repeat ; \
   bool do_time = false ; \
   ticks start = 0, stop = 0 ; \
   if ( fault_type != 0) { \
      printf("Uncaught fault %d\n", fault_type) ; \
      fault_type = 0 ; \
   } \
   do { \
      repeat = false ; \
      if (do_time) { \
         start = getticks() ; \
      }

#define RAJA_FT_END \
      if (do_time) { \
         stop = getticks() ; \
         printf("recoverable fault clock cycles = %16f\n", elapsed(stop, start)) ; \
         do_time = false ; \
         fault_type = 0 ; \
      } \
      if (fault_type < 0) { \
         printf("Unrecoverable fault (restart penalty)\n") ; \
         fault_type = 0 ; \
      } \
      if (fault_type > 0) { \
         /* invalidate cache */ \
         repeat = true ; \
         do_time = true ; \
      } \
   } while (repeat == true) ;

#else
#define RAJA_FT_BEGIN \
   extern volatile int fault_type ; \
   bool repeat ; \
   if ( fault_type == 0) { \
      do { \
         repeat = false ;

#define RAJA_FT_END \
         if (fault_type > 0) { \
            /* invalidate cache */ \
            repeat = true ; \
            fault_type = 0 ; \
         } \
      } while (repeat == true) ; \
   } \
   else { \
      fault_type = 0 ; /* ignore for the simulation */ \
   }

#endif // RAJA_REPORT_FT


#else

#define RAJA_FT_BEGIN

#define RAJA_FT_END

#endif // RAJA_USE_FT

#endif  // closing endif for header file include guard

