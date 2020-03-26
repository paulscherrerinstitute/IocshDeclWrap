#ifndef IOCSH_FUNC_WRAPPER_H
#define IOCSH_FUNC_WRAPPER_H

#ifndef __cplusplus
#error "This header requires C++"
#endif

#include <epicsString.h>
#include <iocsh.h>
#include <string>
#include <vector>
#include <stdlib.h>

/* Helper templates that automate the generation of infamous boiler-plate code
 * which is necessary to wrap a user function for iocsh:
 *
 *  - a iocshFuncDef needs to be created and populated
 *  - an array of iocshArg pointers needs to be generated and populated
 *  - a wrapper needs to be written that extracts arguments from a iocshArgBuf
 *    and forwards them to the user function.
 *  - the 'iocshFuncDef' and wrapper need to be registered with iocsh
 *
 * Using the templates defined in this file makes the process easy:
 *
 * Assume you want to access
 *
 *   extern "C" int myDebugFunc(const char *prefix, int level, unsigned mask);
 *
 * from iocsh. You add the following line to any registrar:
 *
 *   IOCSH_FUNC_WRAP( myDebugFunc );
 *
 * Additional arguments may be added to provide 'help strings' describing the
 * function arguments:
 *
 *   IOCSH_FUNC_WRAP( myDebugFunc, "string_prefix", "int_debug-level", "int_bit-mask" );
 *
 * There is also a helper macro to define a registrar:
 *
 *  IOCSH_FUNC_WRAP_REGISTRAR( myRegistrar,
 *      IOCSH_FUNC_WRAP( myFirstFunction );
 *      IOCSH_FUNC_WRAP( myOtherFunction );
 *  )
 *
 * NOTE: you also need to add a line to your dbd file:
 *
 *  registrar( myRegistrar )
 *
 *******************************************************************************
 * NOTE: when compiling according to the c++98 standard the function that
 *       you want to wrap must have *external linkage*. This restriction
 *       was removed for c++11. (The function is used as a non-type template
 *       argument; ISO/IEC 14882: 14.3.2.1)
 *******************************************************************************
 *
 */

/*
 * Hold temporaries during execution of the user function;
 * in some cases we must make temporary copies of data.
 * May come in handy if a 'Convert::getArg' routine must do more
 * complex conversions...
 *
 * The idea is to let 'Convert::getArg' allocate a new object (via
 * the contexts's 'make' template) and attach ownership
 * to the 'context' which is destroyed once the user
 * function is done.
 */

namespace IocshFuncWrapper {

class ContextElBase {
public:
	/* base class just knows how to delete things */
	virtual ~ContextElBase() {}
};

/* 'Reference holder' for abitrary simple objects. Similar to
 * a (very dumb) smart pointer but without having to worry about 
 * C++11 or boost.
 */
template <typename T, typename I> class ContextEl : public ContextElBase {
	T *p_;
	ContextEl<T,I>(I inival)
	{
		p_ = new T( inival );
	}
public:

	T * p()
	{
		return p_;
	}

	virtual ~ContextEl()
	{
		delete p_;
	}

	friend class Context;
};

/*
 * Specialization for C-strings
 */
template <> class ContextEl<char[], const char *>: public ContextElBase {
	char *p_;
	ContextEl(const char *inival)
	{
		p_ = epicsStrDup( inival );
	}
public:

	typedef char type[];

	type * p()
	{
		return (type*)p_;
	}

	virtual ~ContextEl()
	{
		::free( p_ );
	}

	friend class Context;
};

class Context : public std::vector<ContextElBase*> {
public:
	virtual ~Context()
	{
		iterator it;
		for ( it = begin(); it != end(); ++it ) {
			delete *it;
		}
	}

