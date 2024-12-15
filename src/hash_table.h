#ifndef clox_hash_table_h
#define clox_hash_table_h

#include "value.h"

typedef struct {
  ObjString *key;
  Value value;
} Entry;

typedef struct {
  int count;
  int capacity;
  Entry *entries;
} HashTable;

void initHashTable(HashTable *table);

void freeHashTable(HashTable *table);

bool getTableValue(HashTable *table, ObjString *key, Value *value);

bool setTableValue(HashTable *table, ObjString *key, Value value);

void tableAddAll(HashTable* from, HashTable* to);

bool deleteTableValue(HashTable* table, ObjString* key);


#endif
