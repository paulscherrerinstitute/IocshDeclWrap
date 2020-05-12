var testPassed 0
var testFailed 0

help c10
help c9
help c8
help c7
help c6
help c5
help c4
help c3
help c2
help c1
help c0
c0  0 1 2 3 4 5 6 7 8 9
c1  0 1 2 3 4 5 6 7 8 9
c2  0 1 2 3 4 5 6 7 8 9
c3  0 1 2 3 4 5 6 7 8 9
c4  0 1 2 3 4 5 6 7 8 9
c5  0 1 2 3 4 5 6 7 8 9
c6  0 1 2 3 4 5 6 7 8 9
c7  0 1 2 3 4 5 6 7 8 9
c8  0 1 2 3 4 5 6 7 8 9
c9  0 1 2 3 4 5 6 7 8 9
c10 0 1 2 3 4 5 6 7 8 9

myString("myString")
myStringr("myStringr")
mycString("mycString")
myStringp("myStringp")
mycStringp("mycStringp")
myHello("myHello")
mycHello("mycHello")

myFuncInt( 321 )

myFuncShort(-3)

myFloat(1.234)
myDouble(5.678)

# "vvvvvv Next line is *expected* to fail *********"
myComplex
myComplex 1.234j5.678

genMyType( 44 )

testNonPrinting

chp(44)
hp(45)
chr(84)
hr(85)
cfp(44.55)
fp(45.66)
sr("sr_foo")
csr("csr_foo")
sp("sp_foo")
csp("csp_foo")

testCheck()
exit
