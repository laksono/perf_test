// This work was performed under the auspices of the U.S. Department of Energy by
// Lawrence Livermore National Laboratory under Contract DE-AC52-07NA27344.?

/*!
 ******************************************************************************
 *
 * \file
 *
 * \brief   Implementation file for range index set classes
 *
 * \author  Rich Hornung, Center for Applied Scientific Computing, LLNL
 * \author  Jeff Keasler, Applications, Simulations And Quality, LLNL
 *
 ******************************************************************************
 */

#include "RangeISet.hxx"

#include <iostream>

namespace RAJA {


/*
*************************************************************************
*
* RangeISet class methods
*
*************************************************************************
*/

void RangeISet::print(std::ostream& os) const
{
   os << "\nRangeISet::print : begin, end = "
      << m_begin << ", " << m_end << std::endl;
}


}  // closing brace for RAJA namespace
