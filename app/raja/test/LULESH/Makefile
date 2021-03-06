#  
# THIS IS NOT OPEN SOURCE OR PUBLIC DOMAIN SOFTWARE
#
# See README-RAJA_license.txt for access and distribution restrictions
#

#
#  Modify stuff in this Makefile to fit your machine and compiler.
#
#  Uncomment ONE AND ONLY ONE opt/debug option and "RAJA_ARCH" 
#  variable line for the platform you want. 
#
# IMPORTANT:  Make sure CXXFLAGS are what you want. They are used in 
#             the source code, RAJA header files in particular, to
#             set code compilation options. 
#

#ompt=yes
OPT_DEBUG = opt
#OPT_DEBUG = debug

#rzalastor
#RAJA_ARCH = x86_sse_gnu
RAJA_ARCH = x86_sse_icc

#rzmerl
RAJA_ARCH = x86_avx_icc
RAJA_ARCH = x86_avx_gnu
RAJA_ARCH = x86_sse_gnu

#rzuseq
#RAJA_ARCH = bgq_xlc12
#RAJA_ARCH = bgq_clang
#RAJA_ARCH = bgq_gnu

#rzmic
#RAJA_ARCH = MIC


RAJA_TOPDIR    = ../..
RAJA_INC       = $(RAJA_TOPDIR)/includes/
RAJA_SRC       = $(RAJA_TOPDIR)/sources/


##
## The RAJA_rules.mk file defines macro variables that specify RAJA behavior.
## To change the rules, the file in the RAJA include directory can be edited
## or it can be replaced with custom version here.
##
include $(RAJA_TOPDIR)/build/RAJA_rules.mk

#
# OMPT
#
STATIC_INTEL=-static_intel

ifeq ($(ompt-debug),yes)
OMPT_DEBUG=debug
endif

ifeq ($(ompt),yes)
OMPT_OPENMP=/home/johnmc/pkgs/openmp/lib
OMPT=-Wl,-rpath=$(OMPT_OPENMP) -L$(OMPT_OPENMP) $(OMPT_OPENMP)/libiomp5.so 
OMPT_POSTPROCESS=chrpath -r$(OMPT_OPENMP)
OMPT_POSTPROCESS=echo 
STATIC_INTEL=
else
OMPT_POSTPROCESS=echo 
endif

#
#  MIC
# 
ifeq ($(RAJA_ARCH),MIC)
CXX 		= icc

ifeq ($(OPT_DEBUG),opt)
CXX_COMPILE = $(CXX) -g -O3 -mmic -vec-report3  -inline-max-total-size=20000 -inline-forceinline -ansi-alias -std=c++0x -openmp
LDFLAGS	=
endif

ifeq ($(OPT_DEBUG),debug)
CXX_COMPILE = $(CXX) -g -mmic  -O0 -std=c++0x -openmp
LDFLAGS	=
endif

CXXFLAGS	= -DRAJA_PLATFORM_X86_SSE -DRAJA_COMPILER_ICC
LDPATH		=

endif 


#
#  INTEL RAJA_COMPILER (SSE 4.1) 
# 
ifeq ($(RAJA_ARCH),x86_sse_icc)
CXX = icpc

ifeq ($(OPT_DEBUG),opt)
#CXX_COMPILE = $(CXX) -O3 -msse4.1 -inline-max-total-size=20000 -inline-forceinline -opt-streaming-stores always -ansi-alias -std=c++0x -openmp
CXX_COMPILE = $(CXX) -g -O3 -msse4.1 -align -inline-max-total-size=20000 -inline-forceinline -ansi-alias -std=c++0x -openmp $(STATIC_INTEL)
##CXX_COMPILE = $(CXX) -O3 -msse4.1 -inline-max-total-size=20000 -inline-forceinline -ansi-alias -std=c++0x 
LDFLAGS	= $(OMPT)
endif

ifeq ($(OPT_DEBUG),debug)
CXX_COMPILE = $(CXX) -g -O0 -std=c++0x -debug inline-debug-info -parallel-source-info=2 -debug all -openmp
LDFLAGS	= $(OMPT)
endif