	template <typename T, typename I> T * make(I i)
	{
		ContextEl<T,I> *el = new ContextEl<T,I>( i );
		push_back(el);
		return el->p();
	}
};

template <typename T, typename R> struct Convert {
	/*
	 * Set argument type and default name in iocshArg for type 'T'.
	 */
	static void setArg(iocshArg *arg);
	/*
	 * Retrieve argument from iocshArgBuf and convert into
	 * target type 'R'. May allocate objects in 'Context'.
	 */
	static R    getArg(const iocshArgBuf *, Context *);
};

/*
 * Allocate a new iocshArg struct and call
 * Convert::setArg() for type 'T'
 */
template <typename T> 
iocshArg *makeArg(const char *aname = 0)
{
	iocshArg *rval = new iocshArg;

	rval->name = 0;
	Convert<T,T>::setArg( rval );
	if ( aname ) {
		rval->name = aname;
	}
	return rval;
}

/*
 * Template specializations for basic arithmetic types
 * and strings.
 */

/*
 * SFINAE helpers (we don't necessarily have C++11 and avoid boost)
 */
template <typename T> struct is_int;
#define IOCSH_DECL_WRAPPER_IS_INT(x) \
	template <> struct is_int<x> {   \
		typedef x type;              \
		static const char *name()    \
		{                            \
			return "<" #x ">";       \
        }                            \
	}

/* All of these are extracted from iocshArgBuf->ival */
IOCSH_DECL_WRAPPER_IS_INT(unsigned long long);
IOCSH_DECL_WRAPPER_IS_INT(         long long);
IOCSH_DECL_WRAPPER_IS_INT(unsigned      long);
IOCSH_DECL_WRAPPER_IS_INT(              long);
IOCSH_DECL_WRAPPER_IS_INT(unsigned       int);
IOCSH_DECL_WRAPPER_IS_INT(               int);
IOCSH_DECL_WRAPPER_IS_INT(unsigned     short);
IOCSH_DECL_WRAPPER_IS_INT(             short);
IOCSH_DECL_WRAPPER_IS_INT(unsigned      char);
IOCSH_DECL_WRAPPER_IS_INT(              char);
IOCSH_DECL_WRAPPER_IS_INT(              bool);

#undef IOSH_DECL_WRAPPER_IS_INT

/* Specialization for all integral types */
template <typename T> struct Convert<T, typename is_int<T>::type> {

	typedef typename is_int<T>::type type;

	static void setArg(iocshArg *a)
	{
		a->name = is_int<T>::name();
		a->type = iocshArgInt;
	}

	static type getArg(const iocshArgBuf *arg, Context *ctx)
	{
		return (type)arg->ival;
	}
};

template <typename T> struct is_str;
template <> struct is_str <std::string       > { typedef       std::string   type; };
template <> struct is_str <std::string      &> { typedef       std::string & type; };

template <typename T> struct is_strp;
template <> struct is_strp<std::string      *> { typedef       std::string * type; };
template <> struct is_strp<const std::string*> { typedef const std::string * type; };

static void setArgStr(iocshArg *a)
{
	a->name = "<string>";
	a->type = iocshArgString;
}

/* Specialization for strings and string reference */
template <typename T> struct Convert<T, typename is_str<T>::type> {

	typedef typename is_str<T>::type type;

	static void setArg(iocshArg *a)
	{
		setArgStr( a );
	}

	static type getArg(const iocshArgBuf *a, Context *ctx)
	{
		return * ctx->make<std::string, const char *>( a->sval ? a->sval : "" );
	}
};

/* Specialization for string pointer */
template <typename T> struct Convert<T, typename is_strp<T>::type> {

	typedef typename is_strp<T>::type type;

	static void setArg(iocshArg *a)
	{
		setArgStr( a );
	}

	static type getArg(const iocshArgBuf *a, Context *ctx)
	{
		return a->sval ? ctx->make<std::string, const char *>( a->sval ) : 0;
	}
};

/* Specialization for C-strings */
template <> struct Convert<const char *, const char *> {

	static void setArg(iocshArg *a)
	{
		setArgStr( a );
	}

	static const char * getArg(const iocshArgBuf *a, Context *ctx)
	{
		return a->sval;
	}
};

/* Specialization for mutable C-strings */
template <> struct Convert<char *, char *> {

	static void setArg(iocshArg *a)
	{
		setArgStr( a );
	}

	static char * getArg(const iocshArgBuf *a, Context *ctx)
	{
		return (char*)ctx->make<char[], const char *>( a->sval );
	}
};

/* Specialization for floats */
template <typename T> struct is_flt;
template <> struct is_flt<float > { typedef float  type; static const char *name() { return "<float>" ; } };
template <> struct is_flt<double> { typedef double type; static const char *name() { return "<double>"; } };

template <typename T> struct Convert<T, typename is_flt<T>::type> {
	typedef typename is_flt<T>::type type;

	static void setArg(iocshArg *a)
	{
		a->name = is_flt<T>::name();
		a->type = iocshArgDouble;
	}

	static type getArg(const iocshArgBuf *a, Context *ctx)
	{
		return (type) a->dval;
	}
};

class FuncDef {
private:
	iocshFuncDef *def;
	iocshArg    **args;

