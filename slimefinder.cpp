#include <iostream>
#include <cstdint>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <chrono>

using namespace std;

// Replicate Java's java.util.Random
class JavaRandom {
private:
    uint64_t seed;
    static const uint64_t multiplier = 0x5DEECE66DLL;
    static const uint64_t addend = 0xBLL;
    static const uint64_t mask = (1ULL << 48) - 1;

public:
    JavaRandom(uint64_t initialSeed = 0) {
        setSeed(initialSeed);
    }

    void setSeed(uint64_t newSeed) {
        seed = (newSeed ^ multiplier) & mask;
    }

    int next(int bits) {
        seed = (seed * multiplier + addend) & mask;
        return (int)(seed >> (48 - bits));
    }

    int nextInt(int bound) {
        if (bound <= 0) return 0;
        int r = next(31);
        int m = bound - 1;
        if ((bound & m) == 0) {
            return (int)((bound * (uint64_t)r) >> 31);
        }
        for (int u = r; u - (r = u % bound) + m < 0; u = next(31));
        return r;
    }
};

// Check if a given chunk is a slime chunk
bool isSlimeChunk(int64_t seed, int chunkX, int chunkZ) {
    JavaRandom r;
    
    // In C++, signed integer overflow is Undefined Behavior.
    // Clang -O3 will massively break the seed math if we don't force unsigned wrap-around.
    // Java evaluates these as 32-bit ints (wrapping around) and then casts to 64-bit long.
    int32_t term1 = (int32_t)( (uint32_t)chunkX * (uint32_t)chunkX * 4987142U );
    int32_t term2 = (int32_t)( (uint32_t)chunkX * 5947611U );
    
    int32_t zz = (int32_t)( (uint32_t)chunkZ * (uint32_t)chunkZ );
    int64_t term3 = (int64_t)zz * 4392871LL;
    
    int32_t term4 = (int32_t)( (uint32_t)chunkZ * 389711U );
    
    int64_t chunkSeed = seed + (int64_t)term1 + (int64_t)term2 + term3 + (int64_t)term4 ^ 987234911LL;

    r.setSeed(chunkSeed);
    return r.nextInt(10) == 0;
}

// Basic search parameters
struct SearchConfig {
    int64_t seed = 0;
    int rChunk = 8; // Default search radius in chunks
    bool despawnSphere = true;
    bool exclusionSphere = true;
    int yOffset = 0;
    int chunkWeight = 0;

    int minChunkSize = 0;
    int maxChunkSize = 289;
    int minBlockSize = 0;
    int maxBlockSize = 73984;

    int startX = 0; // Starting block X
    int startZ = 0; // Starting block Z
    int minWidth = 0;
    int maxWidth = 1;
    bool fineSearch = false;
    
    string outputFile = "results.csv";
};

// Computes distance squared
inline int getDistSqr(int dx, int dz) {
    return dx * dx + dz * dz;
}

