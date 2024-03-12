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
import re

import unittest
uscc = "../bin/uscc"

__unittest = True

class PeelTests(unittest.TestCase):

    def setUp(self):
        self.maxDiff = None
        if not os.path.isfile(uscc):
            raise Exception("Can't run without uscc")

    def checkLift(self, fileName):
        # read in expected
        expectFile = open("expected/" + fileName + "_lifted.usc", "r")
        expectedStr = expectFile.read()
        expectedStr_ = re.sub(r"[\n\t\s]*", "", expectedStr)
        expectFile.close()
        try:
            resultStr = subprocess.check_output([uscc, "-L", fileName + ".usc"], stderr=subprocess.STDOUT)
            resultStr_ = re.sub(r"[\n\t\s]*", "", resultStr)
            self.assertMultiLineEqual(expectedStr_, resultStr_)
        except subprocess.CalledProcessError as e:
            self.fail("\n" + e.output)

    def checkPeelLift(self, fileName):
        # read in expected
        expectFile = open("expected/" + fileName + "_lifted.usc", "r")
        expectedStr = expectFile.read()
        expectedStr_ = re.sub(r"[\n\t\s]*", "", expectedStr)
        expectFile.close()
        try:
            resultStr = subprocess.check_output([uscc, "-L", "-P", fileName + ".usc"], stderr=subprocess.STDOUT)
            resultStr_ = re.sub(r"[\n\t\s]*", "", resultStr)
            self.assertMultiLineEqual(expectedStr_, resultStr_)
        except subprocess.CalledProcessError as e:
            self.fail("\n" + e.output)

    def test_lift_peel01(self):
        self.checkPeelLift("peel01")

    def test_lift_peel02(self):
        self.checkPeelLift("peel02")

    def test_lift_peel03(self):
        self.checkPeelLift("peel03")

    def test_lift_peel04(self):
        self.checkPeelLift("peel04")

    def test_lift_peel05(self):
        self.checkPeelLift("peel05")

    def test_lift_peel06(self):
        self.checkPeelLift("peel06")

    def test_lift_emit01(self):
        self.checkLift("emit01")

    def test_lift_emit02(self):
        self.checkLift("emit02")

    def test_lift_emit03(self):
        self.checkLift("emit03")

    def test_lift_emit04(self):
        self.checkLift("emit04")

    def test_lift_emit05(self):
        self.checkLift("emit05")

    def test_lift_emit06(self):
        self.checkLift("emit06")

    def test_lift_emit07(self):
        self.checkLift("emit07")

    def test_lift_emit08(self):
        self.checkLift("emit08")

    def test_lift_emit09(self):
        self.checkLift("emit09")

    def test_lift_emit10(self):
        self.checkLift("emit10")

    def test_lift_emit11(self):
        self.checkLift("emit11")

    def test_lift_emit12(self):
        self.checkLift("emit12")

    def test_lift_quicksort(self):
        self.checkLift("quicksort")

if __name__ == '__main__':
    unittest.main(verbosity=2)
