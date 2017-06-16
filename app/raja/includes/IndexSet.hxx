// This work was performed under the auspices of the U.S. Department of Energy by
// Lawrence Livermore National Laboratory under Contract DE-AC52-07NA27344.?

/*!
 ******************************************************************************
 *
 * \file
 *
 * \brief   RAJA header file defining hybrid index set classes.
 *
 * \author  Rich Hornung, Center for Applied Scientific Computing, LLNL
 * \author  Jeff Keasler, Applications, Simulations And Quality, LLNL
 *
 ******************************************************************************
 */

#ifndef RAJA_IndexSet_HXX
#define RAJA_IndexSet_HXX

#include "config.hxx"

#include "int_datatypes.hxx"

#include "execpolicy.hxx"

#include "RAJAVec.hxx"

#include <stdlib.h>

#if defined(RAJA_USE_STL)
#include <utility>
#endif

#include <iosfwd>


namespace RAJA {

class RangeISet;
class ListISet;


/*!
 ******************************************************************************
 *
 * \brief  Class representing an hybrid index set which is a collection
 *         of index set objects defined above.  Within a hybrid, the
 *         individual index sets are referred to as segments.
 *
 ******************************************************************************
 */
class IndexSet
{
public:

   ///
   /// Nested class representing hybrid index set execution policy. 
   ///
   /// The first template parameter describes the policy for iterating
   /// over segments.  The second describes the execution policy for 
   /// each segment.
   ///
   template< typename SEG_ITER_POLICY_T,
             typename SEG_EXEC_POLICY_T > struct ExecPolicy
   {
      typedef SEG_ITER_POLICY_T seg_it;
      typedef SEG_EXEC_POLICY_T seg_exec;
   };

   ///
   /// Sequential execution policy for hybrid index set.
   ///
   typedef ExecPolicy<RAJA::seq_segit, RAJA::seq_exec> seq_policy;

   ///
   /// Construct empty hybrid index set
   ///
   IndexSet();

   ///
   /// Construct hybrid index set from given index array using parameterized
   /// method buildIndexSet().
   ///
   IndexSet(const Index_type* const indices_in, Index_type length);

#if defined(RAJA_USE_STL)
   ///
   /// Construct hybrid index set from arbitrary object containing indices
   /// using parametrized method buildIndexSet().
   ///
   /// The object must provide the methods: size(), begin(), end().
   ///
   template< typename T> explicit IndexSet(const T& indx);
#endif

   ///
   /// Copy-constructor for hybrid index set
   ///
   IndexSet(const IndexSet& other);

   ///
   /// Copy-assignment operator for hybrid index set
   ///
   IndexSet& operator=(const IndexSet& rhs);

   ///
   /// Destroy index set including all index set segments.
   ///
   ~IndexSet();

   ///
   /// Swap function for copy-and-swap idiom.
   ///
   void swap(IndexSet& other);

   ///
   /// Add contiguous index range segment to hybrid index set 
   /// (adds RangeISet object).
   /// 
   void addRange(Index_type begin, Index_type end);

   ///
   /// Add RangeISet segment to hybrid index set.
   ///
   void addISet(const RangeISet& iset);

   ///
   /// Add segment containing array of indices to hybrid index set 
   /// (adds ListISet object).
   /// 
   /// By default, the method makes a deep copy of given array and index
   /// set object will own the data representing its indices.  If 'Unowned' 
   /// is passed to method, the new segment object does not own its indices 
   /// (i.e., it holds a handle to given array).  In this case, caller is
   /// responsible for managing object lifetimes properly.
   /// 
   void addList(const Index_type* indx, Index_type len,
                IndexOwnership indx_own = Owned);

   ///
   /// Add ListISet segment to hybrid index set.
   /// By default, the method makes a deep copy of given array and index
   /// set object will own the data representing its indices.  If 'Unowned'  
   /// is passed to method, the new segment object does not own its indices
   /// (i.e., it holds a handle to given array).  In this case, caller is
   /// responsible for managing object lifetimes properly.
   ///
   void addISet(const ListISet& iset, 
                IndexOwnership indx_own = Owned);

   ///
   /// Return total length of hybrid index set; i.e., sum of lengths
   /// of all segments.
   ///
   Index_type getLength() const { return m_len; }

   ///
   /// Return total number of segments in hybrid index set.
   ///
   int getNumSegments() const { 
      return m_segments.size(); 
   } 

   ///
   /// Return enum value defining type of segment 'i'.
   /// 
   /// Note: No error-checking on segment index.
   ///
   SegmentType getSegmentType(int i) const { 
      return m_segments[i].m_type; 
   }

