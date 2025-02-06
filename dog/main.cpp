#include <algorithm>
#include <iomanip>
#include <vector>
#include <numeric>
#include <thread>
#include <iostream>
// assuming TcpClient.h contains the previous code
#include "TcpClient.h"

// fr fr no cap this is better than std::sort for small n
template<typename T>
void insertion_sort(std::vector<T>& v) {
    for (size_t i = 1; i < v.size(); i++) {
        T key = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > key) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = key;
    }
}

int main() {
    try {
        TcpClient client("127.0.0.1", 8000);

        // respectfully yeet if we can't connect
        if (!client.connect_with_timeout()) {
            std::cerr << "no connection? skill issue tbh\n";
            return 1;
        }

        const int SAMPLES = 10000;  // real ones test with more samples
        std::vector<double> latencies;
        latencies.reserve(SAMPLES);  // no cap this matters for latency


        int successful = 0;

        // actual measurements
        for (int i = 0; i < SAMPLES; i++) {
            std::string msg = "GET / HTTP/1.1\r\nHost: example.com\r\nConnection: keep-alive\r\n\r\n";



            auto t1 = std::chrono::steady_clock::now();
            if (!client.send_data(msg)) {
                // std::cerr << "fr send failed\n";
                continue;
            }
            auto t2 = std::chrono::steady_clock::now();
            std::string resp = client.recv_data();
            auto t3 = std::chrono::steady_clock::now();

            std::cout << "send: " << std::chrono::duration<double, std::micro>(t2 - t1).count() << "μs\n";
            std::cout << "recv: " << std::chrono::duration<double, std::micro>(t3 - t2).count() << "μs\n";

            if (!resp.empty()) {
                double lat = std::chrono::duration<double, std::micro>(t3 - t2).count();
                latencies.push_back(lat);
            }
            successful++;
        }

        // insertion sort >>> for small n, don't @ meyeah
        insertion_sort(latencies);

        size_t n = latencies.size();
        double median = n % 2 ? latencies[n / 2] :
            (latencies[n / 2 - 1] + latencies[n / 2]) / 2;
        double p99 = latencies[static_cast<size_t>(n * 0.99)];
        double mean = std::accumulate(latencies.begin(), latencies.end(), 0.0) / n;

        // flex those stats
        std::cout << std::fixed << std::setprecision(3)
            << "measured " << n << " samples\n"
            << "succeeded " << successful << "\n"
            << "p50: " << median << "μs\n"
            << "p99: " << p99 << "μs\n"
            << "mean: " << mean << "μs\n"
            << "ratio p99/p50: " << p99 / median << "x\n";

    } catch (const std::exception& e) {
        std::cerr << "bruh: " << e.what() << "\n";
        return 1;
    }
    return 0;
}