void runSearch(const SearchConfig& config) {
    int rExclusionSqr = 24 * 24 - min(config.yOffset * config.yOffset, 24 * 24);
    int rDespawnSqr = 128 * 128 - config.yOffset * config.yOffset;

    ofstream out(config.outputFile);
    if (out.is_open()) {
        out << "block-position;chunk-position;blockSize;chunkSize\n";
    } else {
        cerr << "Warning: Could not open output file: " << config.outputFile << "\n";
    }

    int R_CHUNK = config.rChunk;

    cout << "Starting search...\n";
    cout << "Seed: " << config.seed << "\n";
    cout << "Results will be saved to: " << config.outputFile << "\n\n";

    int matches = 0;
    int currentMaxBlockSize = -1;
    int currentMaxChunkSize = -1;

    // A spiral pattern or simpler box search can be used.
    int centerChunkX = config.startX / 16;
    int centerChunkZ = config.startZ / 16;

    long long spiralLimit = (long long)(2 * config.maxWidth + 1) * (long long)(2 * config.maxWidth + 1);
    long long skippedChunks = config.minWidth > 0 ? (long long)(2 * config.minWidth - 1) * (long long)(2 * config.minWidth - 1) : 0;
    long long totalComputeChunks = spiralLimit - skippedChunks;
    long long processedChunks = 0, totalChunks = totalComputeChunks;

    // 终端 I/O 是非常慢的，会严重拖慢整体速度。我们需要限制更新频率。
    long long reportInterval = std::max(10000LL, totalComputeChunks / 500);

    // 【核心优化2】把 chunkWeights 的计算彻底拿到最外层！
    // 不同区块内的偏移量 (inX, inZ) 完全是周期重复的，权重只和 inX, inZ, dx, dz 有关
    // 所以这总共只需要算 16*16*17*17 次，而不是每个区块都算一次！
    const int LIMIT = 2 * R_CHUNK + 1; // == 17
    vector<int> precomputedBlockWeights(16 * 16 * LIMIT * LIMIT, 0);
    vector<int> precomputedChunkWeights(16 * 16 * LIMIT * LIMIT, 0);

    for (int inX = 0; inX < 16; ++inX) {
        for (int inZ = 0; inZ < 16; ++inZ) {
            for (int dx = -R_CHUNK; dx <= R_CHUNK; ++dx) {
                for (int dz = -R_CHUNK; dz <= R_CHUNK; ++dz) {
                    int weight = 0;
                    for (int bx = 0; bx < 16; ++bx) {
                        for (int bz = 0; bz < 16; ++bz) {
                            // 使用和 Java 端一致的整数计算，消除任何浮点数精度或 ffast-math 的影响
                            // 在 Java 里，isBlockInside 检查距离使用的是：
                            // (blockX - in.x)^2 + (blockZ - in.z)^2
                            int playerDistX = (dx * 16 + bx) - inX;
                            int playerDistZ = (dz * 16 + bz) - inZ;
                            int dsqr = playerDistX * playerDistX + playerDistZ * playerDistZ;

                            bool inside = true;
                            // 注意：原版 java 里的 `dsqr` 计算逻辑如果不同的话我们这里进行了优化修正
                            // 此处的距离是准确的圆的范围
                            if (config.despawnSphere && dsqr > rDespawnSqr) inside = false;
                            if (config.exclusionSphere && dsqr <= rExclusionSqr) inside = false;
                            if (inside) weight++;
                        }
                    }
                    int idx = ((inX * 16 + inZ) * LIMIT + (dx + R_CHUNK)) * LIMIT + (dz + R_CHUNK);
                    precomputedBlockWeights[idx] = weight;
                    precomputedChunkWeights[idx] = (weight > config.chunkWeight) ? 1 : 0;
                }
            }
        }
    }

    // 【核心优化3】缓存整个搜索区域的史莱姆区块结果
    // 因为对于相邻的匹配区块，检查周围 17x17 范围时会重复计算巨量次相同的 isSlimeChunk
    // 提前计算好我们能遇见的所有的可能的区块坐标！
    int searchRadius = config.maxWidth + R_CHUNK;
    long long gridWidth = 2LL * searchRadius + 1;
    vector<uint8_t> slimeChunkCache((size_t)(gridWidth * gridWidth), 0);

    int baseSearchChunkX = centerChunkX - searchRadius;
    int baseSearchChunkZ = centerChunkZ - searchRadius;

    for (long long gx = 0; gx < gridWidth; ++gx) {
        for (long long gz = 0; gz < gridWidth; ++gz) {
            slimeChunkCache[(size_t)(gx * gridWidth + gz)] = isSlimeChunk(config.seed, baseSearchChunkX + (int)gx, baseSearchChunkZ + (int)gz) ? 1 : 0;
        }
    }

    auto startTime = chrono::steady_clock::now();
    auto lastReportTime = startTime;
    long long positionsChecked = 0;
    
    int x = 0, z = 0;
    int dx = 0, dz = -1;

    for (long long i = 0; i < spiralLimit; ++i) {
        int cx = centerChunkX + x;
        int cz = centerChunkZ + z;
        
        bool skip = false;
        if (config.minWidth > 0 &&
            cx >= centerChunkX - config.minWidth + 1 && cx <= centerChunkX + config.minWidth - 1 &&
            cz >= centerChunkZ - config.minWidth + 1 && cz <= centerChunkZ + config.minWidth - 1) {
            skip = true;
        }

        // Spiral path update logic for next iteration
        if (x == z || (x < 0 && x == -z) || (x > 0 && x == 1 - z)) {
            int temp = dx;
            dx = -dz;
            dz = temp;
        }
        x += dx;
        z += dz;
        
        if (skip) continue;

        int startInX = config.fineSearch ? 0 : 8; // 挂机点在区块中心 (偏移为 8)
        int endInX = config.fineSearch ? 15 : 8;
        int startInZ = config.fineSearch ? 0 : 8;
        int endInZ = config.fineSearch ? 15 : 8;

        for (int inX = startInX; inX <= endInX; ++inX) {
            for (int inZ = startInZ; inZ <= endInZ; ++inZ) {
                
                positionsChecked++;

                int blockX = cx * 16 + inX;
                int blockZ = cz * 16 + inZ;

                int blockSize = 0;
                int chunkSize = 0;

                long long startGx = (long long)cx - R_CHUNK - baseSearchChunkX;
                long long startGz = (long long)cz - R_CHUNK - baseSearchChunkZ;
                
                const uint8_t* cacheBase = &slimeChunkCache[(size_t)(startGx * gridWidth + startGz)];
                int weightOffset = (inX * 16 + inZ) * LIMIT * LIMIT;
                const int* bWeightBase = &precomputedBlockWeights[weightOffset];
                const int* cWeightBase = &precomputedChunkWeights[weightOffset];

                for (int d_x = 0; d_x < LIMIT; ++d_x) {
                    const uint8_t* cacheRow = cacheBase + d_x * gridWidth;
                    const int* bWeightRow = bWeightBase + d_x * LIMIT;
                    const int* cWeightRow = cWeightBase + d_x * LIMIT;
                    
                    int b_sum = 0;
                    int c_sum = 0;

                    // Fully unrollable branchless inner loop
                    for (int d_z = 0; d_z < LIMIT; ++d_z) {
                        uint8_t c = cacheRow[d_z]; // 1 or 0
                        b_sum += bWeightRow[d_z] * c;
                        c_sum += cWeightRow[d_z] * c;
                    }
                    
                    blockSize += b_sum;
                    chunkSize += c_sum;
                }

                if ((blockSize >= config.minBlockSize && blockSize <= config.maxBlockSize) || 
                    (chunkSize >= config.minChunkSize && chunkSize <= config.maxChunkSize)) {
                        
                    // 只在大于当前找到的最大值时才记录并写入
                    if (blockSize > currentMaxBlockSize || chunkSize > currentMaxChunkSize) {
                        if (blockSize > currentMaxBlockSize) currentMaxBlockSize = blockSize;
                        if (chunkSize > currentMaxChunkSize) currentMaxChunkSize = chunkSize;

                        matches++;
                        cout << "\r                                                                                                    \r";
                        cout << "New Max found - Pos: " << blockX << "," << blockZ 
                                << " Chunk: " << cx << "c" << inX << "," << cz << "c" << inZ
                                << " Blocks: " << blockSize << " Chunks: " << chunkSize << "\n";
                        
                        lastReportTime = chrono::steady_clock::now(); 
                        
                        if (out.is_open()) {
                            out << blockX << "," << blockZ << ";" 
                                << cx << "c" << inX << "," << cz << "c" << inZ << ";" 
                                << blockSize << ";" << chunkSize << "\n" << flush;
                        }
                    }
                } 
            } // end inZ
        } // end inX
        
        processedChunks++;

        // Progress bar rendering
        auto now = chrono::steady_clock::now();
        if (chrono::duration_cast<chrono::milliseconds>(now - lastReportTime).count() > 100 || processedChunks == totalComputeChunks) {
            lastReportTime = now;
            auto elapsed = chrono::duration_cast<chrono::nanoseconds>(now - startTime).count();

            long long remainingChunks = totalComputeChunks - processedChunks;
            double chunksPerSec = (double)processedChunks / (elapsed / 1e9);
            long long etaSeconds = chunksPerSec > 0 ? (long long)(remainingChunks / chunksPerSec) : 0;

            int percent = (int)((processedChunks * 100) / totalComputeChunks);
            cout << "\r[";
            for (int p = 0; p < 50; ++p) {
                if (p < percent / 2) cout << "=";
                else if (p == percent / 2) cout << ">";
                else cout << " ";
            }
            cout << "] " << percent << "% (" << processedChunks << "/" << totalComputeChunks << ") "
                    << "ETA: " << setfill('0') << setw(2) << (etaSeconds / 3600) << ":"
                    << setw(2) << ((etaSeconds % 3600) / 60) << ":"
                    << setw(2) << (etaSeconds % 60) << flush;
        }
    } // end main loop

    auto endTime = chrono::steady_clock::now();
    auto totalElapsed = chrono::duration_cast<chrono::nanoseconds>(endTime - startTime).count();
    double nsPerCheck = positionsChecked > 0 ? (double)totalElapsed / positionsChecked : 0.0;
    
    // NOTE: This finishes the outer function
    cout << "\nSearch complete. Found " << matches << " matches.\n";
    cout << fixed << setprecision(0) << positionsChecked << " of " << positionsChecked << " positions checked.\n";
    cout << fixed << setprecision(0) << nsPerCheck << " nanoseconds per position\n";
} // END runSearch

