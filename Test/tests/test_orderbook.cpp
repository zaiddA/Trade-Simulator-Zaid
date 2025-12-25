#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "../OrderBook.h"

using Catch::Approx;

TEST_CASE("OrderBook best-ask & best-bid", "[OrderBook]")
{
    OrderBook b;
    // Empty book should report NaN for best prices
    REQUIRE(std::isnan(b.getBestAsk()));
    REQUIRE(std::isnan(b.getBestBid()));

    // Add one ask and one bid
    b.updateLevel(true, 100.0, 1.0);
    b.updateLevel(false, 99.0, 2.0);
    REQUIRE(b.getBestAsk() == Approx(100.0));
    REQUIRE(b.getBestBid() == Approx(99.0));
}

TEST_CASE("VWAP simulation simple fill", "[OrderBook]")
{
    OrderBook b;
    // Two ask levels: price 10 size 5, price 11 size 5
    b.updateLevel(true, 10.0, 5.0);
    b.updateLevel(true, 11.0, 5.0);

    // Simulate fills
    double v1 = b.simulateMarketBuy(50.0);
    REQUIRE(v1 == Approx(10.0).margin(1e-12));

    double v2 = b.simulateMarketBuy(55.0);
    double units = 5.0 + (5.0 / 11.0);
    double expected_v2 = 55.0 / units;
    REQUIRE(v2 == Approx(expected_v2).margin(1e-12));
}

TEST_CASE("Spread & depth getters", "[OrderBook]")
{
    OrderBook b;
    // One ask at 101, one bid at 99
    b.updateLevel(true, 101.0, 1.0);
    b.updateLevel(false, 99.0, 1.0);

    REQUIRE(b.getSpread() == Approx(2.0).margin(1e-12));
    REQUIRE(b.getDepthTopAsks(5) == Approx(1.0).margin(1e-12));
    REQUIRE(b.getDepthTopBids(5) == Approx(1.0).margin(1e-12));
}
