// This work was performed under the auspices of the U.S. Department of Energy by
// Lawrence Livermore National Laboratory under Contract DE-AC52-07NA27344.?

/*!
 ******************************************************************************
 *
 * \file
 *
 * \brief   RAJA header file defining range index set classes.
 *     
 * \author  Rich Hornung, Center for Applied Scientific Computing, LLNL
 * \author  Jeff Keasler, Applications, Simulations And Quality, LLNL
 *
 ******************************************************************************
 */

#ifndef RAJA_RangeISet_HXX
#define RAJA_RangeISet_HXX

#include "config.hxx"

#include "int_datatypes.hxx"

#include "execpolicy.hxx"

#include <iosfwd>


namespace RAJA {


/*!
 ******************************************************************************
 *
 * \brief  Class representing a contiguous range of indices.
 *
 *         Range is specified by begin and end values.
 *         Traversal executes as:  
 *            for (i = m_begin; i < m_end; ++i) {
 *               expression using i as array index.
 *            }
 *
 ******************************************************************************
 */
class RangeISet
{
public:

   ///
   /// Sequential execution policy for range index set.
   ///
   typedef RAJA::seq_exec seq_policy;

   /*
    * Using compiler-generated dtor, copy ctor, copy assignment.
    */

   ///
   /// Construct range index set [begin, end).
   ///
   RangeISet(Index_type begin, Index_type end) 
     : m_begin(begin), m_end(end) { ; }

   ///
   /// Return starting index for index set. 
   ///
   Index_type getBegin() const { return m_begin; }

   ///
   /// Return one past last index for index set. 
   ///
   Index_type getEnd() const { return m_end; }

   ///
   /// Return starting index for index set. 
   ///
   void setBegin(int val) { m_begin = val ; }

   ///
   /// Return one past last index for index set. 
   ///
   void setEnd(int val) { m_end = val ; }

   ///
   /// Return number of indices represented by index set.
   ///
   Index_type getLength() const { return (m_end-m_begin); }

   ///
   /// Return 'Owned' indicating that index set object owns the data
   /// representing its indices.
   ///
   IndexOwnership indexOwnership() const { return Owned; }

   ///
   /// Print index set data to given output stream.
   ///
   void print(std::ostream& os) const;

private:
   //
   // The default ctor is not implemented.
   //
   RangeISet();

   Index_type m_begin;
   Index_type m_end;
};

//
// TODO: Add multi-dim'l ranges, and ability to repeat index sets using 
//       an offset without having to resort to a hybrid, others? 
//


}  // closing brace for RAJA namespace

#endif  // closing endif for header file include guard
