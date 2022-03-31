# `@mizarjp/yaneuraou.komoringheights-mate`

- [KomoringHeights](https://github.com/komori-n/KomoringHeights) is a [YaneuraOu](https://github.com/yaneurao/YaneuraOu) based [Tsume-Shogi](https://en.wikipedia.org/wiki/Tsume_shogi) solver using the df-pn+ algorithm. It is being developed with the goal of solving medium to long Tsume-Shogi games at high speed.
- [YaneuraOu](https://github.com/yaneurao/YaneuraOu) is the World's Strongest Shogi engine(AI player) , WCSC29 1st winner , educational and [USI](http://shogidokoro.starfree.jp/usi.html) compliant engine.
- License: GPLv3

This project is based on the following repository.

- https://github.com/komori-n/KomoringHeights
- https://github.com/yaneurao/YaneuraOu
- https://github.com/arashigaoka/YaneuraOu.wasm
- https://github.com/niklasf/stockfish.wasm
- https://github.com/hi-ogawa/Stockfish

## Requirements

### Secure Context (web)

This project needs access to SharedArrayBuffer, which must be placed in a secure context if it is to run on the Web. For this reason, requires these HTTP headers on the top level response:

```
Cross-Origin-Embedder-Policy: require-corp
Cross-Origin-Opener-Policy: same-origin
```

- (en) [SharedArrayBuffer#security_requirements](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer#security_requirements)
  - (ja) [SharedArrayBuffer#セキュリティの要件](https://developer.mozilla.org/ja/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer#%E3%82%BB%E3%82%AD%E3%83%A5%E3%83%AA%E3%83%86%E3%82%A3%E3%81%AE%E8%A6%81%E4%BB%B6)
- (en) [Secure contexts](https://developer.mozilla.org/en-US/docs/Web/Security/Secure_Contexts)
  - (ja) [セキュアコンテキスト](https://developer.mozilla.org/ja/docs/Web/Security/Secure_Contexts)
- (en) [Cross-Origin-Embedder-Policy](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cross-Origin-Embedder-Policy)
  - (ja) [Cross-Origin-Embedder-Policy](https://developer.mozilla.org/ja/docs/Web/HTTP/Headers/Cross-Origin-Embedder-Policy)
- (en) [Cross-Origin-Opener-Policy](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cross-Origin-Opener-Policy)
  - (ja) [Cross-Origin-Opener-Policy](https://developer.mozilla.org/ja/docs/Web/HTTP/Headers/Cross-Origin-Opener-Policy)

### WebAssembly SIMD, Threads and Atomics

This project uses WebAssembly SIMD, Threads and Atomics.

### Browser

- Chrome
- Firefox 

### JavaScript/WebAssembly execution environment

- Node.js 16.10 or later

## Example

### JavaScript

JavaScript sample code [`example/example.komoringheights-mate.js`](./example/example.komoringheights-mate.js):

```javascript
#!/usr/bin/env node
const { writeFileSync } = require("fs");
const { join } = require("path");
const { cpus, freemem } = require("os");
const { isMainThread, Worker } = require("worker_threads");
const { TsumeData } = require("./example.mate.data");

if(isMainThread) {
  const tasks = [];
  const posQueue = Object.entries(TsumeData);
  let notQueueFinish = true;
  const addTask = () => {
    const entry = posQueue.shift();
    if (entry) {
      const [name, position] = entry;
      tasks.push(new Promise((resolve) => {
        const worker = new Worker(
          join(__dirname, "example.komoringheights-mate.worker.js"),
          { workerData: { name, position } }
        );
        worker.on("message", (mes) => {
          if(typeof mes === "object") {
            addTask();
            resolve(mes);
          } else {
            console.log(`${name} ${mes}`);
          }
        });
      }));
    } else {
      if (notQueueFinish) {
        notQueueFinish = false;
        Promise.all(tasks).then((results) => {
          writeFileSync(join(__dirname, "example.result.txt"), Array.from(results.map(e => `${JSON.stringify(e)}\n`)).join(""));
        });
      }
    }
  };
  // number of worker generate : (number of logical cores) or (free memory size / 1.5Gbyte)
  const num_worker_gen = Math.max(Math.min(cpus().length, freemem() / 1572864 | 0), 1);
  console.log(`generate ${num_worker_gen} workers`);
  for (let i = 0; i < num_worker_gen; ++i) {
    addTask();
  }
}
```

JavaScript worker sample code [`example/example.komoringheights-mate.worker.js`](./example/example.komoringheights-mate.worker.js):

```javascript
const KomoringHeights_MATE = require("@mizarjp/yaneuraou.komoringheights-mate");
const { parentPort, workerData } = require("worker_threads");

KomoringHeights_MATE().then(async (yaneuraou) => {
  const name = String(workerData?.name);
  const position = String(workerData?.position);
  // utils
  const wCache = {};
  const rCache = {};
  yaneuraou.addMessageListener((line) => {
    parentPort?.postMessage(`< ${line}`);
    let f = true;
    Object.keys(wCache)
    .filter((v) => line.startsWith(v))
    .forEach((v) => { rCache[v].push(line); wCache[v] = false; f = false; });
    Object.keys(rCache)
    .filter((v) => f && line.startsWith(v))
    .forEach((v) => { rCache[v].push(line); })
  });
  const postMessage = (command) => {
    parentPort?.postMessage(`> ${command}`);
    yaneuraou.postMessage(command);
  };
  const postMessageWait = (command, waitResp, ...gatherResps) => {
    wCache[waitResp] = true;
    rCache[waitResp] = [];
    for (const gatherResp of gatherResps) {
      rCache[gatherResp] = [];
    }
    postMessage(command);
    return new Promise((resolve) => {
      const poll = () => {
        if (wCache[waitResp]) {
          setTimeout(poll, 1);
        } else {
          delete wCache[waitResp];
          const res = { name, position };
          res[waitResp] = rCache[waitResp];
          for (const gatherResp of gatherResps) {
            res[gatherResp] = rCache[gatherResp];
          }
          resolve(res);
        }
      };
      poll();
    });
  };
  // initialize engine
  await postMessageWait("usi", "usiok");
  postMessage("setoption name PvInterval value 8000");
  await postMessageWait("isready", "readyok");
  postMessage(position);
  // mate search
  const bTime = new Date().getTime();
  const res = await postMessageWait("go mate 1000000", "checkmate", "info");
  const eTime = new Date().getTime();
  res.time = eTime - bTime;
  // send results
  parentPort?.postMessage(`spent ${res.time}ms`);
  parentPort?.postMessage(res);
  // terminate engine
  yaneuraou.postMessage("quit");
  yaneuraou.terminate();
});
```

Execute JavaScript:

```
node example/example.komoringheights-mate.js
```
