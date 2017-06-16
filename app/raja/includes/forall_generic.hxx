// This work was performed under the auspices of the U.S. Department of Energy by
// Lawrence Livermore National Laboratory under Contract DE-AC52-07NA27344.?

/*!
 ******************************************************************************
 *
 * \file
 *
 * \brief   Header file containing RAJA index set iteration template methods 
 *          that take execution policy as a template parameter.
 *
 *          These templates support the following usage pattern:
 *
 *             forall<exec_policy>( index set, loop body );
 *
 *          which is equivalent to:
 *
 *             forall( exec_policy(), index set, loop body );
 *
 *          The former is slightly more concise.
 *
 *          IMPORTANT: Use of any of these methods requires a specialization
 *                     for the given index set type and execution policy.
 *
 * \author  Rich Hornung, Center for Applied Scientific Computing, LLNL
 * \author  Jeff Keasler, Applications, Simulations And Quality, LLNL
 *
 ******************************************************************************
 */

#ifndef RAJA_forall_generic_HXX
#define RAJA_forall_generic_HXX

#include "config.hxx"

#include "int_datatypes.hxx"


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
 * \brief Generic iteration over index range.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename LOOP_BODY>
RAJA_INLINE
void forall(Index_type begin, Index_type end, 
            LOOP_BODY loop_body)
{
   forall( EXEC_POLICY_T(),
           begin, end, 
           loop_body );
}

