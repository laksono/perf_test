// This work was performed under the auspices of the U.S. Department of Energy by
// Lawrence Livermore National Laboratory under Contract DE-AC52-07NA27344.?

/*!
 ******************************************************************************
 *
 * \file
 *
 * \brief   Header file containing RAJA index set iteration template 
 *          methods for OpenMP execution policies.
 *
 *          These methods should work on any platform.
 *
 * \author  Rich Hornung, Center for Applied Scientific Computing, LLNL
 * \author  Jeff Keasler, Applications, Simulations And Quality, LLNL
 *
 ******************************************************************************
 */

#ifndef RAJA_forall_omp_any_HXX
#define RAJA_forall_omp_any_HXX

#include "config.hxx"

#include "int_datatypes.hxx"

#include "execpolicy.hxx"

#include "fault_tolerance.hxx"

#include <sched.h>

#include <omp.h>


namespace RAJA {


//
//////////////////////////////////////////////////////////////////////
//
// Function templates that iterate over range index sets.
//
//////////////////////////////////////////////////////////////////////
//

/*!
 ******************************************************************************
 *
 * \brief  omp parallel for iteration over index range.
 *
 ******************************************************************************
 */
template <typename LOOP_BODY>
RAJA_INLINE
void forall(omp_parallel_for_exec,
            Index_type begin, Index_type end, 
            LOOP_BODY loop_body)
{

   RAJA_FT_BEGIN ;

#pragma omp parallel for
   for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii );
   }

   RAJA_FT_END ;
}

/*!
 ******************************************************************************
 *
 * \brief  omp parallel for iteration over index range set object.
 *
 ******************************************************************************
 */
template <typename LOOP_BODY>
RAJA_INLINE
void forall(omp_parallel_for_exec,
            const RangeISet& is,
            LOOP_BODY loop_body)
{
   const Index_type begin = is.getBegin();
   const Index_type end   = is.getEnd();

   RAJA_FT_BEGIN ;

#pragma omp parallel for
   for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii );
   }

   RAJA_FT_END ;
}

/*!
 ******************************************************************************
 *
 * \brief  omp parallel for minloc reduction over index range.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc(omp_parallel_for_exec,
                   Index_type begin, Index_type end,
                   T* min, Index_type *loc,
                   LOOP_BODY loop_body)
{
   const int nthreads = omp_get_max_threads();

   /* Should we align these temps to coherence boundaries? */
   T  min_tmp[nthreads];
   Index_type loc_tmp[nthreads];

   RAJA_FT_BEGIN

   for ( int i = 0; i < nthreads; ++i ) {
       min_tmp[i] = *min ;
       loc_tmp[i] = *loc ;
   }

#pragma omp parallel for
   for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii, &min_tmp[omp_get_thread_num()],
                     &loc_tmp[omp_get_thread_num()] );
   }

   for ( int i = 1; i < nthreads; ++i ) {
      if ( min_tmp[i] < min_tmp[0] ) {
         min_tmp[0] = min_tmp[i];
         loc_tmp[0] = loc_tmp[i];
      }
   }

   RAJA_FT_END ;

   *min = min_tmp[0] ;
   *loc = loc_tmp[0] ;
}

/*!
 ******************************************************************************
 *
 * \brief  omp parallel for minloc reduction over range index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc(omp_parallel_for_exec,
                   const RangeISet& is,
                   T* min, Index_type *loc,
                   LOOP_BODY loop_body)
{
   forall_minloc(omp_parallel_for_exec(),
                 is.getBegin(), is.getEnd(),
                 min, loc,
                 loop_body);
}

/*!
 ******************************************************************************
 *
 * \brief  omp parallel for maxloc reduction over index range.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc(omp_parallel_for_exec,
                   Index_type begin, Index_type end,
                   T* max, Index_type *loc,
                   LOOP_BODY loop_body)
{
   const int nthreads = omp_get_max_threads();

   /* Should we align these temps to coherence boundaries? */
   T  max_tmp[nthreads];
   Index_type loc_tmp[nthreads];

   RAJA_FT_BEGIN ;

   for ( int i = 0; i < nthreads; ++i ) {
       max_tmp[i] = *max ;
       loc_tmp[i] = *loc ;
   }

