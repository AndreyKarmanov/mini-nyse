#pragma once
#include <string>
#include <chrono>

// types.hpp
namespace trading {
    using OrderId = std::string;  // or uint64_t if you prefer
    using Price = int64_t;
    using Quantity = int64_t;
    
    enum class Side { Buy, Sell };
    enum class OrderType { Market, Limit };
    
    struct Order {
        OrderId id;
        std::string symbol;
        Side side;
        OrderType type;
        Price price;
        Quantity qty;
        Quantity filled = 0;
        std::chrono::system_clock::time_point timestamp;
        
        bool is_filled() const { return qty == filled; }
        Quantity remaining() const { return qty - filled; }
    };
}