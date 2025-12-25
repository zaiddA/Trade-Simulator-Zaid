// main.cpp

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#endif

#include <algorithm>
#include <csignal>
#include <atomic>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <string>
#include <mutex>
#include <limits>
#include <cstdlib>
#include <filesystem>
#include <sstream>

#include "OrderBook.h"

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

// Graceful shutdown flag
static std::atomic<bool> keepRunning{true};
static std::atomic<int64_t> lastOkxTsMs{0};
static std::atomic<int64_t> lastOkxRecvMs{0};
static void handle_signal(int) { keepRunning = false; }

static int64_t nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// Almgren–Chriss impact model
double almgrenChrissImpact(double Q, double V, double sigma, double lambda)
{
    double kappa = 0.1;
    double perm = kappa * sigma / V * Q * Q;
    double risk = lambda * sigma * sigma * Q * Q;
    return perm + risk;
}

// Try to open a model file from multiple candidate locations
static std::ifstream open_model_file(const std::string& filename, const std::string& exeDir)
{
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path(filename),
        std::filesystem::path(exeDir) / filename,
        std::filesystem::path(exeDir).parent_path() / filename,
        std::filesystem::path(exeDir).parent_path().parent_path() / filename};

    for (const auto& p : candidates)
    {
        std::error_code ec;
        if (std::filesystem::exists(p, ec))
        {
            std::ifstream in(p.string());
            if (in) return in;
        }
    }
    return std::ifstream{};
}

static void set_env_var(const std::string& k, const std::string& v)
{
#ifdef _WIN32
    _putenv_s(k.c_str(), v.c_str());
#else
    setenv(k.c_str(), v.c_str(), 1);
#endif
}

// Load simple KEY=VALUE pairs from a .env-style file
static void load_env_file(const std::filesystem::path& baseDir)
{
    std::vector<std::filesystem::path> candidates = {
        baseDir / ".env",
        baseDir.parent_path() / ".env"};
    for (const auto& p : candidates)
    {
        std::error_code ec;
        if (!std::filesystem::exists(p, ec)) continue;
        std::ifstream in(p.string());
        if (!in) continue;
        std::string line;
        while (std::getline(in, line))
        {
            if (line.empty() || line[0] == '#') continue;
            auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);
            if (!key.empty() && !val.empty()) set_env_var(key, val);
        }
        break; // use first found
    }
}

// Command-line usage help
static void print_help()
{
    std::cout
        << "Usage: simulator [options]\n"
        << "  -s,--symbol    SYMBOL     Instrument (default: BTC-USDT)\n"
        << "  -n,--notional  USD        Notional (default: 100)\n"
        << "  -f,--fee       BPS        Taker fee in bps (default: 10)\n"
        << "  -v,--vol       USD        Daily volume (default: 1e9)\n"
        << "  -d,--delay     SECONDS    Warmup delay (default: 5)\n"
        << "  -i,--interval  SECONDS    Simulation interval (default: 5)\n"
        << "  --volatility   FLOAT      Daily sigma (default: 0.005)\n"
        << "  --risk         FLOAT      Risk aversion lambda (default: 1e-6)\n"
        << "  -h,--help                 Show this help message\n";
}

// Maker/Taker classifier
struct MakerTakerModel {
    double intercept = 0.0;
    double w_spread = 0.0;
    double w_depth = 0.0;

    bool load(const std::string& filename) {
        std::ifstream in(filename);
        if (!in) return false;
        json j;
        in >> j;
        intercept = j["intercept"];
        w_spread = j["weights"]["spread"];
        w_depth = j["weights"]["depth_top5"];
        return true;
    }

    double predict(double spread, double depth_top5) const {
        double z = intercept + w_spread * spread + w_depth * depth_top5;
        return 1.0 / (1.0 + std::exp(-z));
    }
};

