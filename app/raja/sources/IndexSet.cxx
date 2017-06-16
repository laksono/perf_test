// This work was performed under the auspices of the U.S. Department of Energy by
// Lawrence Livermore National Laboratory under Contract DE-AC52-07NA27344.?

/*!
 ******************************************************************************
 *
 * \file
 *
 * \brief   Implementation file for hybrid index set classes
 *
 * \author  Rich Hornung, Center for Applied Scientific Computing, LLNL
 * \author  Jeff Keasler, Applications, Simulations And Quality, LLNL
 *
 ******************************************************************************
 */

#include "IndexSet.hxx"

#include "RangeISet.hxx"
#include "ListISet.hxx"

#include <iostream>

namespace RAJA {


/*
*************************************************************************
*
* Public IndexSet methods.
*
*************************************************************************
*/

IndexSet::IndexSet()
: m_len(0)
{
}

IndexSet::IndexSet(const Index_type* const indices_in, Index_type length)
: m_len(0)
{
   buildIndexSet(*this, indices_in, length);
}

IndexSet::IndexSet(const IndexSet& other)
: m_len(0)
{
   copy(other); 
}

IndexSet& IndexSet::operator=(
   const IndexSet& rhs)
{
   if ( &rhs != this ) {
      IndexSet copy(rhs);
      this->swap(copy);
   }
   return *this;
}

IndexSet::~IndexSet()
{
   const int num_segs = getNumSegments();
   for ( int isi = 0; isi < num_segs; ++isi ) {
      SegmentType segtype = getSegmentType(isi);
      const void* iset = getSegmentISet(isi);

      if ( iset ) {

         switch ( segtype ) {

            case _Range_ : {
               RangeISet* is =
                  const_cast<RangeISet*>(
                     static_cast<const RangeISet*>(iset)
                  );
               delete is;
               break;
            }

            case _List_ : {
               ListISet* is =
                  const_cast<ListISet*>(
                     static_cast<const ListISet*>(iset)
                  );
               delete is;
               break;
            }

            default : {
               std::cout << "\t IndexSet dtor: case not implemented!!\n";
            }

         }  // switch ( segtype )

      }  // if ( iset ) 

   }  // for isi...
}

void IndexSet::swap(IndexSet& other)
{
#if defined(RAJA_USE_STL)
   using std::swap;
   swap(m_len, other.m_len);
   swap(m_segments, other.m_segments);
#else
   m_len = other.m_len;
   m_segments = other.m_segments;
#endif
}


/*
*************************************************************************
*
* Methods to add segments to hybrid index set.
*
*************************************************************************
*/

void IndexSet::addRange(Index_type begin, Index_type end)
{
   RangeISet* new_is = new RangeISet(begin, end);
   addSegment( _Range_, new_is );
}

void IndexSet::addISet(const RangeISet& iset)
{
   RangeISet* new_is = new RangeISet(iset);
   addSegment( _Range_, new_is );
}

void IndexSet::addList(const Index_type* indx, 
                       Index_type len,
                       IndexOwnership indx_own)
{
   ListISet* new_is = new ListISet(indx, len, indx_own);
   addSegment( _List_, new_is );
}

void IndexSet::addISet(const ListISet& iset, 
                         IndexOwnership indx_own)
{
   ListISet* new_is = new ListISet(iset.getIndex(),
                                                   iset.getLength(),
                                                   indx_own);
   addSegment( _List_, new_is );
}


/*
*************************************************************************
*
* Print contents of hybrid index set to given output stream.
*
*************************************************************************
*/

void IndexSet::print(std::ostream& os) const
{
   os << "HYBRID INDEX SET : " 
      << getLength() << " length..." << std::endl
      << getNumSegments() << " segments..." << std::endl;

   const int num_segs = getNumSegments();
   for ( int isi = 0; isi < num_segs; ++isi ) {
      SegmentType segtype = getSegmentType(isi);
      const void* iset = getSegmentISet(isi);

      os << "\tSegment " << isi << " : " << std::endl;

      switch ( segtype ) {

         case _Range_ : {
            if ( iset ) {
               const RangeISet* is =
                  static_cast<const RangeISet*>(iset);
               is->print(os);
            } else {
               os << "_Range_ is null" << std::endl;
            }
            break;
         }

         case _List_ : {
            if ( iset ) {
               const ListISet* is =
                  static_cast<const ListISet*>(iset);
               is->print(os);
            } else {
               os << "_List_ is null" << std::endl;
            }
            break;
         }

         default : {
            os << "IndexSet print: case not implemented!!\n";
         }

      }  // switch ( segtype )

   }  // for isi...
}


/*
*************************************************************************
*
* Private helper function to copy hybrid index set segments.
*
*************************************************************************
*/
void IndexSet::copy(const IndexSet& other)
{
   const int num_segs = other.getNumSegments();
   for ( int isi = 0; isi < num_segs; ++isi ) {
      SegmentType segtype = other.getSegmentType(isi);
      const void* iset = other.getSegmentISet(isi);

      if ( iset ) {

         switch ( segtype ) {

            case _Range_ : {
               addISet(*static_cast<const RangeISet*>(iset));
               break;
            }

            case _List_ : {
               addISet(*static_cast<const ListISet*>(iset));
               break;
            }

            default : {
               std::cout << "\t IndexSet::copy: case not implemented!!\n";
            }

         }  // switch ( segtype )

      }  // if ( iset ) 

   }  // for isi...
}



/*
*************************************************************************
*
* IndexSet builder methods.
*
*************************************************************************
*/

void buildIndexSet(IndexSet& hiset,
                     const Index_type* const indices_in, 
                     Index_type length)
{
   if ( length == 0 ) return;

   /* only transform relatively large */
   if (length > RANGE_MIN_LENGTH) {
      /* build a rindex array from an index array */
      Index_type docount = 0 ;
      Index_type inrange = -1 ;

      /****************************/
      /* first, gather statistics */
      /****************************/

      Index_type scanVal = indices_in[0] ;
      Index_type sliceCount = 0 ;
      for (Index_type ii=1; ii<length; ++ii) {
         Index_type lookAhead = indices_in[ii] ;

         if (inrange == -1) {
            if ( (lookAhead == scanVal+1) && 
                 ((scanVal % RANGE_ALIGN) == 0) ) {
              inrange = 1 ;
            }
            else {
              inrange = 0 ;
            }
         }

         if (lookAhead == scanVal+1) {
            if ( (inrange == 0) && ((scanVal % RANGE_ALIGN) == 0) ) {
               if (sliceCount != 0) {
                  docount += 1 + sliceCount ; /* length + singletons */
               }
               inrange = 1 ;
               sliceCount = 0 ;
            }
            ++sliceCount ;  /* account for scanVal */
         }
         else {
            if (inrange == 1) {
               /* we can tighten this up by schleping any trailing */
               /* sigletons off into the subsequent singleton */
               /* array.  We would then also need to recheck the */
               /* final length of the range to make sure it meets */
               /* our minimum length crietria.  If it doesnt, */
               /* we need to emit a random array instead of */
               /* a range array */
               ++sliceCount ;
               docount += 2 ; /* length + begin */
               inrange = 0 ;
               sliceCount = 0 ;
            }
            else {
              ++sliceCount ;  /* account for scanVal */
            }
         }

         scanVal = lookAhead ;
      }  // end loop to gather statistics

      if (inrange != -1) {
         if (inrange) {
            ++sliceCount ;
            docount += 2 ; /* length + begin */
         }
         else {
            ++sliceCount ;
            docount += 1 + sliceCount ; /* length + singletons */
         }
      }
      else if (scanVal != -1) {
         ++sliceCount ;
         docount += 2 ;
      }
      ++docount ; /* zero length termination */

      /* What is the cutoff criteria for generating the rindex array? */
      if (docount < (length*(RANGE_ALIGN-1))/RANGE_ALIGN) {
         /* The rindex array can either contain a pointer into the */
         /* original index array, *or* it can repack the data from the */
         /* original index array.  Benefits of repacking could include */
         /* better use of hardware prefetch streams, or guaranteeing */
         /* alignment of index array segments. */

         /*******************************/
         /* now, build the rindex array */
         /*******************************/

         Index_type dobegin ;
         inrange = -1 ;

         scanVal = indices_in[0] ;
         sliceCount = 0 ;
         dobegin = scanVal ;
         for (Index_type ii=1; ii < length; ++ii) {
            Index_type lookAhead = indices_in[ii] ;

            if (inrange == -1) {
               if ( (lookAhead == scanVal+1) && 
                    ((scanVal % RANGE_ALIGN) == 0) ) {
                 inrange = 1 ;
               }
               else {
                 inrange = 0 ;
                 dobegin = ii-1 ;
               }
            }
            if (lookAhead == scanVal+1) {
               if ( (inrange == 0) && 
                    ((scanVal % RANGE_ALIGN) == 0) ) {
                  if (sliceCount != 0) {
                     hiset.addList(&indices_in[dobegin], 
                                   sliceCount);
                  }
                  inrange = 1 ;
                  dobegin = scanVal ;
                  sliceCount = 0 ;
               }
               ++sliceCount ;  /* account for scanVal */
            }
            else {
               if (inrange == 1) {
               /* we can tighten this up by schleping any trailing */
               /* sigletons off into the subsequent singleton */
               /* array.  We would then also need to recheck the */
               /* final length of the range to make sure it meets */
               /* our minimum length crietria.  If it doesnt, */
               /* we need to emit a random array instead of */
               /* a range array */
                  ++sliceCount ;
                  hiset.addRange(dobegin, dobegin+sliceCount);
                  inrange = 0 ;
                  sliceCount = 0 ;
                  dobegin = ii ;
               }
               else {
                 ++sliceCount ;  /* account for scanVal */
               }
            }

            scanVal = lookAhead ;
         }  // for (Index_type ii ...

         if (inrange != -1) {
            if (inrange) {
               ++sliceCount ;
               hiset.addRange(dobegin, dobegin+sliceCount);
            }
            else {
               ++sliceCount ;
               hiset.addList(&indices_in[dobegin], sliceCount);
            }
         }
         else if (scanVal != -1) {
            hiset.addList(&scanVal, 1);
         }
      }
      else {  // !(docount < (length*RANGE_ALIGN-1))/RANGE_ALIGN)
         hiset.addList(indices_in, length);
      }
   }
   else {  // else !(length > RANGE_MIN_LENGTH)
      hiset.addList(indices_in, length);
   }
}

}  // closing brace for RAJA namespace
