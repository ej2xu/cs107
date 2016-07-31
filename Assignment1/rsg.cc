/**
 * File: rsg.cc
 * ------------
 * Provides the implementation of the full RSG application, which
 * relies on the services of the built-in string, ifstream, vector,
 * and map classes as well as the custom Production and Definition
 * classes provided with the assignment.
 */
 
#include <map>
#include <fstream>
#include <string>
#include <assert.h>

#include "definition.h"
#include "production.h"
#define MAXLINE 50
using namespace std;

void printText(vector<string>& rs);
void doRandomText(map<string, Definition>& grammar, string terminal, vector<string>& rs);

/**
 * Takes a reference to a legitimate infile (one that's been set up
 * to layer over a file) and populates the grammar map with the
 * collection of definitions that are spelled out in the referenced
 * file.  The function is written under the assumption that the
 * referenced data file is really a grammar file that's properly
 * formatted.  You may assume that all grammars are in fact properly
 * formatted.
 *
 * @param infile a valid reference to a flat text file storing the grammar.
 * @param grammar a reference to the STL map, which maps nonterminal strings
 *                to their definitions.
 */

static void readGrammar(ifstream& infile, map<string, Definition>& grammar)
{
  while (true) {
    string uselessText;
    getline(infile, uselessText, '{');
    if (infile.eof()) return;  // true? we encountered EOF before we saw a '{': no more productions!
    infile.putback('{');
    Definition def(infile);
    grammar[def.getNonterminal()] = def;
  }
}

/**
 * Performs the rudimentary error checking needed to confirm that
 * the client provided a grammar file.  It then continues to
 * open the file, read the grammar into a map<string, Definition>,
 * and then print out the total number of Definitions that were read
 * in.  You're to update and decompose the main function to print
 * three randomly generated sentences, as illustrated by the sample
 * application.
 *
 * @param argc the number of tokens making up the command that invoked
 *             the RSG executable.  There must be at least two arguments,
 *             and only the first two are used.
 * @param argv the sequence of tokens making up the command, where each
 *             token is represented as a '\0'-terminated C string.
 */

int main(int argc, char *argv[])
{
  if (argc == 1) {
    cerr << "You need to specify the name of a grammar file." << endl;
    cerr << "Usage: rsg <path to grammar text file>" << endl;
    return 1; // non-zero return value means something bad happened 
  }
  
  ifstream grammarFile(argv[1]);
  if (grammarFile.fail()) {
    cerr << "Failed to open the file named \"" << argv[1] << "\".  Check to ensure the file exists. " << endl;
    return 2; // each bad thing has its own bad return value
  }
  
  // things are looking good...
  map<string, Definition> grammar;
  readGrammar(grammarFile, grammar);
  cout << "The grammar file called \"" << argv[1] << "\" contains "
       << grammar.size() << " definitions." << endl;

  vector<string> rs;

  for (int i =1;i <= 3; i++)
  {
    rs.clear();
    cout << "Version #" << i << endl << endl;
    doRandomText(grammar, "<start>", rs);
    printText(rs);
    cout << endl << endl;
  }
  return 0;
}

void printText(vector<string>& rs) {
  int length = 0;
  vector<string>::const_iterator cur = rs.begin();
  vector<string>::const_iterator end = rs.end();
  for (; cur != end; ++cur) {
    if (*cur == "." || *cur == ",") cout << *cur;
    else {
      length += cur->size();
      if (length > MAXLINE) {
	cout << endl;
	length = 0;
      } else cout << " " << *cur;
    }
  }
}

// recursive loop in the text and produce randomly generated text
void doRandomText(map<string, Definition>& grammar, string terminal, vector<string>& rs)
{
  Definition def = grammar[terminal];
  assert(def.getNonterminal().size() != 0);
  Production prod = def.getRandomProduction();
  for (Production::const_iterator prodIter = prod.begin(); prodIter != prod.end(); prodIter++)
    {
      string text = *prodIter;
      if (text.at(0)=='<' && text.at(text.size()-1)== '>')
	doRandomText(grammar, text, rs);
      else rs.push_back(text);
    }
}

