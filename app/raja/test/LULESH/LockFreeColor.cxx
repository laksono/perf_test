// This work was performed under the auspices of the U.S. Department of Energy by
// Lawrence Livermore National Laboratory under Contract DE-AC52-07NA27344.

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
// A0 B1 A2 B3
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

/* Color the domain-set based on connectivity to the range-set */

/* all elements in each segment are independent */
/* no two segments can be executed in parallel */

void CreateLockFreeColorIndexset(RAJA::IndexSet *retVal,
                                 int *domainToRange, int numEntity,
                                 int numRangePerDomain, int numEntityRange,
                                 int *elemPermutation = 0l,
                                 int *ielemPermutation = 0l)
{
#if 0
   retVal->addRange(0, numEntity) ;
   return ;
#endif
   bool done = false ;
   bool isMarked[numEntity] ;

   int numWorkset = 0 ;
   int worksetDelim[numEntity] ;

   int worksetSize = 0 ;
   int workset[numEntity] ;

   int *rangeToDomain = new int[numEntityRange*numRangePerDomain] ;
   int *rangeToDomainCount = new int[numEntityRange] ;

   memset(rangeToDomainCount, 0, numEntityRange*sizeof(int)) ;

   /* create an inverse mapping */
   for (int i=0; i<numEntity; ++i) {
      for (int j=0 ; j<numRangePerDomain; ++j) {
         int id = domainToRange[i*numRangePerDomain+j] ;
         int idx = id*numRangePerDomain + rangeToDomainCount[id]++ ;
         if(idx > numEntityRange*numRangePerDomain ||
            rangeToDomainCount[id] > numRangePerDomain) {
            printf("foiled!\n") ;
            exit(-1) ;
         }
         rangeToDomain[idx] = i ;
      }
   }

   while (!done) {
      done = true ;

      for (int i=0; i<numEntity; ++i) {
         isMarked[i] = false ;
      }

      for (int i=0; i<worksetSize; ++i) {
         isMarked[workset[i]] = true ;
      }

      for (int i=0; i<numEntity; ++i) {

         if (isMarked[i] == false) {
            done = false ;
            if (worksetSize >= numEntity) {
               printf("foiled!\n") ;
               exit(-1) ;
            }
            workset[worksetSize++] = i ;
            for (int j=0 ; j<numRangePerDomain; ++j) {
               int id = domainToRange[i*numRangePerDomain+j] ;
               for (int k=0; k<rangeToDomainCount[id]; ++k) {
                  int idx = rangeToDomain[id*numRangePerDomain+k] ;
                  if (idx < 0 || idx >= numEntity) {
                     printf("foiled!\n") ;
                     exit(-1) ;
                  }
                  isMarked[idx] = true ;
               }
            }
         }
      }
      if (done == false) {
         worksetDelim[numWorkset++] = worksetSize ;
      }
   }

   delete [] rangeToDomainCount ;
   delete [] rangeToDomain ;

   if (worksetSize != numEntity) {
      printf("foiled!!!\n") ;
      exit (-1) ;
   }

   /* we may want to create a permutation array here */
   if (elemPermutation != 0l) {
      /* send back permutaion array, and corresponding range segments */

      memcpy(elemPermutation, workset, numEntity*sizeof(int)) ;
      if (ielemPermutation != 0l) {
         for (int i=0; i<numEntity; ++i) {
            ielemPermutation[elemPermutation[i]] = i ;
         }
      }
      int end = 0 ;
      for (int i=0; i<numWorkset; ++i) {
         int begin = end ;
         end = worksetDelim[i] ;
         retVal->addRange(begin, end) ;
      }
   }
   else {
      int end = 0 ;
      for (int i=0; i<numWorkset; ++i) {
         int begin = end ;
         end = worksetDelim[i] ;
         bool isRange = true ;
         for (int j=begin+1; j<end; ++j) {
            if (workset[j-1]+1 != workset[j]) {
               isRange = false ;
               break ;
            }
         }
         if (isRange) {
            retVal->addRange(workset[begin], workset[end-1]+1) ;
         }
         else {
            retVal->addList(&workset[begin], end-begin);
            // printf("segment %d\n", i) ;
            // for (int j=begin; j<end; ++j) {
            //    printf("%d\n", workset[j]) ;
            // }
         }
      }
   }
}

#if 0
int main(int arc, char *argv[]) {
   CreateLockFreeColorIndexset(...) ;
   return 0 ;
}
#endif


