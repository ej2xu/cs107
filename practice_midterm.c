void *packetize(const void *image, int size, int packetSize)
{
  void *list;
  void **currp = &list;
  int bytesProcessed = 0;
  while (bytesProcessed < size) {
    if (size - bytesProcessed < packetSize) packetSize = size - bytesProcessed;
    void *cur = malloc(packetSize + sizeof(void *));
    memcpy(cur, (char *)image + bytesProcessed, packetSize);
    bytesProcessed += packetSize;
    *currp = cur;
    currp =(void **)((char *)cur + packetSize)
  }
  *currp = NULL;
  return list;
}

void new()
{
  HashSetNew(&ms->elements, elemSize + sizeof(int), numBuckets, hash, compare, free);
  ms->elemSize = elemSize;
  ms->free = free;
}

vodi dispose(multiset *ms)
{
  HashSetDispose(&ms->elements);
}

void enter(multiset *ms, const void *elem)
{
  void *found = HashSetLookup(&ms->elements, elem);
  if (found == NULL) {
    char pair[ms->elemSize + sizeof(int)];
    memcpy(pair, elem, ms->elemSize);
    *(int *)(pair + ms->elemSize) = 1;
    HashSetEnter(&ms->elements, pair);
  } else {
    if (ms->free != NULL) ms->free(found);
    memcpy(found, elem, ms->elemSize);
    (*(int *)((char *)found + ms->elemSize))++;
  }
}

typedef struct {
  mapfn oriMap;
  void *oriAux;
  int elemSize;
} mapHelper;

void applyMultiMap(void *elem, void *auxData)
{
  mapHelper *helper = auxData;
  int count = *(int *)((char *)elem + helper->elemSize);
  helper->oriMap(elem, count, helper->oriAux);
}

void map(multiset *ms, mutisetMap map, void *auxData)
{
  mapHelper helper = {map, auxData, ms->elemSize};
  HashSetMap(ms, applyMultiMap, &helper);
}

typedef struct {
  const char *plate;
  int tickets;
} maxTickets;

void queenAux(void *elem, int count, void *auxData)
{
  maxTickets *candidate = auxData;
  if (count > candidate->tickets) {
    candidate->tickets = count;
    candidate->plate = elem;
  }
}

void findQueen(multiset *ms, char plateOfQueen[])
{
  maxTickets candidate = {NULL, -1};
  map(ms, queenAux, &candidate);
  strcmp(plateOfQueen, candidate.plate);
}

//section handout 18

typedef struct {
  group *groups;
  int numGroups;
  int arrayLength;
  int groupSize;
} sparsestringarray;

typedef struct {
  bool *bitmap;
  vector strings;
} group;

static void stringFree(void *elem) { free(*(char **)elem); }
void SSANew(sparsestringarray *ssa, int arrayLength, itn groupSize) {
  int size = (arrayLength - 1)/ groupSize + 1;
  ssa->arrayLength = arrayLength;
  ssa->groupSize = groupSize;
  ssa->numGroups = size;
  ssa->groups = malloc(size * sizeof(group));
  for (int i = 0; i < size; i++) {
    int initSize = groupSize * sizeof(bool);
    ssa->groups[i].bitmap = malloc(initSize);
    memset(ssa->groups[i].bitmap, 0, initSize);
    VectorNew(&(ssa->groups[i].strings), sizeof(char *), stringFree, 1);
  }
}

void SSADispose(sparsestringarray *ssa) {
  int size = ssa->groups;
  for (int i = 0; i < size; i++) {
    VectorDispose(&(ssa->groups[i].strings));
    free(ssa->groups[i].bitmap);
  }
  free(ssa->groups);
}

bool SSAInsert(sparsestringarray *ssa, int index, const char *str) {
  int grp = index / ssa->groupSize;
  int boolIndex = index % ssa->groupSize;
  int count = 0;
  for (int i = 0; i < boolIndex; i++)
    if (ssa->groups[grp].bitmap[i]) count++;
  const char *copy = strdup(str);
  bool previouslyInserted = ssa->groups[grp].bitmap[boolIndex];
  if (previouslyInserted)
    VectorReplace(&ssa->groups[grp].strings, &copy, count);
  else {
    VectorInsert(&ssa->groups[grp].strings, &copy, count);
    ssa->groups[grp].bitmap[boolIndex] = true;
  }
  return !previouslyInserted;
}

typedef void (*SSAMapFn)(int index, const char *str, void *auxData);
void SSAMap(sparsestringarray *ssa, SSAMapFn mapfn, void *auxData) {
  int index = 0;
  for (int i = 0; i < ssa->numGroups; i++) {
    int groupSize = ssa->groupSize;
    group *grp = ssa->groups + i;
    if (i == ssa->numGroups - 1 && ssa->arrayLength % ssa->groupSize > 0) groupSize = ssa->arrayLength % ssa->groupSize;
    int count = 0;
    for (int j = 0; j < groupSize; j++) {
      const char *str = "";
      if (grp->bitmap[j]) {
	str = *(char **)VectorNth(&grp->strings, count);
	count++;
      }
      mapfn(index, str, auxData);
      index++;
    }
  }
}

int *serializeList(const void *list) {
  int *result = malloc(sizeof(int));
  int length = sizeof(int);
  const void **curr = (const void **)list;
  int numNodes = 0;
  while(curr != NULL) {
    const char *str = (const char *)(curr + 1);
    result = realloc(result, length + strlen(str) + 1);
    strcpy((char *)result + length, str);
    length += (strlen(str) + 1);
    curr = (const void **) *curr;
    numNodes++;
  }
  *result = numNode;
  return result;
}

typedef struct {
  hashset mappings;
  int keysize;
  int valuesize;
} multitable;
typedef int (*hashFn)(const void *keyAddr, int numBuckets);
typedef int (*cmpFn)(const void *keyAddr1, const void *keyAddr2);

void MultiTableNew(multitable *mt, int keysize, int valuesize, int numBuckets, hashFn hash, cmpFn compare) {
  mt->keysize = keysize;
  mt-> valuesize = valuesize;
  HashSetNew(&mt->mappings, keysize + sizeof(vector), numBuckets, hash, compare, NULL);
}

void MultiTableEnter(multitable *mt, const void *keyAddr, const void *valueAddr) {
  char buffer[mt->keysize + sizeof(vector)];
  vector *values;
  void *found = HashSetLookup(&mt->mappings, keyAddr);
  if (found == NULL) {
    memcpy(buffer, keyAddr, mt->keysize);
    values = (vector *)(buffer + mt->keysize);
    VectorNew(values, mt->valuesize, NULL, 0);
    VectorAppend(values, valueAddr);
    HashSetEnter(&mt->mappings, buffer);
  } else {
    values = (vector *)((char *)found + mt->keysize);
    VectorAppend(values, valueAddr);
  }
}

typedef void (*mapFn)(const void *keyAddr, void *valueAddr, void *auxData);
typedef struct {
  mapFn map;
  int keysize;
  void *auxData;
} helper;

void mapH(void *elemAddr, void *auxData) {
  helper *help = auxData;
  vector *v = (vector *)((char *)elemAddr + help->keysize);
  for(int i = 0; i < VectorLength(v); i++) {
    help->map(elemAddr, VectorNth(v, i), help);
  }
}

void MultiTableMap(multitable *mt, mapFn map, void *auxData) {
  helper help = {map, mt->keysize, auxData};
  HashSetMap(&mt->mappings, mapH, &help);
}