CXXFLAGS	= -DRAJA_PLATFORM_X86_SSE -DRAJA_COMPILER_ICC
LDPATH		=

endif 


#
#  GNU  RAJA_COMPILER (SSE 4.1)
# 
ifeq ($(RAJA_ARCH),x86_sse_gnu)
#CXX 		= /usr/local/bin/g++-4.4.6
#CXX 		= /usr/local/bin/g++-4.6.1
#CXX 		= /usr/apps/gnu/4.7.1/bin/g++
CXX 		= /usr/apps/gnu/4.8.0/bin/g++
CXX 		= g++

ifeq ($(OPT_DEBUG),opt)
#
# Use this with GNU 4.7X and later
CXX_COMPILE = $(CXX) -Ofast -msse4.1 -finline-functions -finline-limit=20000 -std=c++11 -fopenmp
CXX_COMPILE = $(CXX) -g -Ofast -finline-functions -finline-limit=20000 -std=c++11 -fopenmp
#CXX_COMPILE = $(CXX) -O3 -msse4.1 -finline-functions -finline-limit=20000 -std=c++0x -openmp
##CXX_COMPILE = $(CXX) -O3 -msse4.1  -ansi-alias -std=c++0x
## inline flags...
#CXX_COMPILE = $(CXX) -O3 -msse4.1  -finline-functions -finline-limit=20000 -ansi-alias -std=c++0x
## inline flags + others...
##CXX_COMPILE = $(CXX) -O3 -msse4.1  -finline-functions -finline-limit=20000 -fomit-frame-pointer -minline-all-stringops -malign-double -ftree-vectorize -floop-block -ansi-alias -std=c++0x -openmp
##CXX_COMPILE = $(CXX) -O3 -msse4.1  -finline-functions -finline-limit=20000 -fomit-frame-pointer -malign-double -ftree-vectorize -floop-block -ansi-alias -std=c++0x
LDFLAGS	=
endif

ifeq ($(OPT_DEBUG),debug)
CXX_COMPILE = $(CXX) -g -O0 -std=c++0x -openmp
CXX_COMPILE = $(CXX) -O3 -msse4.1 -finline-functions -finline-limit=20000 -std=c++11 -openmp
LDFLAGS	= $(OMPT)
endif

CXXFLAGS 	= -DRAJA_PLATFORM_X86_SSE -DRAJA_COMPILER_GNU
LDPATH		=

endif 


#
#  INTEL RAJA_COMPILER (AVX 2)
# 
ifeq ($(RAJA_ARCH),x86_avx_icc)
CXX             = icpc

ifeq ($(OPT_DEBUG),opt)
#CXX_COMPILE = $(CXX) -O3 -mavx -inline-max-total-size=20000 -inline-forceinline -opt-streaming-stores always -ansi-alias -std=c++0x -openmp
CXX_COMPILE = $(CXX) -g -O3 -mavx -align -inline-max-total-size=20000 -inline-forceinline -ansi-alias -std=c++0x -openmp $(STATIC_INTEL)
##CXX_COMPILE = $(CXX) -O3 -mavx -inline-max-total-size=20000 -inline-forceinline -ansi-alias -std=c++0x 
LDFLAGS	= $(OMPT)
endif

ifeq ($(OPT_DEBUG),debug)
CXX_COMPILE = $(CXX) -g -O0 -std=c++0x -debug inline-debug-info -parallel-source-info=2 -debug all -openmp
LDFLAGS	= $(OMPT)
endif

CXXFLAGS 	= -DRAJA_PLATFORM_X86_AVX -DRAJA_COMPILER_ICC
LDPATH		=

endif



#
#  GNU RAJA_COMPILER (AVX 2)
# 
ifeq ($(RAJA_ARCH),x86_avx_gnu)
CXX 		= g++