	FuncDef(const FuncDef&);
	FuncDef &operator=(const FuncDef&);

public:
	FuncDef(const char *fname, int nargs)
	{
		def         = new iocshFuncDef;
		def->name   = epicsStrDup( fname );
		def->nargs  = nargs;
        args        = new iocshArg*[def->nargs]; 
		def->arg    = args;

		for ( int i = 0; i < def->nargs; i++ ) {
			args[i] = 0;
		}
	}

	void setArg(int i, iocshArg *p)
	{
		if ( i >= 0 && i < def->nargs ) {
			args[i] = p;
		}
	}

	int getNargs()
	{
		return def ? def->nargs : 0;
	}

	iocshFuncDef *release()
	{
		iocshFuncDef *rval = def;
		def = 0;
		return rval;
	}

	
	~FuncDef()
	{
		if ( def ) {
			for ( int i = 0; i < def->nargs; i++ ) {
				if ( args[i] ) {
					// should delete args[i]->name but we don't know if that has been strduped yet
					delete args[i];
				}
			}
			delete [] args;
			delete def->name;
			delete def;
		}
	}
};

iocshFuncDef *buildFuncDef(const char *fname, int nargs)
{
	return 0;
}

};

#if __cplusplus >= 201103L

#include <initializer_list>

template <typename R, typename ...A>
iocshFuncDef  *iocshFuncWrapperBuildArgs( const char *fname, R (*f)(A...), std::initializer_list<const char *> argNames )
{
	std::initializer_list<const char*>::const_iterator it;

	using namespace IocshFuncWrapper;

	FuncDef     funcDef( fname, sizeof...(A) );
	// use array initializer to ensure order of execution
	iocshArg   *argp[ funcDef.getNargs() ] = { (makeArg<A>())... };
	it              = argNames.begin();
	for ( int i = 0; i < funcDef.getNargs(); i++ ) {
		if ( it != argNames.end() ) {
			if (*it) {
				argp[i]->name = *it;
			}
			++it;
		}
		if ( argp[i]->name ) {
			argp[i]->name = epicsStrDup( argp[i]->name );
		}
		funcDef.setArg(i, argp[i]);
	}
	return funcDef.release();
}

/*
 * Helper struct to build a parameter pack of integers for indexing
 * 'iocshArgBuf'.
 * This is necessary because the order in which arguments passed
 * to a function is not defined; we therefore cannot simply
 * expand
 *
 *  f( Convert<A,A>::getArg( args++ )... )
 *
 * because we don't know in which order A... will be evaluated.
 */
