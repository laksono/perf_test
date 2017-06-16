// This work was performed under the auspices of the U.S. Department of Energy by
// Lawrence Livermore National Laboratory under Contract DE-AC52-07NA27344.?

/*!
 ******************************************************************************
 *
 * \file
 *
 * \brief   Header file containing RAJA index set iteration template methods 
 *          using for Intel Cilk Plus execution.
 *
 *          These methods work only on platforms that support Cilk Plus. 
 *
 * \author  Rich Hornung, Center for Applied Scientific Computing, LLNL
 * \author  Jeff Keasler, Applications, Simulations And Quality, LLNL 
 *
 ******************************************************************************
 */

#ifndef RAJA_forall_cilk_icc_HXX
#define RAJA_forall_cilk_icc_HXX

#include "config.hxx"

#include "int_datatypes.hxx"

#include "execpolicy.hxx"

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>


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
 * \brief  cilk_for iteration over index range.
 *
 ******************************************************************************
 */
template <typename LOOP_BODY>
RAJA_INLINE
void forall(cilk_for_exec,
            Index_type begin, Index_type end, 
            LOOP_BODY loop_body)
{
#pragma ivdep
   cilk_for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii );
   }
}

/*!
 ******************************************************************************
 *
 * \brief  cilk_for  iteration over index range set object.
 *
 ******************************************************************************
 */
template <typename LOOP_BODY>
RAJA_INLINE
void forall(cilk_for_exec,
            const RangeISet& is,
            LOOP_BODY loop_body)
{
   const Index_type begin = is.getBegin();
   const Index_type end   = is.getEnd();
#pragma ivdep
   cilk_for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii );
   }
}

/*!
 ******************************************************************************
 *
 * \brief  cilk_for  minloc reduction over index range.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc(cilk_for_exec,
                   Index_type begin, Index_type end,
                   T* min, Index_type *loc,
                   LOOP_BODY loop_body)
{
   const int nworkers = __cilkrts_get_nworkers();

   /* Should we align these temps to coherence boundaries? */
   T  min_tmp[nworkers];
   Index_type loc_tmp[nworkers];

   for ( int i = 0; i < nworkers; ++i ) {
       min_tmp[i] = *min ;
       loc_tmp[i] = *loc ;
   }

   cilk_for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii, &min_tmp[__cilkrts_get_worker_number()], 
                     &loc_tmp[__cilkrts_get_worker_number()] );
   }

   for ( int i = 1; i < nworkers; ++i ) {
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
 * \brief  cilk_for  minloc reduction over range index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc(cilk_for_exec,
                   const RangeISet& is,
                   T* min, Index_type *loc,
                   LOOP_BODY loop_body)
{
   forall_minloc(cilk_for_exec(),
                 is.getBegin(), is.getEnd(),
                 min, loc,
                 loop_body);
}

/*!
 ******************************************************************************
 *
 * \brief  cilk_for  maxloc reduction over index range.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc(cilk_for_exec,
                   Index_type begin, Index_type end,
                   T* max, Index_type *loc,
                   LOOP_BODY loop_body)
{
   const int nworkers = __cilkrts_get_nworkers();

   /* Should we align these temps to coherence boundaries? */
   T  max_tmp[nworkers];
   Index_type loc_tmp[nworkers];

   for ( int i = 0; i < nworkers; ++i ) {
       max_tmp[i] = *max ;
       loc_tmp[i] = *loc ;
   }

   cilk_for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii, &max_tmp[__cilkrts_get_worker_number()],
                     &loc_tmp[__cilkrts_get_worker_number()] );
   }  

   for ( int i = 1; i < nworkers; ++i ) {
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
 * \brief  cilk_for  maxloc reduction over range index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc(cilk_for_exec,
                   const RangeISet& is,
                   T* max, Index_type *loc,
                   LOOP_BODY loop_body)
{
   forall_maxloc(cilk_for_exec(),
                 is.getBegin(), is.getEnd(),
                 max, loc,
                 loop_body);
}

/*!
 ******************************************************************************
 *
 * \brief  cilk_for  sum reduction over index range.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum(cilk_for_exec,
                Index_type begin, Index_type end,
                T* sum,
                LOOP_BODY loop_body)
{
   const int nworkers = __cilkrts_get_nworkers();

   /* Should we align these temps to coherence boundaries? */
   T  sum_tmp[nworkers];

   for ( int i = 0; i < nworkers; ++i ) {
       sum_tmp[i] = 0 ;
   }

   cilk_for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii, &sum_tmp[__cilkrts_get_worker_number()] );
   }

   for ( int i = 0; i < nworkers; ++i ) {
      *sum += sum_tmp[i];
   }
}

/*!
 ******************************************************************************
 *
 * \brief  cilk_for  sum reduction over range index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum(cilk_for_exec,
                const RangeISet& is,
                T* sum,
                LOOP_BODY loop_body)
{
   forall_sum(cilk_for_exec(),
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
 * \brief  cilk_for iteration over indices in indirection array.
 *
 ******************************************************************************
 */
