// node_worker.js — worker_threads entry point for Node benchmark.
const { parentPort } = require("worker_threads");

function countPrimes(limit) {
    let count = 0;
    for (let n = 2; n <= limit; n++) {
        let prime = true;
        for (let d = 2; d * d <= n; d++) {
            if (n % d === 0) { prime = false; break; }
        }
        if (prime) count++;
    }
    return count;
}

parentPort.on("message", ({ jobId, limit }) => {
    const primes = countPrimes(limit);
    parentPort.postMessage({ jobId, primes, limit });
});
