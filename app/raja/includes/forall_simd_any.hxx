// This work was performed under the auspices of the U.S. Department of Energy by
// Lawrence Livermore National Laboratory under Contract DE-AC52-07NA27344.?

/*!
 ******************************************************************************
 *
 * \file
 *
 * \brief   Header file containing RAJA index set iteration template
 *          methods for SIMD execution.
 *
 *          These methods should work on any platform.
 *
 * \author  Rich Hornung, Center for Applied Scientific Computing, LLNL
 * \author  Jeff Keasler, Applications, Simulations And Quality, LLNL
 *
 ******************************************************************************
 */

#ifndef RAJA_forall_simd_any_HXX
#define RAJA_forall_simd_any_HXX

#include "config.hxx"

#include "int_datatypes.hxx"

#include "execpolicy.hxx"

#include "fault_tolerance.hxx"

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
 * \brief  SIMD iteration over index range.
 *         No assumption made on data alignment.
 *
 ******************************************************************************
 */
template <typename LOOP_BODY>
RAJA_INLINE
void forall(simd_exec,
            Index_type begin, Index_type end, 
            LOOP_BODY loop_body)
{
   RAJA_FT_BEGIN ;

RAJA_SIMD
   for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii );
   }

   RAJA_FT_END ;
}

/*!
 ******************************************************************************
 *
 * \brief  SIMD iteration over index range set object.
 *
 ******************************************************************************
 */
template <typename LOOP_BODY>
RAJA_INLINE
void forall(simd_exec,
            const RangeISet& is,
            LOOP_BODY loop_body)
{
   const Index_type begin = is.getBegin();
   const Index_type end   = is.getEnd();

   RAJA_FT_BEGIN ;

RAJA_SIMD
   for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii );
   }

   RAJA_FT_END ;
}

/*!
 ******************************************************************************
 *
 * \brief  SIMD minloc reduction over index range.
 *         No assumption made on data alignment.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc(simd_exec,
                   Index_type begin, Index_type end,
                   T* min, Index_type* loc,
                   LOOP_BODY loop_body)
{
   RAJA_FT_BEGIN ;

RAJA_SIMD
   for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii, min, loc );
   }

   RAJA_FT_END ;
}

/*!
 ******************************************************************************
 *
 * \brief  SIMD minloc reduction over range index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc(simd_exec,
                   const RangeISet& is,
                   T* min, Index_type* loc,
                   LOOP_BODY loop_body)
{
   const Index_type begin = is.getBegin();
   const Index_type end   = is.getEnd();

   RAJA_FT_BEGIN ;

RAJA_SIMD
   for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii, min, loc );
   }

   RAJA_FT_END ;
}

/*!
 ******************************************************************************
 *
 * \brief  SIMD maxloc reduction over index range.
 *         No assumption made on data alignment.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc(simd_exec,
                   Index_type begin, Index_type end,
                   T* max, Index_type* loc,
                   LOOP_BODY loop_body)
{
   RAJA_FT_BEGIN ;

RAJA_SIMD
   for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii, max, loc );
   }

   RAJA_FT_END ;
}

/*!
 ******************************************************************************
 *
 * \brief  SIMD maxloc reduction over range index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc(simd_exec,
                   const RangeISet& is,
                   T* max, Index_type* loc,
                   LOOP_BODY loop_body)
{
   const Index_type begin = is.getBegin();
   const Index_type end   = is.getEnd();

   RAJA_FT_BEGIN ;

RAJA_SIMD
   for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii, max, loc );
   }

   RAJA_FT_END ;
}

/*!
 ******************************************************************************
 *
 * \brief  SIMD sum reduction over index range.
 *         No assumption made on data alignment.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum(simd_exec,
                Index_type begin, Index_type end,
                T* sum,
                LOOP_BODY loop_body)
{
   RAJA_FT_BEGIN ;

RAJA_SIMD
   for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii, sum );
   }

   RAJA_FT_END ;
}

/*!
 ******************************************************************************
 *
 * \brief  SIMD sum reduction over range index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum(simd_exec,
                const RangeISet& is,
                T* sum,
                LOOP_BODY loop_body)
{
   const Index_type begin = is.getBegin();
   const Index_type end   = is.getEnd();

   RAJA_FT_BEGIN ;

RAJA_SIMD
   for ( Index_type ii = begin ; ii < end ; ++ii ) {
      loop_body( ii, sum );
   }

   RAJA_FT_END ;
}



//
//////////////////////////////////////////////////////////////////////
//
// Function templates that iterate over List index sets.
//
// NOTE: These operations will not vectorize, so we force sequential
//       execution.  Hence, they are "fake" SIMD operations.
//
//////////////////////////////////////////////////////////////////////
//

/*!
 ******************************************************************************
 *
 * \brief  "Fake" SIMD iteration over indices in indirection array.
 *
 ******************************************************************************
 */
