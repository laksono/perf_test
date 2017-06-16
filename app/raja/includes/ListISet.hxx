// This work was performed under the auspices of the U.S. Department of Energy by
// Lawrence Livermore National Laboratory under Contract DE-AC52-07NA27344.?

/*!
 ******************************************************************************
 *
 * \file
 *
 * \brief   RAJA header file defining List index set classes.
 *     
 * \author  Rich Hornung, Center for Applied Scientific Computing, LLNL
 * \author  Jeff Keasler, Applications, Simulations And Quality, LLNL
 *
 ******************************************************************************
 */

#ifndef RAJA_ListISet_HXX
#define RAJA_ListISet_HXX

#include "config.hxx"

#include "int_datatypes.hxx"

#include "execpolicy.hxx"

#if defined(RAJA_USE_STL)
#include <utility> 
#endif

#include <iosfwd> 


namespace RAJA {


/*!
 ******************************************************************************
 *
 * \brief  Class representing an arbitrary collection of indices. 
 *
 *         Length indicates number of indices in index array.
 *         Traversal executes as:  
 *            for (i = 0; i < m_len; ++i) {
 *               expression using m_indx[i] as array index.
 *            }
 *
 ******************************************************************************
 */
class ListISet
{
public:

   ///
   /// Sequential execution policy for List index set.
   ///
   typedef RAJA::seq_exec seq_policy;

   ///
   /// Construct List index set from given array with specified 
   /// length.
   ///
   /// By default the ctor performs deep copy of array elements.
   /// If 'Unowned' is passed as last argument, the constructed object
   /// does not own the index data and will hold a pointer to given data.
   /// In this case, caller must manage object lifetimes properly.
   ///
   ListISet(const Index_type* indx, Index_type len,
                    IndexOwnership indx_own = Owned);

#if defined(RAJA_USE_STL)
   ///
   /// Construct List index set from arbitrary object holding 
   /// indices using a deep copy of given data.
   ///
   /// The object must provide methods: empty(), begin(), end().
   ///
   template< typename T> explicit ListISet(const T& indx);
#endif

   ///
   /// Copy-constructor for List index set
   ///
   ListISet(const ListISet& obj);

   ///
   /// Copy-assignment for List index set
   ///
   ListISet& operator=(const ListISet& other);

   ///
   /// Destroy index set included its contents.
   ///
   ~ListISet();

   ///
   /// Swap function for copy-and-swap idiom.
   ///
   void swap(ListISet& other);

   ///
   ///  Return number of indices in index set.
   ///
   Index_type getLength() const { return m_len; }

   ///
   ///  Return const pointer to array of indices in index set.
   ///
   const Index_type* getIndex() const { return m_indx; }

   ///
   /// Return enum value indicating whether index set object owns the data
   /// representing its indices.
   ///
   IndexOwnership indexOwnership() const { return m_indx_own; }
    
   ///
   /// Print index set data to given output stream.
   ///
   void print(std::ostream& os) const;

private:
   //
   // The default ctor is not implemented.
   //
   ListISet();

   //
   // Initialize index data properly based on whether index set object
   // owns the index data.
   //  
   void initIndexData(const Index_type* indx, Index_type len,
                      IndexOwnership indx_own);

   Index_type* __restrict__ m_indx;
   Index_type  m_len;
   IndexOwnership m_indx_own;
};


#if defined(RAJA_USE_STL)
/*!
 ******************************************************************************
 *
 *  \brief Implementation of generic constructor template.
 *
 ******************************************************************************
 */ 
template< typename T> 
ListISet::ListISet(const T& indx)
: m_indx(0), m_len(0), m_indx_own(Unowned)
{
   if ( !indx.empty() ) {
      m_len = indx.size();
      m_indx = new Index_type[m_len];
      std::copy(indx.begin(), indx.end(), m_indx);
      m_indx_own = Owned;
   } 
}
#endif


}  // closing brace for RAJA namespace 


#if defined(RAJA_USE_STL)
/*!
 *  Specialization of std swap method.
 */ 
namespace std {

template< > 
RAJA_INLINE
void swap(RAJA::ListISet& a, RAJA::ListISet& b)
{
   a.swap(b);
}

}
#endif


#endif  // closing endif for header file include guard
