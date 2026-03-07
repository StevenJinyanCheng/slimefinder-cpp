#include <iostream>
using namespace std;
// [Paste isSlimeChunk here]
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
    int rExclusionSqr = 24 * 24;
    int rDespawnSqr = 128 * 128;
    int weight = 0;
    int total_blocks = 0;
    int total_chunks = 0;
    int inX = 0; int inZ = 0;
    for (int dx = -8; dx <= 8; ++dx) {
        for (int dz = -8; dz <= 8; ++dz) {
            int w = 0;
            for (int bx = 0; bx < 16; ++bx) {
                for (int bz = 0; bz < 16; ++bz) {
                    double playerDistX = (dx * 16.0 + bx + 0.5) - (inX + 0.5);
                    double playerDistZ = (dz * 16.0 + bz + 0.5) - (inZ + 0.5);
                    double dsqr = playerDistX * playerDistX + playerDistZ * playerDistZ;
                    if (dsqr <= rDespawnSqr && dsqr > rExclusionSqr) w++;
                }
            }
            if (isSlimeChunk(0, 88 + dx, -797 + dz)) {
                total_blocks += w;
                if (w > 0) total_chunks++;
            }
        }
    }
    cout << "Blocks: " << total_blocks << " Chunks: " << total_chunks << endl;
}
