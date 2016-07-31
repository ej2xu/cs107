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

typedef struct {
  char title[2048];
  char description[2048];
  char url[2048];
  char *activeField;
} rssFeedItem;
//general data struct
typedef struct {
  rssFeedItem item; //just to keep xml parser data
  hashset stopWords;//strings
  hashset articles; //articleData
  hashset indices;  //strings + vector of wordCounter
} rssFeedData;
typedef struct {
  char* title;
  char* url;
} articleData;
typedef struct {
  char* word;
  vector data;// vector of wordCounters
} indexData;
typedef struct{
  articleData *article;
  int counter;
} wordCounter;

static void Welcome(const char *welcomeTextURL);
static void CreateDataStructure(rssFeedData* data);
static void DisposeData(rssFeedData *data);
static void LoadStopWords(const char *StopWordsTextURL, hashset* stopWords);

static void BuildIndices(const char *feedsFileURL ,rssFeedData * data);
static void ProcessFeed(const char *remoteDocumentURL,rssFeedData * data);
static void PullAllNewsItems(urlconnection *urlconn, rssFeedData * data);
static void ProcessStartTag(void *userData, const char *name, const char **atts);
static void ProcessEndTag(void *userData, const char *name);
static void ProcessTextData(void *userData, const char *text, int len);
static void ParseArticle(void *userData);
static void ScanArticle(streamtokenizer *st, void *userData);

static void addWordRecord(hashset *indices, char*someWord, articleData *article);
static articleData* AddArticle(rssFeedData *data);

static void QueryIndices(void* userData);
static void ProcessResponse(const char *word,void* userData);
static bool WordIsWellFormed(const char *word);

static int StringHash(const void *elemAddr, int numBuckets);
static void StringMap(void*elemAddr,void*auxData);
static int StringCmp(const void *one,const void *two);
static void StringFree(void*elemAddr);

static void ArticleMap(void*elemAddr,void*auxData);
static int ArticleHash(const void *elemAddr, int numBuckets);
static int ArticleCmp(const void *oneP,const void *twoP);
static void ArticleFree(void*elemAddr);

static int SortVectorCmpFn(const void *one,const void *two);
static int FindArticleRecordCmpFn(const void *oneP,const void *twoP);
static void PrintResultsMapFn(void*elemAddr,void*auxData);

static void IndexMap(void*elemAddr,void*auxData);
static int IndexHash(const void *elemAddr, int numBuckets);
static int IndexCmp(const void *oneP,const void *twoP);
static void IndexFree(void*elemAddr);

static const char *const kWelcomeTextURL ="http://varren.site44.com/welcome.txt";
static const char *const kDefaultStopWordsURL = "http://varren.site44.com/stop-words.txt";
static const char *const kDefaultFeedsFileURL = "http://varren.site44.com/rss-feeds.txt";

int main(int argc, char **argv)
{
  const char *feedsFileURL = (argc == 1) ? kDefaultFeedsFileURL : argv[1]; 
  Welcome(kWelcomeTextURL); 
  rssFeedData data;
  CreateDataStructure(&data);  
  LoadStopWords(kDefaultStopWordsURL, &data.stopWords);
  BuildIndices(feedsFileURL, &data);  
  QueryIndices(&data);
  DisposeData(&data);
  return 0;
}

static const int kNumStopWordsBuckets = 1009;
static const int kNumIndexBuckets = 10007;
static void CreateDataStructure(rssFeedData* data){
  HashSetNew(&(data->stopWords), sizeof(char *), kNumStopWordsBuckets, StringHash, StringCmp, StringFree);
  HashSetNew(&(data->articles), sizeof(articleData), kNumStopWordsBuckets, StringHash, ArticleCmp, ArticleFree); 
  HashSetNew(&(data->indices), sizeof(indexData), kNumIndexBuckets, StringHash, StringCmp, IndexFree);
  memset(&data->item, 0, sizeof(rssFeedItem));
}

static void DisposeData(rssFeedData *data)
{
  HashSetDispose(&data->indices);
  HashSetDispose(&data->stopWords);
  HashSetDispose(&data->articles);  
}