template <typename LOOP_BODY>
RAJA_INLINE
void forall(cilk_for_exec,
            const Index_type* __restrict__ idx, const Index_type len,
            LOOP_BODY loop_body)
{
#pragma ivdep
   cilk_for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k] );
   }
}

/*!
 ******************************************************************************
 *
 * \brief  cilk_for iteration over List index set object.
 *
 ******************************************************************************
 */
template <typename LOOP_BODY>
RAJA_INLINE
void forall(cilk_for_exec,
            const ListISet& is,
            LOOP_BODY loop_body)
{
   const Index_type* __restrict__ idx = is.getIndex();
   const Index_type len = is.getLength();
#pragma ivdep
   cilk_for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k] );
   }
}

/*!
 ******************************************************************************
 *
 * \brief  cilk_for  minloc reduction over given indirection array.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc(cilk_for_exec,
                   const Index_type* __restrict__ idx, const Index_type len,
                   T* min, Index_type *loc,
                   LOOP_BODY loop_body)
{
   const int nworkers = __cilkrts_get_nworkers();

   /* Should we align these temps to coherence boundaries? */
   T  min_tmp[nworkers];
   Index_type loc_tmp[nworkers];

   for ( int i = 0; i < nworkers; ++i ) {
       min_tmp[i] = *min ;
       loc_tmp[i] = *loc ;
   }

   cilk_for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k], &min_tmp[__cilkrts_get_worker_number()],
                         &loc_tmp[__cilkrts_get_worker_number()] );
   }

   for ( int i = 1; i < nworkers; ++i ) {
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
 * \brief  cilk_for  minloc reduction over List index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc(cilk_for_exec,
                   const ListISet& is,
                   T* min, Index_type *loc,
                   LOOP_BODY loop_body)
{
   forall_minloc(cilk_for_exec(),
                 is.getIndex(), is.getLength(),
                 min, loc,
                 loop_body);
}

/*!
 ******************************************************************************
 *
 * \brief  cilk_for  maxloc reduction over given indirection array.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc(cilk_for_exec,
                   const Index_type* __restrict__ idx, const Index_type len,
                   T* max, Index_type *loc,
                   LOOP_BODY loop_body)
{
   const int nworkers = __cilkrts_get_nworkers();

   /* Should we align these temps to coherence boundaries? */
   T  max_tmp[nworkers];
   Index_type loc_tmp[nworkers];

   for ( int i = 0; i < nworkers; ++i ) {
       max_tmp[i] = *max ;
       loc_tmp[i] = *loc ;
   }

   cilk_for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k], &max_tmp[__cilkrts_get_worker_number()],
                         &loc_tmp[__cilkrts_get_worker_number()] );
   }

   for ( int i = 1; i < nworkers; ++i ) {
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
 * \brief  cilk_for  maxloc reduction over List index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc(cilk_for_exec,
                   const ListISet& is,
                   T* max, Index_type *loc,
                   LOOP_BODY loop_body)
{
   forall_maxloc(cilk_for_exec(),
                 is.getIndex(), is.getLength(),
                 max, loc,
                 loop_body);
}

/*!
 ******************************************************************************
 *
 * \brief  cilk_for  sum reduction over given indirection array.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum(cilk_for_exec,
                const Index_type* __restrict__ idx, const Index_type len,
                T* sum,
                LOOP_BODY loop_body)
{
   const int nthreads = __cilkrts_get_nworkers();

   /* Should we align these temps to coherence boundaries? */
   T  sum_tmp[nthreads];

   for ( int i = 0; i < nthreads; ++i ) {
      sum_tmp[i] = 0 ;
   }

   cilk_for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k], &sum_tmp[__cilkrts_get_worker_number()] );
   }

   for ( int i = 0; i < nthreads; ++i ) {
      *sum += sum_tmp[i];
   }
}

/*!
 ******************************************************************************
 *
 * \brief  cilk_for  sum reduction over List index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum(cilk_for_exec,
                const ListISet& is,
                T* sum,
                LOOP_BODY loop_body)
{
   forall_sum(cilk_for_exec(),
              is.getIndex(), is.getLength(),
              sum,
              loop_body);
}


//
//////////////////////////////////////////////////////////////////////
//
// The following function templates iterate over hybrid index set
// segments using cilk_for. Segment execution is defined by segment
// execution policy template parameter.
//
//////////////////////////////////////////////////////////////////////
//

/*!
 ******************************************************************************
 *
 * \brief  cilk_for iteration over segments of hybrid index set and 
 *         use execution policy template parameter to execute segments.
 *
 ******************************************************************************
 */
template <typename SEG_EXEC_POLICY_T,
          typename LOOP_BODY>
RAJA_INLINE
void forall( IndexSet::ExecPolicy<cilk_for_segit, SEG_EXEC_POLICY_T>,
             const IndexSet& is, LOOP_BODY loop_body )
{
   const int num_seg = is.getNumSegments();
   cilk_for ( int isi = 0; isi < num_seg; ++isi ) {
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

   } // iterate over parts of hybrid index set
}

