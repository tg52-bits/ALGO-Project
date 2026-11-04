# Count-Min Sketch for Heavy Hitters (P21)

Standard Count-Min Sketch (CMS) vs. conservative-update CMS (CU) in C++17, with exact-count
ground truth, synthetic high-velocity streams, and Python/Matplotlib analysis.

## Layout
```
src/cms.hpp          CountMinSketch: update(), update_conservative(), query(), 2-universal hashing (mod 2^61-1)
src/stream.hpp       generators (uniform, Zipf = word frequencies, elephant/mice = packet flows) + exact map
src/experiments.cpp  experiment sweeps -> results/results.csv
src/tests.cpp        correctness checks (no underestimates, CU <= CMS, eps*N bound holds w.h.p.)
scripts/plot.py      figures + summary table
```

## Run
```
make test      # correctness tests
make quick     # ~1 min smoke run (2 trials, 20% stream sizes)
make run       # full run (5 trials, N = 5M)
make plots     # PNG figures in results/
```

## Experiments (all report mean +/- std over trials)
| id | sweep | fixed | question |
|----|-------|-------|----------|
| width | w = 256 ... 65536 | d=4, Zipf 1.1, N=5M | error / memory / HH precision vs width |
| dist | uniform, Zipf 0.6-1.5, flows | w=4096, d=4 | effect of skew |
| depth_fixed_mem | d = 1..8 | w*d = 16384 | best depth/width split |
| depth_fixed_w | d = 1..8 | w = 4096 | throughput cost of depth |
| streamlen | N = 1e5 ... 1e7 | w=4096, d=4 | error grows ~ N/w |

Metrics: mean abs. error over all distinct items, mean abs./relative error on top-100 items, max error,
fraction of items whose error exceeds eps*N (eps = e/w), heavy-hitter precision/recall (threshold phi*N, phi = 0.001),
update throughput (M updates/s) for both sketches and an exact `unordered_map`, memory (sketch = w*d*4 bytes;
exact map is an estimate: ~40 B/entry + bucket array).

## Theory notes for the report
* Parameters: w = ceil(e/eps), d = ceil(ln(1/delta)); space O((1/eps) ln(1/delta)) counters.
* Update/query time O(d). Guarantee: true <= est <= true + eps*N with prob >= 1 - delta.
* CMS never underestimates, so heavy-hitter **recall is 1**; false positives are what hurts (precision).
* Conservative update keeps the one-sided guarantee and is pointwise <= standard CMS for the same hashes
  (tested), but: it needs a read-before-write (~2x slower here), supports only non-negative updates (no deletions),
  and sketches are not mergeable by simple addition.
* Heavy hitters here are found by querying every distinct observed item (offline evaluation). In a true streaming
  deployment, keep a small top-k heap / candidate set updated when est >= phi * (items seen so far).

## Possible extensions
Count-Sketch comparison, top-k heap for online HH detection, real datasets (e.g., word counts from a text corpus,
CAIDA/pcap flows), 16-bit counters, sliding windows.
