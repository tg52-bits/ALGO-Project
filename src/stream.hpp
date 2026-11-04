// Synthetic stream generators + exact ground truth.
#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

inline uint64_t splitmix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
// Map an item/flow id to a scrambled 61-bit key (looks like a hashed 5-tuple / word id).
inline uint64_t make_key(uint64_t id) { return splitmix64(id) & ((1ULL << 61) - 1); }

struct Workload {
    std::string name;        // uniform | zipf | flows
    double param = 0;        // Zipf alpha (0 if n/a)
    uint64_t N = 0;          // stream length
    std::vector<uint64_t> stream;
    std::unordered_map<uint64_t, uint64_t> exact;
    std::vector<std::pair<uint64_t, uint64_t>> sorted;  // (key, count) desc by count
    double exact_mups = 0;       // update speed of exact hash map (M updates/s)
    size_t exact_mem_bytes = 0;  // approximate footprint of exact map
};

class ZipfSampler {
public:
    ZipfSampler(uint64_t U, double alpha) : cdf_(U) {
        double s = 0;
        for (uint64_t i = 0; i < U; ++i) { s += 1.0 / std::pow((double)(i + 1), alpha); cdf_[i] = s; }
        for (auto& v : cdf_) v /= s;
    }
    template <class R> uint64_t operator()(R& rng) {
        double u = std::uniform_real_distribution<double>(0.0, 1.0)(rng);
        return (uint64_t)(std::lower_bound(cdf_.begin(), cdf_.end(), u) - cdf_.begin());
    }
private:
    std::vector<double> cdf_;
};

inline void finalize_workload(Workload& W) {
    auto t0 = std::chrono::steady_clock::now();
    W.exact.reserve(1 << 16);
    for (uint64_t k : W.stream) ++W.exact[k];
    double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    W.exact_mups = (double)W.N / sec / 1e6;
    // node ~ key(8)+val(8)+next(8) + malloc overhead(~16); plus bucket array
    W.exact_mem_bytes = W.exact.size() * 40 + W.exact.bucket_count() * 8;
    W.sorted.assign(W.exact.begin(), W.exact.end());
    std::sort(W.sorted.begin(), W.sorted.end(),
              [](auto& a, auto& b) { return a.second > b.second; });
}

inline Workload gen_uniform(uint64_t N, uint64_t U, uint64_t seed) {
    Workload W; W.name = "uniform"; W.N = N; W.stream.reserve(N);
    std::mt19937_64 rng(seed);
    for (uint64_t i = 0; i < N; ++i) W.stream.push_back(make_key(rng() % U));
    finalize_workload(W); return W;
}

// Word-frequency-like stream: item of rank r has probability ~ 1/r^alpha.
inline Workload gen_zipf(uint64_t N, uint64_t U, double alpha, uint64_t seed) {
    Workload W; W.name = "zipf"; W.param = alpha; W.N = N; W.stream.reserve(N);
    std::mt19937_64 rng(seed);
    ZipfSampler z(U, alpha);
    for (uint64_t i = 0; i < N; ++i) W.stream.push_back(make_key(z(rng)));
    finalize_workload(W); return W;
}

// Packet-flow-like stream: a few "elephant" flows carry most packets, many "mice" carry the rest.
inline Workload gen_flows(uint64_t N, uint64_t seed, uint64_t n_elephants = 200,
                          uint64_t n_mice = 2000000, double elephant_share = 0.6) {
    Workload W; W.name = "flows"; W.N = N; W.stream.reserve(N);
    std::mt19937_64 rng(seed);
    ZipfSampler ele(n_elephants, 1.0);
    std::uniform_real_distribution<double> coin(0.0, 1.0);
    for (uint64_t i = 0; i < N; ++i) {
        if (coin(rng) < elephant_share) W.stream.push_back(make_key(ele(rng)));
        else W.stream.push_back(make_key(n_elephants + rng() % n_mice));
    }
    finalize_workload(W); return W;
}
