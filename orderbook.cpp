#include "types.hpp"
#include "fix.hpp"
#include <map>
#include <list>
#include <memory>
#include <sstream>
#include <iomanip>

namespace trading {

    // fr fr no cap logging utility
    class Logger {
        static inline std::string get_time() {
            auto now = std::chrono::system_clock::now();
            auto tt = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&tt), "%H:%M:%S");
            return ss.str();
        }

    public:
        template<typename... Args>
        static void log(const Args&... args) {
            std::stringstream ss;
            ss << "[" << get_time() << "] ";
            (ss << ... << args);
            std::cout << ss.str() << std::endl;
        }
    };

    template<typename PriceComparator>
    class OrderBook {
        using OrderPtr = std::shared_ptr<Order>;
        using OrderList = std::list<OrderPtr>;
        using PriceLevel = std::pair<Price, OrderList>;
        using OrderMap = std::map<Price, OrderList, PriceComparator>;

        OrderMap orders;
        std::string symbol;

    public:
        OrderBook(std::string sym) : symbol(std::move(sym)) {}

        void add(const OrderPtr& order) {
            orders[order->price].push_back(order);
            Logger::log("added order ", order->id, " @ ", order->price / 10000.0);
        }

        void remove(const OrderPtr& order) {
            auto& list = orders[order->price];
            list.remove(order);
            if (list.empty()) orders.erase(order->price);
            Logger::log("removed order ", order->id);
        }

        OrderPtr best() const {
            if (orders.empty()) return nullptr;
            const auto& best_list = orders.begin()->second;
            return best_list.empty() ? nullptr : best_list.front();
        }

        // no cap this is useful for debugging
        void print_state() const {
            for (const auto& [price, orders] : this->orders) {
                std::stringstream ss;
                ss << std::fixed << std::setprecision(2) << price / 10000.0 << ": ";
                for (const auto& order : orders) {
                    ss << order->id << "(" << order->remaining() << ") ";
                }
                Logger::log(ss.str());
            }
        }
    };

    class MatchingEngine {
        // min heap for asks (selling), max heap for bids (buying)
        using AskBook = OrderBook<std::less<Price>>;
        using BidBook = OrderBook<std::greater<Price>>;
        AskBook asks;
        BidBook bids;

        void match(std::shared_ptr<Order>& incoming) {
            if (incoming->side == Side::Buy) {
                match_order(incoming, asks, bids);
            } else {
                match_order(incoming, bids, asks);
            }
        }

        template<typename ContraBook, typename SameBook>
        void match_order(std::shared_ptr<Order>& incoming,
            ContraBook& contra_book,
            SameBook& same_book) {

            Logger::log(
                "matching ", incoming->id, " ",
                incoming->side == Side::Buy ? "buy" : "sell", " ",
                incoming->qty, " @ ",
                incoming->type == OrderType::Market ? "MKT" :
                std::to_string(incoming->price / 10000.0)
            );

            while (!incoming->is_filled()) {
                auto resting = contra_book.best();
                if (!resting || !price_matches(incoming, resting)) break;

                auto match_qty = std::min(incoming->remaining(), resting->remaining());
                execute_match(incoming, resting, match_qty);

                if (resting->is_filled())
                    contra_book.remove(resting);
            }

            if (!incoming->is_filled() && incoming->type == OrderType::Limit)
                same_book.add(incoming);


            Logger::log("post-match state:");
            Logger::log("asks:");
            asks.print_state();
            Logger::log("bids:");
            bids.print_state();
        }

        bool price_matches(const std::shared_ptr<Order>& incoming,
            const std::shared_ptr<Order>& resting) {
            if (incoming->type == OrderType::Market) return true;
            return incoming->side == Side::Buy ?
                incoming->price >= resting->price :
                incoming->price <= resting->price;
        }

        void execute_match(std::shared_ptr<Order>& incoming,
            std::shared_ptr<Order>& resting,
            Quantity qty) {
            incoming->filled += qty;
            resting->filled += qty;
            Logger::log(
                "match: ", incoming->id, " vs ", resting->id,
                " for ", qty, " @ ", resting->price / 10000.0
            );
        }

    public:
        MatchingEngine(std::string symbol) : asks(symbol), bids(symbol) {}

        void handle(std::shared_ptr<Order> order) {
            match(order);
        }
    };
}  // namespace trading


// fr fr test harness no cap
int main() {
    using namespace trading;

    auto me = std::make_unique<MatchingEngine>("AAPL");

    // create some limit orders innit
    auto make_order = [](std::string id, Side side, OrderType type,
        Price price, Quantity qty) {
        auto order = std::make_shared<Order>();
        order->id = id;
        order->symbol = "AAPL";
        order->side = side;
        order->type = type;
        order->price = price;
        order->qty = qty;
        order->timestamp = std::chrono::system_clock::now();
        return order;
    };

    // add some resting orders
    me->handle(make_order("sell1", Side::Sell, OrderType::Limit,
        fix::to_fix_price(100), 100));
    me->handle(make_order("sell2", Side::Sell, OrderType::Limit,
        fix::to_fix_price(101), 100));
    me->handle(make_order("buy1", Side::Buy, OrderType::Limit,
        fix::to_fix_price(99), 100));

    // send in a fat market order, watch it match
    me->handle(make_order("buymarket", Side::Buy, OrderType::Market, 0, 150));

    return 0;
}