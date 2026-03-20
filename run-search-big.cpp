#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <chrono>
#include <iomanip>
#include <fstream>

using namespace std;
namespace fs = std::filesystem;

struct Config {
    long long seed = 0;
    int minX = 0;
    int minY = 0; // mapped to Z
    int maxX = 0;
    int maxY = 0; // mapped to Z
    int searchChunkSize = 1000; // in blocks
    int processes = 2; // concurrent workers
    string outDir = "out";
    string tempDir = "temp";
};

int main(int argc, char* argv[]) {
    Config config;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--seed" && i + 1 < argc) config.seed = stoll(argv[++i]);
        else if (arg == "--minX" && i + 1 < argc) config.minX = stoi(argv[++i]);
        else if (arg == "--minY" && i + 1 < argc) config.minY = stoi(argv[++i]);
        else if (arg == "--maxX" && i + 1 < argc) config.maxX = stoi(argv[++i]);
        else if (arg == "--maxY" && i + 1 < argc) config.maxY = stoi(argv[++i]);
        else if (arg == "--search-chunk-size" && i + 1 < argc) config.searchChunkSize = stoi(argv[++i]);
        else if (arg == "--processes" && i + 1 < argc) config.processes = stoi(argv[++i]);
        else if (arg == "--out-dir" && i + 1 < argc) config.outDir = argv[++i];
        else if (arg == "--temp-save-dir" && i + 1 < argc) config.tempDir = argv[++i];
        else if (arg == "--help" || arg == "-h") {
            cout << "Usage: run-search-big.exe --seed <int> --minX <int> --minY <int> --maxX <int> --maxY <int> --search-chunk-size <int> --processes <int> --out-dir <dir> --temp-save-dir <dir>\n";
            return 0;
        }
    }

    fs::create_directories(config.outDir);
    fs::create_directories(config.tempDir);

    cout << "Starting big search manager...\n";
    cout << "Seed: " << config.seed << "\n";
    cout << "Range: X[" << config.minX << " to " << config.maxX << "], Z[" << config.minY << " to " << config.maxY << "]\n";
    cout << "Sub-search size: " << config.searchChunkSize << " blocks\n";
    cout << "Concurrent searches (processes): " << config.processes << "\n";
    cout << "NOTE: Since slimefinder-multithread itself uses all CPU threads, keep --processes low (e.g., 1 or 2) to avoid freezing your PC.\n\n";

    queue<pair<string, string>> taskQueue;
    mutex queueMutex;
    atomic<int> completedTasks{0};
    int totalTasks = 0;

    mutex printMutex;
    int globalMaxBlockSize = -1;
    int globalMaxChunkSize = -1;
    string bestBlockStats = "None found";
    string bestChunkStats = "None found";

    // Subdivide the area and queue tasks
    for (int x = config.minX; x <= config.maxX; x += config.searchChunkSize) {
        for (int z = config.minY; z <= config.maxY; z += config.searchChunkSize) {
            
            // Calculate center and maxWidth for slimefinder
            int currentMaxX = min(x + config.searchChunkSize - 1, config.maxX);
            int currentMaxZ = min(z + config.searchChunkSize - 1, config.maxY);
            
            int centerX = (x + currentMaxX) / 2;
            int centerZ = (z + currentMaxZ) / 2;
            
            // maxWidth in chunks (distance from center to edge)
            int widthBlocks = max(currentMaxX - x, currentMaxZ - z) / 2;
            int maxWidthChunks = (widthBlocks / 16) + 1;

            string outFilename = config.tempDir + "/result_" + to_string(x) + "_" + to_string(z) + ".csv";
            
            string cmd = ".\\slimefinder-multithread.exe";
            cmd += " --seed " + to_string(config.seed);
            cmd += " --startX " + to_string(centerX);
            cmd += " --startZ " + to_string(centerZ);
            cmd += " --maxWidth " + to_string(maxWidthChunks);
            cmd += " --quiet";
            cmd += " --out " + outFilename;
            
            taskQueue.push({cmd, outFilename});
            totalTasks++;
        }
    }

    cout << "Total chunks to process: " << totalTasks << "\n\n";

    vector<string> activeFiles(config.processes);

    auto worker = [&](int thread_id) {
        while (true) {
            string cmd;
            string outFilename;
            {
                lock_guard<mutex> lock(queueMutex);
                if (taskQueue.empty()) break;
                auto task = taskQueue.front();
                cmd = task.first;
                outFilename = task.second;
                taskQueue.pop();
            }

            activeFiles[thread_id] = outFilename;
            int result = system(cmd.c_str());

            // Process output file for new global maxes
            ifstream in(outFilename);
            if (in.is_open()) {
                string line;
                getline(in, line); // discard header "block-position;chunk-position;blockSize;chunkSize"
                while (getline(in, line)) {
                    if (line.empty()) continue;
                    
                    size_t p1 = line.find(';');
                    if (p1 == string::npos) continue;
                    size_t p2 = line.find(';', p1 + 1);
                    if (p2 == string::npos) continue;
                    size_t p3 = line.find(';', p2 + 1);
                    if (p3 == string::npos) continue;

                    string blockPos = line.substr(0, p1);
                    string chunkPos = line.substr(p1 + 1, p2 - p1 - 1);
                    int blockSize = stoi(line.substr(p2 + 1, p3 - p2 - 1));
                    int chunkSize = stoi(line.substr(p3 + 1));

                    lock_guard<mutex> lock(printMutex);
                    if (blockSize > globalMaxBlockSize || chunkSize > globalMaxChunkSize) {
                        if (blockSize > globalMaxBlockSize) {
                            globalMaxBlockSize = blockSize;
                            bestBlockStats = "Pos: " + blockPos + " Chunk: " + chunkPos + " Blocks: " + to_string(blockSize) + " Chunks: " + to_string(chunkSize);
                        }
                        if (chunkSize > globalMaxChunkSize) {
                            globalMaxChunkSize = chunkSize;
                            bestChunkStats = "Pos: " + blockPos + " Chunk: " + chunkPos + " Blocks: " + to_string(blockSize) + " Chunks: " + to_string(chunkSize);
                        }

                        cout << "\r                                                                                                    \r";
                        cout << "[Manager] NEW GLOBAL MAX - Pos: " << blockPos << " Chunk: " << chunkPos 
                             << " Blocks: " << blockSize << " Chunks: " << chunkSize << "\n";
                    }
                }
            }

            activeFiles[thread_id] = "";
            ++completedTasks;
        }
    };

    vector<thread> threads;
    for (int i = 0; i < config.processes; ++i) {
        threads.emplace_back(worker, i);
    }

    auto startTime = chrono::steady_clock::now();
    auto lastReportTime = startTime;

    while (true) {
        // --- Real-time CSV scanning ---
        for (int i = 0; i < config.processes; ++i) {
            string currentFile = activeFiles[i];
            if (currentFile.empty()) continue;
            
            ifstream in(currentFile);
            if (in.is_open()) {
                string line;
                getline(in, line); // bypass header
                while (getline(in, line)) {
                    if (line.empty()) continue;
                    
                    size_t p1 = line.find(';');
                    if (p1 == string::npos) continue;
                    size_t p2 = line.find(';', p1 + 1);
                    if (p2 == string::npos) continue;
                    size_t p3 = line.find(';', p2 + 1);
                    if (p3 == string::npos) continue;

                    string blockPos = line.substr(0, p1);
                    string chunkPos = line.substr(p1 + 1, p2 - p1 - 1);
                    int blockSize = stoi(line.substr(p2 + 1, p3 - p2 - 1));
                    int chunkSize = stoi(line.substr(p3 + 1));

                    lock_guard<mutex> lock(printMutex);
                    if (blockSize > globalMaxBlockSize || chunkSize > globalMaxChunkSize) {
                        if (blockSize > globalMaxBlockSize) {
                            globalMaxBlockSize = blockSize;
                            bestBlockStats = "Pos: " + blockPos + " Chunk: " + chunkPos + " Blocks: " + to_string(blockSize) + " Chunks: " + to_string(chunkSize);
                        }
                        if (chunkSize > globalMaxChunkSize) {
                            globalMaxChunkSize = chunkSize;
                            bestChunkStats = "Pos: " + blockPos + " Chunk: " + chunkPos + " Blocks: " + to_string(blockSize) + " Chunks: " + to_string(chunkSize);
                        }

                        cout << "\r                                                                                                    \r";
                        cout << "[Manager] NEW GLOBAL MAX - Pos: " << blockPos << " Chunk: " << chunkPos 
                             << " Blocks: " << blockSize << " Chunks: " << chunkSize << "\n";
                    }
                }
            }
        }
        // ------------------------------

        int currentProcessed = completedTasks.load();
        
        auto now = chrono::steady_clock::now();
        if (chrono::duration_cast<chrono::milliseconds>(now - lastReportTime).count() > 100 || currentProcessed == totalTasks) {
            lastReportTime = now;
            auto elapsed = chrono::duration_cast<chrono::nanoseconds>(now - startTime).count();

            long long remainingTasks = totalTasks - currentProcessed;
            double tasksPerSec = (double)currentProcessed / (elapsed / 1e9);
            long long etaSeconds = tasksPerSec > 0 ? (long long)(remainingTasks / tasksPerSec) : 0;

            int percent = totalTasks > 0 ? (int)((currentProcessed * 100) / totalTasks) : 100;
            
            lock_guard<mutex> lock(printMutex);
            cout << "\r[";
            for (int p = 0; p < 50; ++p) {
                if (p < percent / 2) cout << "=";
                else if (p == percent / 2) cout << ">";
                else cout << " ";
            }
            cout << "] " << percent << "% (" << currentProcessed << "/" << totalTasks << ") "
                 << "ETA: " << setfill('0') << setw(2) << (etaSeconds / 3600) << ":"
                 << setw(2) << ((etaSeconds % 3600) / 60) << ":"
                 << setw(2) << (etaSeconds % 60) << flush;
        }

        if (currentProcessed >= totalTasks) {
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    for (auto& t : threads) {
        t.join();
    }

    cout << "\n\nAll sub-searches completed. Results saved in " << config.tempDir << ".\n";
    if (globalMaxBlockSize > -1 || globalMaxChunkSize > -1) {
        cout << "\n--- FINAL RESULTS ---\n";
        cout << "Best Hit (by Blocks):  " << bestBlockStats << "\n";
        cout << "Best Hit (by Chunks):  " << bestChunkStats << "\n";
    }
    cout << "\nYou can aggregate the CSVs into " << config.outDir << " as needed.\n";

    return 0;
}
