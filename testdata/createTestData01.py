#!/usr/bin/env python3
# Usage:
#   ./createTestData01.py -o stories.log
#
# Will create a log file with 1000 lines in an un-sorted way (random time entries).
# Use LoCo to sort.

import sys, getopt, datetime, random

pieces = [
    "Het was een donkeren, en stormachtige nacht.",
    "And they lived happily ever after.",
    "And then a hippo appeared.",
    "And then a crocodile appeared.",
    "And they all joined forces to pull the tree out of the swamp."
]


def main():
    outputfile = ''
    iterations = 1000
    try:
        opts, args = getopt.getopt(sys.argv[1:], "ho:i:", ["help", "ofile=", "iterations="])
    except getopt.GetoptError:
        print('createTestData01.py -o output.json')
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print('createTestData01.py -o output.json')
            sys.exit()
        elif opt in ("-o", "--ofile"):
            outputfile = arg
        elif opt in ("-i", "--iterations"):
            iterations = arg
    print ("output will be stored in %s" % (outputfile))
    start_stamp = datetime.datetime.now()
    with open(outputfile, 'w') as f:
        for i in range(0,iterations):
            X = random.uniform(1,10000)
            Y = int(random.uniform(0,len(pieces)))
            stamp = (start_stamp + datetime.timedelta(seconds=X)).strftime("%Y-%m-%d %H:%M:%S")
            f.write("%s: %s\n" % (stamp, pieces[Y]))
    

if __name__ == "__main__":
    main()
