# Capy Benchmarks

Reference comparison against Node.js. These are not a claim that Capy is faster —
Node is a 15-year-old production runtime. The goal is to validate architectural
decisions and identify where the design makes a measurable difference.

## Setup

```bash
# Install wrk (macOS)
brew install wrk

# Build Capy (release, no ASan)
cmake --preset macos-dev && cmake --build --preset macos-dev --target capy

# Node version used
node --version
```

## Scenarios

### 1. io_throughput — plain HTTP baseline

Node's home turf. Both servers return a static `"Hello"` string.
If Capy is within range, the core I/O path is sound.

```bash
# Terminal 1
node benchmark/io_throughput/node_server.js

# Terminal 2
./build-macos-dev/capy benchmark/io_throughput/capy_server.js

# Terminal 3 — run each for 30s, 12 threads, 400 connections
wrk -t12 -c400 -d30s http://localhost:3000/   # node
wrk -t12 -c400 -d30s http://localhost:3001/   # capy
```

---

### 2. io_compute — HTTP + JSON per request

Realistic API server workload: parse, compute, serialize on every request.
QuickJS lacks V8's JIT so expect Node to lead here. Gap size is the signal.

```bash
node benchmark/io_compute/node_server.js
./build-macos-dev/capy benchmark/io_compute/capy_server.js

wrk -t12 -c400 -d30s http://localhost:3000/compute
wrk -t12 -c400 -d30s http://localhost:3001/compute
```

---

### 3. cpu_io_concurrent — THE KEY BENCHMARK

5 CPU workers fully saturated. Measure HTTP `/ping` latency while workers run hot.

**What to look for:** Capy's workers run on dedicated OS threads created with
`uv_thread_create` — invisible to libuv's internal threadpool. The event loop
never waits on them. Node's `worker_threads` are also real OS threads, but all
threads (workers + event loop) compete for the same CPU cores.

P99 latency on `/ping` under saturation is the number that matters.

```bash
node benchmark/cpu_io_concurrent/node_server.js
./build-macos-dev/capy benchmark/cpu_io_concurrent/capy_server.js

# Measure ping latency while workers are saturated (they saturate on startup)
wrk -t4 -c50 -d30s http://localhost:3000/ping   # node
wrk -t4 -c50 -d30s http://localhost:3001/ping   # capy
```

---

### 4. memory — RSS footprint under load

QuickJS vs V8 baseline. Not throughput — just how much RAM the process uses
at idle and under sustained request load.

```bash
node benchmark/memory/node_server.js &
NODE_PID=$!
./build-macos-dev/capy benchmark/memory/capy_server.js &
CAPY_PID=$!

# Idle footprint
sleep 2
echo "node RSS (idle):"; ps -o rss= -p $NODE_PID
echo "capy RSS (idle):"; ps -o rss= -p $CAPY_PID

# Under load
wrk -t4 -c100 -d20s http://localhost:3000/ > /dev/null &
wrk -t4 -c100 -d20s http://localhost:3001/ > /dev/null &
sleep 10
echo "node RSS (load):"; ps -o rss= -p $NODE_PID
echo "capy RSS (load):"; ps -o rss= -p $CAPY_PID

kill $NODE_PID $CAPY_PID
```

---

## Methodology

- **Hardware:** record CPU model, core count, RAM
- **OS:** macOS / Linux (specify)
- **Node version:** `node --version`
- **Capy build:** Release, `-O2`, ASan OFF
- **Warmup:** 5s warmup before measuring (wrk does this implicitly with `-d`)
- **Runs:** 3 runs, report median
- **Tool:** `wrk` for throughput/latency, `ps -o rss=` for memory

Scenarios where Node wins are included intentionally.