#pragma omp parallel for 
   for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii, &max_tmp[omp_get_thread_num()],
                     &loc_tmp[omp_get_thread_num()] );
   }

   for ( int i = 1; i < nthreads; ++i ) {
      if ( max_tmp[i] > max_tmp[0] ) {
         max_tmp[0] = max_tmp[i];
         loc_tmp[0] = loc_tmp[i];
      }
   }

   RAJA_FT_END ;

   *max = max_tmp[0] ;
   *loc = loc_tmp[0] ;
}

/*!
 ******************************************************************************
 *
 * \brief  omp parallel for maxloc reduction over range index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc(omp_parallel_for_exec,
                   const RangeISet& is,
                   T* max, Index_type *loc,
                   LOOP_BODY loop_body)
{
   forall_maxloc(omp_parallel_for_exec(),
                 is.getBegin(), is.getEnd(),
                 max, loc,
                 loop_body);
}

/*!
 ******************************************************************************
 *
 * \brief  omp parallel for sum reduction over index range.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum(omp_parallel_for_exec,
                Index_type begin, Index_type end,
                T* sum,
                LOOP_BODY loop_body)
{
   const int nthreads = omp_get_max_threads();

   /* Should we align these temps to coherence boundaries? */
   T  sum_tmp[nthreads];

   RAJA_FT_BEGIN ;

   for ( int i = 0; i < nthreads; ++i ) {
      sum_tmp[i] = 0 ;
   }

#pragma omp parallel for
   for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii, &sum_tmp[omp_get_thread_num()] );
   }

   RAJA_FT_END ;

   for ( int i = 0; i < nthreads; ++i ) {
      *sum += sum_tmp[i];
   }
}

/*!
 ******************************************************************************
 *
 * \brief  omp parallel for sum reduction over range index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum(omp_parallel_for_exec,
                const RangeISet& is,
                T* sum, 
                LOOP_BODY loop_body)
{
   forall_sum(omp_parallel_for_exec(),
              is.getBegin(), is.getEnd(),
              sum,
              loop_body);
}


//
//////////////////////////////////////////////////////////////////////
//
// Function templates that iterate over List index sets.
//
//////////////////////////////////////////////////////////////////////
//

/*!
 ******************************************************************************
 *
 * \brief  omp parallel for iteration over indirection array.
 *
 ******************************************************************************
 */
template <typename LOOP_BODY>
RAJA_INLINE
void forall(omp_parallel_for_exec,
            const Index_type* __restrict__ idx, const Index_type len,
            LOOP_BODY loop_body)
{

   RAJA_FT_BEGIN ;

#pragma novector
#pragma omp parallel for
   for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k] );
   }

   RAJA_FT_END ;
}

/*!
 ******************************************************************************
 *
 * \brief  omp parallel for iteration over List index set object.
 *
 ******************************************************************************
 */
template <typename LOOP_BODY>
RAJA_INLINE
void forall(omp_parallel_for_exec,
            const ListISet& is,
            LOOP_BODY loop_body)
{
   const Index_type* __restrict__ idx = is.getIndex();
   const Index_type len = is.getLength();

   RAJA_FT_BEGIN ;

#pragma novector
#pragma omp parallel for
   for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k] );
   }

   RAJA_FT_END ;
}


/*!
 ******************************************************************************
 *
 * \brief  omp parallel for minloc reduction over given indirection array.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc(omp_parallel_for_exec,
                   const Index_type* __restrict__ idx, const Index_type len,
                   T* min, Index_type *loc,
                   LOOP_BODY loop_body)
{
   const int nthreads = omp_get_max_threads();

   /* Should we align these temps to coherence boundaries? */
   T  min_tmp[nthreads];
   Index_type loc_tmp[nthreads];

   RAJA_FT_BEGIN ;

   for ( int i = 0; i < nthreads; ++i ) {
       min_tmp[i] = *min ;
       loc_tmp[i] = *loc ;
   }

