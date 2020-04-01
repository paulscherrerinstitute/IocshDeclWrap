#include <stdio.h>
#include <stdint.h>
#include <epicsExit.h>
#include <epicsThread.h>
#include <errlog.h>
#include <iocshDeclWrapper.h>
#include <epicsExport.h>
#include <string>
#include <string.h>
#include <complex>

static int testFailed = 0;
static int testPassed = 0;

/*
template <> struct IocshDeclWrapper::PrintFmts<const char *, const char *>
{
	static const char **get() {
		static const char *r[] = { "foo -> %s <-", 0 };
		return r;
	}
};
*/

/* run
 *  grep 'testPassed[\t]*[+][+]' wrapper.cc  | wc
 * over this file
 */
#define NUM_TESTS 27

std::string myString(std::string s)
{
	if ( strcmp( s.c_str(), "myString" ) ) testFailed++; else testPassed++;
	printf("my STRING %s\n", s.c_str());
	return s;
}

std::string & myStringr(std::string &s)
{
	if ( strcmp( s.c_str(), "myStringr" ) ) testFailed++; else testPassed++;
	printf("my STRINGr %s\n", s.c_str());
	/* cannot just hand 's' back since it may not outlive
	 * this function call!
	 */
	return * new std::string( s );
}

const std::string mycString(const std::string s)
{
	if ( strcmp( s.c_str(), "mycString" ) ) testFailed++; else testPassed++;
	printf("my const STRING %s\n", s.c_str());
	return s;
}

std::string * myStringp(std::string *s)
{
	if ( strcmp( s->c_str(), "myStringp" ) ) testFailed++; else testPassed++;
	printf("my STRINGp %s\n", s ? s->c_str() : "<NULL>");
	/* cannot just hand 's' back since it may not outlive
	 * this function call!
	 */
	return new std::string( *s );
}

const std::string * mycStringp(const std::string *s)
{
	if ( strcmp( s->c_str(), "mycStringp" ) ) testFailed++; else testPassed++;
	printf("my cSTRINGp %s\n", s ? s->c_str() : "<NULL>");
	return new std::string( *s );
}

extern "C" void myNoarg()
{
	printf("my NOARG\n");
}

extern "C" void myVoidarg()
{
	printf("my VOIDARG\n");
}

extern "C" float myFloat(float a)
{
	if ( a != (float)1.234 ) {
		printf("Float test FAILED: expected %f, got %f\n", (float)1.234, a);
		testFailed++;
	} else {
		testPassed++;
	}
	printf("my FLOAT: %f\n", a);
	return a;
}

extern "C" double myDouble(double a)
{
	if ( a != 5.678 ) testFailed++; else testPassed++;
	printf("my DOUBLE: %f\n", a);
	return a;
}


extern "C" char * myHello(char *m)
{
	if ( strcmp( m, "myHello" ) ) testFailed++; else testPassed++;
	printf("From myHello: %s\n", m );
	/* cannot just hand 'm' back since it may not outlive
	 * this function call!
	 */
	return strdup( m );
}

extern "C" const char * mycHello(const char *m)
{
	if ( strcmp( m, "mycHello" ) ) testFailed++; else testPassed++;
	printf("From mycHello: %s\n", m);
	/* cannot just hand 'm' back since it may not outlive
	 * this function call!
	 */
	return strdup(m);
}


extern "C" int myFuncUInt(unsigned int a)
{
	printf("myFuncUInt  %i\n", a);
	return 0;
}

extern "C" uint32_t myFuncU32(uint32_t a)
{
	printf("myFuncUInt  %i\n", a);
	return a;
}



extern "C" int myFuncInt(int a)
{
	printf("myFuncInt  %i\n", a);
	return a;
}

extern "C" short myFuncShort(short a)
{
	if ( -3 != a ) testFailed++; else testPassed++;
	printf("myFuncShort %hi\n", a);
	return a;
}

extern "C" int c0()
{
	testPassed++;
	return printf("void\n");
}

