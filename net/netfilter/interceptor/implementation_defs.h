/**
   @copyright
   Copyright (c) 2011 - 2015, INSIDE Secure Oy. All rights reserved.
*/

/*
  This header provides implementation definitions required by software
  modules. The definitions are used only in the implementation code
  including module internal header and implementation files. This
  header file shall not be included from any public header.

  The definitions provided are:

  - possibility to use static inline for function definitions
  - MIN() and MAX() macros
  - BIT_N constants
  - BIT_CLEAR, BIT_SET, BIT_IS_SET macros
  - definition of NULL
  - definition of offsetof
  - macro PARAMETER_NOT_USED
  - TODO macro
  - Debug logging macros
  - macro PRECONDITION
  - macro POSTCONDITION
  - macro PANIC

  This particular version of the public definitions header is based on
  including relevant standard C99 headers.
*/

#ifndef IMPLEMENTATIONDEFS_H
#define IMPLEMENTATIONDEFS_H


#include "public_defs.h"
#include "implementation_config.h"

/*
  inline
*/

/*
  restrict
 */
#if !defined( __STDC_VERSION__) || __STDC_VERSION__ < 199901L

#ifdef __GNUC__
#define restrict __restrict__
#else
#define restrict
#endif

#endif

/* MIN, MAX

   Evaluate to maximum or minimum of two values.

   NOTE:

   warning for side-effects on the following two macros since the
   arguments are evaluated twice changing this to inline functions is
   problematic because of type incompatibilities
*/
#undef MIN
#undef MAX
#define MIN(_x, _y) ((_x) < (_y) ? (_x) : (_y))
#define MAX(_x, _y) ((_x) > (_y) ? (_x) : (_y))


/*
  ARRAY_ITEM_COUNT

  Evaluates to count of items in an array.
*/
#define ARRAY_ITEM_COUNT(array) (sizeof (array) / sizeof((array)[0]))


/* BIT_n

   Bit positions for 32-bit values.

   using postfix "U" to be compatible with uint32
   ("UL" is not needed and gives lint warning)
*/
#define BIT_0   0x00000001U
#define BIT_1   0x00000002U
#define BIT_2   0x00000004U
#define BIT_3   0x00000008U
#define BIT_4   0x00000010U
#define BIT_5   0x00000020U
#define BIT_6   0x00000040U
#define BIT_7   0x00000080U
#define BIT_8   0x00000100U
#define BIT_9   0x00000200U
#define BIT_10  0x00000400U
#define BIT_11  0x00000800U
#define BIT_12  0x00001000U
#define BIT_13  0x00002000U
#define BIT_14  0x00004000U
#define BIT_15  0x00008000U
#define BIT_16  0x00010000U
#define BIT_17  0x00020000U
#define BIT_18  0x00040000U
#define BIT_19  0x00080000U
#define BIT_20  0x00100000U
#define BIT_21  0x00200000U
#define BIT_22  0x00400000U
#define BIT_23  0x00800000U
#define BIT_24  0x01000000U
#define BIT_25  0x02000000U
#define BIT_26  0x04000000U
#define BIT_27  0x08000000U
#define BIT_28  0x10000000U
#define BIT_29  0x20000000U
#define BIT_30  0x40000000U
#define BIT_31  0x80000000U
#define BIT_ALL 0xffffffffU


/* BIT_CLEAR

   Clear bits enabled __bit in variable __bits.
*/
#define BIT_CLEAR(__bits, __bit)                                \
    do {                                                        \
        uint32_t  *__bit_p = (uint32_t *)(void*) &(__bits);     \
        *__bit_p &= ~(__bit);                                   \
    } while (0)


/* BIT_SET

   Enable bits enabled __bit in variable __bits.
*/
#define BIT_SET(__bits, bit)                                    \
    do {                                                        \
        uint32_t  *__bit_p = (uint32_t *)(void*) &(__bits);     \
        *__bit_p |= (bit);                                      \
    } while (0)


/* BIT_IS_SET

   Evaluate to true if one or more bits enabled in __bit are
   enabled in __bits.
*/
#define BIT_IS_SET(__bits, __bit) (((__bits) & (__bit)) != 0)


/* ALIGNED_TO

   Return true if Value is Alignment aligned; false otherwise. That is
   if Value modulo Alignment equals 0.

   NOTE: Only Alignments that are power of 2 are supported. False is
   returned is Alignment is not a power of 2.
*/
#define ALIGNED_TO(Value, Alignment)                                    \
    ((((((Alignment) & (Alignment - 1)) == 0) &&                        \
       ((uintptr_t)(Value) & (Alignment - 1))) == 0) ? true : false)


/* PARAMETER_NOT_USED()

   To mark function input parameters that are purposely not used in
   the function and to prevent compiler warnings on them.
*/
#define PARAMETER_NOT_USED(__identifier)        \
    do { if (__identifier) {} } while (0)


/* IDENTIFIER_NOT_USED()

   To mark function input parameters or local variables that are
   purposely not used in the function and
   to prevent compiler warnings on them.
*/
#define IDENTIFIER_NOT_USED(__identifier)        \
    do { if (__identifier) {} } while (0)


