#pragma once

// libc includes (available in both C and C++)
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "slicec.h"
#include "mapc.h"

typedef struct Function {
    Slice name;
    char* pointer;
    uint64_t numParams;
    Slice* parameters;
} Function;

typedef struct FunctionNode {
    Slice key;
    Function* value;
    // node is a linkedlist, which contains a pointer to the next node in the list
    struct FunctionNode* next;
} FunctionNode;

typedef struct UnorderedFunctionMap {
    uint64_t size;
    uint64_t capacity;
    double loadFactor;
    // map contains an array of linkedlists (nodes)
    FunctionNode** bins;
} UnorderedFunctionMap;

UnorderedFunctionMap* functionMapCreate() {
    // creates the map using malloc and calloc
    UnorderedFunctionMap* map = (UnorderedFunctionMap*) (malloc(sizeof(UnorderedFunctionMap)));
    map -> size = 0;
    map -> capacity = 16;
    map -> loadFactor = 0.75;
    map -> bins = (FunctionNode**) (calloc(16, sizeof(FunctionNode)));
    return map;
}

// helper method that inserts into the map
void functionMapInsertWithBin(FunctionNode** bins, uint64_t binIndex, Slice key, Function* value) {
    // add the new [key, value] pair to the beginning of this current bin
    FunctionNode* addNode = (FunctionNode*) (malloc(sizeof(FunctionNode)));
    addNode -> key = key;
    addNode -> value = value;
    addNode -> next = bins[binIndex];
    bins[binIndex] = addNode;
}

void functionMapExpand(UnorderedFunctionMap* map) {
    // expands the map's capacity by 2 when the size exceeds the load capacity
    uint64_t updatedCapacity = map -> capacity * 2;
    FunctionNode** updatedBins = (FunctionNode**) (calloc(updatedCapacity, sizeof(FunctionNode)));

    // copy over everything from the old bin into the new bin
    for (size_t i = 0; i < map -> capacity; i++) {
        FunctionNode* current = map -> bins[i];
        while (current != NULL) {
            uint64_t hash = hashSlice(current -> key);
            uint64_t binIndex = hash % updatedCapacity;
            functionMapInsertWithBin(updatedBins, binIndex, current -> key, current -> value);
            FunctionNode* next = current -> next;
            free(current);
            current = next;
        }
    }
    free(map -> bins);
    map -> bins = updatedBins;
    map -> capacity = updatedCapacity;
}

// insert a key, value pair into the map
void functionMapInsert(UnorderedFunctionMap* map, Slice key, Function* value) {
    uint64_t hash = hashSlice(key);
    uint64_t binIndex = hash % map -> capacity;
    FunctionNode* current = map -> bins[binIndex];
    while (current != NULL) {
        if (sliceEqualSlice(current -> key, key)) {
            // update the current [key, value] pair that is already in the bin
            current -> value = value;
            return;
        }
        current = current -> next;
    }

    functionMapInsertWithBin(map -> bins, binIndex, key, value);
    map -> size++;

    // check if we need to resize the map
    if (map -> size > map -> capacity * map -> loadFactor) {
        functionMapExpand(map);
    }
}

// returns the value associated with the key in the map
Function* functionMapGet(UnorderedFunctionMap* map, Slice key) {
    uint64_t hash = hashSlice(key);
    uint64_t binIndex = hash % map -> capacity;
    // printf("%s, %ld, %ld\n", key.start, hash, binIndex);
    FunctionNode* current = map -> bins[binIndex];
    while (current != NULL) {
        if (sliceEqualSlice(current -> key, key)) {
            return current -> value;
        }
        current = current -> next;
    }
    return NULL;
}

// free's the map's allocated memory in order to eliminate memory leaks
void functionFreeMap(UnorderedFunctionMap* map) {
    for (size_t i = 0; i < map -> capacity; i++) {
        FunctionNode* current = map -> bins[i];
        while (current != NULL) {
            FunctionNode* next = current -> next;
            Function* func = (current -> value);
            free(func -> parameters);
            free(func);
            free(current);
            current = next;
        } 
    }
    free(map -> bins);
    free(map);
}
