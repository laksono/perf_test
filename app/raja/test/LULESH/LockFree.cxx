// This work was performed under the auspices of the U.S. Department of Energy by
// Lawrence Livermore National Laboratory under Contract DE-AC52-07NA27344.

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include "RAJA.hxx"

// RAJA_THREADS_LIGHT_WEIGHT  -- number of hardware threads/core

// PROFITABLE_ENTITY_THRESHOLD  the minimum entities per thread
//                              so that OMP overhead does not
//                              overwhelm thread performance
//
// LOCK_FREE_SETS               The minimum number of sets per thread
//                              needed to make lock-free profitable

// Example segments in 1d
//
// A0 B1 A2 B3  (letter denotes 'color, number denotes segment id)
//
// First we permute segments for the OMP static schedule:
//
// setenv OMP_NUM_THREADS 2
// A0 A2 B1 B3
//
// There is an assumption that the static schedule will bind a chunk
// to a thread in a deterministic way :
//
// omp forall schedule (static, 1):
//
// setenv OMP_NUM_THREADS 2
// thread   0  1  0  1
// segment  A0 A2 B1 B3
//
// In words, thread 0 *must* execute the even segments in order, and
// thread 1 *must* execute the odd segments in order.
//
//

/* lock-free indexsets are designed to be used with coarse-grained */
/* OMP iteration policies.  The "lock-free" here assumes interactions */
/* among the cell-complex associated with the space being partitioned */
/* are "tightly bound" */


#define PROFITABLE_ENTITY_THRESHOLD 100

/* Planar division */

