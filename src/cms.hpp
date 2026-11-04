// Count-Min Sketch (Cormode & Muthukrishnan, 2005) with optional conservative update.
#pragma once
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>
#include <algorithm>
#include <limits>

class CountMinSketch {
public:
    static constexpr uint64_t P = (1ULL << 61) - 1;  // Mersenne prime for 2-universal hashing

    // width w, depth d. Guarantee (standard CMS): est <= true + eps*N with prob >= 1-delta
    // where w = ceil(e/eps), d = ceil(ln(1/delta)).
    CountMinSketch(size_t w, size_t d, uint64_t seed)
        : w_(w), d_(d), table_(w * d, 0), a_(d), b_(d) {
        std::mt19937_64 rng(seed);
        for (size_t i = 0; i < d_; ++i) {
            a_[i] = (rng() % (P - 1)) + 1;  // a in [1, P-1]
            b_[i] = rng() % P;
        }
    }

    static CountMinSketch from_eps_delta(double eps, double delta, uint64_t seed) {
        size_t w = (size_t)std::ceil(std::exp(1.0) / eps);
        size_t d = (size_t)std::ceil(std::log(1.0 / delta));
        return CountMinSketch(w, d, seed);
    }

    // Standard update: add c to one counter in every row.
    inline void update(uint64_t key, uint32_t c = 1) {
        for (size_t i = 0; i < d_; ++i) table_[i * w_ + index(i, key)] += c;
        total_ += c;
    }

    // Conservative update: raise each of the d counters only up to (current min + c).
    // For c = 1 this increments exactly the counters equal to the current minimum.
    inline void update_conservative(uint64_t key, uint32_t c = 1) {
        size_t idx[kMaxDepth];
        uint32_t mn = std::numeric_limits<uint32_t>::max();
        for (size_t i = 0; i < d_; ++i) {
            idx[i] = i * w_ + index(i, key);
            mn = std::min(mn, table_[idx[i]]);
        }
        uint32_t target = mn + c;
        for (size_t i = 0; i < d_; ++i)
            if (table_[idx[i]] < target) table_[idx[i]] = target;
        total_ += c;
    }

    // Point query: minimum over the d counters (never underestimates).
    inline uint32_t query(uint64_t key) const {
        uint32_t mn = std::numeric_limits<uint32_t>::max();
        for (size_t i = 0; i < d_; ++i) mn = std::min(mn, table_[i * w_ + index(i, key)]);
        return mn;
    }

    size_t width() const { return w_; }
    size_t depth() const { return d_; }
    uint64_t total() const { return total_; }
    size_t memory_bytes() const { return table_.size() * sizeof(uint32_t); }

private:
    static constexpr size_t kMaxDepth = 32;

    static inline uint64_t mod_p(__uint128_t x) {
        uint64_t r = (uint64_t)(x & P) + (uint64_t)(x >> 61);
        r = (r & P) + (r >> 61);
        return r >= P ? r - P : r;
    }
    inline size_t index(size_t row, uint64_t key) const {
        return (size_t)(mod_p((__uint128_t)a_[row] * (key & P) + b_[row]) % w_);
    }

    size_t w_, d_;
    uint64_t total_ = 0;
    std::vector<uint32_t> table_;
    std::vector<uint64_t> a_, b_;
};
