#ifndef IOCSH_FUNC_WRAPPER_H
#define IOCSH_FUNC_WRAPPER_H

/* Till Straumann <till.straumann@psi.ch>, PSI, 2020 */

#ifndef __cplusplus
#error "This header requires C++"
#endif

#include <epicsString.h>
#include <epicsExport.h>
#include <errlog.h>
#include <iocsh.h>
#include <string>
#include <stdexcept>
#include <vector>
#include <complex>
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

namespace IocshDeclWrapper {

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

/*
 * Context holds the object pointers until the Context is
 * destroyed. Its destructor eventually delets all objects
 * held by the Context.
 */
class Context : public std::vector<ContextElBase*> {
public:
	virtual ~Context()
	{
		iterator it;
		for ( it = begin(); it != end(); ++it ) {
			delete *it;
		}
	}

	/*
	 * Create a new object of type T and attach to
	 * the Context.
	 * RETURNS: pointer to the new object.
	 */
	template <typename T, typename I> T * make(I i)
	{
		ContextEl<T,I> *el = new ContextEl<T,I>( i );
		push_back(el);
		return el->p();
	}
};

class ConversionError : public std::runtime_error {
public:
	ConversionError( const std::string & msg )
	: runtime_error( msg )
	{
	}

	ConversionError( const char * msg )
	: runtime_error( std::string( msg ) )
	{
	}
};

/*
 * Converter to map between user function arguments and iocshArg/iocshArgBuf
 */
template <typename T, typename R = T, int USER=0> struct Convert {
	/*
	 * Set argument type and default name in iocshArg for type 'T'.
	 */
	static void setArg(iocshArg *arg);
	/*
	 * Retrieve argument from iocshArgBuf and convert into
	 * target type 'R'. May allocate objects in 'Context'.
	 *
	 * May throw 'ConversionError'.
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
	Convert<T>::setArg( rval );
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
template <typename T, int USER> struct Convert<T, typename is_int<T>::type, USER> {

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
template <> struct is_str <const std::string > { typedef const std::string   type; };
template <> struct is_str <std::string      &> { typedef       std::string & type; };
template <> struct is_str <const std::string&> { typedef const std::string & type; };

template <typename T> struct is_strp;
template <> struct is_strp<std::string      *> { typedef       std::string * type; };
template <> struct is_strp<const std::string*> { typedef const std::string * type; };

template <typename T> struct is_chrp;
template <> struct is_chrp<char             *> { typedef       char        * type; };
template <> struct is_chrp<const char       *> { typedef const char        * type; };

static void setArgStr(iocshArg *a)
{
	a->name = "<string>";
	a->type = iocshArgString;
}

/* Specialization for strings and string reference */
template <typename T, int USER> struct Convert<T, typename is_str<T>::type, USER> {

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
template <typename T, int USER> struct Convert<T, typename is_strp<T>::type, USER> {

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
template <int USER> struct Convert<const char *, const char *, USER> {

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
template <int USER> struct Convert<char *, char *, USER> {

	static void setArg(iocshArg *a)
	{
		setArgStr( a );
	}

	static char * getArg(const iocshArgBuf *a, Context *ctx)
	{
		/* must make a copy of the const char * help in the iocshArgBuf */
		return (char*)ctx->make<char[], const char *>( a->sval );
	}
};

/* Specialization for floats */
template <typename T> struct is_flt;
template <> struct is_flt<float > { typedef float  type; static const char *name() { return "<float>" ; } };
template <> struct is_flt<double> { typedef double type; static const char *name() { return "<double>"; } };

template <typename T, int USER> struct Convert<T, typename is_flt<T>::type, USER> {
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

template <typename T> struct is_cplx;
template <> struct is_cplx< std::complex<float> > {
	typedef std::complex<float> type;
	static const char          *fmt() { return "%g j %g"; }
};

template <> struct is_cplx< std::complex<double> > {
	typedef std::complex<double> type;
	static const char           *fmt() { return "%lg j %lg"; }
};

template <> struct is_cplx< std::complex<long double> > {
	typedef std::complex<long double> type;
	static const char                *fmt() { return "%Lg j %Lg"; }
};

/* Specialization for std::complex */
template <typename T, int USER> struct Convert<T, typename is_cplx<T>::type, USER> {
	typedef typename is_cplx<T>::type type;

	static void setArg(iocshArg *a)
	{
		a->name = "complex number as string: \"<real> j <imag>\"";
		a->type = iocshArgString;
	}

	static type getArg(const iocshArgBuf *a, Context *ctx)
	{
		typename type::value_type r, i;
		if ( ! a->sval || 2 != sscanf( a->sval, is_cplx<T>::fmt(), &r, &i ) ) {
			throw ConversionError("unable to scan argument into '%g j %g' format");
		}
		return type( r, i );
	}
};

/* Work-around for C++89
 *  The idiom
 *
 *     Reference< some_type > :: type
 *
 * yields always a reference type -- even if 'some_type' already
 * is a reference type.
 *
 * With C++11 this is not necessary but it still works...
 */
template <typename T> struct Reference {
	typedef T&             type;
	typedef const T &const_type;
};

template <typename T> struct Reference<T&> {
	typedef T&             type;
	typedef const T &const_type;
};

/*
 * Handling the result of the user function: we use an (overridable) 'print'
 * function that we apply to the result.
 * The problem here is that print( f() ) is an error if f() returns 'void'.
 * We use a trick: since the 'comma' expression '(x, f())' is valid even
 * if f() returns void we can use an object 'x' from a class that overrides
 * the ',' operator. Note that an overridden 'operator,' does not quite behave
 * like the normal one (see C++ standard) -- but the trick leaves the built-in
 * version in place for the 'void' variant while overriding everything else:
 */

template <typename R, bool PRINT=true> class EvalResult {
public:
	/* Printer function */
	typedef void (*PrinterType) (typename Reference<R>::const_type);
private:
	PrinterType pri;
public:
	EvalResult(PrinterType pri)
	: pri( pri )
	{
	}

	/* Our ',' operator applies the printer function */
	void operator,(typename Reference<R>::const_type result)
	{
		if ( PRINT ) {
			pri( result );
		}
	}
};

/*
 * Specialization for 'void' results. *remove* the overridden
 * comma operator.
 * The print function type is just a dummy...
 */
template <bool PRINT> class EvalResult<void, PRINT> {
public:
	/* Printer function */
	typedef void *PrinterType;

	EvalResult(PrinterType pri)
	{
	}

	/* Use the built-in 'operator,' */
};

/*
 * For the default Printer implementation we
 * use specialized 'format strings'.
 * The 'get()' method returns NULL terminated array
 * of format strings. The printer will print the
 * result for each of the formats. This allows, e.g.,
 * for printing an integer result in decimal as well
 * as in hexadecimal.
 */

template <typename T, typename R = T, int USER = 0> struct PrintFmts {
	static const char **get()
	{
		return 0;
	}
};

/*
 * Provide partial specializations for basic
 * types; the user may provide a more specific
 * specialization to override the formats...
 */
template <typename T, int USER> struct PrintFmts<T, bool, USER> {
	static const char **get()
	{
		static const char *r [] = { "%d", 0 };
		return r;
	}
};

template <typename T, int USER> struct PrintFmts<T, char, USER> {
	static const char **get()
	{
		static const char *r [] = { "%c", " (0x%02hhx)", 0 };
		return r;
	}
};

template <typename T, int USER> struct PrintFmts<T, short, USER> {
	static const char **get()
	{
		static const char *r [] = { "%hi", " (0x%04hx)", 0 };
		return r;
	}
};

template <typename T, int USER> struct PrintFmts<T, int, USER> {
	static const char **get()
	{
		static const char *r [] = { "%i", " (0x%08x)", 0 };
		return r;
	}
};

template <typename T, int USER> struct PrintFmts<T, long, USER> {
	static const char **get()
	{
		static const char *r [] = { "%li", " (0x%08lx)", 0 };
		return r;
	}
};

template <typename T, int USER> struct PrintFmts<T, long long, USER> {
	static const char **get()
	{
		static const char *r [] = { "%lli", " (0x%16llx)", 0 };
		return r;
	}
};

template <typename T, int USER> struct PrintFmts<T, unsigned char, USER> {
	static const char **get()
	{
		static const char *r [] = { "%c", " (0x%02hhx)", 0 };
		return r;
	}
};

template <typename T, int USER> struct PrintFmts<T, unsigned short, USER> {
	static const char **get()
	{
		static const char *r [] = { "%hu", " (0x%04hx)", 0 };
		return r;
	}
};

template <typename T, int USER> struct PrintFmts<T, unsigned int, USER> {
	static const char **get()
	{
		static const char *r [] = { "%u", " (0x%08x)", 0 };
		return r;
	}
};

template <typename T, int USER> struct PrintFmts<T, unsigned long, USER> {
	static const char **get()
	{
		static const char *r [] = { "%lu", " (0x%08lx)", 0 };
		return r;
	}
};

template <typename T, int USER> struct PrintFmts<T, unsigned long long, USER> {
	static const char **get()
	{
		static const char *r [] = { "%llu", " (0x%16llx)", 0 };
		return r;
	}
};

template <typename T, int USER> struct PrintFmts<T, float, USER> {
	static const char **get()
	{
		static const char *r [] = { "%g", 0 };
		return r;
	}
};

template <typename T, int USER> struct PrintFmts<T, double, USER> {
	static const char **get()
	{
		static const char *r [] = { "%.10lg", 0 };
		return r;
	}
};

template <typename T, int USER> struct PrintFmts<T, typename is_chrp<T>::type, USER> {
	static const char **get()
	{
		static const char *r [] = { "%s", 0 };
		return r;
	}
};

/*
 * This template can be overridden to handle class T.
 * The template is expanded PrinterBase< sometype, sometype, 0 >
 * and the extra arguments 'R' and 'USER' exist so that
 * the user may write more specific specializations.
 *
 * 'R' is used to provide specializations for sub-types of
 * 'T' (see the 'is_int/is_str/is_cplx' variants).
 *
 * Because we already specialize for 'R' in this file
 * there is the additional argument 'USER' which may be
 * used to provide a yet more specific variant.
 */
template <typename T, typename R, int USER = 0> class PrinterBase {
public:
	static void print( typename Reference<R>::const_type r ) {
		const char **fmts = PrintFmts<R,R>::get();
		if ( ! fmts ) {
			errlogPrintf("<No print format for this return type implemented>\n");
		} else {
			while ( *fmts ) {
				errlogPrintf( *fmts, r );
				fmts++;
			}
			errlogPrintf("\n");
		}
	}
};

/*
 * Specialization for complex types.
 *
 * EXAMPLE: you can override this variant by implementing (specialized for int 0)
 *
 *    template <typename T> class PrinterBase< std::complex<T>, typename is_cplx<std::complex<T> >::type, 0> {
 *         ...
 *    };
 */
template <typename T, int USER> class PrinterBase< T, typename is_cplx< T >::type, USER > {
public:
	static void print( const std::complex<T> &r )
	{
		errlogPrintf("%.10Lg J %.10Lg\n", (long double)r.real(), (long double)r.imag());
	}
};

/*
 * Specialization for string types
 */
template <typename T, int USER> class PrinterBase< T, typename is_str<T>::type, USER > {
public:
	static void print( typename Reference<T>::const_type r )
	{
		PrinterBase< const char *, const char *, USER >::print( r.c_str() );
	}
};

template <typename T, int USER> class PrinterBase< T, typename is_strp<T>::type, USER > {
public:
	static void print( typename Reference<T>::const_type r )
	{
		PrinterBase< const char *, const char *, USER >::print( r->c_str() );
	}
};


/*
 * Finally, the Printer can be specialized for a particular user function 'sig'
 *
 * If you have a user function:
 *
 *    MyRet myFunction( MyArg & );
 *
 * Then you can specialize how the results of this function are printed:
 *
 *    template <> classPrinter< MyRet, MyRet(MyArg&), myFunction > {
 *       public:
 *          static void print( const MyRet &result ) {
 *              <print result here>
 *          }
 *    }
 */
template <typename R, typename SIG, SIG *sig> class Printer : public PrinterBase<R, R> {
};

/*
 * Build a Printer signature
 */
template <typename R, typename SIG> struct Guesser {
	typedef typename EvalResult<R>::PrinterType PrinterType;

	template <SIG *sig> static PrinterType getPrinter()
	{
		return Printer<R, SIG, sig>::print;
	}
};

template <typename SIG> struct Guesser<void, SIG> {
	typedef typename EvalResult<void>::PrinterType PrinterType;

	template <SIG *sig> static PrinterType getPrinter()
	{
		return 0;
	}
};

/*
 * Helper class for building a iocshFuncDef
 */
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

};

#if __cplusplus >= 201103L

#include <initializer_list>

namespace IocshDeclWrapper {

/*
 * Build a iocshFuncDef with associated iocArg structs.
 * If we were to wrap huge masses of user functions then
 * we could keep a cache of most used iocArgs (same epics type,
 * same help string) around but ATM we don't bother...
 */
template <typename R, typename ...A>
iocshFuncDef  *buildArgs( const char *fname, R (*f)(A...), std::initializer_list<const char *> argNames )
{
	std::initializer_list<const char*>::const_iterator it;

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
template <typename ...A> struct ArgOrder {

	template <int ... I> struct Index {
		/* Once we have a pair of parameter packs: A... I... we can expand */
		template <typename R> static R dispatch(R (*f)(A...), const iocshArgBuf *args, Context *ctx)
		{
			return f( Convert<A>::getArg( &args[I], ctx )... );
		}
	};

	/* Recursively build I... */
	template <int i, int ...I> struct C {
		template <typename R> static R concat(R (*f)(A...), const iocshArgBuf *args, Context *ctx)
		{
			return C<i-1, i-1, I...>::concat(f, args, ctx);
		}
	};

	/* Specialization for terminating the recursion */
	template <int ...I> struct C<0, I...> {
		template <typename R> static R concat(R (*f)(A...), const iocshArgBuf *args, Context *ctx)
		{
			return Index<I...>::dispatch(f, args, ctx);
		}
	};

	/* Build index pack and dispatch 'f' */
	template <typename R> static R arrange( R(*f)(A...), const iocshArgBuf *args, Context *ctx)
	{
		return C<sizeof...(A)>::concat( f, args, ctx );
	}
};

/*
 * Use template argument deduction to figure out types 'A' and 'R' automatically
 * so that the correct Guesser can be instantiated.
 */
template <typename SIG, typename R, typename ...A> Guesser<R, SIG> makeGuesser(R (*f)(A...))
{
	return Guesser<R, SIG>();
}

template <bool PRINT, typename R, typename ...A>
static void
dispatch(R (*f)(A...), const iocshArgBuf *args, typename EvalResult<R>::PrinterType printer)
{
	try {
		Context ctx;
		( EvalResult<R, PRINT>( printer ), /* <== magic 'operator,' */
		  ArgOrder<A...>::arrange( f, args , &ctx ) );
	} catch ( ConversionError &e ) {
		errlogPrintf( "Error: Invalid Argument -- %s\n", e.what() );
	}
}

/*
 * This is the 'iocshCallFunc'
 */
template <typename RR, RR *p, bool PRINT=true> void call(const iocshArgBuf *args)
{
	dispatch<PRINT>( p, args, makeGuesser<RR>( p ).template getPrinter<p>() );
}

}

#define IOCSH_FUNC_WRAP(x,argHelps...) do {                                         \
	using IocshDeclWrapper::buildArgs;                                              \
	using IocshDeclWrapper::call;                                                   \
	iocshRegister( buildArgs( #x, x, { argHelps } ), call<decltype(x), x> );        \
  } while (0)

#define IOCSH_FUNC_WRAP_QUIET(x,argHelps...) do {                                   \
	using IocshDeclWrapper::buildArgs;                                              \
	using IocshDeclWrapper::call;                                                   \
	iocshRegister( buildArgs( #x, x, { argHelps } ), call<decltype(x), x, false> ); \
  } while (0)


#else  /* __cplusplus < 201103L */

namespace IocshDeclWrapper {

/*
 * See C++11 version for comments...
 */
template <typename G>
iocshFuncDef  *buildArgs(G guess, const char *fname, const char **argNames)
{
	using namespace IocshDeclWrapper;
	int             N = G::N;
	FuncDef         funcDef( fname, N );
	const char      *aname;
	int             n;

	aname = *argNames++;
	n     = -1;

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A0_T>( aname ) );
	if ( aname )
		aname = *argNames++;

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A1_T>( aname ) );
	if ( aname )
		aname = *argNames++;

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A2_T>( aname ) );
	if ( aname )
		aname = *argNames++;

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A3_T>( aname ) );
	if ( aname )
		aname = *argNames++;

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A4_T>( aname ) );
	if ( aname )
		aname = *argNames++;

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A5_T>( aname ) );
	if ( aname )
		aname = *argNames++;

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A6_T>( aname ) );
	if ( aname )
		aname = *argNames++;

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A7_T>( aname ) );
	if ( aname )
		aname = *argNames++;

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A8_T>( aname ) );
	if ( aname )
		aname = *argNames++;

