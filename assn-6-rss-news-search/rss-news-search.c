#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <expat.h>

#include "url.h"
#include "bool.h"
#include "urlconnection.h"
#include "streamtokenizer.h"
#include "html-utils.h"
#include "vector.h"
#include "hashset.h"

#include "pthread.h" //#include "thread_107.h"
#include "semaphore.h"

typedef struct{
  pthread_mutex_t indicesHashSetLock; 
  pthread_mutex_t articlesVectorLock; 
  pthread_mutex_t stopWordsHashSetLock; 
  sem_t connectionsLock; 
  pthread_mutex_t serverDataLock; // lock for hashset of server-semaphores below
  hashset limitConnToServerLock; //char* and sem_t *
}semafores;

typedef struct {
  hashset stopWords;
  hashset indices;
  vector previouslySeenArticles;
  semafores locks;
  vector threads;
} rssDatabase;

typedef struct {
  char title[2048];
  char url[2048];
  char *activeField; // field that should be populated... 
} rssFeedEntry;

typedef struct {
  rssDatabase *db;
  rssFeedEntry entry;
} rssFeedState;

typedef struct {
  const char *title;
  const char *server;
  const char *fullURL;
} rssNewsArticle;

typedef struct {
  const char *meaningfulWord;
  vector relevantArticles;
} rssIndexEntry;

typedef struct {
  int articleIndex;
  int freq;
} rssRelevantArticleEntry;

// Next 3 are thread parametrs and thread storing structs
typedef struct {
  rssDatabase *db;
  char *title;
  char *URL;
}threadArguments;

typedef struct {
  pthread_t threadID;
  threadArguments *arg;//could use rssFeedState, but it's kinda nasty;p
}threadData;

typedef struct{
  const char *url;
  sem_t *serverLock;
}serverLockData;

static const char *const kWelcomeTextFile = "http://varren.site44.com/welcome.txt";
static const char *const kDefaultStopWordsFile = "http://varren.site44.com/stop-words.txt";
static const char *const kDefaultFeedsFile = "http://varren.site44.com/rss-feeds.txt";
int main(int argc, char **argv)
{
  const char *feedsFileName = (argc == 1) ? kDefaultFeedsFile : argv[1];
  rssDatabase db;    
  initThreadsData(&db);
  //InitThreadPackage(false);  
  Welcome(kWelcomeTextFile);
  LoadStopWords(&db.stopWords, kDefaultStopWordsFile);  
  BuildIndices(&db, feedsFileName);
  
  cleanThreadData(&db);

  QueryIndices(&db);
  pthread_exit(NULL);
  return 0;
}
static const char *const kNewLineDelimiters = "\r\n";
static const int kNumStopWordsBuckets = 1009;
static void LoadStopWords(hashset *stopWords, const char *stopWordsURL)
{
  url u;
  urlconnection urlconn;
  
  URLNewAbsolute(&u, stopWordsURL);
  URLConnectionNew(&urlconn, &u);
  
  if (urlconn.responseCode / 100 == 3) {
    LoadStopWords(stopWords, urlconn.newUrl);
  } else {
    streamtokenizer st;
    char buffer[4096];
    HashSetNew(stopWords, sizeof(char *), kNumStopWordsBuckets, StringHash, StringCompare, StringFree);
    STNew(&st, urlconn.dataStream, kNewLineDelimiters, true);
    while (STNextToken(&st, buffer, sizeof(buffer))) {
      char *stopWord = strdup(buffer);
      HashSetEnter(stopWords, &stopWord);
    }
    STDispose(&st);
  }

  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}
