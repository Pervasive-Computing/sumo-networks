#include <catch2/catch_test_macros.hpp>

#include "ringbuf.hpp"
// #include <vector>

TEST_CASE("ringbuf", "[ringbuf]") {
    auto ringbuf = RingBuffer<int, 4>{};
    REQUIRE(ringbuf.empty());
    REQUIRE(ringbuf.size() == 0);
    REQUIRE(ringbuf.capacity() == 4);

    const int x0 = 0;
    const int x1 = 1;
    const int x2 = 2;
    const int x3 = 3;

    {
        ringbuf.push_back(x0);
        REQUIRE(ringbuf.size() == 1);
        REQUIRE(!ringbuf.empty());
        REQUIRE(!ringbuf.full());
        auto front = ringbuf.front();
        REQUIRE(front.has_value());
        REQUIRE(front.value() == x0);
        auto back = ringbuf.back();
        REQUIRE(back.has_value());
        REQUIRE(back.value() == x0);
    }
    {
        ringbuf.push_back(x1);
        REQUIRE(ringbuf.size() == 2);
        REQUIRE(!ringbuf.empty());
        REQUIRE(!ringbuf.full());
        auto front = ringbuf.front();
        REQUIRE(front.has_value());
        REQUIRE(front.value() == x0);
        auto back = ringbuf.back();
        REQUIRE(back.has_value());
        REQUIRE(back.value() == x1);
    }
    {
        ringbuf.push_back(x2);
        REQUIRE(ringbuf.size() == 3);
        REQUIRE(!ringbuf.empty());
        REQUIRE(!ringbuf.full());
        auto front = ringbuf.front();
        REQUIRE(front.has_value());
        REQUIRE(front.value() == x0);
        auto back = ringbuf.back();
        REQUIRE(back.has_value());
        REQUIRE(back.value() == x2);
    }
    {
        ringbuf.print_internal_state();
        ringbuf.push_back(x3);
        REQUIRE(ringbuf.size() == 4);
        REQUIRE(!ringbuf.empty());
        REQUIRE(ringbuf.full());
        auto front = ringbuf.front();
        REQUIRE(front.has_value());
        REQUIRE(front.value() == x0);
        auto back = ringbuf.back();
        REQUIRE(back.has_value());

        auto underlying = ringbuf.underlying();
        REQUIRE(underlying[0] == x0);
        REQUIRE(underlying[1] == x1);
        REQUIRE(underlying[2] == x2);
        REQUIRE(underlying[3] == x3);
        ringbuf.print_internal_state();

        REQUIRE(back.value() == x3); 
    }
    
}