template <typename ...A> struct IocshFuncWrapperArgOrder {

	template <int ... I> struct Index {
		/* Once we have a pair of parameter packs: A... I... we can expand */
		template <typename R> static void dispatch(R (*f)(A...), const iocshArgBuf *args)
		{
			IocshFuncWrapper::Context ctx;
			f( IocshFuncWrapper::Convert<A,A>::getArg( &args[I], &ctx )... );
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

#else  /* __cplusplus < 201103L */

#include <stdarg.h>

/*
template <typename R>
iocshFuncDef *iocshFuncWrapperBuildArgs( const char *fname, R(*f)(void), ...)
{
	IocshFuncWrapperDef funcDef( fname, 0 );
	return funcDef.release();
}

template <typename R> class IocshFuncWrapperCaller<R, void> {
public:
	typedef R(*type)(void);
	template <type func> static void call(const iocshArgBuf *args)
	{
		func();
	}
};

template <typename R> IocshFuncWrapperCaller<R,void> iocshFuncWrapperMakeCaller(R (*f)(void))
{
	return IocshFuncWrapperCaller<R,void>();
}
*/

template <typename G>
iocshFuncDef  *iocshFuncWrapperBuildArgs(G guess, const char *fname, const char **argNames)
{
	using namespace IocshFuncWrapper;
	int             N = G::N;
	FuncDef         funcDef( fname, N );
	const char      *aname;
	int             n;
	va_list         ap;

//	va_start(ap, fname);
#undef va_arg
#define va_arg(...) *argNames++

	aname = va_arg( ap, const char *);
	n     = -1;

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A0_T>( aname ) );
	if ( aname )
		aname = va_arg( ap, const char * );

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A1_T>( aname ) );
	if ( aname )
		aname = va_arg( ap, const char * );

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A2_T>( aname ) );
	if ( aname )
		aname = va_arg( ap, const char * );

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A3_T>( aname ) );
	if ( aname )
		aname = va_arg( ap, const char * );

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A4_T>( aname ) );
	if ( aname )
		aname = va_arg( ap, const char * );

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A5_T>( aname ) );
	if ( aname )
		aname = va_arg( ap, const char * );

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A6_T>( aname ) );
	if ( aname )
		aname = va_arg( ap, const char * );

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A7_T>( aname ) );
	if ( aname )
		aname = va_arg( ap, const char * );

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A8_T>( aname ) );
	if ( aname )
		aname = va_arg( ap, const char * );

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A9_T>( aname ) );
	if ( aname )
		aname = va_arg( ap, const char * );

done:

	va_end( ap );

	return funcDef.release();
}

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6, typename A7, typename A8, typename A9>
class IocshFuncWrapperCaller
{
public:
	typedef R(*type)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9);
	typedef A0 A0_T;
	typedef A1 A1_T;
	typedef A2 A2_T;
	typedef A3 A3_T;
	typedef A4 A4_T;
	typedef A5 A5_T;
	typedef A6 A6_T;
	typedef A7 A7_T;
	typedef A8 A8_T;
	typedef A9 A9_T;

	const static int N = 10;

	/* We go though all the hoops so that we can create a iocshCallFunc wrapper;
	 * this would not be necessary if EPICS would allow us to pass context to
	 * the iocshCallFunc but that is not possible.
	 * The idea here is to use the function we want to wrap as a non-type template
	 * parameter to 'personalize' the callFunc.
	 *
	 * We use the iocshFuncWrapperMakeCaller() template to generate the correct
	 * template of this class. Once the return and argument types are known
	 * we can obtain the iocshCallFunc pointer like this:
	 *
	 *  iocshFuncWrapperMakeCaller( funcWeWantToWrap ).call<funcWeWantToWrap>
	 *
	 * This instantiates this template and the instantiation 'knows' that 'func'
	 * is in fact 'funcWeWantToWrap'...
	 */
	template <type func> static void call(const iocshArgBuf *args)
	{
		IocshFuncWrapper::Context ctx;
		func( IocshFuncWrapper::Convert<A0,A0>::getArg( &args[0], &ctx ),
		      IocshFuncWrapper::Convert<A1,A1>::getArg( &args[1], &ctx ),
		      IocshFuncWrapper::Convert<A2,A2>::getArg( &args[2], &ctx ),
		      IocshFuncWrapper::Convert<A3,A3>::getArg( &args[3], &ctx ),
		      IocshFuncWrapper::Convert<A4,A4>::getArg( &args[4], &ctx ),
		      IocshFuncWrapper::Convert<A5,A5>::getArg( &args[5], &ctx ),
		      IocshFuncWrapper::Convert<A6,A6>::getArg( &args[6], &ctx ),
		      IocshFuncWrapper::Convert<A7,A7>::getArg( &args[7], &ctx ),
		      IocshFuncWrapper::Convert<A8,A8>::getArg( &args[8], &ctx ),
		      IocshFuncWrapper::Convert<A9,A9>::getArg( &args[9], &ctx ) );
	}
};

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6, typename A7, typename A8>
class IocshFuncWrapperCaller<R, A0, A1, A2, A3, A4, A5, A6, A7, A8, void>
{
public:
	typedef R(*type)(A0, A1, A2, A3, A4, A5, A6, A7, A8);
	typedef A0   A0_T;
	typedef A1   A1_T;
	typedef A2   A2_T;
	typedef A3   A3_T;
	typedef A4   A4_T;
	typedef A5   A5_T;
	typedef A6   A6_T;
	typedef A7   A7_T;
	typedef A8   A8_T;
	typedef void A9_T;