int main(int argc, char *argv[])
{
    std::signal(SIGINT, handle_signal);

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

    std::string symbol = "BTC-USDT";
    double notionalUsd = 100.0, takerBps = 10.0, dailyVolUsd = 1e9;
    int warmupSec = 5, intervalSec = 5;
    double sigma = 0.005, lambda = 1e-6;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "-s" || a == "--symbol") && i + 1 < argc) symbol = argv[++i];
        else if ((a == "-n" || a == "--notional") && i + 1 < argc) notionalUsd = std::stod(argv[++i]);
        else if ((a == "-f" || a == "--fee") && i + 1 < argc) takerBps = std::stod(argv[++i]);
        else if ((a == "-v" || a == "--vol") && i + 1 < argc) dailyVolUsd = std::stod(argv[++i]);
        else if ((a == "-d" || a == "--delay") && i + 1 < argc) warmupSec = std::stoi(argv[++i]);
        else if ((a == "-i" || a == "--interval") && i + 1 < argc) intervalSec = std::stoi(argv[++i]);
        else if (a == "--volatility" && i + 1 < argc) sigma = std::stod(argv[++i]);
        else if (a == "--risk" && i + 1 < argc) lambda = std::stod(argv[++i]);
        else if (a == "-h" || a == "--help") { print_help(); return 0; }
        else { std::cerr << "Unknown option: " << a << "\n"; print_help(); return 1; }
    }

    spdlog::info("Params ▶ symbol={}, notional=${:.2f}, fee={}bps, vol=${:.0f}, delay={}s, interval={}s, sigma={:.3f}%, lambda={:.1e}",
                 symbol, notionalUsd, takerBps, dailyVolUsd, warmupSec, intervalSec, sigma * 100.0, lambda);

    std::string exeDir = std::filesystem::canonical(std::filesystem::path(argv[0])).parent_path().string();
    load_env_file(exeDir);

    ix::initNetSystem();

    // Web UI stream server to broadcast ticks
    int port = 9002;
    if (const char* p = std::getenv("PORT")) port = std::atoi(p);
    ix::WebSocketServer streamServer(port, "0.0.0.0");
    json configMsg = {
        {"type", "config"},
        {"data",
             {{"symbol", symbol},
              {"notional", notionalUsd},
              {"taker_bps", takerBps},
              {"daily_vol_usd", dailyVolUsd},
              {"sigma", sigma},
              {"lambda", lambda},
              {"interval_sec", intervalSec},
              {"warmup_sec", warmupSec}}}};

    streamServer.setOnClientMessageCallback(
        [configMsg](std::shared_ptr<ix::ConnectionState> connectionState,
                    ix::WebSocket &ws,
                    const ix::WebSocketMessagePtr &msg) {
            using namespace ix;
            if (msg->type == WebSocketMessageType::Open)
            {
                spdlog::info("UI client connected: {}", connectionState->getId());
                ws.send(configMsg.dump());
            }
            else if (msg->type == WebSocketMessageType::Close)
            {
                spdlog::info("UI client disconnected: {}", connectionState->getId());
            }
            else if (msg->type == WebSocketMessageType::Message)
            {
                // Respond to ping or ignore other messages for now
                auto j = json::parse(msg->str, nullptr, false);
                if (j.is_object() && j.value("type", "") == "ping")
                {
                    ws.send(R"({"type":"pong"})");
                }
            }
            else if (msg->type == WebSocketMessageType::Error)
            {
                spdlog::warn("UI client error: {}", msg->errorInfo.reason);
            }
        });

    auto listenRes = streamServer.listen();
    if (!listenRes.first)
    {
        spdlog::error("Failed to start UI stream server on 9002: {}", listenRes.second);
        return 1;
    }
    streamServer.start();
    auto broadcastTick = [&streamServer](const json& payload) {
        auto clients = streamServer.getClients();
        for (auto& client : clients)
        {
            if (client)
            {
                client->send(payload.dump());
            }
        }
    };
    spdlog::info("UI stream server listening on ws://localhost:{}/stream", port);

    double intercept = 0.0, w_spread = 0.0, w_depth = 0.0;
    {
        auto in = open_model_file("slippage_model.json", exeDir);
        if (!in)
        {
            spdlog::info("Slippage model not found; using default zero weights.");
        }
        else
        {
            if (in.peek() == std::ifstream::traits_type::eof())
            {
                spdlog::warn("Slippage model file is empty; using defaults.");
            }
            else
            {
                try
                {
                    json m;
                    in >> m;
                    intercept = m.value("intercept", 0.0);
                    w_spread = m["weights"].value("spread", 0.0);
                    w_depth = m["weights"].value("depth_top5", 0.0);
                    spdlog::info("Loaded slippage model ▶ intercept={:.3e}, spread_w={:.3e}, depth_w={:.3e}", intercept, w_spread, w_depth);
                }
                catch (const std::exception& e)
                {
                    spdlog::warn("Failed to parse slippage_model.json; using defaults. {}", e.what());
                }
            }
        }
    }

    MakerTakerModel mt_model;
    {
        auto in = open_model_file("maker_taker_model.json", exeDir);
        if (!in)
        {
            spdlog::info("Maker/taker model not found; using neutral defaults (50/50).");
            mt_model.intercept = 0.0;
            mt_model.w_spread = 0.0;
            mt_model.w_depth = 0.0;
        }
        else
        {
            if (in.peek() == std::ifstream::traits_type::eof())
            {
                spdlog::warn("Maker/taker model file is empty; using defaults.");
                mt_model.intercept = 0.0;
                mt_model.w_spread = 0.0;
                mt_model.w_depth = 0.0;
            }
            else
            {
                try
                {
                    json j;
                    in >> j;
                    mt_model.intercept = j["intercept"];
                    mt_model.w_spread = j["weights"]["spread"];
                    mt_model.w_depth = j["weights"]["depth_top5"];
                    spdlog::info("Loaded maker/taker model ▶ intercept={:.3e}, spread_w={:.3e}, depth_w={:.3e}",
                                 mt_model.intercept, mt_model.w_spread, mt_model.w_depth);
                }
                catch (const std::exception& e)
                {
                    spdlog::warn("Failed to parse maker_taker_model.json; using defaults. {}", e.what());
                    mt_model.intercept = 0.0;
                    mt_model.w_spread = 0.0;
                    mt_model.w_depth = 0.0;
                }
            }
        }
    }

    ix::WebSocket ws;
    OrderBook book;
    spdlog::set_level(spdlog::level::info);

    ws.setUrl("wss://ws.okx.com:8443/ws/v5/public");
    ws.setOnMessageCallback([&](const ix::WebSocketMessagePtr &msg) {
        using namespace ix;
        if (msg->type == WebSocketMessageType::Open) {
            spdlog::info("Connected – subscribing to {}", symbol);
            json sub = { {"op","subscribe"}, {"args", {{{"channel","books"}, {"instId",symbol}}} } };
            ws.send(sub.dump());
        } else if (msg->type == WebSocketMessageType::Message) {
            static std::ofstream rawOut("raw_l2.jsonl", std::ios::app);
            rawOut << msg->str << "\n";
            try {
                auto j = json::parse(msg->str);
                if (j.contains("data")) {
                    for (auto &e : j["data"]) {
                        if (e.contains("ts"))
                        {
                            try
                            {
                                if (e["ts"].is_string())
                                    lastOkxTsMs = std::stoll(e["ts"].get<std::string>());
                                else if (e["ts"].is_number())
                                    lastOkxTsMs = e["ts"].get<int64_t>();
                                lastOkxRecvMs = nowMs();
                            }
                            catch (...)
                            {
                            }
                        }
                        if (e.contains("asks")) {
                            for (auto &lvl : e["asks"]) {
                                double p = std::stod(lvl[0].get<std::string>());
                                double s = std::stod(lvl[1].get<std::string>());
                                book.updateLevel(true, p, s);
                            }
                        }
                        if (e.contains("bids")) {
                            for (auto &lvl : e["bids"]) {
                                double p = std::stod(lvl[0].get<std::string>());
                                double s = std::stod(lvl[1].get<std::string>());
                                book.updateLevel(false, p, s);
                            }
                        }
                    }
                }
            } catch (const std::exception &ex) {
                spdlog::error("JSON parse error: {}", ex.what());
            }
        } else if (msg->type == WebSocketMessageType::Error) {
            spdlog::error("WebSocket error: {}", msg->errorInfo.reason);
        }
    });
    ws.start();

    std::this_thread::sleep_for(std::chrono::seconds(warmupSec));
    spdlog::info("Starting simulation loop every {} seconds", intervalSec);

    while (keepRunning.load()) {
        auto t0 = std::chrono::steady_clock::now();
        int64_t loopTs = nowMs();

        double vwap = book.simulateMarketBuy(notionalUsd);
        double bestAsk = book.getBestAsk();
        double bestBid = book.getBestBid();
        double spread = book.getSpread();
        double depth5 = book.getDepthTopAsks(5);
        double depth5Bid = book.getDepthTopBids(5);
        double mid = (bestAsk + bestBid) / 2.0;
        double imbalance = (depth5 + depth5Bid) > 0.0 ? (depth5Bid - depth5) / (depth5 + depth5Bid) : 0.0;

        double tickLatencyMs = -1.0;
        auto lastTs = lastOkxTsMs.load();
        if (lastTs > 0)
            tickLatencyMs = static_cast<double>(loopTs - lastTs);
        int dropsInWindow = 0;
        auto lastRecv = lastOkxRecvMs.load();
        if (lastRecv > 0 && (loopTs - lastRecv) > intervalSec * 2000)
            dropsInWindow = 1;

        if (std::isnan(vwap) || std::isnan(bestAsk) || std::isnan(bestBid)) {
            spdlog::warn("Not enough depth for ${}", notionalUsd);

            json tick = {
                {"type", "tick"},
                {"status", "insufficient_depth"},
                {"ts", loopTs},
                {"symbol", symbol},
                {"notional", notionalUsd},
                {"taker_bps", takerBps},
                {"spread", spread},
                {"best_bid", bestBid},
                {"best_ask", bestAsk},
                {"depth_top5", depth5},
                {"depth_top5_bids", depth5Bid},
                {"drops_in_window", dropsInWindow},
                {"imbalance", imbalance}};
            if (tickLatencyMs >= 0.0) tick["tick_latency_ms"] = tickLatencyMs;
            broadcastTick(tick);
        } else {
            double slip_vwap = (vwap - mid) / mid * 100.0;
            double slip_mod_pct = intercept + w_spread * spread + w_depth * depth5;
            double slip_mod_usd = slip_mod_pct / 100.0 * notionalUsd;
            double fee_usd = notionalUsd * (takerBps / 10000.0);
            double ac_cost = almgrenChrissImpact(notionalUsd, dailyVolUsd, sigma, lambda);
            double net_ac = slip_mod_usd + fee_usd + ac_cost;
            double takerProb = (!std::isnan(spread) && !std::isnan(depth5))
                                   ? mt_model.predict(spread, depth5)
                                   : std::numeric_limits<double>::quiet_NaN();

            spdlog::info("Sim ▶ VWAP-slip={:.6f}% , Model-slip={:.6f}% (${:.6f}), Fee=${:.2f}, AC Impact=${:.2f}, Net(AC)=${:.2f}",
                         slip_vwap, slip_mod_pct, slip_mod_usd, fee_usd, ac_cost, net_ac);

            if (!std::isnan(spread) && !std::isnan(depth5)) {
                spdlog::info("Maker/Taker ▶ Taker Probability = {:.2f}%", takerProb * 100.0);
            }

            json tick = {
                {"type", "tick"},
                {"status", "ok"},
                {"ts", loopTs},
                {"symbol", symbol},
                {"notional", notionalUsd},
                {"taker_bps", takerBps},
                {"spread", spread},
                {"best_bid", bestBid},
                {"best_ask", bestAsk},
                {"mid", mid},
                {"depth_top5", depth5},
                {"depth_top5_bids", depth5Bid},
                {"vwap_slip_pct", slip_vwap},
                {"model_slip_pct", slip_mod_pct},
                {"model_slip_usd", slip_mod_usd},
                {"fee_usd", fee_usd},
                {"ac_impact_usd", ac_cost},
                {"net_cost_usd", net_ac},
                {"drops_in_window", dropsInWindow},
                {"imbalance", imbalance}};
            if (!std::isnan(takerProb)) tick["taker_prob_pct"] = takerProb * 100.0;
            if (tickLatencyMs >= 0.0) tick["tick_latency_ms"] = tickLatencyMs;
            broadcastTick(tick);
        }

        auto t1 = std::chrono::steady_clock::now();
        int elapsed = int(std::chrono::duration_cast<std::chrono::seconds>(t1 - t0).count());
        int toSleep = std::max(0, intervalSec - elapsed);
        for (int i = 0; i < toSleep && keepRunning.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    ws.stop();
    streamServer.stop();
    ix::uninitNetSystem();
    spdlog::info("Shutdown complete.");
    return 0;
}
