# FileSystem

Async file I/O backed by `uv_fs_*`. All operations dispatch to libuv's threadpool
and call back on the main thread. Callbacks follow the `(err, result)` convention.

## Constructor

```js
const fs = new FileSystem();
```

## Methods

### fs.open(path, callback)

Opens a file and returns a file descriptor.

```js
fs.open("./data.txt", (err, fd) => {
    if (err) return console.log("open failed:", err);
    console.log("fd:", fd);
});
```

### fs.read(fd, length, callback)

Reads up to `length` bytes from an open file descriptor.

```js
fs.read(fd, 1024, (err, data) => {
    if (err) return console.log("read failed:", err);
    console.log(data);
});
```

### fs.close(fd, callback)

Closes an open file descriptor.

```js
fs.close(fd, (err) => {
    if (err) console.log("close failed:", err);
});
```

## Example

```js
const fs = new FileSystem();

fs.open("./README.md", (err, fd) => {
    if (err) return;
    fs.read(fd, 4096, (err, data) => {
        if (!err) console.log(data);
        fs.close(fd, () => {});
    });
});
```
