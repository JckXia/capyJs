// worker_process.js — runs inside each JSWorker's isolated QuickJS runtime.
// Must export a global `process(inputJson)` function that returns a JSON string.

function process(inputJson) {
    const input = JSON.parse(inputJson);

    // Simulate CPU-bound work: count primes up to input.limit
    const limit = input.limit || 1000;
    let count = 0;
    for (let n = 2; n <= limit; n++) {
        let prime = true;
        for (let d = 2; d * d <= n; d++) {
            if (n % d === 0) { prime = false; break; }
        }
        if (prime) count++;
    }

    return JSON.stringify({ primes: count, limit });
}
