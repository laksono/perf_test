// This work was performed under the auspices of the U.S. Department of Energy by
// Lawrence Livermore National Laboratory under Contract DE-AC52-07NA27344.?

/*!
 ******************************************************************************
 *
 * \file
 *
 * \brief   Header file defining RAJA loop execution policies.
 * 
 *          Note: availability of some policies depends on compiler choice.
 *
 * \author  Rich Hornung, Center for Applied Scientific Computing, LLNL
 * \author  Jeff Keasler, Applications, Simulations And Quality, LLNL 
 *
 ******************************************************************************
 */

#ifndef RAJA_execpolicy_HXX
#define RAJA_execpolicy_HXX


#include "config.hxx"


namespace RAJA {


#if defined(RAJA_COMPILER_ICC)

//
// Segment execution policies
//
struct seq_exec {};
struct simd_exec {};

struct omp_parallel_for_exec {};
struct omp_for_nowait_exec {};

struct cilk_for_exec {};

//
// Hybrid segment iteration policies
// 
struct seq_segit {};

struct omp_parallel_for_segit {};

struct cilk_for_segit {};


#endif   // end  Intel compilers.....


#if defined(RAJA_COMPILER_GNU) 

//
// Segment execution policies
//
struct seq_exec {};
struct simd_exec {};
struct omp_parallel_for_exec {};
struct omp_for_nowait_exec {};

//
// Hybrid segment iteration policies
//
struct seq_segit {};
struct omp_parallel_for_segit {};

#endif   // end  GNU compilers.....


#if defined(RAJA_COMPILER_XLC12)

//
// Segment execution policies
//
struct seq_exec {};
struct simd_exec {};
struct omp_parallel_for_exec {};
struct omp_for_nowait_exec {};

//
// Hybrid segment iteration policies
//
struct seq_segit {};
struct omp_parallel_for_segit {};

#endif   // end  xlc v12 compiler on bgq


#if defined(RAJA_COMPILER_CLANG)

//
// Segment exec policies
//
struct seq_execution {};
struct simd_exec {};

//
// Hybrid segment iteration policies
// 
struct seq_segit {};

#endif   // end  CLANG compilers.....


}  // closing brace for RAJA namespace


#endif  // closing endif for header file include guard
