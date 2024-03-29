# IocshDeclWrapper

Till Straumann, 2020/5

*IocshDeclWrapper* is hosted on
[github](https://github.com/paulscherrerinstitute/IocshDeclWrap.git)

## Introduction

This module provides a C++ header file which automates the
generation of boiler-plate code that is necessary for
interfacing a user function to the EPICS `iocsh`.

Wrapping a user function requires the following steps:

1. allocate and populate a `iocshFuncDef` struct
2. allocate and populate a `iocshArg*` array
3. allocate and populate `iocshArg` structs
4. point the `iocshArg*` array elements to the
   structs from step 3.
5. define a wrapper function which extracts function
   arguments from a `iocshArgBuf*` array, passes
   them to the user function and prints its result.
   The values of mutable arguments (passed by pointers
   or references) are also printed.
6. register the wrapper function and `iocshFuncDef`
   with `iocsh`. This is often done via a 'registrar'
   function that must be defined and declared in a
   'dbd' file.

The `iocshDeclWrapper.h` header automates steps 1-5
and optionally (6).

## Example

Assume you want to access a user function (e.g.,
written in 'C' ) from `iocsh`. The function
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
defines and `epicsExports` a registrar function:

    IOCSH_FUNC_WRAP_REGISTRAR(
       <registrar_name>,
       <any number of IOCSH_FUNC_WRAP() statements>
    )

The name of the registrar must also be declared in a `dbd`
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

## `IOCSH_FUNC_WRAP()` and `IOCSH_FUNC_WRAP_QUIET()`

This is the main macro that does the heavy lifting. It
is really easy to use:

    IOCSH_FUNC_WRAP( <function_to_wrap> { ',' <arg_help_string> } );

The first argument identifies the function which you want
to wrap. The declaration of this function must be available
from where you expand the macro (the C++ template needs to
know the exact types of the function's return value and arguments).

Additional arguments are optional; they can be used to give
explanatory names to the arguments which will show up when
the user types

    help <function_to_wrap>

in `iocsh`. By default, a string describing the type of the
argument is used.

If you don't want the return value of the user function to
be printed to the console then you use

    IOCSH_FUNC_WRAP_QUIET( <function_to_wrap>, { ',' <arg_help_string> } );

Use this variant also if no `Printer` (see below) for the
return type of your function is available and you want to
suppress the respective notice which is otherwise printed
to the console. Note that printing of the values of mutable
arguments is also suppressed by this variant.

### C++98 Restrictions

When using a C++98 compiler (no C++11 support) the user
function must have *external linkage*. I.e., it is not
possible to wrap a `static` function.

For C++98 the maximal number of arguments the user function
may have is limited to `IOCSH_FUNC_WRAP_MAX_ARGS` (currently
10). Under C++11 there is no static maximum.

## Namespace `IocshDeclWrapper`

All the templates are defined in their own

    namespace IocshDeclWrapper

This is important to keep in mind if you intend to write
your own specializations for some of the templates.

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

Specializations for dealing with pointers and references to
standard integral types, floats and strings are also provided.

If a particular type cannot be handled by the existing
templates then the you can easily define your own
specialization:

    template <> IocshDeclWrapper::Convert< MYTYPE, MYTYPE > {
    {
      static void setArg( iocshArg *a )
      {
        a->name = "MYTYPE"; /* set default name/help for arg;
                             * shown by iocsh help; this may
                             * be overridden by IOCSH_WRAP_FUNC()
                             * arguments for individual functions.
                             */
        a->type = <iocsh type that is most appropriate>;
      }

      MYTYPE getArg( const iocshArgBuf         *arg,
                     IocshDeclWrapper::Context *ctx,
                     int                        argNo)
      {
         /* extract function argument from *arg, convert to MYTYPE and return */
         return convert_to_MYTYPE( arg );
      }
    };

#### Handling Conversion Errors

In some cases `getArg` cannot convert a value. E.g., when converting
from a string argument and the string is `NULL` or ill-formatted.

In this case `getArg` may throw a `IocshDeclWrapper::ConversionError`
which is caught and results in an error message being printed to
the console. The user function is not executed if such an error
is thrown.

#### Preserving Converted Values in `Convert::getArg Context`

In more complex cases it is necessary to create an intermediate object
during the conversion process. Such an object may need to be passed
to the user function and therefore remain available outside of the scope of
`Context::getArg`. This is what the context is used for. It lets you allocate
something new and attach it to the context. The context is eventually destroyed
after the user function returns. This is always necessary if you cannot
pass an argument by value.

Here is the example where the user function expects a C++ `std::string *` pointer
argument; since the `iocshArgBuf` only holds a `const char *` pointer we have to
create a new `std::string` which must persist until the user function returns:

    template <> IocshDeclWrapper::Convert< string *, string *> {
      static void setArg(iocshArg *a)
      {
        a->name = "<string>";
        a->type = iocshArgString;
      }

      static string * getArg( const iocshArgBuf         *arg,
                              IocshDeclWrapper::Context *ctx,
                              int                        argNo )
      {
         /*
          * 'make' creates a new string and attaches it to the context
          * 'argNo' tells us which argument we are converting.
          *  This can be used for printing arguments that were
          *  possibly modified by the user function (pointer/reference
          *  arguments to non-const objects).
          */
         return ctx->make<string, const char*>( arg->sval, argNo );
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

Note: The templates are always instantiated with USER=0`, thus
a specialization for a different value would never been selected; the
USER argument simply provides the possibility of an additional level
of specialization.)

#### Mutable Arguments

Non-`const` arguments that are passed as pointers or references to
the user function can potentially be modified by the function.

Such arguments should always be created in the `Context` and initialized
from the values passed in by the user.

The wrapper prints the values of mutable arguments after the
user function returns.

### Printing User Function Results

The return value of a user function (provided that the function does
not return `void`) is passed to the `print` `static` member of the
`Printer` template.

Note that printing of results is optional. The templates support a
parameter that causes the user function's return value to be discarded.
In order to deactivate printing of its return value register your
function with the

    IOCSH_FUNC_WRAP_QUIET()

macro (see above).

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
types.

Specializing for `USER=0` you can override an existing implementation
of `PrinterBase`.

#### The `PrintFmts` Template

The default version of `PrinterBase` can handle any data type
which can be printed by `errlogPrintf` using a suitable format.

The purpose of `template <typename T> struct PrintFmts` template is
supplying a suitable `errlogPrintf` format for type `T`.

The `get()` member returns a `NULL` terminated array of formats and
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

However, you may define a specialization that is specific to a
particular user function, e.g.

    int mySpecialFunction();

    template <> class Printer<int, int(), mySpecialFunction> :
    : public PrinterBase<int, int> {
      public:

      static void print(const int &v) {
         /* Example: only a particular bit is of interest: */
         errlogPrintf( "mySpecialFunction returned: %s\n", v & (1<<8) ? "TRUE" : "FALSE" );
      }

    };

While you are not using `PrinterBase::print` in this case you
should still derive your `Printer` class from `PrinterBase`.
This ensures forward-compatibility if the `PrinterBase` class
is ever enhanced with additional members.

#### The `ArgPrinter` Template
This template is used to print the values of mutable arguments
after the user function returns. It can also be specialized for
a particular function signature. Consult the `Printer` template
section for more information.

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

## Overloaded Functions
The templates support overloaded functions. However, you cannot use the
simple `IOCSH_FUNC_WRAP()` macro but you must use

     IOCSH_FUNC_WRAP_OVLD( func, signature, name, argHelps... )

instead. `func` is the name of the function you want to wrap and
`signature` is the list of argument types that identifies the overloaded
variant you want to wrap. Since this list may contain commas it must be
enclosed in parenthesis. E.g.:

     IOCSH_FUNC_WRAP( myFunction, (int, const char*), "myFunction" );

(the parenthesis must be present even if your function has only one
or no argument.) Because `iocsh` does not support overloading functions
you must always specify the  `name` that `iocsh` shall use to identify
your wrapped function. If the `signature` is empty then it is automatically
resolved (which fails if the function is overloaded). In fact, the
`IOCSH_FUNC_WRAP(func,argHelps..)` macro is just a shorthand for

     IOCSH_FUNC_WRAP( func, , #func, argHelps )

and even `IOCSH_FUNC_WRAP_OVLD()` is defined in terms of a yet more
general macro

     IOCSH_FUNC_REGISTER_WRAPPER( func, signature, name, doPrintResult, argHelps... )

which takes an additional argument `doPrintResult` which may be `true` or `false`
and which defines whether `iocsh` should print the return value (and modified
arguments) of your functions.

## Wrapping of C++ Classes

Wrapping of C++ member functions is also possible.

    IOCSH_MEMBER_WRAP( &objectMap, class_type, member, argHelps... )

The member functions are wrapped in a C-function with the signature

    wrapper( const char *objName, args... )

where `args...` are the arguments taken by the member function.

E.g., a class member

    void clazz::doSometing(unsigned, double)

is invoked from the `iocsh`:

    clazz_doSomething objName 1 2.3.4.5

Note that wrapping C++ classes requires C++11 or later.

## Object Registry/Map

In order to properly associate the member functions with a C++ object the
user must provide a map or registry where the C++ objects can be looked up
by name. The map must have a `at(const std::string &)` or `at(const char *)`
member which returns a pointer to the target object.
E.g., for a user class `clazz` a

    std::map<const std::string, clazz*> myMap;

may be employed.

Because the `iocsh` does not support supplying the wrapper functions with any
context the map cannot be passed at run-time. Instead it is passed (under
the hood) as a (non-type) template parameter and thus statically bound to the
wrapper functions. This means, however, that the address of the map *must* be
static:

  - map cannot be allocated on the heap
  - map address cannot be computed etc.

### Overloaded Members

Overloaded members are supported; the user must identify the desired variant
with its argument types (consult the section about overloaded functions).
The convenience macro

    IOCSH_MEMBER_WRAP_OVLD( &objMap, class_type, member, signature, name, argHelps )

can be used in this case.

## Examples

Examples can be found in the test source file

    test/wrapper.cc
