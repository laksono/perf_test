// This work was performed under the auspices of the U.S. Department of Energy by
// Lawrence Livermore National Laboratory under Contract DE-AC52-07NA27344.?

/*!
 ******************************************************************************
 *
 * \file
 *
 * \brief   Implementation file for List index set classes
 *
 * \author  Rich Hornung, Center for Applied Scientific Computing, LLNL
 * \author  Jeff Keasler, Applications, Simulations And Quality, LLNL
 *
 ******************************************************************************
 */

#include "ListISet.hxx"

#include <iostream>

#if !defined(RAJA_USE_STL)
#include <cstdio>
#include <cstring>
#endif

namespace RAJA {


/*
*************************************************************************
*
* Public ListISet class methods.
*
*************************************************************************
*/

ListISet::ListISet(const Index_type* indx, Index_type len,
                                   IndexOwnership indx_own)
{
   initIndexData(indx, len, indx_own);
}

ListISet::ListISet(const ListISet& other)
{
   initIndexData(other.m_indx, other.m_len, other.m_indx_own);
}

ListISet& ListISet::operator=(const ListISet& rhs)
{
   if ( &rhs != this ) {
      ListISet copy(rhs);
      this->swap(copy);
   }
   return *this;
}

ListISet::~ListISet()
{
   if ( m_indx && m_indx_own == Owned ) {
      delete[] m_indx ;
   }
}

void ListISet::swap(ListISet& other)
{
#if defined(RAJA_USE_STL)
   using std::swap;
   swap(m_indx, other.m_indx);
   swap(m_len, other.m_len);
   swap(m_indx_own, other.m_indx_own);
#else
   m_indx = other.m_indx;
   m_len = other.m_len;
   m_indx_own = m_indx_own;
#endif
}

void ListISet::print(std::ostream& os) const
{
   os << "\nListISet : length, owns index = " << m_len << " , " 
      << (m_indx_own == Owned ? "Owned" : "Unowned") << std::endl;
   for (Index_type i = 0; i < m_len; ++i) {
      os << "\t" << m_indx[i] << std::endl;
   }
}

/*
*************************************************************************
*
* Private initialization method.
*
*************************************************************************
*/
void ListISet::initIndexData(const Index_type* indx, 
                                     Index_type len,
                                     IndexOwnership indx_own)
{
   if ( len <= 0 ) {

      m_indx = 0;
      m_len = 0;
      m_indx_own = Unowned;

   } else { 

      m_len = len;
      m_indx_own = indx_own;

      if ( m_indx_own == Owned ) {
         m_indx = new Index_type[len];
#if defined(RAJA_USE_STL)
         std::copy(indx, indx + m_len, m_indx);
#else
         memcpy(m_indx, indx, m_len*sizeof(Index_type));
#endif
      } else {
         // Uh-oh. Using evil const_cast.... 
         m_indx = const_cast<Index_type*>(indx);
      }

   } 
}


}  // closing brace for RAJA namespace
