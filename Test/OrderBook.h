#pragma once

#include <map>
#include <mutex>
#include <cmath>

// Thread-safe L2 order book for simulation
class OrderBook
{
public:
    // Update or remove a level in the book
    void updateLevel(bool isAsk, double price, double size);

    // Simulate a market buy for given USD notional, return VWAP price or NAN
    double simulateMarketBuy(double notionalUsd);

    // Market data accessors
    double getBestAsk() const;
    double getBestBid() const;
    double getSpread() const;
    double getDepthTopAsks(int levels = 5) const;
    double getDepthTopBids(int levels = 5) const;

private:
    std::map<double, double, std::less<>> asks;
    std::map<double, double, std::greater<>> bids;
    mutable std::mutex mtx;
};
