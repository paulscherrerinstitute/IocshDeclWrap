#ifndef IOCSH_FUNC_WRAPPER_H
#define IOCSH_FUNC_WRAPPER_H

#include <epicsString.h>
#include <iocsh.h>
#include <initializer_list>

template <typename T> 
void iocshFuncWrapperSetArg(iocshArg *arg);

template <typename T>
T iocshFuncWrapperGetArg(const iocshArgBuf *);

template <typename T> 
iocshArg *iocshFuncWrapperMakeArg()
{
iocshArg *rval = new iocshArg;
	rval->name = 0;
	iocshFuncWrapperSetArg<T>( rval );
	return rval;
}

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

template <typename R, typename ...A>
static void
dispatch(R (*f)(A...), const iocshArgBuf *args)
{
	// FIXME: order of arg. evaluation undefined
		f( iocshFuncWrapperGetArg<A>( args++ )... );
}

template <typename RR, RR *p> void iocshFuncWrapperCall(const iocshArgBuf *args)
{
	dispatch(p, args);
}

#define R(x,argHelps...) do {\
	iocshRegister( iocshFuncWrapperBuildArgs( #x, x, { argHelps } ), iocshFuncWrapperCall<decltype(x), x> ); \
  } while (0)


#endif