int main(int argc, char* argv[]) {
    SearchConfig config;
    
    // Parse arguments simply
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--seed" && i + 1 < argc) config.seed = stoll(argv[++i]);
        else if (arg == "--fine") config.fineSearch = true;
        else if (arg == "--out" && i + 1 < argc) config.outputFile = argv[++i];
        else if (arg == "--maxWidth" && i + 1 < argc) config.maxWidth = stoi(argv[++i]);
        else if (arg == "--minWidth" && i + 1 < argc) config.minWidth = stoi(argv[++i]);
        else if (arg == "--startX" && i + 1 < argc) config.startX = stoi(argv[++i]);
        else if (arg == "--startZ" && i + 1 < argc) config.startZ = stoi(argv[++i]);
        else if (arg == "--minChunkSize" && i + 1 < argc) config.minChunkSize = stoi(argv[++i]);
        else if (arg == "--maxChunkSize" && i + 1 < argc) config.maxChunkSize = stoi(argv[++i]);
        else if (arg == "--minBlockSize" && i + 1 < argc) config.minBlockSize = stoi(argv[++i]);
        else if (arg == "--maxBlockSize" && i + 1 < argc) config.maxBlockSize = stoi(argv[++i]);
        else if (arg == "--h" || arg == "--help") {
            cout << "Slimefinder C++ Clone\n"
                 << "Options:\n"
                 << "  --seed <int>\n"
                 << "  --startX <int>\n"
                 << "  --startZ <int>\n"
                 << "  --maxWidth <int>\n"
                 << "  --fine\n"
                 << "  --out <filename>\n"
                 << "  --minChunkSize <int> / --maxChunkSize <int>\n"
                 << "  --minBlockSize <int> / --maxBlockSize <int>\n";
            return 0;
        }
    }
    
    runSearch(config);
    return 0;
}

