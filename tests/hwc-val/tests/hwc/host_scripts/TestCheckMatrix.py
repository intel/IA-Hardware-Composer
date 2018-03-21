#!/usr/bin/env python
# -*- coding: ascii -*-

'''
TestCheckMatrix: A script for combining HWC validation test results
into a test check matrix, that is, a matrix comprising of the individual
test results expressed as a series of check failures.
'''

from __future__ import division
from __future__ import print_function
import sys, os, re
from io import open
from xmlbuilder import XMLBuilder

__author__     = u'James Pascoe/Robert Pinsker'
__copyright__  = u'Copyright 2015 Intel Corporation'

### Constants ###

DELIMITER = u","
FILE_NAME_REGEX = u"results_(.+?)\.csv"
FILE_NAME_DEFAULT_REGEX = u"results\.csv"
NUMBER_OF_FILES_REGEX = u"^(\d+)"
TEST_PASS_FAIL_ERROR_REGEX = ur"Test Pass/Fail/Error,(\d+),(\d+),(\d+)"
DEFAULT_FILE_NAME = u"results"

### Classes

class ResultCount:
    def __init__(self):
        self.passes = 0
        self.fails = 0
        self.errors = 0

    def cpass(self):
        self.passes += 1

    def cfail(self):
        self.fails += 1

    def cerr(self):
        self.errors += 1

    def add(self, other):
        self.passes += other.passes
        self.fails += other.fails
        self.errors += other.errors

    def passRate(self):
        try:
            rate = (float(self.passes) / (self.passes + self.fails + self.errors))*100
        except ZeroDivisionError:
            rate = 0.0
        return rate


class Check:
    def __init__(self, name, component):
        self.name = name
        self.component = component
        self.values=[]
        self.count = ResultCount()

    def add(self, startValueCount, values):
        while (len(self.values) < startValueCount):
            values.append()
        self.values.extend(values)


### Functions ###

def generate_xml_file(
    legacy_pass, legacy_fail, legacy_error, \
    total_passes, total_fails, total_errors, total_pass_rate):

    if (legacy_fail > 0):
        overall_result = "F"
    elif (legacy_pass > 0):
        overall_result = "P"
    else:
        overall_result = "E"
    x = XMLBuilder("ROOT")
    with x.TestResult:
        x.Description("Overall Test Result, Individual Test Results and Cumulative Totals")
        with x.Measurements:
            '''Standard PAVE indicators'''
            x.Measurement(unicode(legacy_pass), name="Test Passed")
            x.Measurement(unicode(legacy_fail), name="Test Failed")
            x.Measurement(unicode(legacy_error), name="Test Errored")

            x.Measurement(unicode(total_passes), name="Cumulative: Checks Passed")
            x.Measurement(unicode(total_fails), name="Failed")
            x.Measurement(unicode(total_errors), name="Errors")
            x.Measurement(unicode("{0} %".format(total_pass_rate)), name="Cumulative Pass Rate")

        x.FilePathList()
        x.RebootRequired("1")
        x.FinalResult(overall_result)
    return x

### Main Code ###

