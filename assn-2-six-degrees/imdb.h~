#ifndef __imdb__
#define __imdb__

#include "imdb-utils.h"
#include <string>
#include <vector>
using namespace std;

class imdb {
  
 public:
  
  imdb(const string& directory);

  bool good() const;

  /**
   * Method: getCredits
   * ------------------
   * Searches for an actor/actress's list of movie credits.  The list 
   * of credits is returned via the second argument, which you'll note
   * is a non-const vector<film> reference.  If the specified actor/actress
   * isn't in the database, then the films vector will be left empty.
   *
   * @param player the name of the actor or actresses being queried.
   * @param films a reference to the vector of films that should be updated
   *              with the list of the specified actor/actress's credits.
   * @return true if and only if the specified actor/actress appeared in the
   *              database, and false otherwise.
   */

  bool getCredits(const string& player, vector<film>& films) const;

  /**
   * Method: getCast
   * ---------------
   * Searches the receiving imdb for the specified film and returns the cast
   * by populating the specified vector<string> with the list of actors and actresses
   * who star in it.  If the movie doesn't exist in the database, the players vector
   * is cleared and its size left at 0.
   *
   * 
   * @param movie the film (title and year) being queried
   * @param players a reference to the vector of strings to be updated with the
   *                the list of actors and actresses starring in the specified film.
   *                If the movie doesn't exist, then the players vector would be cleared
   *                of all contents and resized to be of length 0.
   * @return true if and only if the specified movie appeared in the
   *              database, and false otherwise.
   */

  bool getCast(const film& movie, vector<string>& players) const;

  ~imdb();
  
 private:
  static const char *const kActorFileName;
  static const char *const kMovieFileName;
  const void *actorFile;
  const void *movieFile;
  
  /*
    film createFilmObj(char* &entry);
  short getRecordsNum(char* & offset, int byteNameSize);
  char* calculateOffset(int* x, char *z);
  */
  // everything below here is complicated and needn't be touched.
  // you're free to investigate, but you're on your own.
  struct fileInfo {
    int fd;
    size_t fileSize;
    const void *fileMap;
  } actorInfo, movieInfo;
  
  static const void *acquireFileMap(const string& fileName, struct fileInfo& info);
  static void releaseFileMap(struct fileInfo& info);

  // marked as private so imdbs can't be copy constructed or reassigned.
  // if we were to allow this, we'd alias open files and accidentally close
  // files prematurely.. (do NOT implement these... since the client will
  // never call these, the code will never be needed.
  imdb(const imdb& original);
  imdb& operator=(const imdb& rhs);
  imdb& operator=(const imdb& rhs) const;
};

#endif
