# Capy Benchmarks

Reference comparison against Node.js.

**Hardware:** MacBook Air, Apple M2, 8GB RAM, macOS Tahoe 26.2
**Node version:** v24.12.0
**Capy build:** Release, `-O2`, ASan OFF
**Tool:** `wrk`

---

## 1. io_throughput

Plain HTTP, static response, no compute, no workers.

```
wrk -t10 -c10000 -d1m http://localhost:3000/   # node  :3000
wrk -t10 -c10000 -d1m http://localhost:3001/   # capy  :3001
```

| | Req/sec | Avg Latency | Max Latency |
|---|---|---|---|
| Node | 81,440 | 2.95ms | 178.82ms |
| Capy | 229,558 | 1.01ms | 53.95ms |

---

## 2. io_compute

HTTP + JSON parse + serialize per request.

```
wrk -t10 -c10000 -d1m http://localhost:3000/compute   # node
wrk -t10 -c10000 -d1m http://localhost:3001/compute   # capy
```

| | Req/sec | Avg Latency | Max Latency |
|---|---|---|---|
| Node | 71,224 | 3.38ms | 315.80ms |
| Capy | 86,947 | 2.69ms | 110.48ms |

---

## 3. cpu_io_concurrent

Recursive `fib(35)` dispatched to 5 CPU workers per request.

Three configurations. Act 1 uses 500 connections, Act 2 uses 50 connections
(reduced to eliminate queue saturation noise from the native worker numbers).

```
# Act 1
wrk -t5 -c500 -d5m http://localhost:3000/job   # node worker_threads
wrk -t5 -c500 -d5m http://localhost:3001/job   # capy JSWorkerManager

# Act 2
wrk -t5 -c50 -d8m http://localhost:3001/job    # capy NativeFibonacciManager
```

| | Req/sec | Avg Latency | Notes |
|---|---|---|---|
| Node worker_threads | 48.22 | 1.02s | V8 JIT, 14354 timeouts |
| Capy JSWorkerManager | 4.63 | 1.26s | QuickJS, no JIT |
| Capy NativeFibonacciManager | 156.60 | 323.58ms | Native C++, 50 connections |

Note: socket errors on Act 1 runs are expected. fib(35) under 500 connections
saturates the worker queue on both runtimes. The req/sec ratio between
implementations is the signal, not absolute throughput.

---

## 4. memory

RSS under sustained load, 10 threads, 10,000 connections, ~10 minute run.

| | RSS |
|---|---|
| Node | 201,856 KB |
| Capy | 2,192 KB |