	if ( ++n >= N )
		goto done;
	funcDef.setArg( n, makeArg<typename G::A9_T>( aname ) );
	if ( aname ) /* not necessary but leave in case more args are added */
		aname = *argNames++;

done:

	return funcDef.release();
}

/*
 * The purpose of this template is the implementation of a iocshCallFunc wrapper
 * around the user function.
 *
 * Without support for variadic templates (C++98) we must provide a specialization
 * for every possible number of arguments from 0 .. IOCSH_FUNC_WRAP_MAX_ARGS - 1
 * (currently supported max. - 1); see below...
 */

#define IOCSH_DECL_WRAPPER_DO_CALL(args...)                              \
	do {                                                                 \
		try {                                                            \
			EvalResult<R, PRINT>(                                        \
				Guesser<R, type>().template getPrinter<func> ()          \
            ), func( args ); /* <= magic 'operator,' */                  \
		} catch ( IocshDeclWrapper::ConversionError &e ) {               \
			errlogPrintf( "Error: Invalid Argument -- %s\n", e.what() ); \
		}                                                                \
	} while (0)

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6, typename A7, typename A8, typename A9>
class Caller
{
public:
	/* type of the user function */
	typedef R(type)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9);

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
	 * We use the makeCaller() template to generate the correct
	 * template of this class. Once the return and argument types are known
	 * we can obtain the iocshCallFunc pointer like this:
	 *
	 *  makeCaller( funcWeWantToWrap ).call<funcWeWantToWrap>
	 *
	 * This instantiates this template and the instantiation 'knows' that 'func'
	 * is in fact 'funcWeWantToWrap'...
	 */
	template <type *func, bool PRINT> static void call(const iocshArgBuf *args)
	{
		IocshDeclWrapper::Context ctx;
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx ),
			IocshDeclWrapper::Convert<A2>::getArg( &args[2], &ctx ),
			IocshDeclWrapper::Convert<A3>::getArg( &args[3], &ctx ),
			IocshDeclWrapper::Convert<A4>::getArg( &args[4], &ctx ),
			IocshDeclWrapper::Convert<A5>::getArg( &args[5], &ctx ),
			IocshDeclWrapper::Convert<A6>::getArg( &args[6], &ctx ),
			IocshDeclWrapper::Convert<A7>::getArg( &args[7], &ctx ),
			IocshDeclWrapper::Convert<A8>::getArg( &args[8], &ctx ),
			IocshDeclWrapper::Convert<A9>::getArg( &args[9], &ctx )
		);
	}
};

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6, typename A7, typename A8>
class Caller<R, A0, A1, A2, A3, A4, A5, A6, A7, A8, void>
{
public:
	typedef R(type)(A0, A1, A2, A3, A4, A5, A6, A7, A8);
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

