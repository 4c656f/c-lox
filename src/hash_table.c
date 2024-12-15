#include "hash_table.h"
#include "memory.h"
#include "object.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

static Entry *findEntry(Entry *entries, ObjString *key, int capacity) {
  uint32_t initBucketIdx = key->hash % capacity;
  Entry *tombstone = NULL;

  while (true) {
    Entry *entryToCheck = &entries[initBucketIdx];
    if (entryToCheck->key == NULL) {
      if (IS_NIL(entryToCheck->value)) {
        return tombstone != NULL ? tombstone : entryToCheck;
      } else {
        if (tombstone == NULL)
          tombstone = entryToCheck;
      }
      continue;
    }
    if (entryToCheck->key == key) {
      return entryToCheck;
    }

    initBucketIdx = (initBucketIdx + 1) % capacity;
  }
  // unreacheble cause of load factor
  return NULL;
}

static void realocateHashTable(HashTable *table, int newCap) {
  // allocate new buckets
  Entry *newBuckets = ALLOCATE(Entry, newCap);
  // init new buckets
  for (int i = 0; i < newCap; i++) {
    newBuckets[i].key = NULL;
    newBuckets[i].value = NIL_VAL;
  }

  table->count = 0;
  // we need to copy old entries to new buckets cause with capacity grow there
  // will be less collisions
  for (int i = 0; i < table->capacity; i++) {
    Entry *oldEntry = &table->entries[i];
    // if entry in old bucket is not set, then there is nothing to copy
    if (oldEntry->key == NULL) {
      continue;
    }
    // calculate new bucket for old entry
    Entry *entryToInsrt = findEntry(newBuckets, oldEntry->key, newCap);
    entryToInsrt->key = oldEntry->key;
    entryToInsrt->value = oldEntry->value;
    table->count++;
  }

  // don't forget to free old buckets
  FREE_ARRAY(Entry, table->entries, table->capacity);

  // update hashtable with new buckets
  table->entries = newBuckets;
  table->capacity = newCap;
}

void initHashTable(HashTable *table) {
  table->capacity = 0;
  table->count = 0;
  table->entries = NULL;
}

void freeHashTable(HashTable *table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initHashTable(table);
}

bool getTableValue(HashTable *table, ObjString *key, Value *value) {
  if (table->count == 0) {
    return false;
  }
  Entry *foundedEtry = findEntry(table->entries, key, table->capacity);
  if (foundedEtry->key == NULL) {
    return false;
  }
  *value = foundedEtry->value;
  return true;
}

bool setTableValue(HashTable *table, ObjString *key, Value value) {
  // load facor is to hight, we need to grow buckets
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int newCap = GROW_CAPACITY(table->capacity);
    realocateHashTable(table, newCap);
  }
  Entry *foundedEntry = findEntry(table->entries, key, table->capacity);
  bool isNewEntry = foundedEntry->key == NULL;
  if (isNewEntry && IS_NIL(foundedEntry->value)) {
    table->count++;
  }
  foundedEntry->value = value;
  foundedEntry->key = key;
  return isNewEntry;
}

void tableAddAll(HashTable *from, HashTable *to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry *entry = &from->entries[i];
    if (entry->key != NULL) {
      setTableValue(to, entry->key, entry->value);
    }
  }
}

bool deleteTableValue(HashTable *table, ObjString *key) {
  if (table->count == 0)
    return false;
  Entry *foundedEntry = findEntry(table->entries, key, table->capacity);
  if (foundedEntry->key == NULL) {
    return false;
  }
  foundedEntry->key = NULL;
  foundedEntry->value = BOOL_VAL(true);
  return true;
}