	const static int N = 9;

	template <type func> static void call(const iocshArgBuf *args)
	{
		IocshFuncWrapper::Context ctx;
		func( IocshFuncWrapper::Convert<A0,A0>::getArg( &args[0], &ctx ),
		      IocshFuncWrapper::Convert<A1,A1>::getArg( &args[1], &ctx ),
		      IocshFuncWrapper::Convert<A2,A2>::getArg( &args[2], &ctx ),
		      IocshFuncWrapper::Convert<A3,A3>::getArg( &args[3], &ctx ),
		      IocshFuncWrapper::Convert<A4,A4>::getArg( &args[4], &ctx ),
		      IocshFuncWrapper::Convert<A5,A5>::getArg( &args[5], &ctx ),
		      IocshFuncWrapper::Convert<A6,A6>::getArg( &args[6], &ctx ),
		      IocshFuncWrapper::Convert<A7,A7>::getArg( &args[7], &ctx ),
		      IocshFuncWrapper::Convert<A8,A8>::getArg( &args[8], &ctx ) );
	}
};

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6, typename A7>
class IocshFuncWrapperCaller<R, A0, A1, A2, A3, A4, A5, A6, A7, void, void>
{
public:
	typedef R(*type)(A0, A1, A2, A3, A4, A5, A6, A7);
	typedef A0   A0_T;
	typedef A1   A1_T;
	typedef A2   A2_T;
	typedef A3   A3_T;
	typedef A4   A4_T;
	typedef A5   A5_T;
	typedef A6   A6_T;
	typedef A7   A7_T;
	typedef void A8_T;
	typedef void A9_T;

	const static int N = 8;

	template <type func> static void call(const iocshArgBuf *args)
	{
		IocshFuncWrapper::Context ctx;
		func( IocshFuncWrapper::Convert<A0,A0>::getArg( &args[0], &ctx ),
		      IocshFuncWrapper::Convert<A1,A1>::getArg( &args[1], &ctx ),
		      IocshFuncWrapper::Convert<A2,A2>::getArg( &args[2], &ctx ),
		      IocshFuncWrapper::Convert<A3,A3>::getArg( &args[3], &ctx ),
		      IocshFuncWrapper::Convert<A4,A4>::getArg( &args[4], &ctx ),
		      IocshFuncWrapper::Convert<A5,A5>::getArg( &args[5], &ctx ),
		      IocshFuncWrapper::Convert<A6,A6>::getArg( &args[6], &ctx ),
		      IocshFuncWrapper::Convert<A7,A7>::getArg( &args[7], &ctx ) );
	}
};

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6>
class IocshFuncWrapperCaller<R, A0, A1, A2, A3, A4, A5, A6, void, void, void>
{
public:
	typedef R(*type)(A0, A1, A2, A3, A4, A5, A6);
	typedef A0   A0_T;
	typedef A1   A1_T;
	typedef A2   A2_T;
	typedef A3   A3_T;
	typedef A4   A4_T;
	typedef A5   A5_T;
	typedef A6   A6_T;
	typedef void A7_T;
	typedef void A8_T;
	typedef void A9_T;

	const static int N = 7;