namespace IocshDeclWrapperTest {

int c1(int a0)
{
	if ( 0 == a0 ) {
		testPassed++;
	} else {
		testFailed++;
	}
	return printf("A1 %i\n", a0);
}

int c2(int a0, int a1)
{
	if ( 0 == a0 && 1 == a1 ) {
		testPassed++;
	} else {
		testFailed++;
	}
	return printf("A2 %i %i\n", a0, a1);
}

int c3(int a0, int a1, int a2)
{
	if ( 0 == a0 && 1 == a1 && 2 == a2 ) {
		testPassed++;
	} else {
		testFailed++;
	}
	return printf("A3 %i %i %i\n", a0, a1, a2);
}

int c4(int a0, int a1, int a2, int a3)
{
	if ( 0 == a0 && 1 == a1 && 2 == a2 && 3 == a3 ) {
		testPassed++;
	} else {
		testFailed++;
	}
	return printf("A4 %i %i %i %i\n", a0, a1, a2, a3);
}

int c5(int a0, int a1, int a2, int a3, int a4)
{
	if ( 0 == a0 && 1 == a1 && 2 == a2 && 3 == a3 && 4 == a4 ) {
		testPassed++;
	} else {
		testFailed++;
	}
	return printf("A5 %i %i %i %i %i\n", a0, a1, a2, a3, a4);
}

int c6(int a0, int a1, int a2, int a3, int a4, int a5)
{
	if ( 0 == a0 && 1 == a1 && 2 == a2 && 3 == a3 && 4 == a4 &&
         5 == a5 ) {
		testPassed++;
	} else {
		testFailed++;
	}
	return printf("A6 %i %i %i %i %i %i\n", a0, a1, a2, a3, a4, a5);
}

int c7(int a0, int a1, int a2, int a3, int a4, int a5, int a6)
{
	if ( 0 == a0 && 1 == a1 && 2 == a2 && 3 == a3 && 4 == a4 &&
         5 == a5 && 6 == a6 ) {
		testPassed++;
	} else {
		testFailed++;
	}
	return printf("A7 %i %i %i %i %i %i %i\n", a0, a1, a2, a3, a4, a5, a6);
}

int c8(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7)
{
	if ( 0 == a0 && 1 == a1 && 2 == a2 && 3 == a3 && 4 == a4 &&
         5 == a5 && 6 == a6 && 7 == a7 ) {
		testPassed++;
	} else {
		testFailed++;
	}
	return printf("A8 %i %i %i %i %i %i %i %i\n", a0, a1, a2, a3, a4, a5, a6, a7);
}

int c9(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8)
{
	if ( 0 == a0 && 1 == a1 && 2 == a2 && 3 == a3 && 4 == a4 &&
         5 == a5 && 6 == a6 && 7 == a7 && 8 == a8 ) {
		testPassed++;
	} else {
		testFailed++;
	}
	return printf("A9 %i %i %i %i %i %i %i %i %i\n", a0, a1, a2, a3, a4, a5, a6, a7, a8);
}

int c10(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9)
{
	if ( 0 == a0 && 1 == a1 && 2 == a2 && 3 == a3 && 4 == a4 &&
         5 == a5 && 6 == a6 && 7 == a7 && 8 == a8 && 9 == a9 ) {
		testPassed++;
	} else {
		testFailed++;
	}
	return printf("A10 %i %i %i %i %i %i %i %i %i %i\n", a0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
}

class MyType {
	int i;
public:
	MyType(int i)
	: i( i )
	{
	}

	int operator()() const
	{
		return i;
	}
};

MyType testNonPrinting()
{
	testPassed++; /* the printer decrements when executed */
	return MyType( 0 );
}

MyType genMyType(MyType &r)
{
	return r;
}

std::complex<double> myComplex(std::complex<double> val)
{
	if ( 1.234 != val.real() || 5.678 != val.imag() ) testFailed++; else testPassed++;
	printf("myComplex: %g j %g\n", val.real(), val.imag());
	return val;
}


void testCheck()
{
	if ( 0 == testFailed && NUM_TESTS == testPassed ) {
		errlogPrintf("All %d Tests PASSED\n", testPassed);
	} else {
		if ( testFailed ) {
			errlogPrintf("%d tests FAILED\n", testFailed);
		}
		if ( NUM_TESTS != testPassed + testFailed ) {
			errlogPrintf("%d tests MISSED\n", NUM_TESTS - testPassed - testFailed);
		}
		epicsThreadSleep(0.5);
		epicsExit(1);
	}
}

};

namespace IocshDeclWrapper {

using namespace IocshDeclWrapperTest;

template <> class Convert<MyType &, MyType &> {
public:
	static void setArg( iocshArg *a )
	{
		a->name = "MyType";
		a->type = iocshArgInt;
	}

	static MyType & getArg( const iocshArgBuf *arg, Context *ctx )
	{
		testPassed++;
		return *ctx->make<MyType>( arg->ival );
	}
};

template <> class PrinterBase<MyType, MyType> {
public:
	static void print(const MyType & r)
	{
		if ( 44 == r() ) testPassed++; else testFailed++;
		errlogPrintf("Printer for 'MyType' : %i\n", r());
	}
};

template <> class Printer<MyType, MyType(), testNonPrinting> : public PrinterBase<MyType, MyType> {
public:
	static void print(const MyType & r)
	{
		// this should not be called
		testFailed++;
		testPassed--;
		errlogPrintf("Printer for 'testNonPrinting' : %i\n", r());
	}
};

template <> class Printer<int, int(int), myFuncInt> : public PrinterBase<int,int> {
public:
	static void print(const int &v)
	{
		if (321 == v) testPassed++; else testFailed++;
		errlogPrintf("Printer for myFuncInt (v==321) ? %s\n", (321==v) ? "TRUE" : "FALSE" );
	}
};

/* More specialized than:
 *  template <typename T> class PrinterBase< T, typename is_cplx< T >::type > { }
 *
 * Alternatively we could also use
template <typename T> class PrinterBase< T, typename is_cplx< T >::type, 0 > { }
 */
#undef  USE_INTEGER_SPECIFIER
#ifdef  USE_INTEGER_SPECIFIER
template <typename T> class PrinterBase< T, typename is_cplx< T >::type, 0 >
{
public:
	static void print(const T &r)
	{
		const char **fmts = PrintFmts<typename T::value_type>::get();
		if ( ! fmts) {
			testFailed++;
		} else {
			errlogPrintf("My Complex Printer ");
			errlogPrintf( fmts[0], r.real());
			errlogPrintf(" J ");
			errlogPrintf( fmts[0], r.imag());
			errlogPrintf("\n");
			testPassed++;
		}
	}
};
#else
template <typename T> class PrinterBase< std::complex<T>, typename is_cplx< std::complex<T> >::type >
{
public:
	static void print(const std::complex<T> &r)
	{
		const char **fmts = PrintFmts<T>::get();
		if ( ! fmts) {
			testFailed++;
		} else {
			errlogPrintf("My Complex Printer ");
			errlogPrintf( fmts[0], r.real());
			errlogPrintf(" J ");
			errlogPrintf( fmts[0], r.imag());
			errlogPrintf("\n");
			testPassed++;
		}
	}
};
#endif


}

using namespace IocshDeclWrapperTest;

IOCSH_FUNC_WRAP_REGISTRAR(wrapperRegister,
	IOCSH_FUNC_WRAP( myHello    );
	IOCSH_FUNC_WRAP( mycHello   );
	IOCSH_FUNC_WRAP( myFuncShort);
	IOCSH_FUNC_WRAP( myFuncInt  );
	IOCSH_FUNC_WRAP( myFuncUInt );
	IOCSH_FUNC_WRAP( myFuncU32, "uint32_t" );
	IOCSH_FUNC_WRAP( myNoarg    );
	IOCSH_FUNC_WRAP( myVoidarg  );
	IOCSH_FUNC_WRAP( c0, "xxx"  );
	IOCSH_FUNC_WRAP( c1, "h1", "h2", "h3", "h4", "h5", "h6", "h7", "h8", "h9", "h10" );
	IOCSH_FUNC_WRAP( c2, "h1", "h2", "h3", "h4", "h5", "h6", "h7", "h8", "h9", "h10" );
	IOCSH_FUNC_WRAP( c3, "h1", "h2", "h3", "h4", "h5", "h6", "h7", "h8", "h9", "h10" );
	IOCSH_FUNC_WRAP( c4, "h1", "h2", "h3", "h4", "h5", "h6", "h7", "h8", "h9", "h10" );
	IOCSH_FUNC_WRAP( c5, "h1", "h2", "h3", "h4", "h5", "h6", "h7", "h8", "h9", "h10" );
	IOCSH_FUNC_WRAP( c6, "h1", "h2", "h3", "h4", "h5", "h6", "h7", "h8", "h9", "h10" );
	IOCSH_FUNC_WRAP( c7, "h1", "h2", "h3", "h4", "h5", "h6", "h7", "h8", "h9", "h10" );
	IOCSH_FUNC_WRAP( c8, "h1", "h2" );
	IOCSH_FUNC_WRAP( c9, "h1", "h2", "h3", "h4", "h5", "h6", "h7", "h8", "h9", "h10" );
	IOCSH_FUNC_WRAP( c10, "h1", "h2", "h3", "h4", "h5", "h6", "h7", "h8", "h9", "h10");
	IOCSH_FUNC_WRAP( testCheck  );
	IOCSH_FUNC_WRAP( myFloat    );
	IOCSH_FUNC_WRAP( myDouble   );
	IOCSH_FUNC_WRAP( myString   );
	IOCSH_FUNC_WRAP( mycString  );
	IOCSH_FUNC_WRAP( myStringr  );
	IOCSH_FUNC_WRAP( myStringp  );
	IOCSH_FUNC_WRAP( mycStringp );
	IOCSH_FUNC_WRAP( myComplex  );
	IOCSH_FUNC_WRAP( genMyType  );
	IOCSH_FUNC_WRAP_QUIET( testNonPrinting  );
)

epicsExportAddress(int, testPassed);
epicsExportAddress(int, testFailed);
