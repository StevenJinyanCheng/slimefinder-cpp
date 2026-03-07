#include <iostream>
using namespace std;
int main() {
    int rExclusionSqr = 24 * 24;
    int rDespawnSqr = 128 * 128;
    int chunks = 0;
    int inX = 0; int inZ = 0;
    for (int chunkX = -8; chunkX <= 8; ++chunkX) {
        for (int chunkZ = -8; chunkZ <= 8; ++chunkZ) {
            int weight = 0;
            for (int bx = 0; bx < 16; ++bx) {
                for (int bz = 0; bz < 16; ++bz) {
                    double playerDistX = (chunkX * 16.0 + bx + 0.5) - (inX + 0.5);
                    double playerDistZ = (chunkZ * 16.0 + bz + 0.5) - (inZ + 0.5);
                    double dsqr = playerDistX * playerDistX + playerDistZ * playerDistZ;
                    if (dsqr <= rDespawnSqr && dsqr > rExclusionSqr) weight++;
                }
            }
            if (weight > 0) chunks++;
        }
    }
    cout << "Total chunks inside: " << chunks << endl;
}
