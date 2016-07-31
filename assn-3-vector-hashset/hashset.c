#include "hashset.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
typedef struct {
  int size;
  vector *buckets;
  int numBuckets;
  HashSetCompareFunction comparefn;
  HashSetHashFunction hashfn;
} hashset;
*/

void HashSetNew(hashset *h, int elemSize, int numBuckets,
		HashSetHashFunction hashfn, HashSetCompareFunction comparefn, HashSetFreeFunction freefn)
{
  assert (elemSize > 0);
  assert (numBuckets > 0);
  assert (comparefn != NULL && hashfn != NULL);
 
  h->size = 0;
  h->comparefn = comparefn;
  h->hashfn = hashfn;
  h->buckets = malloc(numBuckets * sizeof(vector));
  assert(h->buckets != NULL);
  h->numBuckets = numBuckets;    
  for (int i = 0; i < numBuckets; i++)
    VectorNew(h->buckets + i, elemSize, freefn, 0);
}

void HashSetDispose(hashset *h)
{
  for (int i = 0; i < h->numBuckets; i++)
    VectorDispose(h->buckets + i);
  free(h->buckets);
}

int HashSetCount(const hashset *h) { return  h->size; }

void HashSetMap(hashset *h, HashSetMapFunction mapfn, void *auxData)
{
  assert(mapfn != NULL);
  for(int i = 0; i < h->numBuckets; i++)
    VectorMap(h->buckets + i, mapfn, auxData);
}

int getBucket(const hashset *h, const void *elemAddr)
{
  assert(elemAddr !=NULL);
  int bucket = h->hashfn(elemAddr, h->numBuckets);
  assert(bucket >= 0 && bucket < h->numBuckets);
  return bucket;
}

void HashSetEnter(hashset *h, const void *elemAddr)
{
  int bucket =  getBucket(h, elemAddr);  
  int position = VectorSearch(h->buckets + bucket, elemAddr, h->comparefn, 0, false);
  if (position != -1) VectorReplace(h->buckets + bucket, elemAddr, position);
  else {
    VectorAppend(h->buckets + bucket, elemAddr);
    h->size++;
  }
}

void *HashSetLookup(const hashset *h, const void *elemAddr)
{
  int bucket =  getBucket(h, elemAddr);
  int position = VectorSearch(h->buckets + bucket, elemAddr, h->comparefn, 0, false);
  if(position != -1) return VectorNth(h->buckets + bucket, position);
  else return NULL; 

}