	template <type *func, bool PRINT> static void call(const iocshArgBuf *args)
	{
		IocshDeclWrapper::Context ctx;
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx ),
			IocshDeclWrapper::Convert<A2>::getArg( &args[2], &ctx ),
			IocshDeclWrapper::Convert<A3>::getArg( &args[3], &ctx ),
			IocshDeclWrapper::Convert<A4>::getArg( &args[4], &ctx ),
			IocshDeclWrapper::Convert<A5>::getArg( &args[5], &ctx ),
			IocshDeclWrapper::Convert<A6>::getArg( &args[6], &ctx ),
			IocshDeclWrapper::Convert<A7>::getArg( &args[7], &ctx ),
			IocshDeclWrapper::Convert<A8>::getArg( &args[8], &ctx )
		);
	}
};

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6, typename A7>
class Caller<R, A0, A1, A2, A3, A4, A5, A6, A7, void, void>
{
public:
	typedef R(type)(A0, A1, A2, A3, A4, A5, A6, A7);
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

	template <type *func, bool PRINT> static void call(const iocshArgBuf *args)
	{
		IocshDeclWrapper::Context ctx;
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx ),
			IocshDeclWrapper::Convert<A2>::getArg( &args[2], &ctx ),
			IocshDeclWrapper::Convert<A3>::getArg( &args[3], &ctx ),
			IocshDeclWrapper::Convert<A4>::getArg( &args[4], &ctx ),
			IocshDeclWrapper::Convert<A5>::getArg( &args[5], &ctx ),
			IocshDeclWrapper::Convert<A6>::getArg( &args[6], &ctx ),
			IocshDeclWrapper::Convert<A7>::getArg( &args[7], &ctx )
		);
	}
};

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6>
class Caller<R, A0, A1, A2, A3, A4, A5, A6, void, void, void>
{
public:
	typedef R(type)(A0, A1, A2, A3, A4, A5, A6);
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