   ///
   /// Return const void pointer to index set for segment 'i'.
   /// 
   /// Notes: Pointer must be explicitly cast to proper type before use
   ///        (see getSegmentType() method).
   ///
   ///        No error-checking on segment index.
   ///
   const void* getSegmentISet(int i) const { 
      return m_segments[i].m_iset; 
   } 

   ///
   /// Return enum value indicating whether segment 'i' index set owns the 
   /// data representing its indices.
   /// 
   /// Note: No error-checking on segment index.
   ///
   IndexOwnership segmentIndexOwnership(int i) const {
      return m_segments[i].m_indx_own; 
   } 

   volatile int &segmentSemaphoreValue(int i) {
      return *m_segments[i].semaphore_slot ;
   }

   int &segmentSemaphoreReloadValue(int i) {
      return m_segments[i].semaphore_reload ;
   }

   int &segmentSemaphoreNumDepTasks(int i) {
      return m_segments[i].num_semaphore_notify ;
   }

   int &segmentSemaphoreDepTask(int i, int t) {
      return m_segments[i].semaphore_notify[t] ;
   }

   void setPrivateData(int i, void *p) {
      m_segments[i].m_segmentPrivate = p ;
   }

   void *getPrivateData(int i) {
      return m_segments[i].m_segmentPrivate ;
   }

   ///
   /// Print hybrid index set data, including segments, to given output stream.
   ///
   void print(std::ostream& os) const;

private:
   //
   // Copy function for copy-and-swap idiom (deep copy).
   //
   void copy(const IndexSet& other);

   ///
   /// Private nested class to hold an index segment of a hybrid index set.
   ///
   /// A segment is defined by its type and its index set object.
   ///
   class Segment
   {
   public:
      Segment() 
         : m_type(_Unknown_), m_iset(0), m_indx_own(Unowned),
           num_semaphore_notify(0),
           semaphore_reload(0)
      {
        semaphore_slot = 0 ;
        posix_memalign((void **)(&semaphore_slot), 256, sizeof(int)) ;
        *semaphore_slot = 0 ;
      } 

      template <typename ISET>
      Segment(SegmentType type,  const ISET* iset)
         : m_type(type), m_iset(iset), m_indx_own(iset->indexOwnership()),
           num_semaphore_notify(0), semaphore_reload(0)
      {
        semaphore_slot = 0 ;
        posix_memalign((void **)(&semaphore_slot), 256, sizeof(int)) ;
        *semaphore_slot = 0 ;
      }

      ~Segment() {
         // free((void *)semaphore_slot) ;
      }

      ///
      /// Using compiler-provided dtor, copy ctor, copy-assignment.
      ///

      mutable int semaphore_notify[4] ;
      void* m_segmentPrivate ;
      const void* m_iset;
      SegmentType m_type;
      IndexOwnership m_indx_own;
      mutable int semaphore_reload ;
      mutable int num_semaphore_notify ;
      mutable volatile int *semaphore_slot ;
   };

   //
   // Helper function to add segment.
   //
   template< typename SEG_T> 
   void addSegment(SegmentType seg_type, const SEG_T* seg)
   {
      m_segments.push_back(Segment( seg_type, seg ));
      m_len += seg->getLength();
   } 

   ///
   Index_type  m_len;
   RAJAVec<Segment> m_segments;
}; 


/*!
 ******************************************************************************
 *
 * \brief Initialize hybrid index set from array of indices with given length.
 *
 *        Note given hybrid index set object is assumed to be empty.  
 *
 *        Routine does no error-checking on argements and assumes Index_type
 *        array contains valid indices.
 *
 ******************************************************************************
 */
void buildIndexSet(IndexSet& hiset,
                     const Index_type* const indices_in,
                     Index_type length);

#if defined(RAJA_USE_STL)
/*!
 ******************************************************************************
 *
 * \brief Implementation of generic constructor template.
 *
 ******************************************************************************
 */
template <typename T>
IndexSet::IndexSet(const T& indx)
: m_len(0)
{
   std::vector<Index_type> vec(indx.begin(), indx.end());
   buildIndexSet(*this, &vec[0], vec.size());
}
#endif


}  // closing brace for RAJA namespace

#if defined(RAJA_USE_STL)
/*!
 ******************************************************************************
 *
 *  \brief Specialization of std swap method.
 *
 ******************************************************************************
 */
namespace std {

template< > 
RAJA_INLINE
void swap(RAJA::IndexSet& a, RAJA::IndexSet& b)
{
   a.swap(b);
}

}
#endif

#endif  // closing endif for header file include guard
