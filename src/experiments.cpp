// Experiment driver: runs sweeps, writes one CSV row per (experiment, config, variant, trial).
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include "cms.hpp"
#include "stream.hpp"

static const double kPhi = 0.001;   // heavy-hitter threshold: count >= phi * N
static const size_t kTopK = 100;

static void write_header(std::ostream& o) {
    o << "experiment,dist,param,N,distinct,w,d,variant,trial,mem_bytes,exact_mem_bytes,eps_N,"
         "mean_abs_err,mean_abs_err_topk,mean_rel_err_topk,max_err,frac_exceed,"
         "hh_precision,hh_recall,update_mups,exact_mups\n";
}

static void evaluate(const std::string& exp, const Workload& W, size_t w, size_t d, int trial,
                     std::ostream& out) {
    const double eps_N = std::exp(1.0) / (double)w * (double)W.N;  // CMS additive error bound
    const double thr = kPhi * (double)W.N;
    uint64_t true_hh = 0;
    for (auto& kv : W.sorted) { if ((double)kv.second >= thr) ++true_hh; else break; }

    for (int variant = 0; variant < 2; ++variant) {  // 0 = standard, 1 = conservative
        CountMinSketch cms(w, d, 1000 + trial);       // same hash functions for both variants
        auto t0 = std::chrono::steady_clock::now();
        if (variant == 0) for (uint64_t k : W.stream) cms.update(k);
        else              for (uint64_t k : W.stream) cms.update_conservative(k);
        double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

        double sum_err = 0, max_err = 0;
        uint64_t exceed = 0, reported = 0, tp = 0;
        for (auto& kv : W.exact) {
            double est = cms.query(kv.first);
            double e = est - (double)kv.second;      // never negative
            sum_err += e; max_err = std::max(max_err, e);
            if (e > eps_N) ++exceed;
            if (est >= thr) { ++reported; if ((double)kv.second >= thr) ++tp; }
        }
        double topk_abs = 0, topk_rel = 0;
        size_t k = std::min(kTopK, W.sorted.size());
        for (size_t i = 0; i < k; ++i) {
            double e = (double)cms.query(W.sorted[i].first) - (double)W.sorted[i].second;
            topk_abs += e; topk_rel += e / (double)W.sorted[i].second;
        }
        double prec = reported ? (double)tp / reported : 1.0;
        double rec = true_hh ? (double)tp / true_hh : 1.0;

        out << exp << ',' << W.name << ',' << W.param << ',' << W.N << ',' << W.exact.size() << ','
            << w << ',' << d << ',' << (variant ? "conservative" : "standard") << ',' << trial << ','
            << cms.memory_bytes() << ',' << W.exact_mem_bytes << ',' << eps_N << ','
            << sum_err / W.exact.size() << ',' << topk_abs / k << ',' << topk_rel / k << ','
            << max_err << ',' << (double)exceed / W.exact.size() << ',' << prec << ',' << rec << ','
            << (double)W.N / sec / 1e6 << ',' << W.exact_mups << '\n';
    }
}

int main(int argc, char** argv) {
    std::string path = "results/results.csv";
    int trials = 5; double scale = 1.0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--out") && i + 1 < argc) path = argv[++i];
        else if (!strcmp(argv[i], "--trials") && i + 1 < argc) trials = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atof(argv[++i]);
        else { std::cerr << "usage: " << argv[0] << " [--out f.csv] [--trials n] [--scale s]\n"; return 1; }
    }
    std::ofstream out(path);
    write_header(out);
    const uint64_t N = (uint64_t)(5e6 * scale), U = 1000000;

    for (int t = 0; t < trials; ++t) {
        std::cerr << "trial " << t + 1 << "/" << trials << "\n";
        // A) accuracy & memory vs sketch width (Zipf 1.1), d = 4
        { Workload W = gen_zipf(N, U, 1.1, 42 + t);
          for (size_t w = 256; w <= 65536; w *= 2) evaluate("width", W, w, 4, t, out);
          // E) depth at fixed width (throughput / accuracy vs d)
          for (size_t d : {1, 2, 3, 4, 6, 8}) evaluate("depth_fixed_w", W, 4096, d, t, out);
          // C) depth at fixed memory budget: w*d = 16384
          for (size_t d : {1, 2, 3, 4, 6, 8}) evaluate("depth_fixed_mem", W, 16384 / d, d, t, out); }
        // B) data distributions at fixed sketch (w=4096, d=4)
        { Workload W = gen_uniform(N, U, 7 + t); evaluate("dist", W, 4096, 4, t, out); }
        for (double a : {0.6, 0.8, 1.0, 1.2, 1.5}) {
            Workload W = gen_zipf(N, U, a, 100 + t); evaluate("dist", W, 4096, 4, t, out); }
        { Workload W = gen_flows(N, 200 + t); evaluate("dist", W, 4096, 4, t, out); }
        // D) stream length at fixed sketch (w=4096, d=4)
        for (double n : {1e5, 3e5, 1e6, 3e6, 1e7}) {
            Workload W = gen_zipf((uint64_t)(n * scale), U, 1.1, 300 + t);
            evaluate("streamlen", W, 4096, 4, t, out); }
    }
    std::cerr << "wrote " << path << "\n";
}