static const int kNumIndexEntryBuckets = 10007;
static void BuildIndices(rssDatabase *db, const char *feedsFileURL)
{
  url u;
  urlconnection urlconn;
  URLNewAbsolute(&u, feedsFileURL);
  URLConnectionNew(&urlconn, &u);
  
  if (urlconn.responseCode / 100 == 3) {
    BuildIndices(db, urlconn.newUrl);
  } else {
    streamtokenizer st;
    char remoteFileName[2048];
    HashSetNew(&db->indices, sizeof(rssIndexEntry), kNumIndexEntryBuckets, IndexEntryHash, IndexEntryCompare, IndexEntryFree);
    VectorNew(&db->previouslySeenArticles, sizeof(rssNewsArticle), NewsArticleFree, 0);
  
    STNew(&st, urlconn.dataStream, kNewLineDelimiters, true);
    while (STSkipUntil(&st, ":") != EOF) { // ignore everything up to the first selicolon of the line
      STSkipOver(&st, ": ");		   // now ignore the semicolon and any whitespace directly after it
      STNextToken(&st, remoteFileName, sizeof(remoteFileName));
      ProcessFeed(db, remoteFileName);
    }
  
    printf("\n");
    STDispose(&st);
  }
  
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}
static void ProcessFeed(rssDatabase *db, const char *remoteDocumentName)
{
  url u;
  urlconnection urlconn;  
  URLNewAbsolute(&u, remoteDocumentName);
  URLConnectionNew(&urlconn, &u);
  
  switch (urlconn.responseCode) {
    
      case 0: printf("Unable to connect to \"%s\".  Ignoring...", u.serverName);
	      break;
      case 200: PullAllNewsItems(db, &urlconn);
                break;
      case 301: 
      case 302: ProcessFeed(db, urlconn.newUrl);
                break;
      default: printf("Connection to \"%s\" was established, but unable to retrieve \"%s\". [response code: %d, response message:\"%s\"]\n",
		      u.serverName, u.fileName, urlconn.responseCode, urlconn.responseMessage);
	       break;
  };
  
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}
static void PullAllNewsItems(rssDatabase *db, urlconnection *urlconn)
{
  rssFeedState state = {db}; // passed through the parser by address as auxiliary data.
  streamtokenizer st;
  char buffer[2048];

  XML_Parser rssFeedParser = XML_ParserCreate(NULL);
  XML_SetUserData(rssFeedParser, &state);
  XML_SetElementHandler(rssFeedParser, ProcessStartTag, ProcessEndTag);
  XML_SetCharacterDataHandler(rssFeedParser, ProcessTextData);

  STNew(&st, urlconn->dataStream, "\n", false);
  while (STNextToken(&st, buffer, sizeof(buffer))) {
    XML_Parse(rssFeedParser, buffer, strlen(buffer), false);
  }
  STDispose(&st);
  
  XML_Parse(rssFeedParser, "", 0, true); // instructs the xml parser that we're done parsing..
  XML_ParserFree(rssFeedParser);  
}
static void ProcessStartTag(void *userData, const char *name, const char **atts)
{
  rssFeedState *state = userData;
  rssFeedEntry *entry = &state->entry;
  if (strcasecmp(name, "item") == 0) {
    memset(entry, 0, sizeof(rssFeedEntry));
  } else if (strcasecmp(name, "title") == 0) {
    entry->activeField = entry->title;
  } else if (strcasecmp(name, "link") == 0) {
    entry->activeField = entry->url;
  }
}
static void ProcessEndTag(void *userData, const char *name)
{
  rssFeedState *state = userData;
  rssFeedEntry *entry = &state->entry;
  entry->activeField = NULL;
  if (strcasecmp(name, "item") == 0) {
    NewThreadParseArticle(state->db, entry->title, entry->url);
    //ParseArticle(state->db, entry->title, entry->url); //OLD style
  }
}
static void ProcessTextData(void *userData, const char *text, int len)
{
  rssFeedState *state = userData;
  rssFeedEntry *entry = &state->entry;
  if (entry->activeField == NULL) return; // no place to put data
  char buffer[len + 1];
  memcpy(buffer, text, len);
  buffer[len] = '\0';
  strncat(entry->activeField, buffer, 2048);
}
static void NewThreadParseArticle(rssDatabase *db, const char *articleTitle, const char *articleURL){
  threadData newThreadData;  
  newThreadData.arg = malloc(sizeof(threadArguments));
  newThreadData.arg->db = db;
  newThreadData.arg->title = strdup(articleTitle);
  newThreadData.arg->URL = strdup(articleURL);
  
  int tID = VectorLength(&db->threads);
  
  VectorAppend(&db->threads, &newThreadData);
  threadData *threadP = VectorNth(&db->threads, tID);
 
 if(pthread_create(&threadP->threadID, NULL, PthreadParseArticle, (void *)threadP->arg)!=0){
    printf("Error, thread cannot be created");
    fflush(stdout);
  }
}
static void* PthreadParseArticle(void * threadData){
  threadArguments *arg = threadData;

  ParseArticle(arg->db ,arg->title,arg->URL);
  
  pthread_exit(NULL);
}
static const char *const kTextDelimiters = " \t\n\r\b!@$%^*()_+={[}]|\\'\":;/?.>,<~`";
static void ParseArticle(rssDatabase *db, const char *articleTitle, const char *articleURL)
{      
  url u;
  urlconnection urlconn;
  streamtokenizer st;
  int articleID;

  URLNewAbsolute(&u, articleURL);
  rssNewsArticle newsArticle = { articleTitle, u.serverName, u.fullName };
  
  pthread_mutex_t *articlesLock = &(db->locks.articlesVectorLock);
  pthread_mutex_lock(articlesLock);
  if (VectorSearch(&db->previouslySeenArticles, &newsArticle, NewsArticleCompare, 0, false) >= 0) { 
    pthread_mutex_unlock(articlesLock);
    printf("[Ignoring \"%s\": we've seen it before.]\n", articleTitle);
    URLDispose(&u);     
    return;
  }
  pthread_mutex_unlock(articlesLock);  
  lockConnection(db,u.serverName);
  URLConnectionNew(&urlconn, &u);
  switch (urlconn.responseCode) {
      case 0: printf("Unable to connect to \"%s\".  Domain name or IP address is nonexistent.\n", articleURL);
	      break;
      case 200: //printf("[%s] Ready to Index \"%s\"\n", u.serverName, articleTitle);
	      pthread_mutex_lock(articlesLock);
	      printf("[%s] Indexing \"%s\"\n", u.serverName, articleTitle);
	      NewsArticleClone(&newsArticle, articleTitle, u.serverName, u.fullName);
	      
	      VectorAppend(&db->previouslySeenArticles, &newsArticle);
	      articleID = VectorLength(&db->previouslySeenArticles) - 1;
	      pthread_mutex_unlock(articlesLock);

	      STNew(&st, urlconn.dataStream, kTextDelimiters, false);	
	      ScanArticle(&st, articleID, &db->indices, &db->stopWords,
			  &(db->locks.indicesHashSetLock),&(db->locks.stopWordsHashSetLock) );    	      	    
	      STDispose(&st);
	      
	      break;
      case 301: 
      case 302:{ // just pretend we have the redirected URL all along, though index using the new URL and not the old one... 
	        
	        int newURLLength = strlen(urlconn.newUrl)+1; 
		char newURLBuffer[newURLLength];
		strcpy(newURLBuffer, urlconn.newUrl);
	        URLConnectionDispose(&urlconn);
		unlockConnection(db,u.serverName);
		URLDispose(&u);
		
		ParseArticle(db, articleTitle, newURLBuffer);
                return;
		
      } default: printf("Unable to pull \"%s\" from \"%s\". [Response code: %d] Punting...\n", articleTitle, u.serverName, urlconn.responseCode);
		break;
  }
  
  URLConnectionDispose(&urlconn);
  unlockConnection(db,u.serverName);
  URLDispose(&u);
}
static void ScanArticle(streamtokenizer *st, int articleID, hashset *indices, hashset *stopWords, pthread_mutex_t* indicesLock, pthread_mutex_t* stopWordsLock)
{
  char word[1024];

  while (STNextToken(st, word, sizeof(word))) {
    if (strcasecmp(word, "<") == 0) {
      SkipIrrelevantContent(st);
    } else {
      RemoveEscapeCharacters(word);
      pthread_mutex_lock(stopWordsLock);
      bool startIndexNow = WordIsWorthIndexing(word, stopWords);
      pthread_mutex_unlock(stopWordsLock);
      if (startIndexNow){
	
	pthread_mutex_lock(indicesLock);
	AddWordToIndices(indices, word, articleID);
	//printf("DONE INDEXING");
	pthread_mutex_unlock(indicesLock);
      }
    }
  }
}
static bool WordIsWorthIndexing(const char *word, hashset *stopWords)
{
  return WordIsWellFormed(word) && HashSetLookup(stopWords, &word) == NULL;
}

