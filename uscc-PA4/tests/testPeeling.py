#---------------------------------------------------------
# Copyright (c) 2014, Sanjay Madhav
# All rights reserved.
#
# This file is distributed under the BSD license.
# See LICENSE.TXT for details.
#---------------------------------------------------------
import subprocess
import os
import sys

import unittest
uscc = "../bin/uscc"
lli = "../../bin/lli"

__unittest = True

class EmitTests(unittest.TestCase):

	def setUp(self):
		self.maxDiff = None
		if not os.path.isfile(uscc):
			raise Exception("Can't run without uscc")
		if not os.path.isfile(lli):
			raise Exception("lli not found at ../../bin/lli")

	def checkPeeling(self, fileName):
		# read in expected
		expectFile = open("expected/" + fileName + ".output", "r")
		expectedStr = expectFile.read()
		expectFile.close()
		# first compile the .bc using uscc
		try:
			subprocess.check_call([uscc, "-P", fileName + ".usc"], stderr=subprocess.STDOUT)
		except subprocess.CalledProcessError as e:
			self.fail("\n" + e.output)

		# now run it in lli and compare the output
		try:
			resultStr = subprocess.check_output([lli, fileName + ".bc"], stderr=subprocess.STDOUT)
			self.assertMultiLineEqual(expectedStr, resultStr)
		except subprocess.CalledProcessError as e:
			self.fail("\n" + e.output)

	def test_Peeling_peel01(self):
		self.checkPeeling("peel01")

	def test_Peeling_peel02(self):
		self.checkPeeling("peel02")

	def test_Peeling_peel03(self):
		self.checkPeeling("peel03")

	def test_Peeling_peel04(self):
		self.checkPeeling("peel04")

	def test_Peeling_peel05(self):
		self.checkPeeling("peel05")

	def test_Peeling_peel06(self):
		self.checkPeeling("peel06")

if __name__ == '__main__':
	unittest.main(verbosity=2)