/*!
 ******************************************************************************
 *
 * \brief Generic iterations over range index set object.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename LOOP_BODY>
RAJA_INLINE
void forall(const RangeISet& iset,
            LOOP_BODY loop_body)
{
   forall( EXEC_POLICY_T(),
           iset.getBegin(), iset.getEnd(),
           loop_body );
}

/*!
 ******************************************************************************
 *
 * \brief  Generic minloc reduction over index range.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T, 
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc(Index_type begin, Index_type end,
                   T* min, Index_type* loc,
                   LOOP_BODY loop_body)
{
   forall_minloc( EXEC_POLICY_T(),
                  begin, end, 
                  min, loc,
                  loop_body );
}

/*!
 ******************************************************************************
 *
 * \brief  Generic minloc reduction over range index set object.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc(const RangeISet& iset,
                   T* min, Index_type* loc,
                   LOOP_BODY loop_body)
{
   forall_minloc( EXEC_POLICY_T(),
                  iset.getBegin(), iset.getEnd(),
                  min, loc,
                  loop_body );
}

/*!
 ******************************************************************************
 *
 * \brief  Generic maxloc reduction over index range.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc(Index_type begin, Index_type end,
                   T* max, Index_type* loc,
                   LOOP_BODY loop_body)
{
   forall_maxloc( EXEC_POLICY_T(),
                  begin, end,
                  max, loc,
                  loop_body );
}

/*!
 ******************************************************************************
 *
 * \brief  Generic maxloc reduction over range index set object.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc(const RangeISet& iset,
                   T* max, Index_type* loc,
                   LOOP_BODY loop_body)
{
   forall_maxloc( EXEC_POLICY_T(),
                  iset.getBegin(), iset.getEnd(),
                  max, loc,
                  loop_body );
}

/*!
 ******************************************************************************
 *
 * \brief  Generic sum reduction over index range.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum(Index_type begin, Index_type end,
                T* sum,
                LOOP_BODY loop_body)
{
   forall_sum( EXEC_POLICY_T(),
               begin, end,
               sum,
               loop_body );
}

/*!
 ******************************************************************************
 *
 * \brief  Generic sum reduction over range index set object.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum(const RangeISet& iset,
                T* sum,
                LOOP_BODY loop_body)
{
   forall_sum( EXEC_POLICY_T(),
               iset.getBegin(), iset.getEnd(),
               sum,
               loop_body );
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
 * \brief  Generic iteration over indirection array.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename LOOP_BODY>
RAJA_INLINE
void forall(const Index_type* idx, const Index_type len,
            LOOP_BODY loop_body)
{
   forall( EXEC_POLICY_T(),
           idx, len, 
           loop_body );
}

/*!
 ******************************************************************************
 *
 * \brief Generic iteration over List index set object.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T, 
          typename LOOP_BODY>
RAJA_INLINE
void forall(const ListISet& iset, 
            LOOP_BODY loop_body)
{
   forall( EXEC_POLICY_T(),
           iset.getIndex(), iset.getLength(), 
           loop_body );
}

/*!
 ******************************************************************************
 *
 * \brief  Generic minloc reduction over indirection array.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc(const Index_type* idx, const Index_type len,
                   T* min, Index_type* loc,
                   LOOP_BODY loop_body)
{
   forall_minloc( EXEC_POLICY_T(),
                  idx, len, 
                  min, loc,
                  loop_body );
}

/*!
 ******************************************************************************
 *
 * \brief  Generic minloc reduction over List index set object.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc(const ListISet& iset,
                   T* min, Index_type* loc,
                   LOOP_BODY loop_body)
{
   forall_minloc( EXEC_POLICY_T(),
                  iset.getIndex(), iset.getLength(), 
                  min, loc,
                  loop_body );
}

/*!
 ******************************************************************************
 *
 * \brief  Generic maxloc reduction over indirection array.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc(const Index_type* idx, const Index_type len,
                   T* max, Index_type* loc,
                   LOOP_BODY loop_body)
{
   forall_maxloc( EXEC_POLICY_T(),
                  idx, len,
                  max, loc,
                  loop_body );
}

/*!
 ******************************************************************************
 *
 * \brief  Generic maxloc reduction over List index set object.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc(const ListISet& iset,
                   T* max, Index_type* loc,
                   LOOP_BODY loop_body)
{
   forall_maxloc( EXEC_POLICY_T(),
                  iset.getIndex(), iset.getLength(),
                  max, loc,
                  loop_body );
}

/*!
 ******************************************************************************
 *
 * \brief  Generic sum reduction over indirection array.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum(const Index_type* idx, const Index_type len,
                T* sum,
                LOOP_BODY loop_body)
{
   forall_sum( EXEC_POLICY_T(),
               idx, len,
               sum, 
               loop_body );
}

/*!
 ******************************************************************************
 *
 * \brief  Generic sum reduction over List index set object.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum(const ListISet& iset,
                T* sum, 
                LOOP_BODY loop_body)
{
   forall_sum( EXEC_POLICY_T(),
               iset.getIndex(), iset.getLength(),
               sum,
               loop_body );
}


//
//////////////////////////////////////////////////////////////////////
//
// Methods that iterate over arbitrary index set types.
//
//////////////////////////////////////////////////////////////////////
//

/*!
 ******************************************************************************
 *
 * \brief Generic iteration over arbitrary index set and execution policy.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename INDEXSET_T, 
          typename LOOP_BODY>
RAJA_INLINE
void forall(const INDEXSET_T& iset, LOOP_BODY loop_body)
{
   forall(EXEC_POLICY_T(),
          iset, loop_body);
}

/*!
 ******************************************************************************
 *
 * \brief Generic minloc reduction iteration over arbitrary index set 
 *        and execution policy.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename INDEXSET_T, 
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_minloc(const INDEXSET_T& iset, 
                   T* min, Index_type *loc,
                   LOOP_BODY loop_body)
{
   forall_minloc(EXEC_POLICY_T(),
                 iset, 
                 min, loc,
                 loop_body);
}

/*!
 ******************************************************************************
 *
 * \brief Generic maxloc reduction iteration over arbitrary index set
 *        and execution policy.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename INDEXSET_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_maxloc(const INDEXSET_T& iset,
                   T* max, Index_type *loc,
                   LOOP_BODY loop_body)
{
   forall_maxloc(EXEC_POLICY_T(),
                 iset,
                 max, loc,
                 loop_body);
}

/*!
 ******************************************************************************
 *
 * \brief Generic sum reduction iteration over arbitrary index set
 *        and execution policy.
 *
 ******************************************************************************
 */
template <typename EXEC_POLICY_T,
          typename INDEXSET_T,
          typename T,
          typename LOOP_BODY>
RAJA_INLINE
void forall_sum(const INDEXSET_T& iset,
                T* sum,
                LOOP_BODY loop_body)
{
   forall_sum(EXEC_POLICY_T(),
              iset,
              sum,
              loop_body);
}


}  // closing brace for RAJA namespace


#endif  // closing endif for header file include guard
