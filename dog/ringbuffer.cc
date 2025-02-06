#include <atomic>
#include <array>

template<typename T, size_t Size>

class RingBuffer {
private:
    std::array<T, Size> buffer;
    std::atomic<size_t> read_idx;
    std::atomic<size_t> write_idx;
public:
    RingBuffer();
    ~RingBuffer();

    bool push() {
        size_t curr, next;
        do {
            curr = read_idx.load(std::memory_order_relaxed);
            next = (current + 1) % Size;
            if (next == read_idx.load(std::memory_order_acquire))
                return false; // full
        } while (!write_idx.compare_exchange_weak(
            curr,
            next,
            std::memory_order_release,
            std::memory_order_relaxed));
        return true;
    };
    bool pop();
};