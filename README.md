# QuantSim: Real-Time Crypto Market Simulator

## Overview

QuantSim is a high-performance C++ trade simulator that connects to the OKX exchange using WebSockets and simulates market buy orders to estimate execution cost. It models:

- VWAP-based slippage (actual vs. predicted)
- Exchange taker fees
- Market impact using the Almgren-Chriss model
- Maker/taker probabilities via logistic regression
- Net transaction cost in real-time

## Features

- ✅ Real-time L2 data ingestion (WebSocket)
- ✅ Thread-safe order book
- ✅ Market order simulation with VWAP
- ✅ Trained JSON model for slippage prediction
- ✅ Logistic regression classifier for maker/taker estimation
- ✅ Rule-based fee model
- ✅ Almgren-Chriss impact simulation
- ✅ Logging with `spdlog`
- ✅ JSON parsing via `nlohmann/json`
- ✅ Unit testing with Catch2
- ✅ Performance benchmarking via Google Benchmark

## Requirements

- C++17 compiler (MinGW or MSVC)
- CMake >= 3.14
- vcpkg for dependency management

Dependencies:

- ixwebsocket
- spdlog
- nlohmann-json
- OpenSSL
- Catch2 (testing)
- benchmark (optional)

## Setup

```bash
cd C:/dev
# Clone vcpkg
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat

# Install dependencies
./vcpkg install ixwebsocket[ssl] nlohmann-json spdlog openssl catch2 benchmark
```

## Build

```bash
cd Test
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## Run

### Basic Demo
```bash
cd build/Release
./simulator.exe --symbol BTC-USDT --notional 1000 --fee 10 --delay 5 --interval 5
```

### Advanced Demo (Show Cost Variation)
```bash
./simulator.exe --symbol BTC-USDT --notional 100000 --fee 5 --delay 5 --interval 5
```

### Example Output:

```
[info] Params ▶ symbol=BTC-USDT, notional=$100000.00, fee=5bps, vol=$1000000000, delay=5s, interval=5s, sigma=0.500%, lambda=1.0e-06
[info] Loaded slippage model ▶ intercept=8.000e-04, spread_w=5.000e-04, depth_w=-3.000e-04
[info] Loaded maker/taker model ▶ intercept=5.146e+00, spread_w=9.480e-01, depth_w=-8.150e-01
[info] Connected – subscribing to BTC-USDT
[info] Starting simulation loop every 5 seconds
[info] Sim ▶ VWAP-slip=0.001868% ($1.868084), Model-slip=0.000680% ($0.679968), Fee=$50.0000, AC Impact=$0.255000, Net(VWAP)=$52.123084, Net(Model)=$50.934968
[info] Maker/Taker ▶ Taker Probability = 99.17%
```

## Output Explanation

- **VWAP-slip**: Actual slippage from walking the ask book
- **Model-slip**: Predicted slippage using linear regression
- **Fee**: Exchange taker fee in dollars
- **AC Impact**: Almgren-Chriss market impact estimate
- **Net(VWAP)**: Total cost using actual VWAP slippage
- **Net(Model)**: Total cost using predicted slippage
- **Taker Probability**: Likelihood of being a taker (aggressive order)

## Model Training

To train regression models using Python:

```bash
python extract_features.py
python train_slippage_model.py
python train_maker_taker_model.py
```

Generates:

- `slippage_model.json`
- `maker_taker_model.json`

## Testing

```bash
ctest -C Release
```

## Benchmarking

```bash
cmake --build . --config Release --target orderbook_bench
./Release/orderbook_bench.exe
```

## Project Structure

```
Test/
├── main.cpp                 # Main simulator with WebSocket and simulation loop
├── OrderBook.h/.cpp         # Thread-safe L2 order book implementation
├── CMakeLists.txt           # Build configuration
├── slippage_model.json      # Linear regression coefficients
├── maker_taker_model.json   # Logistic regression coefficients
├── train_*.py              # Python training scripts
├── tests/                  # Unit tests
├── benchmarks/             # Performance benchmarks
└── build/Release/          # Compiled executable and dependencies
```

## Key Components

### OrderBook
- Thread-safe L2 order book with ordered maps
- VWAP simulation for market buy orders
- Best bid/ask, spread, and depth calculations

### Models
- **Slippage Model**: Linear regression on spread + depth features
- **Maker/Taker Model**: Logistic regression for fee optimization
- **Almgren-Chriss**: Market impact estimation

### Real-time Pipeline
- WebSocket connection to OKX
- JSON parsing of L2 updates
- Periodic cost simulation and logging

## Business Value

This simulator helps traders:
- Estimate execution costs before placing orders
- Optimize order timing and sizing
- Understand maker vs taker fee exposure
- Track execution quality over time

## Author

This project was developed by Zaid (B.Tech, IIIT Pune)

---

*Note: The simulator is ready for demo with working models and real-time data connection.* 


