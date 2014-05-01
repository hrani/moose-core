//
//  moose/assert.hpp - MOOSE_ASSERT(expr)
//                     MOOSE_ASSERT_MSG(expr, msg)
//                     MOOSE_VERIFY(expr)
//
//  Copyright (c) 2001, 2002 Peter Dimov and Multi Media Ltd.
//  Copyright (c) 2007 Peter Dimov
//  Copyright (c) Beman Dawes 2011
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.moose.org/LICENSE_1_0.txt)
//
//  Note: There are no include guards. This is intentional.
//
//  See http://www.moose.org/libs/utility/assert.html for documentation.
//

//
// Stop inspect complaining about use of 'assert':
//
// mooseinspect:naassert_macro
//
// Log: Changed.
//

//--------------------------------------------------------------------------------------//
//                                     MOOSE_ASSERT                                     //
//--------------------------------------------------------------------------------------//

#undef MOOSE_ASSERT

#if defined(MOOSE_DISABLE_ASSERTS)

# define MOOSE_ASSERT(expr) ((void)0)

#elif defined(MOOSE_ENABLE_ASSERT_HANDLER)

#include "current_function.hpp"

namespace moose
{
  void assertion_failed(char const * expr,
                        char const * function, char const * file, long line); // user defined
} // namespace moose

#define MOOSE_ASSERT(expr) ((expr) \
  ? ((void)0) \
  : ::moose::assertion_failed(#expr, MOOSE_CURRENT_FUNCTION, __FILE__, __LINE__))

#else
# include <assert.h> // .h to support old libraries w/o <cassert> - effect is the same
# define MOOSE_ASSERT(expr) assert(expr)
#endif

//--------------------------------------------------------------------------------------//
//                                   MOOSE_ASSERT_MSG                                   //
//--------------------------------------------------------------------------------------//

# undef MOOSE_ASSERT_MSG

#if defined(MOOSE_DISABLE_ASSERTS) || defined(NDEBUG)

  #define MOOSE_ASSERT_MSG(expr, msg) ((void)0)

#elif defined(MOOSE_ENABLE_ASSERT_HANDLER)

  #include "current_function.hpp"

  namespace moose
  {
    void assertion_failed_msg(char const * expr, char const * msg,
                              char const * function, char const * file, long line); // user defined
  } // namespace moose

  #define MOOSE_ASSERT_MSG(expr, msg) ((expr) \
    ? ((void)0) \
    : ::moose::assertion_failed_msg(#expr, msg, MOOSE_CURRENT_FUNCTION, __FILE__, __LINE__))

#else
  #ifndef MOOSE_ASSERT_HPP
    #define MOOSE_ASSERT_HPP
    #include <cstdlib>
    #include <iostream>
    #include "current_function.hpp"

    //  IDE's like Visual Studio perform better if output goes to std::cout or
    //  some other stream, so allow user to configure output stream:
    #ifndef MOOSE_ASSERT_MSG_OSTREAM
    # define MOOSE_ASSERT_MSG_OSTREAM std::cerr
    #endif

    namespace moose
    { 
      namespace assertion 
      { 
        namespace detail
        {
          inline void assertion_failed_msg(char const * expr, char const * msg, char const * function,
            char const * file, long line)
          {
            MOOSE_ASSERT_MSG_OSTREAM
              << "***** Internal Program Error - assertion (" << expr << ") failed in "
              << function << ":\n"
              << file << '(' << line << "): " << msg << std::endl;
			#ifdef UNDER_CE
				// The Windows CE CRT library does not have abort() so use exit(-1) instead.
				std::exit(-1);
			#else
				std::abort();
			#endif
          }
        } // detail
      } // assertion
    } // detail
  #endif

  #define MOOSE_ASSERT_MSG(expr, msg) ((expr) \
    ? ((void)0) \
    : ::moose::assertion::detail::assertion_failed_msg(#expr, msg, \
          MOOSE_CURRENT_FUNCTION, __FILE__, __LINE__))
#endif

//--------------------------------------------------------------------------------------//
//                                     MOOSE_VERIFY                                     //
//--------------------------------------------------------------------------------------//

#undef MOOSE_VERIFY

#if defined(MOOSE_DISABLE_ASSERTS) || ( !defined(MOOSE_ENABLE_ASSERT_HANDLER) && defined(NDEBUG) )

# define MOOSE_VERIFY(expr) ((void)(expr))

#else

# define MOOSE_VERIFY(expr) MOOSE_ASSERT(expr)

#endif