void CreateLockFreeBlockIndexset(RAJA::IndexSet *retVal,
                                 int fastDim, int midDim, int slowDim)
{
   // RAJA::IndexSet *retVal = new RAJA::IndexSet() ;
   int numThreads = omp_get_max_threads() ;

   // printf("Lock-free created\n") ;

   if ((midDim | slowDim) == 0)  /* 1d mesh */
   {
      if (fastDim/PROFITABLE_ENTITY_THRESHOLD <= 1) {
          // printf("%d %d\n", 0, fastDim) ;
          retVal->addRange(0, fastDim) ;
      }
      else {
         /* This just sets up the schedule -- a truly safe */
         /* execution of this schedule would require a check */
         /* for completion of dependent threads before execution. */

         /* We might want to force one thread if the */
         /* profitability ratio is really bad, but for */
         /* now use the brain dead approach. */
         int numSegments = numThreads*3 ;
         for (int lane=0; lane<3; ++lane) {
            for (int i=lane; i<numSegments; i += 3)
            {
               int start = i*fastDim/numSegments ;
               int end   = (i+1)*fastDim/numSegments ;
               // printf("%d %d\n", start, end) ;
               retVal->addRange(start, end) ;
            }
         }
      }
   }
   else if (slowDim == 0) /* 2d mesh */
   {
      int rowsPerSegment = midDim/(3*numThreads) ;
      if (rowsPerSegment == 0) {
          // printf("%d %d\n", 0, fastDim*midDim) ;
          retVal->addRange(0, fastDim*midDim) ;
      }
      else {
         /* This just sets up the schedule -- a truly safe */
         /* execution of this schedule would require a check */
         /* for completion of dependent threads before execution. */

         /* We might want to force one thread if the */
         /* profitability ratio is really bad, but for */
         /* now use the brain dead approach. */
         int numSegments = 3*numThreads ;
         int segmentSize = midDim / numThreads ;
         int segmentThresh = midDim % numSegments ;
         for (int lane=0; lane<3; ++lane) {
            for (int i=0; i<numThreads; ++i)
            {
               int startRow = i*midDim/numThreads ;
               int endRow = (i+1)*midDim/numThreads ;
               int start = startRow*fastDim ;
               int end   = endRow*fastDim ;
               int len = end - start ;
               // printf("%d %d\n", start + (lane  )*len/3,
               //                   start + (lane+1)*len/3  ) ;
               retVal->addRange(start + (lane  )*len/3,
                                       start + (lane+1)*len/3  ) ;
            }
         }
      }
   }
   else { /* 3d mesh */
      /* Need at least 3 full planes per thread */
      /* and at least one segment per plane */
      const int segmentsPerThread = 2 ;
      int rowsPerSegment = slowDim/(segmentsPerThread*numThreads) ;
      if (rowsPerSegment == 0) {
          // printf("%d %d\n", 0, fastDim*midDim*slowDim) ;
          retVal->addRange(0, fastDim*midDim*slowDim) ;
          printf("Failure to create lockfree indexset\n") ;
          exit(-1) ;
      }
      else {
         /* This just sets up the schedule -- a truly safe */
         /* execution of this schedule would require a check */
         /* for completion of dependent threads before execution. */

         /* We might want to force one thread if the */
         /* profitability ratio is really bad, but for */
         /* now use the brain dead approach. */
         int numSegments = segmentsPerThread*numThreads ;
         int segmentSize = slowDim / numThreads ;
         int segmentThresh = slowDim % numSegments ;
         for (int lane=0; lane<segmentsPerThread; ++lane) {
            for (int i=0; i<numThreads; ++i)
            {
               int startPlane = i*slowDim/numThreads ;
               int endPlane = (i+1)*slowDim/numThreads ;
               int start = startPlane*fastDim*midDim ;
               int end   = endPlane*fastDim*midDim ;
               int len = end - start ;
               // printf("%d %d\n", start + (lane  )*len/segmentsPerThread,
               //                   start + (lane+1)*len/segmentsPerThread  );
               retVal->addRange(start + (lane  )*len/segmentsPerThread,
                                       start + (lane+1)*len/segmentsPerThread);
            }
         }

         /* Set up dependency graph */

         if (segmentsPerThread == 1) {
            /* This dependency graph should impose serialization */
            for (int i=0; i<numThreads; ++i) {
               retVal->segmentSemaphoreValue(i) = (( i == 0) ? 0 : 1) ;
               retVal->segmentSemaphoreReloadValue(i) = ((i == 0) ? 0 : 1) ;
               if (i != numThreads-1) {
                  retVal->segmentSemaphoreNumDepTasks(i) = 1 ;
                  retVal->segmentSemaphoreDepTask(i, 0) = i+1 ;
               }
            }
         }
         else {
            /* This dependency graph relies on omp schedule(static, 1) */
            /* but allows a minimumal set of dependent tasks be used */
            int borderSeg = numThreads*(segmentsPerThread-1) ;
            for (int i=1; i<numThreads; ++i) {
               retVal->segmentSemaphoreReloadValue(i) = 1 ;
               retVal->segmentSemaphoreNumDepTasks(i) = 1 ;
               retVal->segmentSemaphoreDepTask(i, 0) 
                        = borderSeg + i - 1 ;

               retVal->segmentSemaphoreValue(borderSeg+i-1) = 1 ;
               retVal->segmentSemaphoreReloadValue(borderSeg+i-1) = 1 ;
               retVal->segmentSemaphoreNumDepTasks(borderSeg+i-1) = 1 ;
               retVal->segmentSemaphoreDepTask(borderSeg+i-1, 0) = i ;
            }
         }
      }
   }

   /* Print the dependency schedule for segments */
   if (0) 
   {
      /* summarize dependency schedule */
      int numSeg = retVal->getNumSegments();
      for(int ii=0; ii<numSeg; ++ii) {
         const RAJA::RangeISet *ris = 
            static_cast<const RAJA::RangeISet*>(retVal->getSegmentISet(ii)) ;
         printf("%d (%7d,%7d) init=%d, reload=%d",
                ii, ris->getBegin(), ris->getEnd(),
                retVal->segmentSemaphoreValue(ii),
                retVal->segmentSemaphoreReloadValue(ii)) ;
         int numDepTasks = retVal->segmentSemaphoreNumDepTasks(ii) ;
         if (numDepTasks != 0) {
            printf(", dep=") ;
            for (int jj=0; jj<numDepTasks; ++jj) {
               printf("%d ", retVal->segmentSemaphoreDepTask(ii, jj)) ;
            }
         }
         printf("\n") ;
      }
   }

   // return retVal ;
}

#if 0
int main(int arc, char *argv[]) {
   CreateLockFreeBlockIndexset(atoi(argv[1]), atoi(argv[2]), atoi(argv[3])) ;
   return 0 ;
}
#endif