	template <type *func, bool PRINT> static void call(const iocshArgBuf *args)
	{
		IocshDeclWrapper::Context ctx;
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx ),
			IocshDeclWrapper::Convert<A2>::getArg( &args[2], &ctx ),
			IocshDeclWrapper::Convert<A3>::getArg( &args[3], &ctx ),
			IocshDeclWrapper::Convert<A4>::getArg( &args[4], &ctx ),
			IocshDeclWrapper::Convert<A5>::getArg( &args[5], &ctx ),
			IocshDeclWrapper::Convert<A6>::getArg( &args[6], &ctx )
		);
	}
};

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5>
class Caller<R, A0, A1, A2, A3, A4, A5, void, void, void, void>
{
public:
	typedef R(type)(A0, A1, A2, A3, A4, A5);
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

	template <type *func, bool PRINT> static void call(const iocshArgBuf *args)
	{
		IocshDeclWrapper::Context ctx;
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx ),
			IocshDeclWrapper::Convert<A2>::getArg( &args[2], &ctx ),
			IocshDeclWrapper::Convert<A3>::getArg( &args[3], &ctx ),
			IocshDeclWrapper::Convert<A4>::getArg( &args[4], &ctx ),
			IocshDeclWrapper::Convert<A5>::getArg( &args[5], &ctx )
		);
	}
};

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4>
class Caller<R, A0, A1, A2, A3, A4, void, void, void, void, void>
{
public:
	typedef R(type)(A0, A1, A2, A3, A4);
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