/**
 * Adds the specified word (already deemed to be worth indexing)
 * to the set of indices, attaching it to the specified articleID (from
 * which the actual article can be easily recovered.)
 *
 * @param indices the set of indices being built.
 * @param word the word being added to the set of indices.
 * @param articleIndex the index of the relevant article where the word was found.
 *
 * No return value.
 */

static void AddWordToIndices(hashset *indices, const char *word, int articleIndex)
{
  rssIndexEntry indexEntry = { word }; // partial intialization
  rssIndexEntry *existingIndexEntry = HashSetLookup(indices, &indexEntry);
  if (existingIndexEntry == NULL) {
    indexEntry.meaningfulWord = strdup(word);
    VectorNew(&indexEntry.relevantArticles, sizeof(rssRelevantArticleEntry), NULL, 0);
    HashSetEnter(indices, &indexEntry);
    existingIndexEntry = HashSetLookup(indices, &indexEntry); // pretend like it's been there all along
    assert(existingIndexEntry != NULL);
  }

  rssRelevantArticleEntry articleEntry = { articleIndex, 0 };
  int existingArticleIndex =
    VectorSearch(&existingIndexEntry->relevantArticles, &articleEntry, ArticleIndexCompare, 0, false);
  if (existingArticleIndex == -1) {
    VectorAppend(&existingIndexEntry->relevantArticles, &articleEntry);
    existingArticleIndex = VectorLength(&existingIndexEntry->relevantArticles) - 1;
  }
  
  rssRelevantArticleEntry *existingArticleEntry = 
    VectorNth(&existingIndexEntry->relevantArticles, existingArticleIndex);
  existingArticleEntry->freq++;
}

