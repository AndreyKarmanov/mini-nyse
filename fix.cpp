#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <iostream>

namespace fix {

    constexpr char FIX_VERSION[] = "FIX.4.2";

    using Price = int64_t;
    using Quantity = int64_t;

    constexpr Price to_fix_price(int64_t dollars, int64_t cents = 0, int64_t subcents = 0) {
        return dollars * 10000 + cents * 100 + subcents;
    }

    std::string price_to_string(Price price) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(4) << (price / 10000.0);
        return ss.str();
    }

    Price string_to_price(const std::string& price_str) {
        double temp = std::stod(price_str);
        return static_cast<Price>(temp * 10000);
    }


    // Common FIX tags - only the ones we actually use
    struct Tags {
        static constexpr int AvgPx = 6;
        static constexpr int BeginString = 8;
        static constexpr int BodyLength = 9;
        static constexpr int CheckSum = 10;
        static constexpr int ClOrdID = 11;
        static constexpr int CumQty = 14;
        static constexpr int ExecID = 17;
        static constexpr int HandlInst = 21;
        static constexpr int MsgSeqNum = 34;
        static constexpr int MsgType = 35;
        static constexpr int OrderID = 37;
        static constexpr int OrderQty = 38;
        static constexpr int OrdStatus = 39;
        static constexpr int OrdType = 40;
        static constexpr int Price = 44;
        static constexpr int SendingTime = 52;
        static constexpr int Side = 54;
        static constexpr int Symbol = 55;
        static constexpr int TransactTime = 60;
        static constexpr int ExecType = 150;
        static constexpr int LeavesQty = 151;
    };

    // Message types we care about
    struct MsgTypes {
        static constexpr char NewOrderSingle = 'D';
        static constexpr char ExecutionReport = '8';
    };

    struct OrderTypes {
        static constexpr char Market = '1';
        static constexpr char Limit = '2';
        static constexpr char Stop = '3';
    };

    struct HandlInst {
        static constexpr char Automated = '1';
        static constexpr char Manual = '2';
        static constexpr char Both = '3';
    };

    struct Sides {
        static constexpr char Buy = '1';
        static constexpr char Sell = '2';
    };

    class FixMessage {
    private:
        std::unordered_map<int, std::string> fields;
        static constexpr char SOH = '\x01';
        static constexpr char PIPE = '|';

        static std::string get_current_time() {
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::gmtime(&time_t_now), "%Y%m%d-%H:%M:%S.000");
            return ss.str();
        }

        uint8_t calculate_checksum(const std::string& msg) const {
            uint32_t sum = 0;
            for (char c : msg) sum += static_cast<uint8_t>(c);
            return sum % 256;
        }

    public:
        FixMessage() {
            set_field(Tags::BeginString, FIX_VERSION);
            set_field(Tags::SendingTime, get_current_time());
            // should have the sender / target comp id but we don't care about that
        }

        // separate so we can do conversions and stuff in the future!
        void set_quantity(int tag, Quantity value) { set_field(tag, std::to_string(value)); }
        void set_price(int tag, Price value) { set_field(tag, price_to_string(value)); }

        void set_field(int tag, uint value) { fields[tag] = std::to_string(value); }
        void set_field(int tag, const std::string& value) { fields[tag] = value; }
        void set_field(int tag, char value) { fields[tag] = std::string(1, value); }

        // Field getters with type conversion
        std::string get_string(int tag) const {
            auto it = fields.find(tag);
            if (it == fields.end()) throw std::runtime_error("Field not found: " + std::to_string(tag));
            return it->second;
        }

        Price get_price(int tag) const { return string_to_price(get_string(tag)); }
        Quantity get_quantity(int tag) const { return std::stoll(get_string(tag)); }

        int get_int(int tag) const { return std::stoi(get_string(tag)); }
        char get_char(int tag) const { return get_string(tag)[0]; }

        bool has_field(int tag) const { return fields.find(tag) != fields.end(); }

        // Serialize to FIX wire format
        std::string serialize(bool human_readable = false) const {
            const char delimiter = human_readable ? PIPE : SOH;
            std::stringstream body;

            // Build message body (excluding header fields)
            for (const auto& [tag, value] : fields) {
                if (tag != Tags::BeginString && tag != Tags::BodyLength && tag != Tags::CheckSum) {
                    body << tag << "=" << value << delimiter;
                }
            }

            // Calculate body length
            std::string body_str = body.str();
            std::string length_str = std::to_string(body_str.length());

            // Construct full message
            std::stringstream full_msg;
            full_msg << Tags::BeginString << "=" << fields.at(Tags::BeginString) << delimiter
                << Tags::BodyLength << "=" << length_str << delimiter
                << body_str;

            std::string msg = full_msg.str();

            // Add checksum
            std::stringstream final_msg;
            final_msg << msg << Tags::CheckSum << "="
                << std::setfill('0') << std::setw(3)
                << calculate_checksum(msg) << delimiter;

            return final_msg.str();
        }

        // Parse FIX message
        static FixMessage parse(const std::string& msg) {
            FixMessage fix_msg;
            std::stringstream ss(msg);
            std::string field;

            // Handle both SOH and PIPE delimiters
            char delimiter = msg.find(SOH) != std::string::npos ? SOH : PIPE;

            while (std::getline(ss, field, delimiter)) {
                if (field.empty()) continue;

                size_t eq_pos = field.find('=');
                if (eq_pos == std::string::npos)
                    throw std::runtime_error("Invalid FIX field format: " + field);

                int tag = std::stoi(field.substr(0, eq_pos));
                std::string value = field.substr(eq_pos + 1);
                fix_msg.set_field(tag, value);
            }

            return fix_msg;
        }
    };

    // Helper for creating common message types
    class FixMessageFactory {
    private:
        static uint next_seq_num;

    public:
        static FixMessage create_new_order_single(
            const std::string& cl_ord_id,
            const std::string& symbol,
            char side,
            Price price,
            Quantity quantity,
            char ord_type
        ) {
            FixMessage msg;
            msg.set_field(Tags::MsgType, MsgTypes::NewOrderSingle);
            msg.set_field(Tags::MsgSeqNum, next_seq_num++);
            msg.set_field(Tags::ClOrdID, cl_ord_id);
            msg.set_field(Tags::Symbol, symbol);
            msg.set_field(Tags::Side, side);
            msg.set_field(Tags::TransactTime, msg.get_string(Tags::SendingTime));
            msg.set_field(Tags::OrdType, ord_type);
            msg.set_quantity(Tags::OrderQty, quantity);
            if (ord_type == OrderTypes::Limit) msg.set_price(Tags::Price, price);
            return msg;
        }

        static FixMessage create_execution_report(
            const std::string& cl_ord_id,
            const std::string& order_id,
            const std::string& exec_id,
            char exec_type,
            char ord_status,
            const std::string& symbol,
            char side,
            Quantity leaves_qty,
            Quantity cum_qty,
            Price avg_px,
            Price price
        ) {
            FixMessage msg;
            msg.set_field(Tags::MsgType, MsgTypes::ExecutionReport);
            msg.set_field(Tags::MsgSeqNum, next_seq_num++);
            msg.set_field(Tags::OrderID, order_id);
            msg.set_field(Tags::ClOrdID, cl_ord_id);
            msg.set_field(Tags::ExecID, exec_id);
            msg.set_field(Tags::ExecType, exec_type);
            msg.set_field(Tags::OrdStatus, ord_status);
            msg.set_field(Tags::Symbol, symbol);
            msg.set_field(Tags::Side, side);
            msg.set_quantity(Tags::LeavesQty, leaves_qty);
            msg.set_quantity(Tags::CumQty, cum_qty);
            msg.set_price(Tags::AvgPx, avg_px);
            // only required if price was specified in the original order (limit)
            // but we will always include cause y not
            msg.set_price(Tags::Price, price);
            return msg;
        }
    };

    uint FixMessageFactory::next_seq_num = 1;

} // namespace fix

// Example usage:
void fix_example() {
    // Create a new order
    auto new_order = fix::FixMessageFactory::create_new_order_single(
        "ORD123", "APPL", fix::Sides::Buy, 1000000, 100, fix::OrderTypes::Limit
    );

    // Serialize and parse (showing both wire and human readable formats)
    std::string wire_format = new_order.serialize();
    std::string human_format = new_order.serialize(true);

    // Parse back
    auto parsed = fix::FixMessage::parse(wire_format);

    // Access fields
    std::cout << "ClOrdID: " << parsed.get_string(fix::Tags::ClOrdID) << '\n';
    std::cout << "Symbol: " << parsed.get_string(fix::Tags::Symbol) << '\n';
    std::cout << "Side: " << parsed.get_char(fix::Tags::Side) << '\n';
    std::cout << "OrderQty: " << parsed.get_quantity(fix::Tags::OrderQty) << '\n';
    std::cout << "Price: " << fix::price_to_string(parsed.get_price(fix::Tags::Price)) << '\n';
}

int main() {
    fix_example();
    return 0;
}