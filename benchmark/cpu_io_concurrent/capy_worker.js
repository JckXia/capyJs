// capy_worker.js — runs inside each JSWorker's isolated QuickJS runtime.
// Must define a global process(inputJson) → JSON string.

function process(inputJson) {
    const { limit } = JSON.parse(inputJson);
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
