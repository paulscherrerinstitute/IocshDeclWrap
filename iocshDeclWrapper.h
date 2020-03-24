#ifndef IOCSH_FUNC_WRAPPER_H
#define IOCSH_FUNC_WRAPPER_H

#include <epicsString.h>
#include <iocsh.h>
#include <initializer_list>

/*
 * Set argument type and default name in iocshArg for type 'T'.
 * This template is to be specialized for multiple 'T'.
 */
template <typename T> 
void iocshFuncWrapperSetArg(iocshArg *arg);

/*
 * Extract type 'T' from iocshArgBuf.
 * This template is to be specialized for multiple 'T'.
 */
template <typename T>
T iocshFuncWrapperGetArg(const iocshArgBuf *);

/*
 * Allocate a new iocshArg struct and call
 * iocshFuncWrapperSetArg() for type 'T'
 */
template <typename T> 
iocshArg *iocshFuncWrapperMakeArg()
{
	iocshArg *rval = new iocshArg;

	rval->name = 0;
	iocshFuncWrapperSetArg<T>( rval );
	return rval;
}

/*
 * Template specializations for basic arithmetic types
 * and strings.
 */
template <>
void iocshFuncWrapperSetArg<const char *>(iocshArg *a)
{
	if ( ! a->name ) {
		a->name = "<string>";
	}
	a->type = iocshArgString;
}

template <>
const char * iocshFuncWrapperGetArg<const char *>(const iocshArgBuf *arg)
{
	return arg->sval;
}

template <>
void iocshFuncWrapperSetArg<int>(iocshArg *a)
{
	if ( ! a->name ) {
		a->name = "<int>";
	}
	a->type = iocshArgInt;
}

template <>
int iocshFuncWrapperGetArg<int>(const iocshArgBuf *arg)
{
	return arg->ival;
}

template <>
void iocshFuncWrapperSetArg<unsigned int>(iocshArg *a)
{
	if ( ! a->name ) {
		a->name = "<uint>";
	}
	a->type = iocshArgInt;
}

template <>
unsigned int iocshFuncWrapperGetArg<unsigned int>(const iocshArgBuf *arg)
{
	return arg->ival;
}


template <typename R, typename ...A>
iocshFuncDef  *iocshFuncWrapperBuildArgs( const char *fname, R (*f)(A...), std::initializer_list<const char *> argNames )
{
	std::initializer_list<const char*>::const_iterator it;
	iocshFuncDef   *funcDef;
	funcDef         = new iocshFuncDef;
	funcDef->name   = epicsStrDup( fname );
	funcDef->nargs  = sizeof...(A);
	iocshArg **args = new iocshArg*[funcDef->nargs]; 
	funcDef->arg    = args;
	// use array initializer to ensure order of execution
	iocshArg   *argp[ funcDef->nargs ] = { (iocshFuncWrapperMakeArg<A>())... };
	it              = argNames.begin();
	for ( int i = 0; i < funcDef->nargs; i++ ) {
		if ( it != argNames.end() ) {
			if (*it) {
				argp[i]->name = *it;
			}
			++it;
		}
		if ( argp[i]->name ) {
			argp[i]->name = epicsStrDup( argp[i]->name );
		}
		args[i] = argp[i];
	}
	return funcDef;
}

/*
 * Helper struct to build a parameter pack of integers for indexing
 * 'iocshArgBuf'.
 * This is necessary because the order in which arguments passed
 * to a function is not defined; we therefore cannot simply
 * expand
 *
 *  f( iocshFuncWrapperGetArg<A>( args++ )... )
 *
 * because we don't know in which order A... will be evaluated.
 */
template <typename ...A> struct IocshFuncWrapperArgOrder {

	template <int ... I> struct Index {
		/* Once we have a pair of parameter packs: A... I... we can expand */
		template <typename R> static void dispatch(R (*f)(A...), const iocshArgBuf *args)
		{
			f( iocshFuncWrapperGetArg<A>( &args[I] )... );
		}
	};

	/* Recursively build I... */
	template <int i, int ...I> struct C {
		template <typename R> static void concat(R (*f)(A...), const iocshArgBuf *args)
		{
			C<i-1, i-1, I...>::concat(f, args);
		}
	};

	/* Specialization for terminating the recursion */
	template <int ...I> struct C<0, I...> {
		template <typename R> static void concat(R (*f)(A...), const iocshArgBuf *args)
		{
			Index<I...>::dispatch(f, args);
		}
	};

	/* Build index pack and dispatch 'f' */
	template <typename R> static void arrange( R(*f)(A...), const iocshArgBuf *args)
	{
		C<sizeof...(A)>::concat( f, args );
	}
};

template <typename R, typename ...A>
static void
iocshFuncWrapperDispatch(R (*f)(A...), const iocshArgBuf *args)
{
	IocshFuncWrapperArgOrder<A...>::arrange( f, args );
}

template <typename RR, RR *p> void iocshFuncWrapperCall(const iocshArgBuf *args)
{
	iocshFuncWrapperDispatch(p, args);
}

#define IOCSH_FUNC_WRAP(x,argHelps...) do {\
	iocshRegister( iocshFuncWrapperBuildArgs( #x, x, { argHelps } ), iocshFuncWrapperCall<decltype(x), x> ); \
  } while (0)


#endif
