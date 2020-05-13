var testPassed 0
var testFailed 0

# Command output checking:
#   START
#   COMMANDS
#   END
#
#   START: #####
#   END:   #####
#   COMMANDS:
#       PATTERNS
#       COMMAND
#
#   PATTERNS:
#       PATTERN | COMMENT
#
#   COMMENT:  ^[ \t]*#[^#].*    (line starting with whitespace, a hash tag followed by non-hash)
#   PATTERN:  ^[ \t]*##[r=]##.* (whitespace, ##=## or ##r##, pattern)
#
#   ##=## defines a 'literal' pattern
#   ##r## defines a 'regexp'  pattern
#
# As many patterns must precede a command as answering lines from the command
# are expected. Empty lines are not allowed.

#####
##=##c10 h1 h2 h3 h4 h5 h6 h7 h8 h9 h10
help c10
##=##c9 h1 h2 h3 h4 h5 h6 h7 h8 h9
help c9
##=##c8 h1 h2 <int> <int> <int> <int> <int> <int>
help c8
##=##c7 h1 h2 h3 h4 h5 h6 h7
help c7
##=##c6 h1 h2 h3 h4 h5 h6
help c6
##=##c5 h1 h2 h3 h4 h5
help c5
##=##c4 h1 h2 h3 h4
help c4
##=##c3 h1 h2 h3
help c3
##=##c2 h1 h2
help c2
##=##c1 h1
help c1
##=##c0
help c0
##=##void
##=##5 (0x00000005)
c0  0 1 2 3 4 5 6 7 8 9
##=##A1 0
##=##5 (0x00000005)
c1  0 1 2 3 4 5 6 7 8 9
##=##A2 0 1
##=##7 (0x00000007)
c2  0 1 2 3 4 5 6 7 8 9
##=##A3 0 1 2
##=##9 (0x00000009)
c3  0 1 2 3 4 5 6 7 8 9
##=##A4 0 1 2 3
##=##11 (0x0000000b)
c4  0 1 2 3 4 5 6 7 8 9
##=##A5 0 1 2 3 4
##=##13 (0x0000000d)
c5  0 1 2 3 4 5 6 7 8 9
##=##A6 0 1 2 3 4 5
##=##15 (0x0000000f)
c6  0 1 2 3 4 5 6 7 8 9
##=##A7 0 1 2 3 4 5 6
##=##17 (0x00000011)
c7  0 1 2 3 4 5 6 7 8 9
##=##A8 0 1 2 3 4 5 6 7
##=##19 (0x00000013)
c8  0 1 2 3 4 5 6 7 8 9
##=##A9 0 1 2 3 4 5 6 7 8
##=##21 (0x00000015)
c9  0 1 2 3 4 5 6 7 8 9
##=##A10 0 1 2 3 4 5 6 7 8 9
##=##24 (0x00000018)
c10 0 1 2 3 4 5 6 7 8 9
##=##my STRING myString
##r##0x[0-9a-f]+ [-][>] myString
##=##Mutable arguments after execution:
##r##arg[[]0[]]: 0x[0-9a-f]+ [-][>] myString
myString("myString")
##=##my STRINGr myStringr
##r##0x[0-9a-f]+ [-][>] myStringr
##=##Mutable arguments after execution:
##r##arg[[]0[]]: 0x[0-9a-f]+ [-][>] myStringr
myStringr("myStringr")
##=##my const STRING mycString
##r##0x[0-9a-f]+ [-][>] mycString
##=##Mutable arguments after execution:
##r##arg[[]0[]]: 0x[0-9a-f]+ [-][>] mycString
mycString("mycString")
##=##my STRINGp myStringp
##r##0x[0-9a-f]+ [-][>] myStringp
##=##Mutable arguments after execution:
##r##arg[[]0[]]: 0x[0-9a-f]+ [-][>] myStringp
myStringp("myStringp")
##=##my cSTRINGp mycStringp
##r##0x[0-9a-f]+ [-][>] mycStringp
mycStringp("mycStringp")
##=##From myHello: myHello
##r##0x[0-9a-f]+ [-][>] myHello
##=##Mutable arguments after execution:
##r##arg[[]0[]]: 0x[0-9a-f]+ [-][>] myHello
myHello("myHello")
##=##From mycHello: mycHello
##r##0x[0-9a-f]+ [-][>] mycHello
mycHello("mycHello")
##=##myFuncInt  321
##=##Printer for myFuncInt (v==321) ? TRUE
myFuncInt( 321 )
##=##myFuncShort -3
##=##-3 (0xfffd)
myFuncShort(-3)
##=##my FLOAT: 1.234000
##=##1.234
myFloat(1.234)
##=##my DOUBLE: 5.678000
##=##5.678
myDouble(5.678)
##r##^\n$
# "vvvvvv Next line is *expected* to fail *********"
# with:
#   Error: Invalid Argument -- unable to scan argument into '%g j %g' format
#
myComplex
##=##myComplex: 1.234 j 5.678
##=##My Complex Printer 1.234 J 5.678
myComplex 1.234j5.678
##=##Printer for 'MyType' : 44
genMyType( 44 )
##r##^\n$
testNonPrinting
##=##44 (0x002c)
chp(44)
##=##46 (0x002e)
##=##Mutable arguments after execution:
##=##arg[0]: 46 (0x002e)
hp(45)
##=##84 (0x0054)
chr(84)
##=##86 (0x0056)
##=##Mutable arguments after execution:
##=##arg[0]: 86 (0x0056)
hr(85)
##=##44.55
cfp(44.55)
##=##46.66
##=##Mutable arguments after execution:
##=##arg[0]: 46.66
fp(45.66)
##r##0x[0-9a-f]+ [-][>] haggaloo
##=##Mutable arguments after execution:
##r##arg[[]0[]]: 0x[0-9a-f]+ [-][>] haggaloo
sr("sr_foo")
##r##0x[0-9a-f]+ [-][>] csr_foo
csr("csr_foo")
##r##0x[0-9a-f]+ [-][>] haggaloo
##=##Mutable arguments after execution:
##r##arg[[]0[]]: 0x[0-9a-f]+ [-][>] haggaloo
sp("sp_foo")
##r##0x[0-9a-f]+ [-][>] csp_foo
csp("csp_foo")
#####
testCheck()
exit
