# QuantSim: Real-Time Crypto Market Simulator

## Overview
QuantSim ingests OKX L2 via WebSocket, maintains a thread-safe order book, and simulates market buys on a fixed interval to expose execution costs in real time. It models:
- VWAP-based slippage (actual vs predicted)
- Exchange taker fees
- Almgren–Chriss market impact
- Maker/taker probability (logistic regression)
- Net cost in dollars

It also broadcasts each simulation tick over a local WebSocket so the React dashboard can visualize costs, slippage, depth, and microstructure health.

## Requirements
- C++17 compiler (MSVC/MinGW on Windows, clang/gcc on macOS)
- CMake >= 3.14
- Dependencies: ixwebsocket, spdlog, nlohmann-json, OpenSSL, Catch2 (tests), benchmark (optional)
- Frontend: Node 18+ (Vite + React + TS)

## Backend setup (what we actually used)
- ixwebsocket is not in Homebrew; use **vcpkg** on both macOS and Windows to fetch it.

### macOS (Apple Silicon) steps
```bash
# Install pkg-config (needed for zlib)
brew install pkg-config

# Install vcpkg and deps
cd /Users/zaid
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
./vcpkg install "ixwebsocket[ssl]" spdlog nlohmann-json openssl catch2 benchmark
export VCPKG_ROOT=/Users/zaid/vcpkg

# Configure & build (clean dir)
cd /Users/zaid/Trade-Simulator-Zaid
rm -rf Test/build-macos
cmake -S Test -B Test/build-macos \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build Test/build-macos --config Release
```

### Windows (vcpkg + MSVC) steps
Run in a VS Developer PowerShell so `cl` is on PATH:
```powershell
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install "ixwebsocket[ssl]" nlohmann-json spdlog openssl catch2 benchmark
setx VCPKG_ROOT "%CD%"

# new shell to pick up VCPKG_ROOT
cd C:\Users\zaida\Downloads\Trade-Simulator-Zaid\Test
rmdir /s /q build-win 2>nul
mkdir build-win
cd build-win
cmake -G "Visual Studio 17 2022" -A x64 .. `
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake `
  -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```
Binary: `build-win\Release\simulator.exe`.

### Run simulator + UI stream
```bash
./Release/simulator --symbol BTC-USDT --notional 1000 --fee 10 --delay 5 --interval 5
```
- Connects to OKX L2, warms up for `delay` seconds, then simulates a market buy every `interval` seconds.
- Broadcasts ticks on `ws://localhost:9002/stream` (fields: spread, depth, VWAP slip, model slip, fee, AC impact, net cost, maker/taker prob, latency, imbalance).

## Frontend (Execution Desk)
- Live KPIs: net cost breakdown, VWAP vs model slippage, spread/depth/imbalance, maker/taker tilt, latency/drops.
- Charts: net cost components over time, actual vs modeled slippage.
- Microstructure: best bid/ask, mid, depth, imbalance, tick latency.
- Execution Insights: rolling stats from the last runs (avg net cost, max slip, tightest spread, best depth, avg latency, latest maker/taker tilt).
- Feed URL: defaults to `ws://localhost:9002/stream`; override with `VITE_FEED_URL`.

### Run frontend
```bash
cd frontend
npm install
npm run dev
# open the shown localhost URL; it auto-connects to the stream
```

## Model training (Python)
From `Test/`:
```bash
python extract_features.py
python train_slippage_model.py
python train_maker_taker_model.py
```
Outputs `slippage_model.json` and `maker_taker_model.json`.
The repo includes sample model files in `Test/`. If missing/empty, the simulator falls back to neutral defaults (logs a warning and keeps running).

## Testing
```bash
cd Test/build
ctest -C Release
```

## Benchmarking
```bash
cmake --build . --config Release --target orderbook_bench
./Release/orderbook_bench.exe
```

## Project structure
```
Test/
├── main.cpp          # Simulator + OKX ingest + tick broadcaster
├── OrderBook.h/.cpp  # Thread-safe order book + VWAP simulator
├── CMakeLists.txt    # Build config
├── tests/            # Catch2 unit tests
├── benchmarks/       # Google Benchmark
├── train_*.py        # Model training scripts
└── build/            # CMake build outputs

frontend/
├── src/App.tsx       # Dashboard UI
├── src/useFeed.ts    # WebSocket feed hook
└── ...
```

## Key components
- OrderBook: thread-safe L2 with VWAP simulation, best bid/ask, spread, depth.
- Models: linear regression (slippage), logistic regression (maker/taker), Almgren–Chriss impact.
- Real-time pipeline: OKX WebSocket → book updates → periodic simulation → log + UI tick broadcast.

## Data flow at a glance
1) Connect to OKX L2 (books channel for your symbol).
2) Maintain a thread-safe order book (bids/asks) as updates arrive.
3) On each interval, simulate a market buy of the configured notional:
   - Compute VWAP slip (actual), model slip (linear reg), taker fee, AC impact, maker/taker probability.
4) Log results and broadcast a JSON tick over the local WebSocket for the UI.

## Business value
- Estimate execution costs before sending orders.
- Understand fee exposure and maker/taker tilt.
- Explain cost drivers (slippage vs fee vs impact) with live charts and microstructure context.