ifeq ($(OPT_DEBUG),opt)
#
# Use this with GNU 4.7X and later
CXX_COMPILE = $(CXX) -g -Ofast -mavx -finline-functions -finline-limit=20000 -std=c++11 -fopenmp
#
# These should work with older compiler versions...
#CXX_COMPILE = $(CXX) -O3 -mavx -ansi-alias -std=c++0x -openmp
#CXX_COMPILE = $(CXX) -O3 -mavx -std=c++0x
##CXX_COMPILE = $(CXX) -O3 -mavx -ansi-alias -std=c++0x
##CXX_COMPILE = $(CXX) -O3 -mavx -finline-functions -finline-limit=20000 -fomit-frame-pointer -malign-double -ftree-vectorize -floop-block -ansi-alias -std=c++0x
LDFLAGS	= $(OMPT)
endif

ifeq ($(OPT_DEBUG),debug)
CXX_COMPILE = $(CXX) -g -O0 -std=c++0x -openmp
LDFLAGS	=
endif

CXXFLAGS 	= -DRAJA_PLATFORM_X86_AVX -DRAJA_COMPILER_GNU
LDPATH		=

endif


#
#  XLC 12 RAJA_COMPILER  (BG/Q)
# 
ifeq ($(RAJA_ARCH),bgq_xlc12)
CXX 		= mpixlcxx_r

ifeq ($(OPT_DEBUG),opt)
#CXX_COMPILE = $(CXX) -O3 -qarch=qp -qhot=novector -qsimd=auto -qlanglvl=extended0x -qstrict -qinline=20000 -qsmp=omp
#CXX_COMPILE = $(CXX) -O3 -qarch=qp -qhot=novector -qsimd=auto -qlanglvl=extended0x -qnostrict -qinline=20000 -qsmp=omp
CXX_COMPILE = $(CXX) -O3 -qarch=qp -qhot=novector -qsimd=auto -qlanglvl=extended0x -qnostrict -qinline=auto:level=10 -qsmp=omp
##
## USE THESE LINE TO GENERATE VECTORIZATION REPORT INFO...
#CXX_COMPILE = $(CXX) -O3 -qarch=qp -qhot=novector -qsimd=auto -qlanglvl=extended0x -qstrict -qinline=20000  -qlist -qsource -qlistopt -qreport
#CXX_COMPILE = $(CXX) -O3 -qarch=qp -qhot=novector -qsimd=auto -qlanglvl=extended0x -qnostrict -qinline=20000  -qlist -qsource -qlistopt -qreport
#CXX_COMPILE = $(CXX) -O3 -qarch=qp -qhot=novector -qsimd=auto -qlanglvl=extended0x -qinline=100 -qlistfmt=html=inlines
LDFLAGS	= 
endif

ifeq ($(OPT_DEBUG),debug)
CXX_COMPILE = $(CXX) -g -O0 -qarch=qp -qlanglvl=extended0x -qsmp=omp
LDFLAGS	= 
endif

CXXFLAGS 	= -DRAJA_PLATFORM_BGQ -DRAJA_COMPILER_XLC12
LDPATH		=

endif


#
#  Clang C++ compiler for BG/Q
# 
ifeq ($(RAJA_ARCH),bgq_clang)
#CXX 		= /usr/apps/gnu/clang/bin/mpiclang++11
#CXX 		= /usr/apps/gnu/clang/bin/bgclang++11

#Specific versions
#CXX            = /usr/apps/gnu/clang/r176829-20130309/bin/bgclang++11
#CXX             = /usr/apps/gnu/clang/r176751-20130307/bin/bgclang++11
#CXX             = /usr/apps/gnu/clang/r181589-20130510/bin/bgclang++11
CXX             = /usr/apps/gnu/clang/r189357-20130827/bin/bgclang++11

ifeq ($(OPT_DEBUG),opt)
#CXX_COMPILE = $(CXX) -O3 -finline-functions -finline-limit=20000 -fomit-frame-pointer -minline-all-stringops -malign-double -ftree-vectorize -floop-block -ansi-alias -std=c++0x
#Opt 3
#CXX_COMPILE = $(CXX) -O3 -finline-functions -finline-limit=20000 -malign-double -std=c++0x
#Opt 2
CXX_COMPILE = $(CXX) -O3 -finline-functions  -ffast-math -std=c++0x
#Opt 1
#CXX_COMPILE = $(CXX) -O3 -finline-functions  -std=c++0x
#Opt 0
#CXX_COMPILE = $(CXX) -O0 -finline-functions  -std=c++0x
LDFLAGS	=
endif