	template <type *func, bool PRINT> static void call(const iocshArgBuf *args)
	{
		IocshDeclWrapper::Context ctx;
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx ),
			IocshDeclWrapper::Convert<A2>::getArg( &args[2], &ctx ),
			IocshDeclWrapper::Convert<A3>::getArg( &args[3], &ctx ),
			IocshDeclWrapper::Convert<A4>::getArg( &args[4], &ctx )
		);
	}
};

template <typename R, typename A0, typename A1, typename A2, typename A3>
class Caller<R, A0, A1, A2, A3, void, void, void, void, void, void>
{
public:
	typedef R(type)(A0, A1, A2, A3);
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

	template <type *func, bool PRINT> static void call(const iocshArgBuf *args)
	{
		IocshDeclWrapper::Context ctx;
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx ),
			IocshDeclWrapper::Convert<A2>::getArg( &args[2], &ctx ),
			IocshDeclWrapper::Convert<A3>::getArg( &args[3], &ctx )
		);
	}
};

template <typename R, typename A0, typename A1, typename A2>
class Caller<R, A0, A1, A2, void, void, void, void, void, void, void>
{
public:
	typedef R(type)(A0, A1, A2);
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

	template <type *func, bool PRINT> static void call(const iocshArgBuf *args)
	{
		IocshDeclWrapper::Context ctx;
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx ),
			IocshDeclWrapper::Convert<A2>::getArg( &args[2], &ctx )
		);
	}
};