#pragma novector
#pragma omp parallel for
   for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k], &min_tmp[omp_get_thread_num()], 
                         &loc_tmp[omp_get_thread_num()] );
   }

   for ( int i = 1; i < nthreads; ++i ) {
      if ( min_tmp[i] < min_tmp[0] ) {
         min_tmp[0] = min_tmp[i];
         loc_tmp[0] = loc_tmp[i];
      }
   }

   RAJA_FT_END ;

   *min = min_tmp[0] ;
   *loc = loc_tmp[0] ;
}

/*!
 ******************************************************************************
 *
 * \brief  omp parallel for minloc reduction over List index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc(omp_parallel_for_exec,
                   const ListISet& is,
                   T* min, Index_type *loc,
                   LOOP_BODY loop_body)
{
   forall_minloc(omp_parallel_for_exec(),
                 is.getIndex(), is.getLength(),
                 min, loc,
                 loop_body);
}

/*!
 ******************************************************************************
 *
 * \brief  omp parallel for maxloc reduction over given indirection array.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc(omp_parallel_for_exec,
                   const Index_type* __restrict__ idx, const Index_type len,
                   T* max, Index_type *loc,
                   LOOP_BODY loop_body)
{
   const int nthreads = omp_get_max_threads();

   /* Should we align these temps to coherence boundaries? */
   T  max_tmp[nthreads];
   Index_type loc_tmp[nthreads];

   RAJA_FT_BEGIN ;

   for ( int i = 0; i < nthreads; ++i ) {
       max_tmp[i] = *max ;
       loc_tmp[i] = *loc ;
   }

#pragma novector
#pragma omp parallel for
   for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k], &max_tmp[omp_get_thread_num()],
                         &loc_tmp[omp_get_thread_num()] );
   }

   for ( int i = 1; i < nthreads; ++i ) {
      if ( max_tmp[i] > max_tmp[0] ) {
         max_tmp[0] = max_tmp[i];
         loc_tmp[0] = loc_tmp[i];
      }
   }

   RAJA_FT_END ;

   *max = max_tmp[0] ;
   *loc = loc_tmp[0] ;
}

/*!
 ******************************************************************************
 *
 * \brief  omp parallel for maxloc reduction over List index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc(omp_parallel_for_exec,
                   const ListISet& is,
                   T* max, Index_type *loc,
                   LOOP_BODY loop_body)
{
   forall_maxloc(omp_parallel_for_exec(),
                 is.getIndex(), is.getLength(),
                 max, loc,
                 loop_body);
}

/*!
 ******************************************************************************
 *
 * \brief  omp parallel for sum reduction over given indirection array.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum(omp_parallel_for_exec,
                const Index_type* __restrict__ idx, const Index_type len,
                T* sum,
                LOOP_BODY loop_body)
{
   const int nthreads = omp_get_max_threads();

   /* Should we align these temps to coherence boundaries? */
   T  sum_tmp[nthreads];

   RAJA_FT_BEGIN ;
   for ( int i = 0; i < nthreads; ++i ) {
      sum_tmp[i] = 0 ;
   }

#pragma novector
#pragma omp parallel for
   for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k], &sum_tmp[omp_get_thread_num()] );
   }

   RAJA_FT_END ;

   for ( int i = 0; i < nthreads; ++i ) {
      *sum += sum_tmp[i];
   }
}

/*!
 ******************************************************************************
 *
 * \brief  omp parallel for sum reduction over List index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum(omp_parallel_for_exec,
                const ListISet& is,
                T* sum,
                LOOP_BODY loop_body)
{
   forall_sum(omp_parallel_for_exec(),
              is.getIndex(), is.getLength(),
              sum,
              loop_body);
}


//
//////////////////////////////////////////////////////////////////////
//
// The following function templates iterate over hybrid index set
// segments using omp execution policies.
//
//////////////////////////////////////////////////////////////////////
//

/*!
 ******************************************************************************
 *
 * \brief  Iterate over hybrid index set segments using omp parallel for 
 *         execution policy and use execution policy template parameter 
 *         for segments.
 *
 ******************************************************************************
 */
template <typename SEG_EXEC_POLICY_T,
          typename LOOP_BODY>