ifeq ($(OPT_DEBUG),debug)
CXX_COMPILE = $(CXX) -g -O0 -std=c++0x
LDFLAGS	=
endif

CXXFLAGS 	= -DRAJA_PLATFORM_BGQ -DRAJA_COMPILER_CLANG
LDPATH		=

endif 


#
#  GNU compiler for BG/Q
#
ifeq ($(RAJA_ARCH),bgq_gnu)
#CXX             = /bgsys/drivers/ppcfloor/gnu-linux/powerpc64-bgq-linux/bin/g++
CXX             = /usr/local/tools/compilers/ibm/mpicxx-4.7.2

#Previous versions

ifeq ($(OPT_DEBUG),opt)
CXX_COMPILE = $(CXX) -O3 -mcpu=a2 -mtune=a2 -finline-functions -finline-limit=20000 -std=c++0x -fopenmp

##LDFLAGS = -lmass
LDFLAGS = 
endif

ifeq ($(OPT_DEBUG),debug)
CXX_COMPILE = $(CXX) -g -O0 -std=c++0x -fopenmp
LDFLAGS =
endif

CXXFLAGS        = -DRAJA_PLATFORM_BGQ -DRAJA_COMPILER_GNU
LDPATH          =

endif

CXXFLAGS_BUILD = -I. -I$(RAJA_INC) $(CXXFLAGS) $(RAJA_RULES) $(LULESH_OPTS)

RAJAOBJS := $(patsubst %.cxx,%.o,$(wildcard $(RAJA_SRC)/*.cxx))

#TESTOBJS := $(patsubst %.cxx,%.o,$(wildcard ./*.cxx))

OBJFILES_PAR_FT = luleshRAJA-parallel-FT.o $(RAJAOBJS)
OBJFILES_PAR    = luleshRAJA-parallel.o LockFree.o LockFreeColor.o $(RAJAOBJS)
OBJFILES_SER    = luleshRAJA-serial.o LockFree.o LockFreeColor.o $(RAJAOBJS)

default:
	echo "Please specify version to build make serial, make parallel, or make parallel-ft"

parallel-ft: $(OBJFILES_PAR_FT)
	$(CXX_COMPILE) $(OBJFILES_PAR_FT) $(LDFLAGS) $(LDPATH) -o  lulesh-RAJA-parallel-ft.exe
	$(OMPT_POSTPROCESS) lulesh-RAJA-parallel-ft.exe 

parallel: $(OBJFILES_PAR)
	$(CXX_COMPILE) $(OBJFILES_PAR) $(LDFLAGS) $(LDPATH) -o  lulesh-RAJA-parallel.exe
	$(OMPT_POSTPROCESS) lulesh-RAJA-parallel.exe

serial: $(OBJFILES_SER)
	$(CXX_COMPILE) $(OBJFILES_SER) $(LDFLAGS) $(LDPATH) -o  lulesh-RAJA-serial.exe

## Uncomment this line to generate an executable to run...
%.o : %.cxx ; $(CXX_COMPILE) -c -o $@ $< $(CXXFLAGS_BUILD) 

## The following can be used to generate vectorization report and 
## assmbly code output for Intel compilers...
#%.o : %.cxx ; $(CXX_COMPILE) -S -o $@ $< $(CXXFLAGS_BUILD)
#%.o : %.cxx ; $(CXX_COMPILE) -S -vec-report3 -o $@ $< $(CXXFLAGS_BUILD)
#%.o : %.cxx ; $(CXX_COMPILE) -S -fcode-asm -vec-report3 -o $@ $< $(CXXFLAGS_BUILD)

## The following can be used to generate vectorization report and 
## assmbly code output for IBM XLC compilers...
## See compiler flag options in the appropriate section above.
#%.o : %.cxx ; $(CXX_COMPILE) -c -o $@ $< $(CXXFLAGS_BUILD)


clean-obj: 
	rm -rf *.o $(RAJA_SRC)/*.o

clean: clean-obj
	rm -rf *.s *.lst *.exe 