template <typename R, typename A0, typename A1>
class Caller<R, A0, A1, void, void, void, void, void, void, void, void>
{
public:
	typedef R(type)(A0, A1);
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

	template <type *func, bool PRINT> static void call(const iocshArgBuf *args)
	{
		IocshDeclWrapper::Context ctx;
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx )
		);
	}
};


template <typename R, typename A0>
class Caller<R, A0, void, void, void, void, void, void, void, void, void>
{
public:
	typedef R(type)(A0);
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

	template <type *func, bool PRINT> static void call(const iocshArgBuf *args)
	{
		IocshDeclWrapper::Context ctx;
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx )
		);
	}
};

template <typename R>
class Caller<R, void, void, void, void, void, void, void, void, void, void>
{
public:
	typedef R(type)(void);

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

	template <type *func, bool PRINT> static void call(const iocshArgBuf *args)
	{
		IOCSH_DECL_WRAPPER_DO_CALL( );
	}
};

/* Function templates which use argument deduction to find the argument and return types
 * of function 'f'.
 * Return a properly parametrized Caller<> object. This allows us to infer
 * the argument and return types of the function we want to wrap without the user having
 * to give us any more information.
 *
 * Build a iocshFuncDef containing descriptions of the arguments 'funcWeWantToWrap' expects.
 *
 *	buildArgs( makeCaller(funcWeWantToWrap), ... )
 *
 * Build a iocshCallFunc. Calling this wrapper results in 'funcWeWantToWrap' being executed
 * with arguments extracted from a iocshArgBuf:
 *
 *	makeCaller(funcWeWantToWrap).call<funcWeWantToWrap>
 *
 */
