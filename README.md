# IocshDeclWrapper

Till Straumann, 2020/3

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
   arguments from a `iocshArgBuf*` array and passes
   them to the user function.
6. register the wrapper function and `iocshFuncDef`
   with iocsh. This is often done via a 'registrar'
   function that must be defined and declared in a
   'dbd' file.

The `iocshDeclWrapper.h` header automates steps 1-5
and optionally (6).

## Example

Assume you want to access a user function (e.g.,
written in 'C' ) from iocsh. The function shall
be defined in a header file:

    myHeader.h:

      int myFunction(int debugLevel, const char *comment);

Wrapping requires C++; thus you must create or add to
a c++ file:

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
defines a registrar function:

    IOCSH_FUNC_WRAP_REGISTRAR(
       <registrar_name>,
       <any number of IOCSH_FUNC_WRAP() statements>
    )

The name of the registrar must also be declared in a 'dbd'
file.
Note that multiple `IOCSH_FUNC_WRAP()` statements may be
present in a single registrar. It is also not mandatory to
use the `IOCSH_FUNC_WRAP_REGISTRAR()` macro at all.
`IOCSH_FUNC_WRAP`() may be added to *any* registrar file, the
IOC's `main` function or any other appropriate place for
executing `iocshRegister()` (it must be a c++ source, though;
plain C does not work).
Again: it is perfectly fine to wrap C-functions; just
the `IOCSH_FUNC_WRAP()` macro must be expanded from C++ code.

## `IOCSH_FUNC_WRAP()`

This is the main macro that does the heavy lifting. It
is really easy to use:

    IOCSH_FUNC_WRAP( <function_to_wrap> { , <arg_help_string> } );

The first argument identifies the function which you want
to wrap. The declaration of this function must be available
from where you expand the macro (the c++ template needs to
know the exact type of the function's return value and arguments).

Additional arguments are optional; they can be used to give
explanatory names to the arguments which will show up when
the user types

    help <function_to_wrap>

in iocsh. By default, a string describing the type of the
argument is used.

### C++98 Restriction

When using a c++98 compiler (no c++11 support) the user
function must have *external linkage*. I.e., it is not
possible to wrap a `static` function.

## Template Specializations

The templates handle the passing of arguments between
iocshArgBuf and the user function. This is implemented
by template specializations for specific types. The
standard integral types as well as char* (strings)
and double/float are already implemented.

If a particular type cannot be handled by the existing
templates then the you can easily define your own
specializations:

    template <> IocshFuncWrapper::Convert< MYTYPE, MYTYPE > {
    {
      static void setArg( iocshArg *a )
      {
        a->name = "MYTYPE"; /* set default name/help for arg;
                             * shown by iocsh help but may
                             * be overridden by IOCSH_WRAP_FUNC
                             * argument
                             */
        a->type = <iocsh type that is most appropriate>;
      }

      MYTYPE getArg( const iocshArgBuf       *arg,
                     IocshFuncWrapperContext *ctx )
      {
         /* extract function argument from *arg, convert to MYTYPE and return */
         return convert_to_MYTYPE( arg );
      }
    };

### `Convert::getArg Context`

In more complex cases it is necessary to create an intermediate object
during the conversion process. Such an object may need to be passed
to the user function, i.e., be available outside of the scope of
`Context::getArg`. This is what the context is used for. It lets you allocate
something new and attach to the context. The context is then destroyed
after the user function is done executing. Here is the example
where the user function expects a c++ string pointer argument; since
the iocshArgBuf only holds a const char pointer we must create a
new string which must exist until the user function returns:

    template <> IocshFuncWrapper::Convert< string *, string *> {
      static void setArg(iocshArg *a)
      {
        a->name = "<string>";
        a->type = iocshArgString;
      }

      static string * getArg( const iocshArgBuf       *arg,
                              IocshFuncWrapperContext *ctx )
      {
         /* 'make' creates a new string and attaches it to the context */
         return ctx->make<string, const char*>( arg->sval );
      }
    };

The `make<typename T, typename I>` works automatically as long as
type `T` has a constructor that takes an argument of type `I`.

### `Convert<>` Template Arguments

The `Convert<typename T, typename R>` template expects two arguments.
This allows for SFINAE-style specializations that can handle multiple
related types. Consult the header for examples/guidance.
