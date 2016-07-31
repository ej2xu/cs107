#include <stdio.h>

#define entrysize(elemsize) (sizeof(bool) + elemsize)
#define Nthentry(base, n, elemsize) ((char *)base + n * entrysize(elemsize))

typedef struct {
	void *elems;
	int elemsize;
	int count;
	int alloc;
	HashSetHashFunction hashfn;
	HashSetCompareFunction cmpfn;
	HashSetFreeFunction freefn;
} hashset;

void HashSetNew(hashset *hs, int elemsize, HashSetHashFunction hashfn, HashSetCompareFunction cmpfn, HashSetFreeFunction freefn) {
	hs->elemsize = elemsize;
	hs->count = 0;
	hs->alloc = 64;
	hs->hashfn = hashfn;
	hs->cmpfn = cmpfn;
	hs->freefn = freefn;
	hs->elems = malloc(64 * entrysize(elemsize));
	
	for (int i = 0; i < 64; i++)
		*(bool *)Nthentry(hs->elems, i, elemsize) = false;
}

bool HashSetEnter(hashset *hs, void *elem) {
	if (hs->count > (3 * hs->alloc / 4)) HashSetRehash(hs);
	int hashcode = hs->hashfn(elem, hs->alloc);
	int delta = 0;
	while (true) {
		hashcode += delta;
		hashcode %= hs->alloc;
		bool *occupied = (bool *)Nthentry(hs->elems, hashcode, hs->elemsize);
		void *targetAddr = occupied + 1;
		if (!*occupied) {
			*occupied = true;
			memcpy(targetAddr, elem, hs->elemsize);
			hs->count++;
			return true;
		} else if (hs->cmpfn(targetAddr, elem) == 0) {
			if (hs->freefn) hs->freefn(targetAddr);
			memcpy(targetAddr, elem, hs->elemsize);
			return false;
		}
		delta++;
	}
}

static void HashSetRehash(hashset *hs) {
	hashset clone;
	memcpy(&clone, hs, sizeof(hashset));
	bool *occupied;
	hs->count = 0;
	hs->alloc *= 2;
	hs->elems = malloc(hs->alloc * entrysize(hs->elemsize));
	for (int i = 0; i < hs->alloc; i++)
		*(bool *)Nthentry(hs->elems, i, elemsize) = false;
	for (int i = 0; i < clone->alloc, i++) {
		occupied = (bool *)Nthentry(clone->elems, i, clone->elemsize);
		if (*occupied) HashSetEnter(hs, occupied + 1);
	}
	free(clone.elems);
}

person *decompress(void *image) {
	int numP = *(int *)image;
	person *people = malloc(numP * sizeof(person));
	int offset = sizeof(int);
	for (int i = 0; i < numP; i++) {
		char *name = (char *)image + offset;
		people[i].name = strdup(name);
		offset += strlen(name) + 1;
		while (offset % sizeof(int) != 0) offset++;
		int numF = *(int *)((char *)image + offset);
		people[i].numfriends = numF;
		people[i].friends = malloc(numF * sizeof(char *));
		offset += sizeof(int);
		char **friends = (char **)((char *)image + offset);
		for (int j = 0; j < numF; j++) {
			people[i].friends[j] = strdup(friends[j]);
		}
		offset += sizeof(char *) * numF;
	}
	return people;
}