if __name__ == u'__main__':

    '''Check arguments and print a usage message if no files have
    have been specified'''
    if (len(sys.argv) < 3):
        sys.stderr.write("Usage: " + sys.argv[0] + " { file.csv | file1.csv ... filen.csv } out.xml [ destfile.csv ]\n")
        sys.exit(0)

    inputFiles=[]
    xmlFileName=""
    destFileName="results_final.csv"

    for filename in sys.argv[1:]:
        if filename.endswith(".csv"):
            if xmlFileName=="":
                inputFiles.append(filename)
            else:
                destFileName=filename
        else:
            xmlFileName=filename

    '''Open the output file'''
    out = open(destFileName,"w")

    '''Read the contents of each file into the test check matrix i.e.
    a dictionary (indexed by component) of dictionaries (indexed by check)
    which map to a list of the individual test results. Store the file
    names and column totals so that they can be included in the output'''
    test_check_matrix = {}

    '''Declare lists for the file names, errored files and tests. Use dictionaries
    for the column and component totals (see below for details).'''
    files = []
    test_names = []

    checks={}
    columns=[]

    # Process each file in-turn
    processing_file = 0
    num_files = 1
    for file_name in inputFiles:

        # Perform some sanity checks
        try:
            if os.stat(file_name).st_size == 0:
                print(u"Warning: " + file_name + u" is empty - skipping file")
                continue
        except OSError:
            print(u"Warning: " + file_name + u" can not be found - skipping file")
            continue

        # Looks good - open the file
        file = open(file_name,'r')

        not_eof = True
        while (not_eof):

            '''Process the test pass / fail / error line from the harness results'''
            line = file.readline().strip()
            match = re.search(TEST_PASS_FAIL_ERROR_REGEX, line)
            if match:
                legacy_pass = int(match.group(1))
                legacy_fail = int(match.group(2))
                legacy_error = int(match.group(3))
            else:
                print (u"Expecting to see test pass/fail/error line. Instead saw: " + line)
                sys.exit(0)

            '''Process the file's header. Store the test name (as defined in the header
            for use when printing the output.'''
            line = file.readline().strip()
            files = line.split(DELIMITER)[2:]

            line = file.readline().strip()
            if (line[:16] != "Check,Component,"):
                print (u"Malformed header, rejecting file "+file_name)
                break
            header = line.split(DELIMITER)
            file_test_names = header[2:]
            numColumnsSoFar = len(test_names)
            test_names += file_test_names

            # Load the main body of the file
            for line in file:
                fields = line.strip().split(DELIMITER)
                if (len(fields) < 3):
                    print ("Warning: ignoring malformed line - " + line)
                    continue

                '''Field 0 is the check name. Field 1 is the component and Fields 2..n
                are the test value (blank indicates that the check was not exercised,
                0 indicates that the check was exercised but was not trigerred and a
                positive value of N indicates that the check failed N times'''
                checkName = fields[0]
                component = fields[1]

                if checkName in checks:
                    checks[checkName].add(numColumnsSoFar, fields[2:])
                else:
                    checkData = Check(checkName, component)
                    checkData.add(numColumnsSoFar, fields[2:])
                    checks[checkName] = checkData

            else: # Matches the for - only executed when we have reached eof
                not_eof = False
                file.close()

    # Generate the row, column and component totals
    columnTotals=[]
    componentTotals={}
    for test in test_names:
        columnTotals.append(ResultCount())

    for checkName, check in checks.iteritems():
        for c in range(len(check.values)):
            value = check.values[c]

            if check.component in componentTotals:
                comp = componentTotals[check.component]
            else:
                comp = ResultCount()

            if value == "":
                pass    # no result
            elif int(value) == 0:
                check.count.cpass()
                columnTotals[c].cpass()
                comp.cpass()
            else:
                if check.component == "Test":
                    check.count.cerr()
                    columnTotals[c].cerr()
                    comp.cerr()
                else:
                    check.count.cfail()
                    columnTotals[c].cfail()
                    comp.cfail()

            componentTotals[check.component] = comp

    # Generate grand total
    grandTotal = ResultCount()
    for compName, comp in componentTotals.iteritems():
        grandTotal.add(comp)

    # Write the output

    if (len(files) == 0):
        print(u"No valid input data")
        sys.exit(1)

    # Print the summary section
    print(u"Grand Total" +
          DELIMITER + u"Passes" +
          DELIMITER + u"Fails" +
          DELIMITER + u"Errors" +
          DELIMITER + u"Pass Rate", file=out)

    print(u"Total" +
          DELIMITER + unicode(grandTotal.passes) +
          DELIMITER + unicode(grandTotal.fails) +
          DELIMITER + unicode(grandTotal.errors) +
          DELIMITER + unicode(grandTotal.passRate()) + u"%", file=out)

    print(u"", file=out)

    print(u"Component" +
          DELIMITER + u"Passes" +
          DELIMITER + u"Fails" +
          DELIMITER + u"Errors" +
          DELIMITER + u"Pass Rate", file=out)

    for component, values in iter(sorted(componentTotals.iteritems())):
        print(component + DELIMITER + unicode(values.passes) +
              DELIMITER + unicode(values.fails) +
              DELIMITER + unicode(values.errors) +
              DELIMITER + unicode(values.passRate()) + u"%", file=out)

    # Leave some space between the summary and the detailed section
    print(u"", file=out)

    # Print the table header for the detailed section
    print (u"Check Name" + DELIMITER + u"Component", end=u"", file=out)
    for test in test_names:
        print (DELIMITER+test, end=u"", file=out)
    print(u"", file=out)

    print (DELIMITER, end=u"", file=out)
    for file_name in files:
        print (DELIMITER+file_name, end=u"", file=out)

    print(DELIMITER + u"Passes" + DELIMITER + u"Fails" + DELIMITER + u"Errors" + DELIMITER + u"Pass Rate", file=out)

    for checkName,check in iter(sorted(checks.iteritems())):
        print (check.name + DELIMITER + check.component, end=u"", file=out)

        for value in check.values:
            print (DELIMITER + value, end=u"", file=out)

        pr = check.count.passRate()
        print (DELIMITER + unicode(check.count.passes) + DELIMITER + unicode(check.count.fails) + DELIMITER + unicode(check.count.errors)
            + DELIMITER + unicode(pr) + u"%", file=out)

    print (u"Passes" + DELIMITER, end=u"", file=out)
    for t in columnTotals:
        print (DELIMITER + unicode(t.passes), end=u"", file=out)
    print(u"", file=out)

    print (u"Fails" + DELIMITER, end=u"", file=out)
    for t in columnTotals:
        print (DELIMITER + unicode(t.fails), end=u"", file=out)
    print(u"", file=out)

    print (u"Errors" + DELIMITER, end=u"", file=out)
    for t in columnTotals:
        print (DELIMITER + unicode(t.errors), end=u"", file=out)
    print(u"", file=out)

    print (u"Pass Rate" + DELIMITER, end=u"", file=out)
    for t in columnTotals:
        print (DELIMITER + unicode(t.passRate()) + u"%", end=u"", file=out)
    print(u"", file=out)

    out.close()

    # Generate the XML output
    with open(xmlFileName, 'wb') as f:
        f.write(unicode(generate_xml_file(
            legacy_pass, legacy_fail, legacy_error,
            grandTotal.passes, grandTotal.fails, grandTotal.errors, grandTotal.passRate())))