/* NULL
 */
#ifndef NULL
#define NULL 0
#endif


/* offsetof
 */
#ifndef offsetof
#define offsetof(type, member) ((size_t) &(((type *) NULL)->member))
#endif


/* alignmentof
 */
#define alignmentof(type) (offsetof(struct { char x; type y; }, y))

/*
  Define C99 fixed with integer print formatting macros.

  PRId8    PRId16    PRIi32    PRIi64
  PRIi8    PRIi16    PRId32    PRId64
  PRIo8    PRIo16    PRIo32    PRIo64
  PRIu8    PRIu16    PRIu32    PRIu64
  PRIx8    PRIx16    PRIx32    PRIx64
  PRIX8    PRIX16    PRIX32    PRIX64

*/

#ifdef INTTYPES_HEADER
#include INTTYPES_HEADER
#endif


/* memset, memcpy
 */
#ifdef STRING_HEADER
#include STRING_HEADER
#endif


/* snprintf
 */
#ifdef SNPRINTF_HEADER
#include SNPRINTF_HEADER
#endif


/* TODO

   Macro to mark things-to-be-done. Does not produce run time
   behaviour. May produce a compile time warning.

   The arguments are:
   todo            a string describing what needs to be done
*/
#define TODO(__todo) TODO_IMPLEMENTATION(__todo)


/* DEBUG_FAIL
   DEBUG_HIGH
   DEBUG_MEDIUM
   DEBUG_LOW

   Macros for debug logging. The macros are thread safe in any
   environment it matters. For debug log filtering purposes the macros
   expect __DEBUG_MODULE__ to be defined. The __DEBUG_MODULE__ must be
   defined as an identifier not a string. For example,

   #define __DEBUG_MODULE__ ipsecsocket

   The log priority levels are embedded to the macro names. The intended
   uses of the debugging levels are:

   FAIL

   For logging runtime failure that cause a function to return an
   error. Failures that a caused by bugs should be handled with
   appropriate macro to abort execution.

   HIGH

   For logging normal rare events of high importance.

   MEDIUM

   For logging normal events that do not cause flooding the debug
   log.

   LOW

   For logging frequent events.


   The arguments for the macros are:
   level            preprocessing token defining debug level and
                    is one of FAIL, HIGH, MEDIUM, LOW

   flow             a preprocessing token defining debug flow and
                    may be any valid identifier string e.g.
                    DEBUG_FAIL(INIT, "Error happened!");

   ...              includes a printf format string and possible
                    arguments
*/
#define DEBUG_FAIL(flow, ...)   DEBUG_IMPLEMENTATION(FAIL, flow, __VA_ARGS__)
#define DEBUG_HIGH(flow, ...)   DEBUG_IMPLEMENTATION(HIGH, flow, __VA_ARGS__)
#define DEBUG_MEDIUM(flow, ...) DEBUG_IMPLEMENTATION(MEDIUM, flow, __VA_ARGS__)
#define DEBUG_LOW(flow, ...)    DEBUG_IMPLEMENTATION(LOW, flow, __VA_ARGS__)


#define DEBUG_DUMP(flow, dumper, data, len, ...) \
    DEBUG_DUMP_IMPLEMENTATION(flow, dumper, data, len, __VA_ARGS__)

#define DEBUG_DUMP_LINE(context, ...) \
    DEBUG_DUMP_LINE_IMPLEMENTATION(context, __VA_ARGS__)

/*
  DEBUG_STRBUF_GET

  Macro that returns a handle to a debug string buffer allocator. The
  actual implementation may vary.

  May be used only in the parameter list of
  DEBUG_{FAIL,HIGH,MEDIUM,LOW} macros.

  Sample usage:

  DEBUG_FAIL(
          DEFAULT,
          "Error happened: %s.",
          error_to_str(
                  DEBUG_STRBUF_GET(),
                  error_code));
*/
#define DEBUG_STRBUF_GET() DEBUG_STRBUF_GET_IMPLEMENTATION()


/*
  DEBUG_STRBUF_TYPE

  Macro that defines the return type of the DEBUG_STRBUF_GET() macro.

  Sample usage:

  const char *error_to_str(DEBUG_STRBUF_TYPE, int error_code);
 */
#define DEBUG_STRBUF_TYPE DEBUG_STRBUF_TYPE_IMPLEMENTATION


/*
  DEBUG_STRBUF_ALLOC(dsb, n)

  Macro that allocates buffer for a string of n bytes from dsb of type
  DEBUG_STRBUF_TYPE. That is allocation if for n chars plus nul
  termination character. If allocation fails, NULL is returned.

  The allocation must not be freed or stored anywhere. The allocation
  can be returned to be used as variadic argument for %s formatter for
  the DEBUG_{FAIL,HIGH,MEDIUM,LOW} macros.

  Sample usage:

  const char *error_to_str(DEBUG_STRBUF_TYPE dsb, int error_code)
  {
    char *buf = DEBUG_STRBUF_ALLOC(dsb, 10);

    if (buf != NULL)
      {
        snprintf(buf, 10, "Error %d", error_code);

        return buf;
      }

      ...
  }
*/
#define DEBUG_STRBUF_ALLOC(dsb, n) DEBUG_STRBUF_ALLOC_IMPLEMENTATION(dsb, n)