static const char *const kNewLineDelimiters = "\r\n";
static void Welcome(const char *welcomeTextURL)
{
  url u;
  urlconnection urlconn;
  URLNewAbsolute(&u, welcomeTextURL);  
  URLConnectionNew(&urlconn, &u);  
  if (urlconn.responseCode / 100 == 3) {
    Welcome(urlconn.newUrl);
  } else {
    streamtokenizer st;
    char buffer[4096];
    STNew(&st, urlconn.dataStream, kNewLineDelimiters, true);
    while (STNextToken(&st, buffer, sizeof(buffer))) {      
      printf("%s\n", buffer);
    }  
    printf("\n");
    fflush(stdout);
    STDispose(&st); // remember that STDispose doesn't close the file, since STNew doesn't open one.. 
  }
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

static void LoadStopWords(const char *StopWordsTextURL, hashset* stopWords)
{
  url u;
  urlconnection urlconn;
  URLNewAbsolute(&u, StopWordsTextURL);  
  URLConnectionNew(&urlconn, &u);
  if (urlconn.responseCode / 100 == 3) {
    LoadStopWords(urlconn.newUrl, stopWords);
  } else {
    streamtokenizer st;
    char buffer[4096];
    STNew(&st, urlconn.dataStream, kNewLineDelimiters, true);
    while (STNextToken(&st, buffer, sizeof(buffer))) {
      char *copy = strdup(buffer);
      HashSetEnter(stopWords, &copy);  
    }  
    STDispose(&st);
  }
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

static void BuildIndices(const char *feedsFileURL ,rssFeedData * data)
{
  url u;
  urlconnection urlconn;
  
  URLNewAbsolute(&u, feedsFileURL);
  URLConnectionNew(&urlconn, &u);
 
  if (urlconn.responseCode / 100 == 3) { // redirection, so recurse
    BuildIndices(urlconn.newUrl, data);
  } else {
    streamtokenizer st;
    char remoteDocumentURL[2048]; 
    STNew(&st, urlconn.dataStream, kNewLineDelimiters, true);
    while (STSkipUntil(&st, ":") != EOF) { // ignore everything up to the first selicolon of the line
      STSkipOver(&st, ": ");		   // now ignore the semicolon and any whitespace directly after it
      STNextToken(&st, remoteDocumentURL, sizeof(remoteDocumentURL));   
      ProcessFeed(remoteDocumentURL, data);
    }   
    printf("\n");
    STDispose(&st);
  }
  
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

static void ProcessFeed(const char *remoteDocumentURL ,rssFeedData *data)
{
  url u;
  urlconnection urlconn;
  URLNewAbsolute(&u, remoteDocumentURL);
  URLConnectionNew(&urlconn, &u); 
  switch (urlconn.responseCode) {
  case 0: printf("Unable to connect to \"%s\".  Ignoring...", u.serverName); break;
  case 200: PullAllNewsItems(&urlconn, data); break;
  case 301: 
  case 302: ProcessFeed(urlconn.newUrl, data); break;
  default: printf("Connection to \"%s\" was established, but unable to retrieve \"%s\". [response code: %d, response message:\"%s\"]\n",
		  u.serverName, u.fileName, urlconn.responseCode, urlconn.responseMessage); break;
  };
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

/**
 * Function: PullAllNewsItems
 * --------------------------
 * Steps though the data of what is assumed to be an RSS feed identifying the names and
 * URLs of online news articles.  Check out "assn-4-rss-news-search-data/sample-rss-feed.txt"
 * for an idea of what an RSS feed from the www.nytimes.com (or anything other server that 
 * syndicates is stories).
 *
 * PullAllNewsItems views a typical RSS feed as a sequence of "items", where each item is detailed
 * using a generalization of HTML called XML.  A typical XML fragment for a single news item will certainly
 * adhere to the format of the following example:
 *
 * <item>
 *   <title>At Installation Mass, New Pope Strikes a Tone of Openness</title>
 *   <link>http://www.nytimes.com/2005/04/24/international/worldspecial2/24cnd-pope.html</link>
 *   <description>The Mass, which drew 350,000 spectators, marked an important moment in the transformation of Benedict XVI.</description>
 *   <author>By IAN FISHER and LAURIE GOODSTEIN</author>
 *   <pubDate>Sun, 24 Apr 2005 00:00:00 EDT</pubDate>
 *   <guid isPermaLink="false">http://www.nytimes.com/2005/04/24/international/worldspecial2/24cnd-pope.html</guid>
 * </item>
 *
 */

static void PullAllNewsItems(urlconnection *urlconn, rssFeedData *data)
{
  streamtokenizer st;
  char buffer[2048];
  
  XML_Parser rssFeedParser = XML_ParserCreate(NULL);
  XML_SetUserData(rssFeedParser, data);
  XML_SetElementHandler(rssFeedParser, ProcessStartTag, ProcessEndTag);
  XML_SetCharacterDataHandler(rssFeedParser, ProcessTextData);

  STNew(&st, urlconn->dataStream, "\n", false);
  
  while (STNextToken(&st, buffer, sizeof(buffer))) {
    XML_Parse(rssFeedParser, buffer, strlen(buffer), false);
  }
  STDispose(&st);
  
  XML_Parse(rssFeedParser, "", 0, true);
  XML_ParserFree(rssFeedParser);  
}

static void ProcessStartTag(void *userData, const char *name, const char **atts)
{
  rssFeedData *data = userData;
  rssFeedItem *item = &data->item;
  
  if (strcasecmp(name, "item") == 0) {
    memset(item, 0, sizeof(rssFeedItem));
  } else if (strcasecmp(name, "title") == 0) {
    item->activeField = item->title;
  } else if (strcasecmp(name, "description") == 0) {
    item->activeField = item->description;
  } else if (strcasecmp(name, "link") == 0) {
    item->activeField = item->url;
  }
}

static void ProcessEndTag(void *userData, const char *name)
{
  rssFeedData *data = userData;
  rssFeedItem *item = &data->item; 
  
  item->activeField = NULL;
  if (strcasecmp(name, "item") == 0)
    ParseArticle(userData); 
}

static void ProcessTextData(void *userData, const char *text, int len)
{ 
  rssFeedData *data = userData;
  rssFeedItem *item = &data->item;  
  if (item->activeField == NULL) return; // no place to put data
  char buffer[len + 1];
  memcpy(buffer, text, len);
  buffer[len] = '\0';
  strncat(item->activeField, buffer, 2048);
  
}

static const char *const kTextDelimiters = " \t\n\r\b!@$%^*()_+={[}]|\\'\":;/?.>,<~-`";
static void ParseArticle(void *userData)
{

  rssFeedData *data = userData;
  rssFeedItem *item = &data->item; 
  articleData article = {item->title, item->url};
  if (HashSetLookup(&(data->articles), &article) == NULL) {
    url u;
    urlconnection urlconn;
    streamtokenizer st;
    URLNewAbsolute(&u, item->url);
    URLConnectionNew(&urlconn, &u);

    switch (urlconn.responseCode) {
    case 0: printf("Unable to connect to \"%s\".  Domain name or IP address is nonexistent.\n", item->url);
      break;
    case 200: printf("[%s] Indexing \"%s\"\n", u.serverName, item->title);
      STNew(&st, urlconn.dataStream, kTextDelimiters, false);
      ScanArticle(&st, data);
      STDispose(&st);
      break;
    case 301: 
    case 302:
    case 303: // just pretend we have the redirected URL, though index using the new URL and not the old one...
      strcpy(item->url,urlconn.newUrl);
      ParseArticle(data);
      break;
    default: printf("Unable to pull \"%s\" from \"%s\". [Response code: %d] Punting...\n", item->title, u.fullName, urlconn.responseCode);
      break;
    }  
    URLConnectionDispose(&urlconn);  
    URLDispose(&u);
  }
}

static void ScanArticle(streamtokenizer *st, void* userData)
{
  rssFeedData *data = userData;
  articleData* article = AddArticle(data);  
  int numWords = 0;
  char word[1024];
  char longestWord[1024] = {'\0'};
  while (STNextToken(st, word, sizeof(word))) {
    if (strcasecmp(word, "<") == 0) {
      SkipIrrelevantContent(st); // in html-utls.h
    } else {
      RemoveEscapeCharacters(word);
      if (WordIsWellFormed(word)) {
	char* dummy = word;//need this becouse cant do &word in c
	if(HashSetLookup(&data->stopWords, &dummy) == NULL){// skip stopwords
	  addWordRecord(&data->indices, word, article);	  
	  numWords++;
	  if (strlen(word) > strlen(longestWord))
	    strcpy(longestWord, word);
	}
      }
    }
  }
  printf("\tWe counted %d well-formed words [including duplicates].\n", numWords);
  printf("\tThe longest word scanned was \"%s\".", longestWord);
  if (strlen(longestWord) >= 15 && (strchr(longestWord, '-') == NULL)) 
    printf(" [Ooooo... long word!]");
  printf("\n");
}

static articleData* AddArticle(rssFeedData *data) {
  articleData article;
  article.title = strdup((data->item).title);
  article.url =  strdup((data->item).url);
  HashSetEnter(&(data->articles), &article);
  return HashSetLookup(&(data->articles), &article);
}

static void addWordRecord(hashset *indices, char *someWord, articleData *article) {
  indexData indexEntry;
  indexData *found;
  indexEntry.word = someWord;
  wordCounter entry = {article, 1};
  if ((found = HashSetLookup(indices, &indexEntry)) == NULL) {
    indexEntry.word = strdup(someWord);
    VectorNew(&indexEntry.data, sizeof(wordCounter), NULL, 1);    
    VectorAppend(&indexEntry.data, &entry);
    HashSetEnter(indices, index);
  } else {
    int elemPos = VectorSearch(&(found->data), &entry, FindArticleRecordCmpFn, 0, false);
    if (elemPos == -1) VectorAppend(&(found->data), &entry);
    else {
      wordCounter *foundRec = VectorNth(&(found->data), elemPos);
      foundRec->counter++;
    }
  }
}

static void QueryIndices(void* userData)
{
  char response[1024];
  while (true) {
    printf("Please enter a single query term that might be in our set of indices [enter to quit]: ");
    fgets(response, sizeof(response), stdin);
    response[strlen(response) - 1] = '\0';
    if (strcasecmp(response, "") == 0) break;
    ProcessResponse(response, userData);
  }
}

static void printResult(indexData* resultData, const char *askedWord){
  if(resultData != NULL) {	
    vector resultVector = resultData->data;
    printf("there are %d records of this word", VectorLength(&resultVector));
    VectorSort(&resultVector, SortVectorCmpFn);
    int i = 1;
    VectorMap(&resultVector, PrintResultsMapFn, &i);
    printf("\n");
  }
  else printf("\tWe dont't have records about \"%s\" into our set of indices.\n", askedWord);
}

static void ProcessResponse(const char *askedWord, void* userData)
{ 
  if (WordIsWellFormed(askedWord)) {
    rssFeedData *data = userData;
    if(HashSetLookup(&data->stopWords, &askedWord) == NULL) {          
      indexData* resultData = HashSetLookup(&data->indices, &askedWord); 
      printResult(resultData, askedWord);
    }
    else printf("\tToo common a word to be taken seriously. Try something more specificn %s \n", askedWord);
  }
  else printf("\tWe won't be allowing words like \"%s\" into our set of indices.\n", askedWord);
}

static bool WordIsWellFormed(const char *word)
{
  int i;
  if (strlen(word) == 0) return true;
  if (!isalpha((int) word[0])) return false;
  for (i = 1; i < strlen(word); i++)
    if (!isalnum((int) word[i]) && (word[i] != '-')) return false; 

  return true;
}

static const signed long kHashMultiplier = -1664117991L;
static int StringHash(const void *elemAddr, int numBuckets){
  char *s = *(char **)elemAddr;
  unsigned long hashcode = 0;
  for (int i = 0; i < strlen(s); i++)
    hashcode = hashcode * kHashMultiplier + tolower(s[i]);
  return hashcode % numBuckets;
}
static void StringMap(void *elemAddr,void *auxData){  printf(":: %s ", *(char **)elemAddr);}
static int StringCmp(const void *one,const void *two){  return strcasecmp(*(char **)one, *(char **)two);}
static void StringFree(void *elemAddr){  free(*(char **)elemAddr); }

static void ArticleMap(void *elemAddr, void *auxData){
  articleData *data = elemAddr;  
  printf("\narticle:: %s\n url:: %s", data->title, data->url);
  fflush(stdout);
}
static int ArticleCmp(const void *oneP, const void *twoP){
  const articleData* one = oneP;
  const articleData* two = twoP;   
  if(strcasecmp(one->url,two->url) == 0 || strcasecmp(one->title,two->title) == 0) 
    return 0;
  else
    return strcasecmp(one->url,two->url);  
}
static void ArticleFree(void *elemAddr){
  articleData* data = elemAddr;
  free(data->title);
  free(data->url);
}

static int SortVectorCmpFn(const void *one, const void *two){
  const wordCounter* first = one;
  const wordCounter* second = two;
  if(first->counter > second->counter)
    return -1;
  else if(first->counter < second-> counter)
    return 1;
  else 
    return 0;
}

static int FindArticleRecordCmpFn(const void *oneP, const void *twoP){
  const wordCounter* one = oneP;
  const wordCounter* two = twoP;
  return ArticleCmp(one->article, two->article);
}

static void PrintResultsMapFn(void *elemAddr, void *auxData){
  wordCounter *data = elemAddr;
  printf("\n %d)\t Article:: %s Count:: %d", (*(int *)auxData)++, data->article->title, data->counter); 
  fflush(stdout);
}
static void IndexMap(void *elemAddr, void *auxData){
  indexData *index = elemAddr;  
  printf("\n word:::::::::::::::::::%s:::::::::::::::::::::::::::::::: ", index->word);
  fflush(stdout);
  int i = 0;  
  VectorMap(&index->data, PrintResultsMapFn, &i);
}
static void IndexFree(void *elemAddr){
  indexData *index = elemAddr;
  free(index->word);
  VectorDispose(&index->data);
}