/** 
 * Function: QueryIndices
 * ----------------------
 * Standard query loop that allows the user to specify a single search term, and
 * then proceeds (via ProcessResponse) to list up to 10 articles (sorted by relevance)
 * that contain that word.
 *
 * @param db the address of the full RSS database, with access to the master
 *           set of stop words, scanned articles
 * 
 * No return value.
 */

static void QueryIndices(rssDatabase *db)
{
  char response[1024];
  while (true) {
    printf("Please enter a single query term that might be in our set of indices [enter to quit]: ");
    fgets(response, sizeof(response), stdin);
    response[strlen(response) - 1] = '\0';
    if (strcasecmp(response, "") == 0) break;
    ProcessResponse(db, response);
  }
  
  HashSetDispose(&db->indices);
  VectorDispose(&db->previouslySeenArticles); 
  HashSetDispose(&db->stopWords);
}

/** 
 * Function: ProcessResponse
 * -------------------------
 * Placeholder implementation for what will become the search of a set of indices
 * for a list of web documents containing the specified word.
 *
 * @param db the address of the rssDatabase housing the stop word set, the set of indices,
 *           and the list of previously parsed articles.
 * @param word the word typed in by the user.
 *
 * No return value.
 */

static void ProcessResponse(rssDatabase *db, const char *word)
{
  if (!WordIsWellFormed(word)) {
    printf("That search term couldn't possibly be in our set of indices.\n\n");
    return;
  } 
  
  if (HashSetLookup(&db->stopWords, &word) != NULL) {
    printf("\"%s\" is too common a word to be taken seriously.  Please be more specific.\n\n", word);
    return;
  }

  rssIndexEntry entry = { word };
  rssIndexEntry *existingIndex = HashSetLookup(&db->indices, &entry);
  if (existingIndex == NULL) {
    printf("None of today's news articles contain the word \"%s\".\n\n", word);
    return;
  }

  ListTopArticles(existingIndex, &db->previouslySeenArticles);
}

/**
 * Brute force listing of the top ten articles (or all of them, if there are ten
 * or less of them) for the specified word.  
 *
 * @param matchingEntry the address of the rssIndexEntry housing the word of interest
 *                      and the full list of matching articles (with frequency counts).
 * @param previouslySeenAricles the list of all articles ever parsed.
 *
 * No return value.
 */

