#include <vector>
#include <set>
#include <queue>
#include <string>
#include <iostream>
#include <iomanip>
#include "imdb.h"
#include "path.h"
using namespace std;

static string promptForActor(const string& prompt, const imdb& db)
{
  string response;
  while (true) {
    cout << prompt << " [or <enter> to quit]: ";
    getline(cin, response);
    if (response == "") return "";
    vector<film> credits;
    if (db.getCredits(response, credits)) return response;
    cout << "We couldn't find \"" << response << "\" in the movie database. "
	 << "Please try again." << endl;
  }
}

void getPath(queue <path>& partialPath, set<string>& seenActors,
	     set<film>& seenFilms,const string& target,const imdb& db, const bool& reverse)
{
  while(partialPath.size()>0 && partialPath.front().getLength() <= 5)
  {
  path currPath = partialPath.front();
  partialPath.pop();
  string currPlayer = currPath.getLastPlayer();
  
  vector<film> currPlayerFilms; 
  db.getCredits(currPlayer, currPlayerFilms);
     
  for(u_int i =0;i<currPlayerFilms.size();i++)
    {
      film currFilm = currPlayerFilms[i];
      if(seenFilms.find(currFilm)==seenFilms.end())
	{
	  seenFilms.insert(currFilm);
	  vector <string> currFilmPlayers;
	  db.getCast(currFilm, currFilmPlayers);
	      
	  for(u_int j = 0; j< currFilmPlayers.size();j++)
	    {
	      string costar = currFilmPlayers[j];
	      if(seenActors.find(costar)==seenActors.end())
		{
		  seenActors.insert(costar);
		  path newPath = currPath;
		  newPath.addConnection(currFilm, costar);
		  if(costar == target){
		    if (reverse) newPath.reverse();
		    newPath.print();
		    return;
		  }
		  partialPath.push(newPath);
		}
	    }   
	}
    }
  }
   cout << endl << "No path between those two people could be found." << endl << endl;

}

void generateShortestPath(string &source, string &target, const imdb& db)
{
  bool reverse = false;
  string tmp;
  vector<film> srcCredits, trgCredits;
  db.getCredits(source, srcCredits);
  db.getCredits(target, trgCredits);
  if (srcCredits.size() > trgCredits.size()) {
    reverse = true;
    tmp = source;
    source = target;
    target = tmp;
  }
  queue<path> partialPath;
  set<string> seenActors;
  set<film> seenFilms;
  path resultPath(source);
  partialPath.push(resultPath);
  seenActors.insert(source);
  getPath(partialPath, seenActors, seenFilms, target, db, reverse);
}

int main(int argc, const char *argv[])
{
  imdb db(determinePathToData(argv[1])); // inlined in imdb-utils.h
  if (!db.good()) {
    cout << "Failed to properly initialize the imdb database." << endl;
    cout << "Please check to make sure the source files exist and that you have permission to read them." << endl;
    exit(1);
  }
  
  while (true) {
    string source = promptForActor("Actor or actress", db);
    if (source == "") break;
    string target = promptForActor("Another actor or actress", db);
    if (target == "") break;
    if (source == target) {
      cout << "Good one.  This is only interesting if you specify two different people." << endl;
    } else {
      generateShortestPath(source, target, db);
    }
  }
  
  cout << "Thanks for playing!" << endl;
  return 0;
}

