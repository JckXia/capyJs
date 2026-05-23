# Timers

Standard timer functions, backed by libuv's timer handles. All callbacks fire on the main thread.

## setTimeout(callback, delayMs)

Fires `callback` once after `delayMs` milliseconds.

```js
setTimeout(() => {
    console.log("fired after 1 second");
}, 1000);
```

## setInterval(callback, intervalMs) → timerId

Fires `callback` repeatedly every `intervalMs` milliseconds. Returns a timer ID.

```js
const id = setInterval(() => {
    console.log("tick");
}, 500);
```

## clearInterval(timerId)

Stops a repeating timer.

```js
let count = 0;
const id = setInterval(() => {
    if (++count === 5) clearInterval(id);
}, 200);
```

## Notes

- `setImmediate` is not currently implemented.
- Promises and `async/await` are not currently implemented. All async operations use callbacks.
- Timers that outlive their associated resources (e.g. a `res` object) must be cleared before
  calling `res.end()`. See [server.md](server.md) for lifecycle details.
