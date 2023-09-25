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

typedef struct Node {
    Slice key;
    uint64_t value;
    // node is a linkedlist, which contains a pointer to the next node in the list
    struct Node* next;
} Node;

typedef struct UnorderedMap {
    uint64_t size;
    uint64_t capacity;
    double loadFactor;
    // map contains an array of linkedlists (nodes)
    Node** bins;
} UnorderedMap;

UnorderedMap* mapCreate() {
    // creates the map using malloc and calloc
    UnorderedMap* map = (UnorderedMap*) (malloc(sizeof(UnorderedMap)));
    map -> size = 0;
    map -> capacity = 16;
    map -> loadFactor = 0.75;
    map -> bins = (Node**) (calloc(16, sizeof(Node)));
    return map;
}

// helper method that inserts into the map
void mapInsertWithBin(Node** bins, uint64_t binIndex, Slice key, uint64_t value) {
    // add the new [key, value] pair to the beginning of this current bin
    Node* addNode = (Node*) (malloc(sizeof(Node)));
    addNode -> key = key;
    addNode -> value = value;
    addNode -> next = bins[binIndex];
    bins[binIndex] = addNode;
}

void mapExpand(UnorderedMap* map) {
    // expands the map's capacity by 2 when the size exceeds the load capacity
    uint64_t updatedCapacity = map -> capacity * 2;
    Node** updatedBins = (Node**) (calloc(updatedCapacity, sizeof(Node)));

    // copy over everything from the old bin into the new bin
    for (size_t i = 0; i < map -> capacity; i++) {
        Node* current = map -> bins[i];
        while (current != NULL) {
            uint64_t hash = hashSlice(current -> key);
            uint64_t binIndex = hash % updatedCapacity;
            mapInsertWithBin(updatedBins, binIndex, current -> key, current -> value);
            Node* next = current -> next;
            free(current);
            current = next;
        }
    }
    free(map -> bins);
    map -> bins = updatedBins;
    map -> capacity = updatedCapacity;
}

// insert a key, value pair into the map
void mapInsert(UnorderedMap* map, Slice key, uint64_t value) {
    uint64_t hash = hashSlice(key);
    uint64_t binIndex = hash % map -> capacity;
    Node* current = map -> bins[binIndex];
    while (current != NULL) {
        if (sliceEqualSlice(current -> key, key)) {
            // update the current [key, value] pair that is already in the bin
            current -> value = value;
            return;
        }
        current = current -> next;
    }

    mapInsertWithBin(map -> bins, binIndex, key, value);
    map -> size++;

    // check if we need to resize the map
    if (map -> size > map -> capacity * map -> loadFactor) {
        mapExpand(map);
    }
}

// returns the value associated with the key in the map
uint64_t mapGet(UnorderedMap* map, Slice key) {
    uint64_t hash = hashSlice(key);
    uint64_t binIndex = hash % map -> capacity;
    // printf("%s, %ld, %ld\n", key.start, hash, binIndex);
    Node* current = map -> bins[binIndex];
    while (current != NULL) {
        if (sliceEqualSlice(current -> key, key)) {
            return current -> value;
        }
        current = current -> next;
    }
    return 0;
}

// returns if the map contains the given key
bool mapContains(UnorderedMap* map, Slice key) {
    uint64_t hash = hashSlice(key);
    uint64_t binIndex = hash % map -> capacity;
    // printf("%s, %ld, %ld\n", key.start, hash, binIndex);
    Node* current = map -> bins[binIndex];
    while (current != NULL) {
        if (sliceEqualSlice(current -> key, key)) {
            return true;
        }
        current = current -> next;
    }
    return false;
}

// free's the map's allocated memory in order to eliminate memory leaks
void freeMap(UnorderedMap* map) {
    for (size_t i = 0; i < map -> capacity; i++) {
        Node* current = map -> bins[i];
        while (current != NULL) {
            Node* next = current -> next;
            free(current);
            current = next;
        } 
    }
    free(map -> bins);
    free(map);
}
