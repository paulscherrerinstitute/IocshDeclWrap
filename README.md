# IocshDeclWrapper

Till Straumann, 2020/3

This module is hosted on
[github](https://github.com/paulscherrerinstitute/IocshDeclWrap.git)

## Introduction

This module provides a C++ header file which automates the
generation of boiler-plate code that is necessary for
interfacing a user function to the EPICS iocsh.

Wrapping a user function requires the following steps:

1. allocate and populate a `iocshFuncDef` struct
2. allocate and populate a `iocshArg*` array
3. allocate and populate `iocshArg` structs
4. point the `iocshArg*` array elements to the
   structs from step 3.
5. define a wrapper function which extracts function
   arguments from a `iocshArgBuf*` array, passes
   them to the user function and prints its result.
6. register the wrapper function and `iocshFuncDef`
   with iocsh. This is often done via a 'registrar'
   function that must be defined and declared in a
   'dbd' file.

The `iocshDeclWrapper.h` header automates steps 1-5
and optionally (6).

## Example

Assume you want to access a user function (e.g.,
written in 'C' ) from iocsh. The function
`myFunction` shall be declared in a header file:

    myHeader.h:

      int myFunction(int debugLevel, const char *comment);

Wrapping requires C++; thus you must create or add to
a C++ file:

    extern "C" {
    #include <myHeader.h>
    };
    #include <iocshDeclWrapper.h>

    IOCSH_FUNC_WRAP_REGISTRAR( myRegistrar,
      IOCSH_FUNC_WRAP( myFunction, "debugLevel(int)", "comment(string)" );
      /* more functions may be registered here */
    )

And in a 'dbd' file you add:

    registrar( myRegistrar );

## `IOCSH_FUNC_WRAP_REGISTRAR()`

The `IOCSH_FUNC_WRAP_REGISTRAR` macro is a convenience macro which
defines and 'epicsExports' a registrar function:

    IOCSH_FUNC_WRAP_REGISTRAR(
       <registrar_name>,
       <any number of IOCSH_FUNC_WRAP() statements>
    )

The name of the registrar must also be declared in a 'dbd'
file.

Note that multiple `IOCSH_FUNC_WRAP()` statements may be
present in a single registrar. It is also not mandatory to
use the `IOCSH_FUNC_WRAP_REGISTRAR()` macro at all.
`IOCSH_FUNC_WRAP`() may be added to *any* registrar routine, the
IOC's `main` function or any other appropriate place for
executing `iocshRegister()` (it must be a C++ source, though;
plain C does not work).
Again: it is perfectly fine to wrap C-functions; just
the `IOCSH_FUNC_WRAP()` macro must be expanded from C++ code.

## `IOCSH_FUNC_WRAP()`

This is the main macro that does the heavy lifting. It
is really easy to use:

    IOCSH_FUNC_WRAP( <function_to_wrap> { , <arg_help_string> } );

The first argument identifies the function which you want
to wrap. The declaration of this function must be available
from where you expand the macro (the C++ template needs to
know the exact types of the function's return value and arguments).

Additional arguments are optional; they can be used to give
explanatory names to the arguments which will show up when
the user types

    help <function_to_wrap>

in iocsh. By default, a string describing the type of the
argument is used.

### C++98 Restrictions

When using a C++98 compiler (no C++11 support) the user
function must have *external linkage*. I.e., it is not
possible to wrap a `static` function.

For C++98 the maximal number of arguments the user function
may have is limited to `IOCSH_FUNC_WRAP_MAX_ARGS` (currently
10). Under C++11 there is no static maximum.

## Template Specializations

Converting function arguments and printing of function
results is handled by `Convert` and `Printer` templates,
respectively.

These templates are specialized for specific types. The
user may easily add specializations for types that are
not implemented yet or in order to override an existing
implementation.

Note that a suitable declaration of the template specialization
must be visible to the compiler from where `IOCSH_WRAP_FUNC()`
is expanded.

### Argument Conversions

The `Convert` templates handle the passing of arguments between
`iocshArgBuf` and the user function. This is implemented
by template specializations for specific types. The
standard integral types as well as `char*` (C-strings),
`std::string`, `double`, `float` and `std::complex` are
already implemented.

If a particular type cannot be handled by the existing
templates then the you can easily define your own
specialization:

    template <> IocshDeclWrapper::Convert< MYTYPE, MYTYPE > {
    {
      static void setArg( iocshArg *a )
      {
        a->name = "MYTYPE"; /* set default name/help for arg;
                             * shown by iocsh help but may
                             * be overridden by IOCSH_WRAP_FUNC()
                             * argument
                             */
        a->type = <iocsh type that is most appropriate>;
      }

      MYTYPE getArg( const iocshArgBuf         *arg,
                     IocshDeclWrapper::Context *ctx )
      {
         /* extract function argument from *arg, convert to MYTYPE and return */
         return convert_to_MYTYPE( arg );
      }
    };

#### Handling Conversion Errors

In some cases `getArg` cannot convert a value. E.g., when converting
from a string argument and the string is NULL or ill-formatted.

In this case `getArg` may throw a `IocshDeclWrapper::ConversionError`
which is caught and results in an error message being printed to
the console. The user function is not executed if such an error
is thrown.

#### `Convert::getArg Context`

In more complex cases it is necessary to create an intermediate object
during the conversion process. Such an object may need to be passed
to the user function and therefore remain available outside of the scope of
`Context::getArg`. This is what the context is used for. It lets you allocate
something new and attach it to the context. The context is eventually destroyed
after the user function returns.

Here is the example where the user function expects a C++ `std::string *` pointer
argument; since the `iocshArgBuf` only holds a `const char *` pointer we have to
create a new `std::string` which must exist until the user function returns:

    template <> IocshDeclWrapper::Convert< string *, string *> {
      static void setArg(iocshArg *a)
      {
        a->name = "<string>";
        a->type = iocshArgString;
      }

      static string * getArg( const iocshArgBuf         *arg,
                              IocshDeclWrapper::Context *ctx )
      {
         /* 'make' creates a new string and attaches it to the context */
         return ctx->make<string, const char*>( arg->sval );
      }
    };

The `make<typename T, typename I>` works automatically as long as
type `T` has a constructor that takes an argument of type `I`.

#### `Convert<>` Template Arguments

The `Convert<typename T, typename R, int USER>` template expects up
to three arguments.
The `R` type argument allows for SFINAE-style specializations that can
handle multiple related types. Consult the header for examples/guidance.

The `USER` argument can be used to add an even more specific
specialization in order to override a default converter.

If the user wishes, for example, to provide his/her own conversion for
integral types then the template may be specialized for the explicit
USER value 0:

    template <typename T> struct Convert<T, typename is_int<T>::type, 0> {
    }

### Printing User Function Results

The return value of a user function (provided that the function does
not return `void`) is passed to the `print` static member of the
`Printer` template.

#### The `PrinterBase` Template

`PrinterBase` is specialized for particular types of function return
values and handles printing for individual types.

    template <typename T, typename R, int USER = 0> class PrinterBase {
      public:
        /* The Reference<R>::const_type idiom is for C++98
         * and resolves to 'const R &' -- even if R is already
         * a reference type.
         */
	    static void print( typename Reference<R>::const_type r );
    };

You can implement a `PrinterBase` class specialization for your own
types or by specializing `USER` to 0 you can override an existing
implementation of `PrinterBase`.

#### The `PrintFmts` Template

The default version of `PrinterBase` can handle any data type
which can be printed by `errlogPrintf` using a suitable format.

The purpose of `template <typename T> struct PrintFmts` template is
supplying a suitable `errlogPrintf` format for type `T`.

The `get()` member returns a NULL terminated array of formats and
`PrinterBase` prints the function result for each one of these formats.
This can be used, e.g., to print decimal as well as hexadecimal
versions of a result.

As usual, the default specializations for the standard types
may be overridden by providing a more specific version for the
explicit `USER=0` argument.

#### The `Printer` Template

The purpose of `Printer` is adding the possibility for providing
a specialization for a specific user function.
	
The `Printer` template normally derives from its base class `PrinterBase`
and simply uses the base classes' `print` function:

    template <typename R, typename SIG, SIG *sig> class Printer
    : public PrinterBase<R, R>
    {
    };

However, you may define a specialization that is specific for a particular
user function, e.g.:

    int mySpecialFunction();

    template <> class Printer<int, int(), mySpecialFunction> : 
    : public PrinterBase<int, int> {
      public:

      static void print(const int &v) {
         /* Example: only a particular bit is of interest: */
         errlogPrintf( "mySpecialFunction returned: %s\n", v & (1<<8) ? "TRUE" : "FALSE" );
      }

    };
    
#### Printing Summary

Summarizing the purpose of the multiple print-related templates:

- The default implementation of `PrinterBase` can handle types
  which can be printed using standard `printf`-style formats.
- The default implementation of `PrinterBase` uses a suitable
  specialization of `PrintFmts` to obtain a `printf`-style format.
- In order to print a specific data type:
    - specialize `PrintFmts` if possible
    - if the type cannot be displayed by `printf` or you don't
      like the default version then specialize `PrinterBase`
      to handle the specific type.
- If you need a special `print` method specifically for your user-
  function then you should specialize `Printer` for your function.
