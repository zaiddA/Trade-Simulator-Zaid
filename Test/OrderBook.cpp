#include "OrderBook.h"

#include <limits>

void OrderBook::updateLevel(bool isAsk, double price, double size)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (isAsk)
    {
        if (size <= 0.0)
            asks.erase(price);
        else
            asks[price] = size;
    }
    else
    {
        if (size <= 0.0)
            bids.erase(price);
        else
            bids[price] = size;
    }
}

double OrderBook::simulateMarketBuy(double notionalUsd)
{
    std::lock_guard<std::mutex> lock(mtx);
    double remaining = notionalUsd;
    double cost = 0.0;
    double acquired = 0.0;

    for (const auto &[price, size] : asks)
    {
        double levelNotional = price * size;
        if (remaining <= 1e-9)
            break;

        if (levelNotional <= remaining)
        {
            cost += levelNotional;
            acquired += size;
            remaining -= levelNotional;
        }
        else
        {
            double fillSize = remaining / price;
            cost += price * fillSize;
            acquired += fillSize;
            remaining = 0.0;
            break;
        }
    }

    if (acquired == 0.0)
        return std::numeric_limits<double>::quiet_NaN();

    return cost / acquired;
}

double OrderBook::getBestAsk() const
{
    std::lock_guard<std::mutex> lock(mtx);
    return asks.empty()
               ? std::numeric_limits<double>::quiet_NaN()
               : asks.begin()->first;
}

double OrderBook::getBestBid() const
{
    std::lock_guard<std::mutex> lock(mtx);
    return bids.empty()
               ? std::numeric_limits<double>::quiet_NaN()
               : bids.begin()->first;
}

double OrderBook::getSpread() const
{
    double a = getBestAsk();
    double b = getBestBid();
    if (std::isnan(a) || std::isnan(b))
        return std::numeric_limits<double>::quiet_NaN();
    return a - b;
}

double OrderBook::getDepthTopAsks(int levels) const
{
    std::lock_guard<std::mutex> lock(mtx);
    double sum = 0.0;
    int count = 0;
    for (auto it = asks.begin(); it != asks.end() && count < levels; ++it, ++count)
    {
        sum += it->second;
    }
    return sum;
}

double OrderBook::getDepthTopBids(int levels) const
{
    std::lock_guard<std::mutex> lock(mtx);
    double sum = 0.0;
    int count = 0;
    for (auto it = bids.begin(); it != bids.end() && count < levels; ++it, ++count)
    {
        sum += it->second;
    }
    return sum;
}