	template <type func> static void call(const iocshArgBuf *args)
	{
		IocshFuncWrapper::Context ctx;
		func( IocshFuncWrapper::Convert<A0,A0>::getArg( &args[0], &ctx ),
		      IocshFuncWrapper::Convert<A1,A1>::getArg( &args[1], &ctx ),
		      IocshFuncWrapper::Convert<A2,A2>::getArg( &args[2], &ctx ),
		      IocshFuncWrapper::Convert<A3,A3>::getArg( &args[3], &ctx ),
		      IocshFuncWrapper::Convert<A4,A4>::getArg( &args[4], &ctx ),
		      IocshFuncWrapper::Convert<A5,A5>::getArg( &args[5], &ctx ),
		      IocshFuncWrapper::Convert<A6,A6>::getArg( &args[6], &ctx ) );
	}
};

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5>
class IocshFuncWrapperCaller<R, A0, A1, A2, A3, A4, A5, void, void, void, void>
{
public:
	typedef R(*type)(A0, A1, A2, A3, A4, A5);
	typedef A0   A0_T;
	typedef A1   A1_T;
	typedef A2   A2_T;
	typedef A3   A3_T;
	typedef A4   A4_T;
	typedef A5   A5_T;
	typedef void A6_T;
	typedef void A7_T;
	typedef void A8_T;
	typedef void A9_T;

	const static int N = 6;

	template <type func> static void call(const iocshArgBuf *args)
	{
		IocshFuncWrapper::Context ctx;
		func( IocshFuncWrapper::Convert<A0,A0>::getArg( &args[0], &ctx ),
		      IocshFuncWrapper::Convert<A1,A1>::getArg( &args[1], &ctx ),
		      IocshFuncWrapper::Convert<A2,A2>::getArg( &args[2], &ctx ),
		      IocshFuncWrapper::Convert<A3,A3>::getArg( &args[3], &ctx ),
		      IocshFuncWrapper::Convert<A4,A4>::getArg( &args[4], &ctx ),
		      IocshFuncWrapper::Convert<A5,A5>::getArg( &args[5], &ctx ) );
	}
};

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4>
class IocshFuncWrapperCaller<R, A0, A1, A2, A3, A4, void, void, void, void, void>
{
public:
	typedef R(*type)(A0, A1, A2, A3, A4);
	typedef A0   A0_T;
	typedef A1   A1_T;
	typedef A2   A2_T;
	typedef A3   A3_T;
	typedef A4   A4_T;
	typedef void A5_T;
	typedef void A6_T;
	typedef void A7_T;
	typedef void A8_T;
	typedef void A9_T;

	const static int N = 5;

	template <type func> static void call(const iocshArgBuf *args)
	{
		IocshFuncWrapper::Context ctx;
		func( IocshFuncWrapper::Convert<A0,A0>::getArg( &args[0], &ctx ),
		      IocshFuncWrapper::Convert<A1,A1>::getArg( &args[1], &ctx ),
		      IocshFuncWrapper::Convert<A2,A2>::getArg( &args[2], &ctx ),
		      IocshFuncWrapper::Convert<A3,A3>::getArg( &args[3], &ctx ),
		      IocshFuncWrapper::Convert<A4,A4>::getArg( &args[4], &ctx ) );
	}
};

template <typename R, typename A0, typename A1, typename A2, typename A3>
class IocshFuncWrapperCaller<R, A0, A1, A2, A3, void, void, void, void, void, void>
{
public:
	typedef R(*type)(A0, A1, A2, A3);
	typedef A0   A0_T;
	typedef A1   A1_T;
	typedef A2   A2_T;
	typedef A3   A3_T;
	typedef void A4_T;
	typedef void A5_T;
	typedef void A6_T;
	typedef void A7_T;
	typedef void A8_T;
	typedef void A9_T;

	const static int N = 4;

	template <type func> static void call(const iocshArgBuf *args)
	{
		IocshFuncWrapper::Context ctx;
		func( IocshFuncWrapper::Convert<A0,A0>::getArg( &args[0], &ctx ),
		      IocshFuncWrapper::Convert<A1,A1>::getArg( &args[1], &ctx ),
		      IocshFuncWrapper::Convert<A2,A2>::getArg( &args[2], &ctx ),
		      IocshFuncWrapper::Convert<A3,A3>::getArg( &args[3], &ctx ) );
	}
};

