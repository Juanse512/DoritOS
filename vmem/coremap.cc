#include "vmem/coremap.hh"
#include "lib/assert.hh"
#include <stdio.h>

CoreMap::CoreMap(int _numFrames) {
    // Initialize the coremap
    numFrames = _numFrames;
    coremap = new int * [numFrames];
    for (int i = 0; i < numFrames; i++) {
        coremap[i] = new int[2];
        coremap[i][0] = -1;
        coremap[i][1] = -1;
    }
}

CoreMap::~CoreMap() {
    // Free the coremap
    for (int i = 0; i < numFrames; i++) {
        delete[] coremap[i];
    }
    delete[] coremap;
}

int
CoreMap::Find(int virtualAdress, int pid){
    for (int i = 0; i < numFrames; i++) {
        if (!Test(i)) {
            Mark(virtualAdress, pid, i);
            return i;
        }
    }
    return -1;
}

bool
CoreMap::Test(int frame){
    ASSERT(frame < numFrames);
    return (coremap[frame][0] != -1);
}

void
CoreMap::Mark(int virtualAdress, int pid, int frame){
    coremap[frame][0] = virtualAdress;
    coremap[frame][1] = pid;
    return;
}

void
CoreMap::Clear(int frame){
    ASSERT(frame < numFrames);
    coremap[frame][0] = -1;
    coremap[frame][1] = -1;
}

int*
CoreMap::getFrame(int frame){
    return coremap[frame];
}
void
CoreMap::Print(){
    for (int i = 0; i < numFrames; i++) {
        if (Test(i)) {
            printf("Frame %d: Virtual Address: %d, PID: %d\n", i, coremap[i][0], coremap[i][1]);
        }
    }
}