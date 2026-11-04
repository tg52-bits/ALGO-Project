// Sanity tests: no underestimates, CU <= standard pointwise, bound holds w.h.p., exact for tiny streams.
#include <cassert>
#include <cstdio>
#include "cms.hpp"
#include "stream.hpp"
int main() {
    Workload W = gen_zipf(500000, 100000, 1.1, 1);
    CountMinSketch a(1024, 4, 5), b(1024, 4, 5);
    for (auto k : W.stream) { a.update(k); b.update_conservative(k); }
    size_t exceed = 0; double eps_N = std::exp(1.0) / 1024 * W.N;
    for (auto& kv : W.exact) {
        uint32_t ea = a.query(kv.first), eb = b.query(kv.first);
        assert(ea >= kv.second);          // CMS never underestimates
        assert(eb >= kv.second);          // neither does CU
        assert(eb <= ea);                 // CU is never worse (same hashes)
        if (ea - kv.second > eps_N) ++exceed;
    }
    double frac = (double)exceed / W.exact.size();
    assert(frac <= std::exp(-4.0) + 0.01);  // delta = e^-d
    CountMinSketch c(1 << 16, 5, 9);
    c.update(10, 3); c.update(11, 2);
    assert(c.query(10) == 3 && c.query(11) == 2);
    std::printf("all tests passed (fraction exceeding eps*N: %.5f)\n", frac);
}