template <typename R, typename A0, typename A1, typename A2>
class IocshFuncWrapperCaller<R, A0, A1, A2, void, void, void, void, void, void, void>
{
public:
	typedef R(*type)(A0, A1, A2);
	typedef A0   A0_T;
	typedef A1   A1_T;
	typedef A2   A2_T;
	typedef void A3_T;
	typedef void A4_T;
	typedef void A5_T;
	typedef void A6_T;
	typedef void A7_T;
	typedef void A8_T;
	typedef void A9_T;

	const static int N = 3;

	template <type func> static void call(const iocshArgBuf *args)
	{
		IocshFuncWrapper::Context ctx;
		func( IocshFuncWrapper::Convert<A0,A0>::getArg( &args[0], &ctx ),
		      IocshFuncWrapper::Convert<A1,A1>::getArg( &args[1], &ctx ),
		      IocshFuncWrapper::Convert<A2,A2>::getArg( &args[2], &ctx ) );
	}
};

template <typename R, typename A0, typename A1>
class IocshFuncWrapperCaller<R, A0, A1, void, void, void, void, void, void, void, void>
{
public:
	typedef R(*type)(A0, A1);
	typedef A0   A0_T;
	typedef A1   A1_T;
	typedef void A2_T;
	typedef void A3_T;
	typedef void A4_T;
	typedef void A5_T;
	typedef void A6_T;
	typedef void A7_T;
	typedef void A8_T;
	typedef void A9_T;

	const static int N = 2;

	template <type func> static void call(const iocshArgBuf *args)
	{
		IocshFuncWrapper::Context ctx;
		func( IocshFuncWrapper::Convert<A0,A0>::getArg( &args[0], &ctx ),
		      IocshFuncWrapper::Convert<A1,A1>::getArg( &args[1], &ctx ) );
	}
};


template <typename R, typename A0>
class IocshFuncWrapperCaller<R, A0, void, void, void, void, void, void, void, void, void>
{
public:
	typedef R(*type)(A0);
	typedef A0   A0_T;
	typedef void A1_T;
	typedef void A2_T;
	typedef void A3_T;
	typedef void A4_T;
	typedef void A5_T;
	typedef void A6_T;
	typedef void A7_T;
	typedef void A8_T;
	typedef void A9_T;

	const static int N = 1;

	template <type func> static void call(const iocshArgBuf *args)
	{
		IocshFuncWrapper::Context ctx;
		func( IocshFuncWrapper::Convert<A0,A0>::getArg( &args[0], &ctx ) );
	}
};

template <typename R>
class IocshFuncWrapperCaller<R, void, void, void, void, void, void, void, void, void, void>
{
public:
	typedef R(*type)(void);

	typedef void A0_T;
	typedef void A1_T;
	typedef void A2_T;
	typedef void A3_T;
	typedef void A4_T;
	typedef void A5_T;
	typedef void A6_T;
	typedef void A7_T;
	typedef void A8_T;
	typedef void A9_T;

	const static int N = 0;

	template <type func> static void call(const iocshArgBuf *args)
	{
		func( );
	}
};

/* Function templates which use argument deduction to find the argument and return types
 * of function 'f'.
 * Return a properly parametrized IocshFuncWrapperCaller<> object. This allows us to infer
 * the argument and return types of the function we want to wrap without the user having
 * to give us any more information.
 *
 * Build a iocshFuncDef containing descriptions of the arguments 'funcWeWantToWrap' expects.
 *
 *	iocshFuncWrapperBuildArgs( iocshFuncWrapperMakeCaller(funcWeWantToWrap), ... )
 *
 * Build a iocshCallFunc. Calling this wrapper results in 'funcWeWantToWrap' being executed
 * with arguments extracted from a iocshArgBuf:
 *
 *	iocshFuncWrapperMakeCaller(funcWeWantToWrap).call<funcWeWantToWrap>
 *
 */
