# Slimefinder C++

A C++ high-performance, single-file clone of [Nukelawe/slimefinder](https://github.com/Nukelawe/slimefinder), but focused purely on the search logic and slime chunk detection. Since it's in C++, it doesn't process images, making it faster and easier to deploy for CLI searches.

## Features Supported
- ✅ Core Java `Random` logic recreation in C++
- ✅ Valid Minecraft slime chunk generation formula matching
- ✅ Despawn sphere and Exclusion sphere masking calculations
- ✅ Standard and "fine" search granularity
- ✅ Chunk / block size tracking
- ✅ Output to `.csv` results format

## Compilation

You can compile the code using any standard C++ compiler (requires C++11 or newer).

Using `g++`:
```bash
g++ -O3 -o slimefinder slimefinder.cpp
```

Using MSVC (Developer Command Prompt):
```bash
cl /EHsc /O2 slimefinder.cpp
```

## Usage Example

To find a chunk configuration around coordinates `(X=0, Z=0)` in a 10 chunk radius, looking for at least 3 slime chunks:
```bash
./slimefinder --seed 123456789 --startX 0 --startZ 0 --maxWidth 10 --minChunkSize 3 --out results.csv
```

To run a deep search over all valid block bounds in those chunks:
```bash
./slimefinder --seed 123456789 --startX 0 --startZ 0 --maxWidth 10 --minChunkSize 3 --fine
```

### Available Options
| Option             | Description                                          | Default      |
| ------------------ | ---------------------------------------------------- | ------------ |
| `--seed <int>`     | Your Minecraft World Seed                            | `0`          |
| `--startX <int>`   | Central starting X block coord                       | `0`          |
| `--startZ <int>`   | Central starting Z block coord                       | `0`          |
| `--maxWidth <int>` | Chunks radius bounds to search around                | `1`          |
| `--fine`           | Enables Fine search (every block position searched)  | `false`      |
| `--minChunkSize`   | Min surface slime chunks configured                  | `0`          |
| `--maxChunkSize`   | Max surface slime chunks                             | `289`        |
| `--minBlockSize`   | Min slime chunk blocks connected in mask             | `0`          |
| `--maxBlockSize`   | Max slime chunk blocks connected in mask             | `73984`      |
| `--out <file>`     | Custom `.csv` file to output to                      | `results.csv`|

Enjoy your C++ fast CLI Slimefinder!
