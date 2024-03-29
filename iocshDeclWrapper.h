#ifndef IOCSH_FUNC_WRAPPER_H
#define IOCSH_FUNC_WRAPPER_H

/* Till Straumann <till.straumann@psi.ch>, PSI, 2020 */

#ifndef __cplusplus
#error "This header requires C++"
#endif

#include <epicsString.h>
#include <epicsStdio.h>
#include <epicsExport.h>
#include <errlog.h>
#include <iocsh.h>
#include <string>
#include <stdexcept>
#include <vector>
#include <complex>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

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

/*
 * SFINAE helpers (we don't necessarily have C++11 and avoid boost)
 */
template <typename T> struct is_int;
#define IOCSH_DECL_WRAPPER_IS_INT(x) \
	template <> struct is_int<x> {   \
		typedef x type;              \
		static const char *name()    \
		{                            \
			return  #x ;             \
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

template <typename T> struct is_flt;
template <> struct is_flt<float > { typedef float  type; static const char *name() { return "float" ; } };
template <> struct is_flt<double> { typedef double type; static const char *name() { return "double"; } };


template <typename T> struct is_str;
template <> struct is_str <std::string       > { typedef       std::string   type; };
template <> struct is_str <const std::string > { typedef const std::string   type; };
template <> struct is_str <std::string      &> { typedef       std::string & type; };
template <> struct is_str <const std::string&> { typedef const std::string & type; };

template <typename T> struct is_strp;
template <> struct is_strp<std::string      *> { typedef       std::string * type; };
template <> struct is_strp<const std::string*> { typedef const std::string * type; };

template <typename T> struct is_chrp           { typedef                  T  falsetype; };
template <> struct is_chrp<char             *> { typedef       char        * type; };
template <> struct is_chrp<const char       *> { typedef const char        * type; };

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

template <typename T> struct skipref {
	typedef       T  type;
	typedef const T  const_type;
	typedef       T& ref_type;
	typedef const T& const_ref_type;
};

template <typename T> struct skipref<const  T  > {
	typedef       T  type;
	typedef const T  const_type;
	typedef       T& ref_type;
	typedef const T& const_ref_type;
};

template <typename T> struct skipref<       T &> {
	typedef       T  type;
	typedef const T  const_type;
	typedef       T& ref_type;
	typedef const T& const_ref_type;
};

template <typename T> struct skipref<const  T &> {
	typedef       T  type;
	typedef const T  const_type;
	typedef       T& ref_type;
	typedef const T& const_ref_type;
};

template <typename T> struct is_const {
	const static bool value = false;
};

template <typename T> struct is_const<const T> {
	const static bool value = true;
};

template <typename T> struct is_const<const T *> {
	const static bool value = true;
};

template <typename T> struct is_const<const T &> {
	const static bool value = true;
};

template <typename T> struct is_ptr {
	const static bool value = false;
};

template <typename T> struct is_ptr<T *> {
	const static bool value = true;
};

template <typename T> struct is_ptr<T *const> {
	const static bool value = true;
};

template <typename T> struct is_ref {
	const static bool value = false;
};

template <typename T> struct is_ref<T &> {
	const static bool value = true;
};

template <typename T> struct textract           { typedef T element_type; };
template <typename T> struct textract<const T > { typedef T element_type; };
template <typename T> struct textract<const T*> { typedef T element_type; };
template <typename T> struct textract<const T&> { typedef T element_type; };
template <typename T> struct textract<      T*> { typedef T element_type; };
template <typename T> struct textract<      T&> { typedef T element_type; };

/*
 * Elementary scalar types and pointers to them
 */
template <typename T, typename U = T, int USER = 0> struct is_scal;
template <typename T, typename R = T, int USER = 0> struct is_scalp;
template <typename T, typename R = T, int USER = 0> struct is_scalr;

/*
 * Connect any 'is_int<>' to integer iocsh arg
 */
template <typename T, int USER> struct is_scal< T, typename is_int<T>::type, USER > : public is_int<T> {

	typedef int iocsh_c_type;

	static iocsh_c_type iocshVal(const iocshArgBuf *a)
	{
		return a->ival;
	}

	static void setArg(iocshArg *a)
	{
		a->name = is_int<T>::name();
		a->type = iocshArgInt;
	}
};

/*
 * Connect any 'is_flt<>' to double iocsh arg
 */
template <typename T, int USER> struct is_scal< T, typename is_flt<T>::type, USER > : public is_flt<T> {

	typedef double iocsh_c_type;

	static iocsh_c_type iocshVal(const iocshArgBuf *a)
	{
		return a->dval;
	}

	static void setArg(iocshArg *a)
	{
		a->name = is_flt<T>::name();
		a->type = iocshArgDouble;
	}
};

/*
 * Pointer to a scalar
 */
template <typename T, int USER> struct is_scalp<T*, typename is_scal<T>::type *, USER> {
	typedef typename is_scal<T>::type  btype;
	typedef typename is_scal<T>::type *ptype;
	typedef is_scal<T>                 stype;
};


template <typename T, int USER> struct is_scalp<const T*, const typename is_scal<T>::type *, USER> {
	typedef typename is_scal<T>::type  const  btype;
	typedef typename is_scal<T>::type  const *ptype;
	typedef is_scal<T>                        stype;
};

/*
 * Reference to a scalar
 */
template <typename T, int USER> struct is_scalr<T&, typename is_scal<T>::type &, USER> {
	typedef typename is_scal<T>::type  btype;
	typedef typename is_scal<T>::type &rtype;
	typedef is_scal<T>                 stype;
};


template <typename T, int USER> struct is_scalr<const T&, const typename is_scal<T>::type &, USER> {
	typedef typename is_scal<T>::type  const  btype;
	typedef typename is_scal<T>::type  const &rtype;
	typedef is_scal<T>                        stype;
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
 * For the default Printer implementation we
 * use specialized 'format strings'.
 * The 'get()' method returns NULL terminated array
 * of format strings. The printer will print the
 * result for each of the formats. This allows, e.g.,
 * for printing an integer result in decimal as well
 * as in hexadecimal.
 */

template <typename T, typename R = T, int USER = 0> struct PrintFmts
/*
 * define to debug specializations - the compiler will tell you what it's
 * looking for instead of using this fallback.
 */
#ifndef IOCSH_DECL_WRAPPER_PRINTFMTS_DEBUG
{
	static const char **get()
	{
		return 0;
	}
}
#endif
;

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
		static const char *r [] = { "%p -> ", "%s", 0 };
		return r;
	}
};

template <typename T, int USER> struct PrintFmts<T *, typename is_chrp<T*>::falsetype, USER> {
	static const char **get()
	{
		static const char *r[] = { "%p", 0 };
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
		const char **fmts = PrintFmts<typename skipref<R>::type>::get();
		if ( ! fmts ) {
			errlogPrintf("<No print format for this return type implemented>\n");
		} else {
			while ( *fmts ) {
				epicsStdoutPrintf( *fmts, r );
				fmts++;
			}
			epicsStdoutPrintf("\n");
		}
	}
};

/*
 * Specialization for C-Strings (override scalar pointer specialization)
 */
template<int USER> class PrinterBase<const char *, const char *, USER> {
public:
	static void print( typename Reference<const char*>::const_type r ) {
		const char **fmts = PrintFmts<const char *, const char *>::get();
		if ( ! fmts ) {
			errlogPrintf("<No print format for this return type implemented>\n");
		} else {
			while ( *fmts ) {
				epicsStdoutPrintf( *fmts, r );
				fmts++;
			}
			epicsStdoutPrintf("\n");
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
		epicsStdoutPrintf("%.10Lg J %.10Lg\n", (long double)r.real(), (long double)r.imag());
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
		PrinterBase< const char *, const char *, USER >::print( r ? r->c_str() : 0 );
	}
};

class ContextElBase {
public:
	virtual bool isConst() const = 0;
	virtual void print()     {}
	virtual ~ContextElBase() {}
};

/* 'Reference holder' for abitrary simple objects. Similar to
 * a (very dumb) smart pointer but without having to worry about
 * C++11 or boost.
 */
template <typename T, typename I> class ContextEl : public ContextElBase {
protected:
	T *p_;
	ContextEl(I inival)
	{
		p_ = new T( inival );
	}
public:

	virtual bool isConst() const
	{
		return is_const<T>::value;
	}

	T * p()
	{
		return p_;
	}

	virtual void print()
	{
		PrinterBase<T, T, 0>::print( *p() );
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
protected:
	char *p_;
	ContextEl(const char *inival)
	{
		p_ = inival ? epicsStrDup( inival ) : 0;
	}
public:

	virtual bool isConst() const
	{
		return false;
	}

	typedef char type[];

	type * p()
	{
		return (type*)p_;
	}

	virtual void print()
	{
		PrinterBase< const char *, const char *, 0 >::print( p_ );
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
private:
	const iocshArgBuf *args_;
	std::vector<int>   mutableArgIdx_;

public:
	Context(const iocshArgBuf *args, unsigned numArgs)
	: args_         ( args        ),
	  mutableArgIdx_( numArgs, -1 )
	{
	}

	virtual unsigned getNumArgs() const
	{
		return mutableArgIdx_.size();
	}

	virtual ContextElBase * getArg(unsigned idx) const
	{
	int argIdx;

		if ( idx >= getNumArgs() || (argIdx = mutableArgIdx_[idx]) < 0 )
			return 0;
		return (*this)[argIdx];
	}

	virtual ~Context()
	{
	iterator it;

		for ( it = begin(); it != end(); ++it ) {
			delete *it;
		}
	}

	const iocshArgBuf *getArgBuf()
	{
		return args_;
	}

	/*
	 * Create a new object of type T and attach to
	 * the Context.
	 * RETURNS: pointer to the new object.
	 */
	template <typename T, typename I> T * make(I i, int recordIdx = -1)
	{
		ContextEl<T,I> *el = new ContextEl<T,I>( i );
		if ( recordIdx >= 0 && (unsigned)recordIdx < mutableArgIdx_.size() ) {
			mutableArgIdx_[ recordIdx ] = size();
		}
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
 * Default implementation
 */
struct ArgPrinterBase {
public:
	/*
	 * Print arguments recorded in the context. These
	 * are mutable arguments that can be modified by
	 * the user function.
	 */
	static void printArgs(Context *ctx)
	{
	bool     headerPrinted = false;
	unsigned i;
	unsigned nargs         = ctx->getNumArgs();

		for ( i = 0; i < nargs; i++ ) {
			ContextElBase *el = ctx->getArg( i );
			if ( el && ! el->isConst() ) {
				if ( ! headerPrinted ) {
					epicsStdoutPrintf("Mutable arguments after execution:\n");
					headerPrinted = true;
				}
				epicsStdoutPrintf("arg[%i]: ", i); el->print();
			}
		}
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
template <typename R, typename SIG, SIG *sig, int USER = 0> class Printer    : public PrinterBase<R, R> {
};

template <typename SIG, SIG *sig, int USER = 0> class ArgPrinter : public ArgPrinterBase {
};

typedef void (*ArgPrinterType)(Context*);

/*
 * Converter to map between user function arguments and iocshArg/iocshArgBuf
 */
template <typename T, typename R = T, int USER=0> struct Convert
{
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
	static R    getArg(const iocshArgBuf *, Context *, int argNo);
};

/*
 * Allocate a new iocshArg struct and call
 * Convert::setArg() for type 'T'
 */
template <typename T>
iocshArg *makeArg(const char *aname = 0)
{
	iocshArg *rval = new iocshArg;
	::memset( rval, 0, sizeof( *rval ) );

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

template <int USER> struct ArgName {
	static const char *make(const char *fmt, ...)
	{
	int   len  = 0;
	char *rval = 0;
	va_list ap;
		va_start(ap, fmt);
		len = vsnprintf(rval, len, fmt, ap);
		va_end(ap);
		if ( len++ < 0 || 0 == (rval = (char*)malloc( len )) ) {
			return 0;
		}
		va_start(ap, fmt);
		len = vsnprintf(rval, len, fmt, ap);
		va_end(ap);
		
		if ( len < 0 ) {
			free(rval);
			rval = 0;
		}
		return rval;
	}
};

/* Specialization for all integral types */
template <typename T, int USER> struct Convert<T, typename is_int<T>::type, USER> {

	typedef typename is_int<T>::type type;

	static void setArg(iocshArg *a)
	{
	static const char *n = 0;
		if ( !n ) {
			n = ArgName<0>::make( "<%s>", is_int<T>::name() );
		}
		a->name = n ? n : "";
		a->type = iocshArgInt;
	}

	static type getArg(const iocshArgBuf *arg, Context *ctx, int argNo)
	{
		return (type)arg->ival;
	}
};

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

	static type getArg(const iocshArgBuf *a, Context *ctx, int argNo)
	{
		if ( is_const<T>::value )
			argNo = -1; // not a mutable arg
		return * ctx->make<std::string, const char *>( a->sval ? a->sval : "", argNo );
	}
};

/* Specialization for string pointer */
template <typename T, int USER> struct Convert<T, typename is_strp<T>::type, USER> {

	typedef typename is_strp<T>::type type;

	static void setArg(iocshArg *a)
	{
		setArgStr( a );
	}

	static type getArg(const iocshArgBuf *a, Context *ctx, int argNo)
	{
		if ( is_const<T>::value )
			argNo = -1; // not a mutable arg
		return a->sval ? ctx->make<std::string, const char *>( a->sval, argNo ) : 0;
	}
};

/* Specialization for C-strings */
template <int USER> struct Convert<const char *, const char *, USER> {

	static void setArg(iocshArg *a)
	{
		setArgStr( a );
	}

	static const char * getArg(const iocshArgBuf *a, Context *ctx, int argNo)
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

	static char * getArg(const iocshArgBuf *a, Context *ctx, int argNo)
	{
		/* must make a copy of the const char * help in the iocshArgBuf */
		return (char*)ctx->make<char[], const char *>( a->sval, argNo );
	}
};

/* Specialization for floats */
template <typename T, int USER> struct Convert<T, typename is_flt<T>::type, USER> {
	typedef typename is_flt<T>::type type;

	static void setArg(iocshArg *a)
	{
	static const char *n = 0;
		if ( !n ) {
			n = ArgName<0>::make("<%s>", is_flt<T>::name());
		}
		a->name = n ? n : "";
		a->type = iocshArgDouble;
	}

	static type getArg(const iocshArgBuf *a, Context *ctx, int argNo)
	{
		return (type) a->dval;
	}
};

/*
 * Specialization to pointers to ints and doubles
 */
template <typename T, int USER> struct Convert<T*, typename is_scalp<T*>::ptype, USER> {

	typedef is_scalp<T*> scalp;

	static void setArg(iocshArg *a)
	{
		scalp::stype::setArg( a );
	}

	static T * getArg(const iocshArgBuf *a, Context *ctx, int argNo)
	{
	T * elp = ctx->make<T, typename scalp::stype::iocsh_c_type>( scalp::stype::iocshVal( a ), argNo );
		
		return elp;
	}
};

/*
 * Specialization to references to ints and doubles
 */
template <typename T, int USER> struct Convert<T&, typename is_scalr<T&>::rtype, USER> {

	typedef is_scalr<T&> scalr;

	static void setArg(iocshArg *a)
	{
		scalr::stype::setArg( a );
	}

	static T & getArg(const iocshArgBuf *a, Context *ctx, int argNo)
	{
	T * elp = ctx->make<T, typename scalr::stype::iocsh_c_type>( scalr::stype::iocshVal( a ), argNo );
		
		return *elp;
	}
};


/* Specialization for std::complex */
template <typename T, int USER> struct Convert<T, typename is_cplx<T>::type, USER> {
	typedef typename is_cplx<T>::type type;

	static void setArg(iocshArg *a)
	{
		a->name = "complex number as string: \"<real> j <imag>\"";
		a->type = iocshArgString;
	}

	static type getArg(const iocshArgBuf *a, Context *ctx, int argNo)
	{
		typename type::value_type r, i;
		if ( ! a->sval || 2 != sscanf( a->sval, is_cplx<T>::fmt(), &r, &i ) ) {
			throw ConversionError("unable to scan argument into '%g j %g' format");
		}
		return type( r, i );
	}
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
 * Build a Printer signature
 */
template <typename R, typename SIG> struct Guesser {
	typedef typename EvalResult<R>::PrinterType PrinterType;

	template <SIG *sig> static PrinterType getPrinter()
	{
		return Printer<R, SIG, sig>::print;
	}

	template <SIG *sig> static ArgPrinterType getArgPrinter()
	{
		return ArgPrinter<SIG, sig>::printArgs;
	}
};

/*
 * Special case if the user-function returns 'void'.
 * PrinterType is then a dummy 'void *'.
 */
template <typename SIG> struct Guesser<void, SIG> {
	typedef typename EvalResult<void>::PrinterType PrinterType;

	template <SIG *sig> static PrinterType getPrinter()
	{
		return 0;
	}

	template <SIG *sig> static ArgPrinterType getArgPrinter()
	{
		return ArgPrinter<SIG, sig>::printArgs;
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
		::memset(def, 0, sizeof(*def));
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

/* A special trick to let the user specify overloaded functions. We want to do this
 * with a final macro:
 *   #define _WRAP( fun, overload_args, name, help... )
 * Because the pre-processor requires parentheses around anything containing commas
 * the macro must be expanded like this:
 *
 *   _WRAP( myFunc, (int, char*), "myFunc_1" )
 *
 * The following templates allow us to get rid of the parentheses and use the type
 * signatures...
 */
template <typename T> struct DropBraces;

/* First the specialization for T=void which auto-derives the types; this is used
 * when a function is not overloaded
 */
template <> struct DropBraces<void> {

	template <typename R, typename ...A> struct TypeHelper {
		typedef R (FuncType)(A...);
	};

	template <typename R, typename ...A>
	static TypeHelper<R,A...> type(R (*f)(A...))
	{
		return TypeHelper<R,A...>();
	}

	template <typename R, typename C, typename ...A>
	struct MemberTypeHelper {
		typedef R (C::*MemberFuncType)(A...);

		template <typename T> struct IDENT {
			typedef T type;
		};

		template <R (C::*f)(A...), class M, IDENT<M>::type m >
		static R wrapper(const char *name, A...args)
		{
			C *obj = m->at(name);
			return ((*obj).*f)(args...);
		}

		template <R (C::*f)(A...)>
		static R wrappert(const char *name, A...args)
		{
			C *obj = 0;
			return ((*obj).*f)(args...);
		}
	};

	template <typename R, typename C, typename ...A>
	static MemberTypeHelper<R, C, A...>
	memberType(R (C::*f)(A...))
	{
		return MemberTypeHelper<R, C, A...>();
	}

	/*
	 * Build a iocshFuncDef with associated iocArg structs.
	 * If we were to wrap huge masses of user functions then
	 * we could keep a cache of most used iocArgs (same epics type,
	 * same help string) around but ATM we don't bother...
	 */
	template <typename R, typename ...A>
	static iocshFuncDef  *buildArgs( const char *fname, R (*f)(A...), std::initializer_list<const char *> argNames )
	{
		std::initializer_list<const char*>::const_iterator it;

		FuncDef     funcDef( fname, sizeof...(A) );
		// use array initializer to ensure order of execution
		iocshArg   *argp[] = { (makeArg<A>())... };
		if ( sizeof...(A) != funcDef.getNargs() ) {
			throw std::runtime_error("IocshDeclWrapper: internal error - number of argument mismatch");
		}
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
};

/* Specialization used when we are given an explicit signature
 */
template <typename T, typename ...SIG> struct DropBraces<T(SIG...)> {

	template <typename R>
	static DropBraces<void>::TypeHelper<R,SIG...> type(R (*f)(SIG...))
	{
		return DropBraces<void>::TypeHelper<R, SIG...>();
	}

	template <typename R, typename C>
	static DropBraces<void>::MemberTypeHelper<R,C,SIG...> memberType(R (C::*f)(SIG...))
	{
		return DropBraces<void>::MemberTypeHelper<R, C, SIG...>();
	}


	template <typename R>
	static iocshFuncDef  *buildArgs( const char *fname, R (*f)(SIG...), std::initializer_list<const char *> argNames )
	{
		return DropBraces<void>::buildArgs<R, SIG...>( fname, f, argNames );
	}
};

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
			return f( Convert<A>::getArg( &args[I], ctx, I )... );
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
dispatch(R (*f)(A...), const iocshArgBuf *args, typename EvalResult<R>::PrinterType printer, ArgPrinterType printArgs)
{
	try {
		Context ctx( args, sizeof...(A) );
		( EvalResult<R, PRINT>( printer ), /* <== magic 'operator,' */
		  ArgOrder<A...>::arrange( f, args , &ctx ) );
		if ( PRINT ) {
			printArgs( &ctx );
		}
	} catch ( ConversionError &e ) {
		errlogPrintf( "Error: Invalid Argument -- %s\n", e.what() );
	} catch ( std::exception &e ) {
		errlogPrintf( "Error: Exception -- %s\n", e.what() );
	} catch ( ... ) {
		errlogPrintf( "Error: Unknown Exception\n" );
        }
}

/*
 * This is the 'iocshCallFunc'
 */
template <typename RR, RR *p, bool PRINT=true> void call(const iocshArgBuf *args)
{
	dispatch<PRINT>( p, args, makeGuesser<RR>( p ).template getPrinter<p>(), makeGuesser<RR>( p ).template getArgPrinter<p>() );
}

}

#define IOCSH_FUNC_REGISTER_WRAPPER(x,signature,nm,doPrint,argHelps...) do {                     \
	using IocshDeclWrapper::DropBraces;                                                      \
	using IocshDeclWrapper::call;                                                            \
	iocshRegister( DropBraces<void signature>::buildArgs( nm, x, { argHelps } ), call<decltype(DropBraces<void signature>::type(x))::FuncType, x, doPrint> );        \
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

#define IOCSH_DECL_WRAPPER_DO_CALL(args...)                                              \
	do {                                                                             \
		try {                                                                    \
			EvalResult<R, PRINT>(                                            \
				Guesser<R, type>().template getPrinter<func> ()          \
            ), func( args ); /* <= magic 'operator,' */                                  \
			if ( PRINT ) {                                                   \
				Guesser<R, type>().template getArgPrinter<func>()( &ctx );   \
			}                                                                \
		} catch ( IocshDeclWrapper::ConversionError &e ) {                       \
			errlogPrintf( "Error: Invalid Argument -- %s\n", e.what() );     \
		} catch ( std::exception &e ) {                                          \
			errlogPrintf( "Error: Exception -- %s\n", e.what() );            \
		} catch ( ... ) {                                                        \
			errlogPrintf( "Error: Unknown Exception\n" );                    \
		}                                                                        \
	} while (0)

template <typename R, typename A0=void, typename A1=void, typename A2=void, typename A3=void, typename A4=void,
                      typename A5=void, typename A6=void, typename A7=void, typename A8=void, typename A9=void>
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
		IocshDeclWrapper::Context ctx( args, N );
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx, 0 ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx, 1 ),
			IocshDeclWrapper::Convert<A2>::getArg( &args[2], &ctx, 2 ),
			IocshDeclWrapper::Convert<A3>::getArg( &args[3], &ctx, 3 ),
			IocshDeclWrapper::Convert<A4>::getArg( &args[4], &ctx, 4 ),
			IocshDeclWrapper::Convert<A5>::getArg( &args[5], &ctx, 5 ),
			IocshDeclWrapper::Convert<A6>::getArg( &args[6], &ctx, 6 ),
			IocshDeclWrapper::Convert<A7>::getArg( &args[7], &ctx, 7 ),
			IocshDeclWrapper::Convert<A8>::getArg( &args[8], &ctx, 8 ),
			IocshDeclWrapper::Convert<A9>::getArg( &args[9], &ctx, 9 )
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
		IocshDeclWrapper::Context ctx( args, N );
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx, 0 ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx, 1 ),
			IocshDeclWrapper::Convert<A2>::getArg( &args[2], &ctx, 2 ),
			IocshDeclWrapper::Convert<A3>::getArg( &args[3], &ctx, 3 ),
			IocshDeclWrapper::Convert<A4>::getArg( &args[4], &ctx, 4 ),
			IocshDeclWrapper::Convert<A5>::getArg( &args[5], &ctx, 5 ),
			IocshDeclWrapper::Convert<A6>::getArg( &args[6], &ctx, 6 ),
			IocshDeclWrapper::Convert<A7>::getArg( &args[7], &ctx, 7 ),
			IocshDeclWrapper::Convert<A8>::getArg( &args[8], &ctx, 8 )
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
		IocshDeclWrapper::Context ctx( args, N );
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx, 0 ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx, 1 ),
			IocshDeclWrapper::Convert<A2>::getArg( &args[2], &ctx, 2 ),
			IocshDeclWrapper::Convert<A3>::getArg( &args[3], &ctx, 3 ),
			IocshDeclWrapper::Convert<A4>::getArg( &args[4], &ctx, 4 ),
			IocshDeclWrapper::Convert<A5>::getArg( &args[5], &ctx, 5 ),
			IocshDeclWrapper::Convert<A6>::getArg( &args[6], &ctx, 6 ),
			IocshDeclWrapper::Convert<A7>::getArg( &args[7], &ctx, 7 )
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
		IocshDeclWrapper::Context ctx( args, N );
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx, 0 ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx, 1 ),
			IocshDeclWrapper::Convert<A2>::getArg( &args[2], &ctx, 2 ),
			IocshDeclWrapper::Convert<A3>::getArg( &args[3], &ctx, 3 ),
			IocshDeclWrapper::Convert<A4>::getArg( &args[4], &ctx, 4 ),
			IocshDeclWrapper::Convert<A5>::getArg( &args[5], &ctx, 5 ),
			IocshDeclWrapper::Convert<A6>::getArg( &args[6], &ctx, 6 )
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
		IocshDeclWrapper::Context ctx( args, N );
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx, 0 ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx, 1 ),
			IocshDeclWrapper::Convert<A2>::getArg( &args[2], &ctx, 2 ),
			IocshDeclWrapper::Convert<A3>::getArg( &args[3], &ctx, 3 ),
			IocshDeclWrapper::Convert<A4>::getArg( &args[4], &ctx, 4 ),
			IocshDeclWrapper::Convert<A5>::getArg( &args[5], &ctx, 5 )
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
		IocshDeclWrapper::Context ctx( args, N );
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx, 0 ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx, 1 ),
			IocshDeclWrapper::Convert<A2>::getArg( &args[2], &ctx, 2 ),
			IocshDeclWrapper::Convert<A3>::getArg( &args[3], &ctx, 3 ),
			IocshDeclWrapper::Convert<A4>::getArg( &args[4], &ctx, 4 )
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
		IocshDeclWrapper::Context ctx( args, N );
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx, 0 ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx, 1 ),
			IocshDeclWrapper::Convert<A2>::getArg( &args[2], &ctx, 2 ),
			IocshDeclWrapper::Convert<A3>::getArg( &args[3], &ctx, 3 )
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
		IocshDeclWrapper::Context ctx( args, N );
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx, 0 ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx, 1 ),
			IocshDeclWrapper::Convert<A2>::getArg( &args[2], &ctx, 2 )
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
		IocshDeclWrapper::Context ctx( args, N );
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx, 0 ),
			IocshDeclWrapper::Convert<A1>::getArg( &args[1], &ctx, 1 )
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
		IocshDeclWrapper::Context ctx( args, N );
		IOCSH_DECL_WRAPPER_DO_CALL(
			IocshDeclWrapper::Convert<A0>::getArg( &args[0], &ctx, 0 )
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
		IocshDeclWrapper::Context ctx( args, N );
		IOCSH_DECL_WRAPPER_DO_CALL();
	}
};

/* A special trick to let the user specify overloaded functions. We want to do this
 * with a final macro:
 *   #define _WRAP( fun, overload_args, name, help... )
 * Because the pre-processor requires parentheses around anything containing commas
 * the macro must be expanded like this:
 *
 *   _WRAP( myFunc, (int, char*), "myFunc_1" )
 *
 * The following templates allow us to get rid of the parentheses and use the type
 * signatures...
 */
template <typename T> struct DropBraces;

/* First the specialization for T=void which auto-derives the types; this is used
 * when a function is not overloaded
 */
template <> struct DropBraces<void> {

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
static Caller<R,A0,A1,A2,A3,A4,A5,A6,A7,A8,A9> makeCaller(R (*f)(A0,A1,A2,A3,A4,A5,A6,A7,A8,A9))
{
	return Caller<R,A0,A1,A2,A3,A4,A5,A6,A7,A8,A9>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6, typename A7, typename A8>
static Caller<R,A0,A1,A2,A3,A4,A5,A6,A7,A8,void> makeCaller(R (*f)(A0,A1,A2,A3,A4,A5,A6,A7,A8))
{
	return Caller<R,A0,A1,A2,A3,A4,A5,A6,A7,A8,void>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6, typename A7>
static Caller<R,A0,A1,A2,A3,A4,A5,A6,A7,void,void> makeCaller(R (*f)(A0,A1,A2,A3,A4,A5,A6,A7))
{
	return Caller<R,A0,A1,A2,A3,A4,A5,A6,A7,void,void>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6>
static Caller<R,A0,A1,A2,A3,A4,A5,A6,void,void,void> makeCaller(R (*f)(A0,A1,A2,A3,A4,A5,A6))
{
	return Caller<R,A0,A1,A2,A3,A4,A5,A6,void,void,void>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5>
static Caller<R,A0,A1,A2,A3,A4,A5,void,void,void,void> makeCaller(R (*f)(A0,A1,A2,A3,A4,A5))
{
	return Caller<R,A0,A1,A2,A3,A4,A5,void,void,void,void>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4>
static Caller<R,A0,A1,A2,A3,A4,void,void,void,void,void> makeCaller(R (*f)(A0,A1,A2,A3,A4))
{
	return Caller<R,A0,A1,A2,A3,A4,void,void,void,void,void>();
}

template <typename R, typename A0, typename A1, typename A2, typename A3>
static Caller<R,A0,A1,A2,A3,void,void,void,void,void,void> makeCaller(R (*f)(A0,A1,A2,A3))
{
	return Caller<R,A0,A1,A2,A3,void,void,void,void,void,void>();
}

template <typename R, typename A0, typename A1, typename A2>
static Caller<R,A0,A1,A2,void,void,void,void,void,void,void> makeCaller(R (*f)(A0,A1,A2))
{
	return Caller<R,A0,A1,A2,void,void,void,void,void,void,void>();
}

template <typename R, typename A0, typename A1>
static Caller<R,A0,A1,void,void,void,void,void,void,void,void> makeCaller(R (*f)(A0,A1))
{
	return Caller<R,A0,A1,void,void,void,void,void,void,void,void>();
}

template <typename R, typename A0>
static Caller<R,A0,void,void,void,void,void,void,void,void,void> makeCaller(R (*f)(A0))
{
	return Caller<R,A0,void,void,void,void,void,void,void,void,void>();
}

template <typename R>
static Caller<R,void,void,void,void,void,void,void,void,void,void> makeCaller(R (*f)(void))
{
	return Caller<R,void,void,void,void,void,void,void,void,void,void>();
}
};

/* Now the trick to get rid of parenthesis: */
template <typename T, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6, typename A7, typename A8, typename A9>
struct DropBraces<T(A0,A1,A2,A3,A4,A5,A6,A7,A8,A9)> {
	template <typename R>
	static Caller<R,A0,A1,A2,A3,A4,A5,A6,A7,A8,A9>
	makeCaller(R (*f)(A0,A1,A2,A3,A4,A5,A6,A7,A8,A9))
	{
		return Caller<R,A0,A1,A2,A3,A4,A5,A6,A7,A8,A9>();
	}
};

template <typename T, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6, typename A7, typename A8>
struct DropBraces<T(A0,A1,A2,A3,A4,A5,A6,A7,A8)> {
	template <typename R>
	static Caller<R,A0,A1,A2,A3,A4,A5,A6,A7,A8>
	makeCaller(R (*f)(A0,A1,A2,A3,A4,A5,A6,A7,A8))
	{
		return Caller<R,A0,A1,A2,A3,A4,A5,A6,A7,A8>();
	}
};

template <typename T, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6, typename A7>
struct DropBraces<T(A0,A1,A2,A3,A4,A5,A6,A7)> {
	template <typename R>
	static Caller<R,A0,A1,A2,A3,A4,A5,A6,A7>
	makeCaller(R (*f)(A0,A1,A2,A3,A4,A5,A6,A7))
	{
		return Caller<R,A0,A1,A2,A3,A4,A5,A6,A7>();
	}
};

template <typename T, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5, typename A6>
struct DropBraces<T(A0,A1,A2,A3,A4,A5,A6)> {
	template <typename R>
	static Caller<R,A0,A1,A2,A3,A4,A5,A6>
	makeCaller(R (*f)(A0,A1,A2,A3,A4,A5,A6))
	{
		return Caller<R,A0,A1,A2,A3,A4,A5,A6>();
	}
};

template <typename T, typename A0, typename A1, typename A2, typename A3, typename A4,
                      typename A5>
struct DropBraces<T(A0,A1,A2,A3,A4,A5)> {
	template <typename R>
	static Caller<R,A0,A1,A2,A3,A4,A5>
	makeCaller(R (*f)(A0,A1,A2,A3,A4,A5))
	{
		return Caller<R,A0,A1,A2,A3,A4,A5>();
	}
};

template <typename T, typename A0, typename A1, typename A2, typename A3, typename A4>
struct DropBraces<T(A0,A1,A2,A3,A4)> {
	template <typename R>
	static Caller<R,A0,A1,A2,A3,A4>
	makeCaller(R (*f)(A0,A1,A2,A3,A4))
	{
		return Caller<R,A0,A1,A2,A3,A4>();
	}
};

template <typename T, typename A0, typename A1, typename A2, typename A3>
struct DropBraces<T(A0,A1,A2,A3)> {
	template <typename R>
	static Caller<R,A0,A1,A2,A3>
	makeCaller(R (*f)(A0,A1,A2,A3))
	{
		return Caller<R,A0,A1,A2,A3>();
	}
};

template <typename T, typename A0, typename A1, typename A2>
struct DropBraces<T(A0,A1,A2)> {
	template <typename R>
	static Caller<R,A0,A1,A2>
	makeCaller(R (*f)(A0,A1,A2))
	{
		return Caller<R,A0,A1,A2>();
	}
};

template <typename T, typename A0, typename A1>
struct DropBraces<T(A0,A1)> {
	template <typename R>
	static Caller<R,A0,A1>
	makeCaller(R (*f)(A0,A1))
	{
		return Caller<R,A0,A1>();
	}
};

template <typename T, typename A0>
struct DropBraces<T(A0)> {
	template <typename R>
	static Caller<R,A0>
	makeCaller(R (*f)(A0))
	{
		return Caller<R,A0>();
	}
};

template <typename T>
struct DropBraces<T(void)> {
	template <typename R>
	static Caller<R>
	makeCaller(R (*f)(void))
	{
		return Caller<R>();
	}
};

#undef IOCSH_DECL_WRAPPER_DO_CALL

}

#define IOCSH_FUNC_WRAP_MAX_ARGS 10

#define IOCSH_FUNC_REGISTER_WRAPPER(x,signature,nm,doPrint,argHelps...) do {                    \
	const char *argNames[IOCSH_FUNC_WRAP_MAX_ARGS + 1] = { argHelps };                      \
	using IocshDeclWrapper::buildArgs;                                                      \
	using IocshDeclWrapper::DropBraces;                                                     \
	iocshRegister( buildArgs( DropBraces<void signature>::makeCaller(x), nm, argNames ), DropBraces<void signature>::makeCaller(x).call<x,doPrint> );  \
	} while (0)

#endif /* __cplusplus >= 201103L */

#define IOCSH_FUNC_WRAP_REGISTRAR( registrarName, wrappers... ) \
static void registrarName() \
{ \
  wrappers \
} \
epicsExportRegistrar( registrarName ); \

/* Convenience macros */
#define IOCSH_FUNC_WRAP_OVLD( x, signature, nm, argHelps...) IOCSH_FUNC_REGISTER_WRAPPER(x, signature, nm, true,  argHelps)
#define IOCSH_FUNC_WRAP(      x,                argHelps...) IOCSH_FUNC_REGISTER_WRAPPER(x,          , #x, true,  argHelps)
#define IOCSH_FUNC_WRAP_QUIET(x,                argHelps...) IOCSH_FUNC_REGISTER_WRAPPER(x,          , #x, false, argHelps)

#if __cplusplus >= 201103L
#define IOCSH_MFUNC_WRAPPER(cls,memb,signature,map) decltype(DropBraces<void signature>::memberType( &cls::memb ))::template wrapper< &cls::memb, decltype(map), map>

#define IOCSH_MEMBER_WRAP_OVLD(map, cls, memb, signature, nm, argHelps...) \
	IOCSH_FUNC_REGISTER_WRAPPER( IOCSH_MFUNC_WRAPPER( cls, memb, signature, map ), , nm, true, "objName", argHelps )

#define IOCSH_MEMBER_WRAP(     map, cls, memb,                argHelps...) \
	IOCSH_FUNC_REGISTER_WRAPPER( IOCSH_MFUNC_WRAPPER( cls, memb,          , map ), , #cls"_"#memb, true, "objName", argHelps )

#endif

#endif
