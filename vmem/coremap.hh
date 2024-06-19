#ifndef COREMAP_HH
#define COREMAP_HH

#include "lib/utility.hh"


class CoreMap {
    public:
        CoreMap(int _numFrames);
        ~CoreMap();
        int Find(int virtualAddress, int pid);
        void Mark(int virtualAddress, int pid, int frame);
        void Clear(int frame);
        bool Test(int frame);
        void Print();
        int *getFrame(int frame);
    private:
        int numFrames;
        int **coremap;
};
#endif // COREMAP_HH
