
//=============================================================================
//
//  Helper Functions for binary reading / writing
//
//=============================================================================

#ifndef ASTER_TOPOLOGY_BINARY_HELPER_HH
#define ASTER_TOPOLOGY_BINARY_HELPER_HH


//== INCLUDES =================================================================

#include <AsterTopology/Core/System/config.h>
// -------------------- STL
#if defined( OM_CC_MIPS )
#  include <stdio.h>
#else
#  include <cstdio>
#endif
#include <iosfwd>
// -------------------- AsterTopology


//== NAMESPACES ===============================================================

namespace AsterTopology {
namespace IO {


//=============================================================================


/** \name Handling binary input/output.
    These functions take care of swapping bytes to get the right Endian.
*/
//@{

//-----------------------------------------------------------------------------


/** Binary read a \c short from \c _is and perform byte swapping if
    \c _swap is true */
short int read_short(FILE* _in, bool _swap=false);

/** Binary read an \c int from \c _is and perform byte swapping if
    \c _swap is true */
int read_int(FILE* _in, bool _swap=false);

/** Binary read a \c float from \c _is and perform byte swapping if
    \c _swap is true */
float read_float(FILE* _in, bool _swap=false);

/** Binary read a \c double from \c _is and perform byte swapping if
    \c _swap is true */
double read_double(FILE* _in, bool _swap=false);

/** Binary read a \c short from \c _is and perform byte swapping if
    \c _swap is true */
short int read_short(std::istream& _in, bool _swap=false);

/** Binary read an \c int from \c _is and perform byte swapping if
    \c _swap is true */
int read_int(std::istream& _in, bool _swap=false);

/** Binary read a \c float from \c _is and perform byte swapping if
    \c _swap is true */
float read_float(std::istream& _in, bool _swap=false);

/** Binary read a \c double from \c _is and perform byte swapping if
    \c _swap is true */
double read_double(std::istream& _in, bool _swap=false);


/** Binary write a \c short to \c _os and perform byte swapping if
    \c _swap is true */
void write_short(short int _i, FILE* _out, bool _swap=false);

/** Binary write an \c int to \c _os and perform byte swapping if
    \c _swap is true */
void write_int(int _i, FILE* _out, bool _swap=false);

/** Binary write a \c float to \c _os and perform byte swapping if
    \c _swap is true */
void write_float(float _f, FILE* _out, bool _swap=false);

/** Binary write a \c double to \c _os and perform byte swapping if
    \c _swap is true */
void write_double(double _d, FILE* _out, bool _swap=false);

/** Binary write a \c short to \c _os and perform byte swapping if
    \c _swap is true */
void write_short(short int _i, std::ostream& _out, bool _swap=false);

/** Binary write an \c int to \c _os and perform byte swapping if
    \c _swap is true */
void write_int(int _i, std::ostream& _out, bool _swap=false);

/** Binary write a \c float to \c _os and perform byte swapping if
    \c _swap is true */
void write_float(float _f, std::ostream& _out, bool _swap=false);

/** Binary write a \c double to \c _os and perform byte swapping if
    \c _swap is true */
void write_double(double _d, std::ostream& _out, bool _swap=false);

//@}


//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
#endif // ASTER_TOPOLOGY_MESHREADER_HH defined
//=============================================================================