template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6, typename A7, typename A8, typename A9>
Caller<R,A0,A1,A2,A3,A4,A5,A6,A7,A8,A9> makeCaller(R (*f)(A0,A1,A2,A3,A4,A5,A6,A7,A8,A9))
{
	return Caller<R,A0,A1,A2,A3,A4,A5,A6,A7,A8,A9>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6, typename A7, typename A8>
Caller<R,A0,A1,A2,A3,A4,A5,A6,A7,A8,void> makeCaller(R (*f)(A0,A1,A2,A3,A4,A5,A6,A7,A8))
{
	return Caller<R,A0,A1,A2,A3,A4,A5,A6,A7,A8,void>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6, typename A7>
Caller<R,A0,A1,A2,A3,A4,A5,A6,A7,void,void> makeCaller(R (*f)(A0,A1,A2,A3,A4,A5,A6,A7))
{
	return Caller<R,A0,A1,A2,A3,A4,A5,A6,A7,void,void>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6>
Caller<R,A0,A1,A2,A3,A4,A5,A6,void,void,void> makeCaller(R (*f)(A0,A1,A2,A3,A4,A5,A6))
{
	return Caller<R,A0,A1,A2,A3,A4,A5,A6,void,void,void>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5>
Caller<R,A0,A1,A2,A3,A4,A5,void,void,void,void> makeCaller(R (*f)(A0,A1,A2,A3,A4,A5))
{
	return Caller<R,A0,A1,A2,A3,A4,A5,void,void,void,void>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4>
Caller<R,A0,A1,A2,A3,A4,void,void,void,void,void> makeCaller(R (*f)(A0,A1,A2,A3,A4))
{
	return Caller<R,A0,A1,A2,A3,A4,void,void,void,void,void>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3>
Caller<R,A0,A1,A2,A3,void,void,void,void,void,void> makeCaller(R (*f)(A0,A1,A2,A3))
{
	return Caller<R,A0,A1,A2,A3,void,void,void,void,void,void>();
}

template <typename R, typename A0, typename A1, typename A2>
Caller<R,A0,A1,A2,void,void,void,void,void,void,void> makeCaller(R (*f)(A0,A1,A2))
{
	return Caller<R,A0,A1,A2,void,void,void,void,void,void,void>();
}

template <typename R, typename A0, typename A1>
Caller<R,A0,A1,void,void,void,void,void,void,void,void> makeCaller(R (*f)(A0,A1))
{
	return Caller<R,A0,A1,void,void,void,void,void,void,void,void>();
}

template <typename R, typename A0>
Caller<R,A0,void,void,void,void,void,void,void,void,void> makeCaller(R (*f)(A0))
{
	return Caller<R,A0,void,void,void,void,void,void,void,void,void>();
}

template <typename R>
Caller<R,void,void,void,void,void,void,void,void,void,void> makeCaller(R (*f)(void))
{
	return Caller<R,void,void,void,void,void,void,void,void,void,void>();
}

#undef IOCSH_DECL_WRAPPER_DO_CALL

}

#define IOCSH_FUNC_WRAP_MAX_ARGS 10

#define IOCSH_FUNC_WRAP(x,argHelps...) do {                                                 \
	const char *argNames[IOCSH_FUNC_WRAP_MAX_ARGS + 1] = { argHelps };                      \
	using IocshDeclWrapper::buildArgs;                                                      \
	using IocshDeclWrapper::makeCaller;                                                     \
	iocshRegister( buildArgs( makeCaller(x), #x, argNames ), makeCaller(x).call<x,true> );  \
	} while (0)

#define IOCSH_FUNC_WRAP_QUIET(x,argHelps...) do {                                           \
	const char *argNames[IOCSH_FUNC_WRAP_MAX_ARGS + 1] = { argHelps };                      \
	using IocshDeclWrapper::buildArgs;                                                      \
	using IocshDeclWrapper::makeCaller;                                                     \
	iocshRegister( buildArgs( makeCaller(x), #x, argNames ), makeCaller(x).call<x,false> ); \
	} while (0)


#endif /* __cplusplus >= 201103L */

#define IOCSH_FUNC_WRAP_REGISTRAR( registrarName, wrappers... ) \
static void registrarName() \
{ \
  wrappers \
} \
epicsExportRegistrar( registrarName ); \

#endif