/*!
 ******************************************************************************
 *
 * \brief  cilk_for minloc reduction over segments of hybrid index set and
 *         use execution policy template parameter to execute segments.
 *
 ******************************************************************************
 */
template <typename SEG_EXEC_POLICY_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc( IndexSet::ExecPolicy<cilk_for_segit, SEG_EXEC_POLICY_T>,
                    const IndexSet& is,
                    T* min, Index_type *loc,
                    LOOP_BODY loop_body )
{
   const int nworkers = __cilkrts_get_nworkers();

   /* Should we align these temps to coherence boundaries? */
   T  min_tmp[nworkers];
   Index_type loc_tmp[nworkers];

   for ( int i = 0; i < nworkers; ++i ) {
       min_tmp[i] = *min ;
       loc_tmp[i] = *loc ;
   }

   const int num_seg = is.getNumSegments();
   cilk_for ( int isi = 0; isi < num_seg; ++isi ) {
      SegmentType segtype = is.getSegmentType(isi);
      const void* iset = is.getSegmentISet(isi);

      switch ( segtype ) {

         case _Range_ : {
            forall_minloc(
               SEG_EXEC_POLICY_T(),
               *(static_cast<const RangeISet*>(iset)),
               &min_tmp[__cilkrts_get_worker_number()],
               &loc_tmp[__cilkrts_get_worker_number()],
               loop_body
            );
            break;
         }

         case _List_ : {
            forall_minloc(
               SEG_EXEC_POLICY_T(),
               *(static_cast<const ListISet*>(iset)),
               &min_tmp[__cilkrts_get_worker_number()],
               &loc_tmp[__cilkrts_get_worker_number()],
               loop_body
            );
            break;
         }

         default : {
         }

      }  // switch on segment type

   } // iterate over segments of hybrid index set


   for ( int i = 1; i < nworkers; ++i ) {
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
 * \brief  cilk_for maxloc  reduction over segments of hybrid index set and
 *         use execution policy template parameter to execute segments.
 *
 ******************************************************************************
 */
template <typename SEG_EXEC_POLICY_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc( IndexSet::ExecPolicy<cilk_for_segit, SEG_EXEC_POLICY_T>,
                    const IndexSet& is,
                    T* max, Index_type *loc,
                    LOOP_BODY loop_body )
{
   const int nworkers = __cilkrts_get_nworkers();

   /* Should we align these temps to coherence boundaries? */
   T  max_tmp[nworkers];
   Index_type loc_tmp[nworkers];

   for ( int i = 0; i < nworkers; ++i ) {
       max_tmp[i] = *max ;
       loc_tmp[i] = *loc ;
   }

   const int num_seg = is.getNumSegments();
   cilk_for ( int isi = 0; isi < num_seg; ++isi ) {
      SegmentType segtype = is.getSegmentType(isi);
      const void* iset = is.getSegmentISet(isi);

      switch ( segtype ) {

         case _Range_ : {
            forall_maxloc(
               SEG_EXEC_POLICY_T(),
               *(static_cast<const RangeISet*>(iset)),
               &max_tmp[__cilkrts_get_worker_number()],
               &loc_tmp[__cilkrts_get_worker_number()],
               loop_body
            );
            break;
         }

         case _List_ : {
            forall_maxloc(
               SEG_EXEC_POLICY_T(),
               *(static_cast<const ListISet*>(iset)),
               &max_tmp[__cilkrts_get_worker_number()],
               &loc_tmp[__cilkrts_get_worker_number()],
               loop_body
            );
            break;
         }

         default : {
         }

      }  // switch on segment type

   } // iterate over segments of hybrid index set

   for ( int i = 1; i < nworkers; ++i ) {
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
 * \brief  cilk_for sum  reduction over segments of hybrid index set and
 *         use execution policy template parameter to execute segments.
 *
 ******************************************************************************
 */
template <typename SEG_EXEC_POLICY_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum( IndexSet::ExecPolicy<cilk_for_segit, SEG_EXEC_POLICY_T>,
                 const IndexSet& is,
                 T* sum,
                 LOOP_BODY loop_body )
{
   const int nthreads = __cilkrts_get_nworkers();

   /* Should we align these temps to coherence boundaries? */
   T  sum_tmp[nthreads];

   for ( int i = 0; i < nthreads; ++i ) {
      sum_tmp[i] = 0 ;
   }

   const int num_seg = is.getNumSegments();
   cilk_for ( int isi = 0; isi < num_seg; ++isi ) {
      SegmentType segtype = is.getSegmentType(isi);
      const void* iset = is.getSegmentISet(isi);

      switch ( segtype ) {

         case _Range_ : {
            forall_sum(
               SEG_EXEC_POLICY_T(),
               *(static_cast<const RangeISet*>(iset)),
               &sum_tmp[__cilkrts_get_worker_number()],
               loop_body
            );
            break;
         }

         case _List_ : {
            forall_sum(
               SEG_EXEC_POLICY_T(),
               *(static_cast<const ListISet*>(iset)),
               &sum_tmp[__cilkrts_get_worker_number()],
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


}  // closing brace for RAJA namespace

#endif  // closing endif for header file include guard

