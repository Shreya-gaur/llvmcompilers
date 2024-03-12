#---------------------------------------------------------
# Copyright (c) 2024, Jianping Zeng.
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

__unittest = True

class EmitTests(unittest.TestCase):

    def setUp(self):
        self.maxDiff = None
        if not os.path.isfile(uscc):
            raise Exception("Can't run without uscc")

    def checkRDA(self, fileName, detection = False):
        # read in expected
        if detection:
            expectFile = open("expected/" + fileName + ".uninitialized.output",
                              "r")
        else:
            expectFile = open("expected/" + fileName + ".rda.output", "r")
        expectedStr = expectFile.read()
        expectFile.close()
        # Check if the output string is expected.
        if detection:
            try:
                resultStr = subprocess.check_output([uscc, "-uninitialized-vars",
                                                     fileName + ".usc"],
                                                    stderr=subprocess.STDOUT)
                self.assertMultiLineEqual(expectedStr, resultStr)
            except subprocess.CalledProcessError as e:
                self.fail("\n" + e.output)
        else:
            try:
                resultStr = subprocess.check_output([uscc, "-rda",
                                                     fileName + ".usc"],
                                                    stderr=subprocess.STDOUT)
                self.assertMultiLineEqual(expectedStr, resultStr)
            except subprocess.CalledProcessError as e:
                self.fail("\n" + e.output)


    def test_rda01(self):
        self.checkRDA("rda01")
    def test_rda02(self):
        self.checkRDA("rda02")
    def test_rda03(self):
        self.checkRDA("rda03")
    def test_rda04(self):
        self.checkRDA("rda04")
    def test_rda05(self):
        self.checkRDA("rda05")
    def test_rda06(self):
        self.checkRDA("rda06")
    def test_rda07(self):
        self.checkRDA("rda07")
    def test_rda08(self):
        self.checkRDA("rda08")
    def test_rda09(self):
        self.checkRDA("rda09")
    def test_rda10(self):
        self.checkRDA("rda10")
    def test_rda11(self):
        self.checkRDA("rda11")
    def test_uninitializedVars01(self):
        self.checkRDA("rda01", True)
    def test_uninitializedVars02(self):
        self.checkRDA("rda02", True)
    def test_uninitializedVars03(self):
        self.checkRDA("rda03", True)
    def test_uninitializedVars04(self):
        self.checkRDA("rda04", True)
    def test_uninitializedVars05(self):
        self.checkRDA("rda05", True)
    def test_uninitializedVars06(self):
        self.checkRDA("rda06", True)
    def test_uninitializedVars07(self):
        self.checkRDA("rda07", True)
    def test_uninitializedVars08(self):
        self.checkRDA("rda08", True)
    def test_uninitializedVars09(self):
        self.checkRDA("rda09", True)
    def test_uninitializedVars10(self):
        self.checkRDA("rda10", True)
    def test_uninitializedVars11(self):
        self.checkRDA("rda11", True)

if __name__ == '__main__':
    unittest.main(verbosity=2)