/*
  DEBUG_STRBUF_BUFFER_GET(dsb, buf_p, len_p)
  DEBUG_STRBUF_BUFFER_COMMIT(dsb, len)

  Macro that returns pointer to and size of a buffer available for
  formatter.

  The pointer is always valid, but the returned length may be zero.

  The pointer must not be freed or stored anywhere. The pointer
  can be returned to be used as variadic argument for %s formatter for
  the DEBUG_{FAIL,HIGH,MEDIUM,LOW} macros.

  If the pointer is returned from the formatter function the space
  actually used must be commited to the strbuf using
  DEBUG_STRBUF_BUFFER_COMMIT().

  Sample usage:

  const char *error_to_str(DEBUG_STRBUF_TYPE dsb, int error_code)
  {
    char *buf;
    int len;

    DEBUG_STRBUF_BUFFER_GET(dsb, &buf, &len);

    len = snprintf(buf, len, "Error %d", error_code);

    DEBUG_STRBUF_BUFFER_COMMIT(dsb, len + 1);

    return buf;
  }
*/
#define DEBUG_STRBUF_BUFFER_GET(dsb, buf_p, len_p)           \
    DEBUG_STRBUF_BUFFER_GET_IMPLEMENTATION(dsb, buf_p, len_p)

#define DEBUG_STRBUF_BUFFER_COMMIT(dsb, len)                 \
    DEBUG_STRBUF_BUFFER_COMMIT_IMPLEMENTATION(dsb, len)


/*
  DEBUG_STRBUF_ERROR

  Macro to be returned instead of string when DEBUG_STRBUF_ALLOC()
  returns NULL.


  Sample usage:

  const char *error_to_str(DEBUG_STRBUF_TYPE dsb, int error_code)
  {
    char *buf = DEBUG_STRBUF_ALLOC(dsb, 10);

    if (buf == NULL)
      {
        return DEBUG_STRBUF_ERROR;
      }

      ...
  }
*/
#define DEBUG_STRBUF_ERROR DEBUG_STRBUF_ERROR_IMPLEMENTATION


#define STR_BOOL(b) STR_BOOL_IMPLEMENTATION(b)

#define STR_HEXBUF(buf, len) STR_HEXBUF_IMPLEMENTATION(buf, len)

/* ASSERT()

   The ASSERT macro provides a "normal" assert.
*/
#define ASSERT(__condition)                   \
    ASSERT_IMPLEMENTATION(                    \
            (__condition),                    \
            "assertion failed")

/* PRECONDITION

   To define preconditions of a function. Preconditions are conditions
   that the implementation of the function assumes to hold when the
   function is called. Preconditions are not to be checked by
   production builds and no errors are returned when preconditions do
   not hold. Defining preconditions with this macro documents clearly
   what is assumed to hold and provides a possibility to assert such
   conditions on debug builds.
*/
#define PRECONDITION(__condition)            \
    ASSERT_IMPLEMENTATION(                   \
            (__condition),                   \
            "precondition failed")

/* POSTCONDITION

   To define postconditions of a function. Same rationale as for
   PRECONDITION above.
*/
#define POSTCONDITION(__condition)           \
    ASSERT_IMPLEMENTATION(                   \
            (__condition),                   \
            "postcondition failed")


/* PANIC()

   Macro to be called from code branches that should never be reached.
   The macro aborts the execution calling. By default this is done calling
   DEBUG_abort() function declared below in this header.
*/
#define PANIC(...)                           \
    PANIC_IMPLEMENTATION(__VA_ARGS__)


/* COMPILE_GLOBAL_ASSERT

   Macro to make global scope compile time assertions. The condition
   must be a constant C expression (expression that can be evaluated
   at compile time) evaluating to true when the required condition
   holds and to false otherwise.

   When condition evaluates to true the macro has no effect.

   When condition evaluates to false the compilation fails.
*/
#define COMPILE_GLOBAL_ASSERT(condition)                \
    COMPILE_GLOBAL_ASSERT_IMPLEMENTATION(condition)


/* COMPILE_STATIC_ASSERT

   Macro to make function scope compile time assertions. The condition
   must be a constant C expression (expression that can be evaluated
   at compile time) evaluating to true when the required condition
   holds and to false otherwise.

   When condition evaluates to true the macro has no effect.

   When condition evaluates to false the compilation fails.
*/
#define COMPILE_STATIC_ASSERT(condition)                \
    COMPILE_STATIC_ASSERT_IMPLEMENTATION(condition)

#ifdef DEBUG_IMPLEMENTATION_HEADER
#include DEBUG_IMPLEMENTATION_HEADER
#endif

#endif /* IMPLEMENTATIONDEFS_H */
