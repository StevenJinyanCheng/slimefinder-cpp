#include <iostream>
#include <vector>
using namespace std;
// [Paste identical JavaRandom, isSlimeChunk, getDistSqr, etc]
class JavaRandom {
private:
    uint64_t seed;
    static const uint64_t multiplier = 0x5DEECE66DLL;
    static const uint64_t addend = 0xBLL;
    static const uint64_t mask = (1ULL << 48) - 1;

public:
    JavaRandom(uint64_t initialSeed = 0) { setSeed(initialSeed); }
    void setSeed(uint64_t newSeed) { seed = (newSeed ^ multiplier) & mask; }
    int next(int bits) {
        seed = (seed * multiplier + addend) & mask;
        return (int)(seed >> (48 - bits));
    }
    int nextInt(int bound) {
        if (bound <= 0) return 0;
        int r = next(31);
        int m = bound - 1;
        if ((bound & m) == 0) { return (int)((bound * (uint64_t)r) >> 31); }
        for (int u = r; u - (r = u % bound) + m < 0; u = next(31));
        return r;
    }
};

bool isSlimeChunk(int64_t seed, int chunkX, int chunkZ) {
    JavaRandom r;
    int64_t chunkSeed = seed + 
        (int64_t)(chunkX * chunkX * 4987142) +
        (int64_t)(chunkX * 5947611) +
        (int64_t)(chunkZ * chunkZ) * 4392871LL +
        (int64_t)(chunkZ * 389711) ^ 987234911LL;

    r.setSeed(chunkSeed);
    return r.nextInt(10) == 0;
}
int main() {
    // Slimefinder.cpp logic
    int rExclusionSqr = 24 * 24;
    int rDespawnSqr = 128 * 128;
    int R_CHUNK = 8;
    int LIMIT = 17;
    vector<int> precomputedBlockWeights(LIMIT * LIMIT, 0);
    int inX = 0, inZ = 0;
    for (int dx = -R_CHUNK; dx <= R_CHUNK; ++dx) {
        for (int dz = -R_CHUNK; dz <= R_CHUNK; ++dz) {
            int weight = 0;
            for (int bx = 0; bx < 16; ++bx) {
                for (int bz = 0; bz < 16; ++bz) {
                    int pX = (dx * 16 + bx) - inX;
                    int pZ = (dz * 16 + bz) - inZ;
                    int dsqr = pX * pX + pZ * pZ;
                    if (dsqr <= rDespawnSqr && dsqr > rExclusionSqr) weight++;
                }
            }
            int idx = (dx + R_CHUNK) * LIMIT + (dz + R_CHUNK);
            precomputedBlockWeights[idx] = weight;
        }
    }
    int blockSize1 = 0;
    for (int dx = 0; dx < LIMIT; ++dx) {
        for (int dz = 0; dz < LIMIT; ++dz) {
            uint8_t c = isSlimeChunk(0, 80 + dx, -805 + dz) ? 1 : 0;
            blockSize1 += c * precomputedBlockWeights[dx * LIMIT + dz];
        }
    }
    cout << "Slimefinder logic blockSize (INT): " << blockSize1 << endl;
}