static void ListTopArticles(rssIndexEntry *matchingEntry, vector *previouslySeenArticles)
{
  int i, numArticles, articleIndex, count;
  rssRelevantArticleEntry *relevantArticleEntry;
  rssNewsArticle *relevantArticle;
  
  numArticles = VectorLength(&matchingEntry->relevantArticles);
  printf("Nice! We found %d article%s that include%s the word \"%s\". ", 
	 numArticles, (numArticles == 1) ? "" : "s", (numArticles != 1) ? "" : "s", matchingEntry->meaningfulWord);
  if (numArticles > 10) { printf("[We'll just list 10 of them, though.]"); numArticles = 10; }
  printf("\n\n");
  
  VectorSort(&matchingEntry->relevantArticles, ArticleFrequencyCompare);
  for (i = 0; i < numArticles; i++) {
    relevantArticleEntry = VectorNth(&matchingEntry->relevantArticles, i);
    articleIndex = relevantArticleEntry->articleIndex;
    count = relevantArticleEntry->freq;
    relevantArticle = VectorNth(previouslySeenArticles, articleIndex);
    printf("\t%2d.) \"%s\" [search term occurs %d time%s]\n", i + 1, 
	   relevantArticle->title, count, (count == 1) ? "" : "s");
    printf("\t%2s   \"%s\"\n", "", relevantArticle->fullURL);
  }
  
  printf("\n");
}
static bool WordIsWellFormed(const char *word)
{
  if (strlen(word) == 0) return true;
  if (!isalpha((int) word[0])) return false;
  for (int i = 1; i < strlen(word); i++)
    if (!isalnum((int) word[i]) && (word[i] != '-')) return false; 

  return true;
}
static const signed long kHashMultiplier = -1664117991L;
static int StringHash(const void *elem, int numBuckets)  
{ 
  unsigned long hashcode = 0;
  const char *s = *(const char **) elem;

  for (int i = 0; i < strlen(s); i++)
    hashcode = hashcode * kHashMultiplier + tolower(s[i]);  
  
  return hashcode % numBuckets;                                
}
static int StringCompare(const void *elem1, const void *elem2)
{
  const char *s1 = *(const char **) elem1;
  const char *s2 = *(const char **) elem2;
  return strcasecmp(s1, s2);
}
static void StringFree(void *elem)
{
  free(*(char **)elem);
}
static void NewsArticleClone(rssNewsArticle *article, const char *title, 
			     const char *server, const char *fullURL)
{
  article->title = strdup(title);
  article->server = strdup(server);
  article->fullURL = strdup(fullURL);
}
static int NewsArticleCompare(const void *elem1, const void *elem2)
{
  const rssNewsArticle *article1 = elem1;
  const rssNewsArticle *article2 = elem2;
  if ((StringCompare(&article1->title, &article2->title) == 0) &&
      (StringCompare(&article1->server, &article2->server) == 0)) return 0;
  
  return StringCompare(&article1->fullURL, &article2->fullURL);
}
static void NewsArticleFree(void *elem)
{
  rssNewsArticle *article = elem;
  StringFree(&article->title);
  StringFree(&article->server);
  StringFree(&article->fullURL);
}
static int IndexEntryHash(const void *elem, int numBuckets)
{
  const rssIndexEntry *entry = elem;
  return StringHash(&entry->meaningfulWord, numBuckets);
}
static int IndexEntryCompare(const void *elem1, const void *elem2)
{
  const rssIndexEntry *entry1 = elem1;
  const rssIndexEntry *entry2 = elem2;
  return StringCompare(&entry1->meaningfulWord, &entry2->meaningfulWord);
}
static void IndexEntryFree(void *elem)
{
  rssIndexEntry *entry = elem;
  StringFree(&entry->meaningfulWord);
  VectorDispose(&entry->relevantArticles);
}