template <typename LOOP_BODY>
RAJA_INLINE
void forall(simd_exec,
            const Index_type* __restrict__ idx, const Index_type len,
            LOOP_BODY loop_body)
{
   RAJA_FT_BEGIN ;

#pragma novector
   for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k] );
   }

   RAJA_FT_END ;
}

/*!
 ******************************************************************************
 *
 * \brief  "Fake" SIMD iteration over List index set object.
 *
 ******************************************************************************
 */
template <typename LOOP_BODY>
RAJA_INLINE
void forall(simd_exec,
            const ListISet& is,
            LOOP_BODY loop_body)
{
   const Index_type* __restrict__ idx = is.getIndex();
   const Index_type len = is.getLength();

   RAJA_FT_BEGIN ;

#pragma novector
   for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k] );
   }

   RAJA_FT_END ;
}

/*!
 ******************************************************************************
 *
 * \brief  "Fake" SIMD minloc reduction over indices in indirection array.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc(simd_exec,
                   const Index_type* __restrict__ idx, const Index_type len,
                   T* min, Index_type* loc,
                   LOOP_BODY loop_body)
{
   RAJA_FT_BEGIN ;

#pragma novector
   for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k], min, loc );
   }

   RAJA_FT_END ;
}

/*!
 ******************************************************************************
 *
 * \brief  "Fake" SIMD minloc reduction over List index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc(simd_exec,
                   const ListISet& is,
                   T* min, Index_type* loc,
                   LOOP_BODY loop_body)
{
   const Index_type* __restrict__ idx = is.getIndex();
   const Index_type len = is.getLength();

   RAJA_FT_BEGIN ;

#pragma novector
   for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k], min, loc );
   }

   RAJA_FT_END ;
}

/*!
 ******************************************************************************
 *
 * \brief  "Fake" SIMD maxloc reduction over indices in indirection array.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc(simd_exec,
                   const Index_type* __restrict__ idx, const Index_type len,
                   T* max, Index_type* loc,
                   LOOP_BODY loop_body)
{
   RAJA_FT_BEGIN ;

#pragma novector
   for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k], max, loc );
   }

   RAJA_FT_END ;
}

/*!
 ******************************************************************************
 *
 * \brief  "Fake" SIMD maxloc reduction over List index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc(simd_exec,
                   const ListISet& is,
                   T* max, Index_type* loc,
                   LOOP_BODY loop_body)
{
   const Index_type* __restrict__ idx = is.getIndex();
   const Index_type len = is.getLength();

   RAJA_FT_BEGIN ;

#pragma novector
   for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k], max, loc );
   }

   RAJA_FT_END ;
}

/*!
 ******************************************************************************
 *
 * \brief  "Fake" SIMD sum reduction over indices in indirection array.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum(simd_exec,
                const Index_type* __restrict__ idx, const Index_type len,
                T* sum,
                LOOP_BODY loop_body)
{
   RAJA_FT_BEGIN ;

#pragma novector
   for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k], sum );
   }

   RAJA_FT_END ;
}

/*!
 ******************************************************************************
 *
 * \brief  "Fake" SIMD sum reduction over List index set object.
 *
 ******************************************************************************
 */
template <typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum(simd_exec,
                const ListISet& is,
                T* sum,
                LOOP_BODY loop_body)
{
   const Index_type* __restrict__ idx = is.getIndex();
   const Index_type len = is.getLength();

   RAJA_FT_BEGIN ;

#pragma novector
   for ( Index_type k = 0 ; k < len ; ++k ) {
      loop_body( idx[k], sum );
   }

   RAJA_FT_END ;
}


//
//////////////////////////////////////////////////////////////////////
//
// SIMD execution policy does not apply to iteration over hybrid index 
// set segments iteration, only to execution of individual segments.
//
//////////////////////////////////////////////////////////////////////
//


}  // closing brace for RAJA namespace


#endif  // closing endif for header file include guard