RAJA_INLINE
void forall( IndexSet::ExecPolicy<omp_parallel_for_segit, SEG_EXEC_POLICY_T>,
             const IndexSet& iss, LOOP_BODY loop_body )
{
   IndexSet &is = (*const_cast<IndexSet *>(&iss)) ;

   const int num_seg = is.getNumSegments();

#pragma omp parallel for schedule(static, 1)
   for ( int isi = 0; isi < num_seg; ++isi ) {
      volatile int *semMem =
         reinterpret_cast<volatile int *>(&is.segmentSemaphoreValue(isi)) ;
      
      while(*semMem != 0) {
         /* spin or (better) sleep here */ ;
        // printf("%d ", *semMem) ;
        // sleep(1) ;
        // volatile int spin ;
        // for (spin = 0; spin<1000; ++spin) {
        //    spin = spin ;
        // }
        sched_yield() ;
      }

      SegmentType segtype = is.getSegmentType(isi);
      const void* iset = is.getSegmentISet(isi);

      switch ( segtype ) {

         case _Range_ : {
            forall(
               SEG_EXEC_POLICY_T(),
               *(static_cast<const RangeISet*>(iset)),
               loop_body
            );
            break;
         }

         case _List_ : {
            forall(
               SEG_EXEC_POLICY_T(),
               *(static_cast<const ListISet*>(iset)),
               loop_body
            );
            break;
         }

         default : {
         }

      }  // switch on segment type

      if (is.segmentSemaphoreReloadValue(isi) != 0) {
         is.segmentSemaphoreValue(isi) = is.segmentSemaphoreReloadValue(isi) ;
      }

      if (is.segmentSemaphoreNumDepTasks(isi) != 0) {
         for (int ii=0; ii<is.segmentSemaphoreNumDepTasks(isi); ++ii) {
           /* alternateively, we could get the return value of this call */
           /* and actively launch the task if we are the last depedent task. */
           /* in that case, we would not need the semaphore spin loop above */
           int seg = is.segmentSemaphoreDepTask(isi, ii) ;
           __sync_fetch_and_sub(&is.segmentSemaphoreValue(seg), 1) ;
         }
      }

   } // iterate over segments of hybrid index set
}

/*!
 ******************************************************************************
 *
 * \brief  Minloc operation that iterates over hybrid index set segments 
 *         using omp parallel for execution policy and uses execution 
 *         policy template parameter to execute segments.
 *
 ******************************************************************************
 */
template <typename SEG_EXEC_POLICY_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc( IndexSet::ExecPolicy<omp_parallel_for_segit, SEG_EXEC_POLICY_T>,
                    const IndexSet& is, 
                    T* min, Index_type *loc,
                    LOOP_BODY loop_body )
{
   const int nthreads = omp_get_max_threads();

   /* Should we align these temps to coherence boundaries? */
   T  min_tmp[nthreads];
   Index_type loc_tmp[nthreads];

   for ( int i = 0; i < nthreads; ++i ) {
       min_tmp[i] = *min ;
       loc_tmp[i] = *loc ;
   }

   const int num_seg = is.getNumSegments();

#pragma omp parallel for 
   for ( int isi = 0; isi < num_seg; ++isi ) {
      SegmentType segtype = is.getSegmentType(isi);
      const void* iset = is.getSegmentISet(isi);

      switch ( segtype ) {

         case _Range_ : {
            forall_minloc(
               SEG_EXEC_POLICY_T(),
               *(static_cast<const RangeISet*>(iset)),
               &min_tmp[omp_get_thread_num()], 
               &loc_tmp[omp_get_thread_num()],
               loop_body
            );
            break;
         }

         case _List_ : {
            forall_minloc(
               SEG_EXEC_POLICY_T(),
               *(static_cast<const ListISet*>(iset)),
               &min_tmp[omp_get_thread_num()], 
               &loc_tmp[omp_get_thread_num()],
               loop_body
            );
            break;
         }

         default : {
         }

      }  // switch on segment type

   } // iterate over segments of hybrid index set

   for ( int i = 1; i < nthreads; ++i ) {
      if ( min_tmp[i] < min_tmp[0] ) {
         min_tmp[0] = min_tmp[i];
         loc_tmp[0] = loc_tmp[i];
      }
   }

   *min = min_tmp[0] ;
   *loc = loc_tmp[0] ;
}