static int ArticleIndexCompare(const void *elem1, const void *elem2)
{
  const rssRelevantArticleEntry *entry1 = elem1;
  const rssRelevantArticleEntry *entry2 = elem2;
  return entry1->articleIndex - entry2->articleIndex;
}
static int ArticleFrequencyCompare(const void *elem1, const void *elem2)
{
  const rssRelevantArticleEntry *entry1 = elem1;
  const rssRelevantArticleEntry *entry2 = elem2;
  return entry2->freq - entry1->freq;
}
static void ThreadDataFree(void *elem){
  threadData* data = elem;
  pthread_join(data->threadID,NULL);
  threadArguments* arg =data->arg;
  StringFree(&arg->title);
  StringFree(&arg->URL);
  free(data->arg);
}

static int ConnectionsLockHash(const void* elemAddr, int numBuckets){
  const serverLockData *entry = elemAddr;
  return StringHash(&entry->url, numBuckets);
}
static int ConnectionsLockCompare(const void* elemAddr1, const void* elemAddr2){
  const serverLockData * one = elemAddr1;
  const serverLockData * two = elemAddr2;
  return StringCompare(&one->url,&two->url);
}
static void ConnectionsLockFree(void* elemAddr){
  serverLockData * data = elemAddr;
  StringFree(&data->url);
  sem_destroy(data->serverLock);
  free(data->serverLock);
}
static const int kNumOfServersBuckets = 1007;
static const int kNumOfConnections = 20;
static void initThreadsData(rssDatabase *db)
{
  VectorNew(&db->threads, sizeof(threadData),ThreadDataFree,0);
  HashSetNew(&(db->locks.limitConnToServerLock),sizeof(serverLockData),kNumOfServersBuckets, 
	     ConnectionsLockHash, ConnectionsLockCompare,ConnectionsLockFree);
  
  pthread_mutex_init(&(db->locks.serverDataLock), NULL);  
  pthread_mutex_init(&(db->locks.articlesVectorLock), NULL);
  pthread_mutex_init(&(db->locks.indicesHashSetLock), NULL);
  pthread_mutex_init(&(db->locks.stopWordsHashSetLock), NULL);
  sem_init(&(db->locks.connectionsLock),0,kNumOfConnections);
  
}
static void cleanThreadData(rssDatabase *db) 
{
  VectorDispose(&db->threads); //shall be first becouse it executes pthread_join(); 
  HashSetDispose(&(db->locks.limitConnToServerLock));

  pthread_mutex_destroy(&(db->locks.serverDataLock));  
  pthread_mutex_destroy(&(db->locks.articlesVectorLock));
  pthread_mutex_destroy(&(db->locks.indicesHashSetLock));
  pthread_mutex_destroy(&(db->locks.stopWordsHashSetLock));
  sem_destroy(&(db->locks.connectionsLock));
  
}
static const int kSimultaneousServerConn = 6;
static sem_t* findServerLock(hashset *serverLocks, pthread_mutex_t *dataLock, const char* serverURL){
 
  pthread_mutex_lock(dataLock);
  sem_t * serverLock;
  serverLockData newLockData = {serverURL}; 
  serverLockData* lockDataP = HashSetLookup(serverLocks, &newLockData);
  
  if(lockDataP==NULL){

    newLockData.url = strdup(serverURL);
    newLockData.serverLock = malloc(sizeof(sem_t));
    sem_init(newLockData.serverLock,0,kSimultaneousServerConn);

    HashSetEnter(serverLocks, &newLockData);
    lockDataP = HashSetLookup(serverLocks, &newLockData);
    //create semaphore
  }
  serverLock = lockDataP->serverLock;
  
  pthread_mutex_unlock(dataLock);
  return serverLock;
}
static void lockConnection(rssDatabase *db, const char* serverURL){
  sem_t *conCounter = &(db->locks.connectionsLock);
  sem_t *serverLock =  findServerLock(&(db->locks.limitConnToServerLock),&(db->locks.serverDataLock),serverURL);
  sem_wait(conCounter);
  sem_wait(serverLock);
}

static void unlockConnection(rssDatabase *db, const char* serverURL){
  sem_t *conCounter = &(db->locks.connectionsLock);  
  sem_t *serverLock =  findServerLock(&(db->locks.limitConnToServerLock),&(db->locks.serverDataLock),serverURL);
  sem_post(conCounter);  
  sem_post(serverLock);
}
