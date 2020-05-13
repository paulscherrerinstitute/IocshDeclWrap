#!/usr/bin/python3
import re
import sys

expectedCommands = 47

separator=re.compile("^#####\n$")
comment  =re.compile("^[ \t]*[#][^#].*")
pattern  =re.compile("^([ \t]*##([r=])##)([^\n]*[\n]?)$")

class Match:
  def __init__(self, typ, pat):
    if 'r' == typ:
      self.isRE = True
    elif '=' == typ:
      self.isRE = False
    else:
      raise RuntimeError("Match: unexpected pattern type")
    self.pat = pat

  def match(self, s):
    if self.isRE:
      return None != re.fullmatch( self.pat, s )
    else:
      return s == self.pat

  def __repr__(self):
    if self.isRE:
      return("  RE: {}".format(self.pat))
    else:
      return("  LI: {}".format(self.pat))

class Parser:
  def __init__(self):
    self.lno = 0
    self.l   = None
    self.getLineRaw()

  def getLineRaw(self):
    self.l    = sys.stdin.readline()
    if 0 == len(self.l):
      raise RuntimeError("EOL reached")
    self.lno += 1

  def getLine(self):
    self.getLineRaw()
    while None != comment.match(self.l) :
      self.getLineRaw()

  def skipHeader(self):
    while None == separator.fullmatch(self.l):
      self.getLineRaw()
    # skip the separator line
    self.getLine()

  def getPatterns(self):
    p = []
    while True:
      if None != separator.fullmatch(self.l):
        if len(p) > 0:
          raise RuntimeError("getPatterns: terminator while assembling patterns")
        else:
          ## don't get new line -- we're done
          return p
      m = pattern.fullmatch(self.l)
      if None == m:
        break
      g = m.groups()
      p.append( Match( g[1], g[2] ) )
      self.getLine()
    if 0 == len(p):
      raise RuntimeError("No patterns found (line {})".format(self.getLno()))
    return p
  
  def getAnswer(self):
    # no comments allowed in command + answer
    # 1st. iteration skip command itself
    ans = []
    while True:
      self.getLineRaw()
  	# if last command has empty answer then a separator is legal
      if None != separator.fullmatch(self.l) or None != pattern.fullmatch(self.l):
        break
      ans.append(self.l)
    return ans

  def getLno(self):
    return self.lno
  
  def parse(self):
    self.skipHeader()
    passedOutputs  = 0
    passedCommands = 0
    while True:
      p = self.getPatterns()
      if False:
        print("Got patterns")
        for pp in p:
          print( pp )
      if len(p) == 0:
        return passedOutputs, passedCommands
      a = self.getAnswer()
      if 0 == len(a):
        # empty answer
        if 1 == len(p) and not p[0].match("\n"):
          passedOutputs  += 1
          passedCommands += 1
        else:
          raise RuntimeError("no answer (line {})", self.getLno())
        continue
      if len(p) != len(a):
        for pp in p:
          print("  {}".format(pp))
        for aa in a:
          print("  A: {}".format(aa))
        raise RuntimeError("number of patterns ({}) does not match number of answers ({}) (before line {})".format(len(p), len(a), self.getLno()))
      for i in range(len(p)):
        if not p[i].match( a[i] ):
          print("Line {:d}: mismatch".format(self.getLno() - len(p) + i))
          print("  Got: '{}'".format(a[i]))
          print("  Exp: '{}'".format(p[i]))
          raise RuntimeError("mismatch")
        passedOutputs += 1
      passedCommands += 1
  
p = Parser()

answers, commands = p.parse()

if ( commands != expectedCommands ):
  raise RuntimeError("Expected {} commands, but encountered only {}".format(expectedCommands, commands))

print("TESTS PASSED: {} commands verified".format(commands))