template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6, typename A7, typename A8, typename A9>
IocshFuncWrapperCaller<R,A0,A1,A2,A3,A4,A5,A6,A7,A8,A9> iocshFuncWrapperMakeCaller(R (*f)(A0,A1,A2,A3,A4,A5,A6,A7,A8,A9))
{
	return IocshFuncWrapperCaller<R,A0,A1,A2,A3,A4,A5,A6,A7,A8,A9>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6, typename A7, typename A8>
IocshFuncWrapperCaller<R,A0,A1,A2,A3,A4,A5,A6,A7,A8,void> iocshFuncWrapperMakeCaller(R (*f)(A0,A1,A2,A3,A4,A5,A6,A7,A8))
{
	return IocshFuncWrapperCaller<R,A0,A1,A2,A3,A4,A5,A6,A7,A8,void>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6, typename A7>
IocshFuncWrapperCaller<R,A0,A1,A2,A3,A4,A5,A6,A7,void,void> iocshFuncWrapperMakeCaller(R (*f)(A0,A1,A2,A3,A4,A5,A6,A7))
{
	return IocshFuncWrapperCaller<R,A0,A1,A2,A3,A4,A5,A6,A7,void,void>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6>
IocshFuncWrapperCaller<R,A0,A1,A2,A3,A4,A5,A6,void,void,void> iocshFuncWrapperMakeCaller(R (*f)(A0,A1,A2,A3,A4,A5,A6))
{
	return IocshFuncWrapperCaller<R,A0,A1,A2,A3,A4,A5,A6,void,void,void>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5>
IocshFuncWrapperCaller<R,A0,A1,A2,A3,A4,A5,void,void,void,void> iocshFuncWrapperMakeCaller(R (*f)(A0,A1,A2,A3,A4,A5))
{
	return IocshFuncWrapperCaller<R,A0,A1,A2,A3,A4,A5,void,void,void,void>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4>
IocshFuncWrapperCaller<R,A0,A1,A2,A3,A4,void,void,void,void,void> iocshFuncWrapperMakeCaller(R (*f)(A0,A1,A2,A3,A4))
{
	return IocshFuncWrapperCaller<R,A0,A1,A2,A3,A4,void,void,void,void,void>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3>
IocshFuncWrapperCaller<R,A0,A1,A2,A3,void,void,void,void,void,void> iocshFuncWrapperMakeCaller(R (*f)(A0,A1,A2,A3))
{
	return IocshFuncWrapperCaller<R,A0,A1,A2,A3,void,void,void,void,void,void>();
}

template <typename R, typename A0, typename A1, typename A2>
IocshFuncWrapperCaller<R,A0,A1,A2,void,void,void,void,void,void,void> iocshFuncWrapperMakeCaller(R (*f)(A0,A1,A2))
{
	return IocshFuncWrapperCaller<R,A0,A1,A2,void,void,void,void,void,void,void>();
}

template <typename R, typename A0, typename A1>
IocshFuncWrapperCaller<R,A0,A1,void,void,void,void,void,void,void,void> iocshFuncWrapperMakeCaller(R (*f)(A0,A1))
{
	return IocshFuncWrapperCaller<R,A0,A1,void,void,void,void,void,void,void,void>();
}

template <typename R, typename A0>
IocshFuncWrapperCaller<R,A0,void,void,void,void,void,void,void,void,void> iocshFuncWrapperMakeCaller(R (*f)(A0))
{
	return IocshFuncWrapperCaller<R,A0,void,void,void,void,void,void,void,void,void>();
}

template <typename R>
IocshFuncWrapperCaller<R,void,void,void,void,void,void,void,void,void,void> iocshFuncWrapperMakeCaller(R (*f)(void))
{
	return IocshFuncWrapperCaller<R,void,void,void,void,void,void,void,void,void,void>();
}

#define IOCSH_FUNC_WRAP_MAX_ARGS 10

#define IOCSH_FUNC_WRAP(x,argHelps...) do { \
	const char *argNames[IOCSH_FUNC_WRAP_MAX_ARGS + 1] = { argHelps }; \
	iocshRegister( iocshFuncWrapperBuildArgs( iocshFuncWrapperMakeCaller(x), #x, argNames ), iocshFuncWrapperMakeCaller(x).call<x> ); \
	} while (0)

#endif /* __cplusplus >= 201103L */

#define IOCSH_FUNC_WRAP_REGISTRAR( registrarName, wrappers... ) \
static void registrarName() \
{ \
  wrappers \
} \
epicsExportRegistrar( registrarName ); \

#endif