/*!
 ******************************************************************************
 *
 * \brief  Maxloc operation that iterates over hybrid index set segments 
 *         using omp parallel for execution policy and uses execution 
 *         policy template parameter to execute segments.
 *
 ******************************************************************************
 */
template <typename SEG_EXEC_POLICY_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc( IndexSet::ExecPolicy<omp_parallel_for_segit, SEG_EXEC_POLICY_T>,
                    const IndexSet& is, 
                    T* max, Index_type *loc,
                    LOOP_BODY loop_body )
{
   const int nthreads = omp_get_max_threads();

   /* Should we align these temps to coherence boundaries? */
   T  max_tmp[nthreads];
   Index_type loc_tmp[nthreads];

   for ( int i = 0; i < nthreads; ++i ) {
       max_tmp[i] = *max ;
       loc_tmp[i] = *loc ;
   }

   const int num_seg = is.getNumSegments();

#pragma omp parallel for
   for ( int isi = 0; isi < num_seg; ++isi ) {
      SegmentType segtype = is.getSegmentType(isi);
      const void* iset = is.getSegmentISet(isi);

      switch ( segtype ) {

         case _Range_ : {
            forall_maxloc(
               SEG_EXEC_POLICY_T(),
               *(static_cast<const RangeISet*>(iset)),
               &max_tmp[omp_get_thread_num()], 
               &loc_tmp[omp_get_thread_num()],
               loop_body
            );
            break;
         }

         case _List_ : {
            forall_maxloc(
               SEG_EXEC_POLICY_T(),
               *(static_cast<const ListISet*>(iset)),
               &max_tmp[omp_get_thread_num()], 
               &loc_tmp[omp_get_thread_num()],
               loop_body
            );
            break;
         }

         default : {
         }

      }  // switch on segment type

   } // iterate over segments of hybrid index set

   for ( int i = 1; i < nthreads; ++i ) {
      if ( max_tmp[i] > max_tmp[0] ) {
         max_tmp[0] = max_tmp[i];
         loc_tmp[0] = loc_tmp[i];
      }
   }

   *max = max_tmp[0] ;
   *loc = loc_tmp[0] ;
}

/*!
 ******************************************************************************
 *
 * \brief  Sum operation that iterates over hybrid index set segments
 *         using omp parallel for execution policy and uses execution
 *         policy template parameter to execute segments.
 *
 ******************************************************************************
 */
template <typename SEG_EXEC_POLICY_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum( IndexSet::ExecPolicy<omp_parallel_for_segit, SEG_EXEC_POLICY_T>,
                 const IndexSet& is,
                 T* sum,
                 LOOP_BODY loop_body )
{
   const int nthreads = omp_get_max_threads();

   /* Should we align these temps to coherence boundaries? */
   T  sum_tmp[nthreads];

   for ( int i = 0; i < nthreads; ++i ) {
       sum_tmp[i] = 0 ;
   }

   const int num_seg = is.getNumSegments();

#pragma omp parallel for
   for ( int isi = 0; isi < num_seg; ++isi ) {
      SegmentType segtype = is.getSegmentType(isi);
      const void* iset = is.getSegmentISet(isi);

      switch ( segtype ) {

         case _Range_ : {
            forall_sum(
               SEG_EXEC_POLICY_T(),
               *(static_cast<const RangeISet*>(iset)),
               &sum_tmp[omp_get_thread_num()],
               loop_body
            );
            break;
         }

         case _List_ : {
            forall_sum(
               SEG_EXEC_POLICY_T(),
               *(static_cast<const ListISet*>(iset)),
               &sum_tmp[omp_get_thread_num()],
               loop_body
            );
            break;
         }

         default : {
         }

      }  // switch on segment type

   } // iterate over segments of hybrid index set

   for ( int i = 0; i < nthreads; ++i ) {
      *sum += sum_tmp[i];
   }
}

#include "forall_segments.hxx"

RAJA_INLINE
void atomicAdd(double &accum, double value) {
#pragma omp atomic
   accum += value ;
}

}  // closing brace for RAJA namespace

#endif  // closing endif for header file include guard
