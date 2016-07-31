#!/usr/bin/env python

import random # for seed, random
import sys    # for stdout

def changeAlignment(DNA1, DNA2, score):
	return {'strand1':DNA1, 'strand2':DNA2, 'score':score}

def key(DNA1, DNA2):
        return DNA1 + "-" + DNA2

def findOptimalAlignment(alignment, cache):
	DNA1 = alignment['strand1']
	DNA2 = alignment['strand2']
	score = alignment['score']
        tmpkey = key(DNA1, DNA2)
	if cache.has_key(tmpkey): return cache[tmpkey]
	
	if len(DNA1) == 0: return changeAlignment(len(DNA2) * ' ', DNA2, len(DNA2) * -2)
	if len(DNA2) == 0: return changeAlignment(DNA1, len(DNA1) * ' ', len(DNA1) * -2)

	# There's the scenario where the two leading bases of
	# each strand are forced to align, regardless of whether or not
	# they actually match.
	
	bestAlignment = findOptimalAlignment(changeAlignment(DNA1[1:], DNA2[1:], score), cache)
	resultDNA1 = DNA1[0] + bestAlignment['strand1']
	resultDNA2 = DNA2[0] + bestAlignment['strand2']
	resultScore = bestAlignment['score']
	
	if DNA1[0] == DNA2[0]: # no benefit from making other recursive calls
                resultScore += 1
		cache[tmpkey] = changeAlignment(resultDNA1, resultDNA2, resultScore)
		return cache[tmpkey]
        else: resultScore -= 1
		
	# It's possible that the leading base of strand1 best
	# matches not the leading base of strand2, but the one after it.
 
	bestAlignment = findOptimalAlignment(changeAlignment(DNA1, DNA2[1:], score), cache)
	bestScore = bestAlignment['score'] - 2  # penalize for insertion of space
	if resultScore < bestScore:
		resultDNA1 = " "     + bestAlignment['strand1']
		resultDNA2 = DNA2[0] + bestAlignment['strand2']
		resultScore = bestScore
		
	bestAlignment = findOptimalAlignment(changeAlignment(DNA1[1:], DNA2, score), cache)
	bestScore = bestAlignment['score'] - 2 # penalize for insertion of space	
	if resultScore < bestScore:
		resultDNA1 = DNA1[0] + bestAlignment['strand1']
		resultDNA2 = " "     + bestAlignment['strand2']
		resultScore = bestScore	

	cache[tmpkey] = changeAlignment(resultDNA1, resultDNA2, resultScore)
	return cache[tmpkey]		

def generateRandomDNAStrand(minlength, maxlength):
	assert minlength > 0, \
	       "Minimum length passed to generateRandomDNAStrand" \
	       "must be a positive number" # these \'s allow mult-line statements
	assert maxlength >= minlength, \
	       "Maximum length passed to generateRandomDNAStrand must be at " \
	       "as large as the specified minimum length"
	strand = ""
	length = random.choice(xrange(minlength, maxlength + 1))
	bases = ['A', 'T', 'G', 'C']
	for i in xrange(length):
                strand += random.choice(bases)
        return strand

def printAlignment(alignment, out = sys.stdout):
	DNA1 = alignment['strand1']
	DNA2 = alignment['strand2']
	resultPlusString = ""
	resultMinusString = ""	
	for i in range(0, len(DNA1)):
		if DNA1[i] == DNA2[i]:
			resultPlusString += "1"
			resultMinusString += " "
		elif DNA1[i]== " " or DNA2[i] == " ":
			resultPlusString += " "
			resultMinusString += "2"
		else: 
			resultPlusString += " "
			resultMinusString += "1"
	out.write("Optimal alignment score is " + str(alignment['score']) + "\n\n" +
		  "   +  " + resultPlusString + "\n" + 
		  "      " + DNA1 + "\n" + 
		  "      " + DNA2 + "\n" +
		  "   -  " + resultMinusString + "\n\n")
 
def main():
	while (True):
		sys.stdout.write("Generate random DNA strands? ")
		answer = sys.stdin.readline()
		if answer == "no\n": break
		strand1 = generateRandomDNAStrand(40, 80)
		strand2 = generateRandomDNAStrand(40, 100)
		sys.stdout.write("Aligning these two strands:\n" + strand1 + "\n")
		sys.stdout.write(strand2 + "\n")
                cache = {}
		alignment = findOptimalAlignment({'strand1':strand1, 'strand2':strand2, 'score':0}, cache)
		printAlignment(alignment)
		
if __name__ == "__main__":
  main()
