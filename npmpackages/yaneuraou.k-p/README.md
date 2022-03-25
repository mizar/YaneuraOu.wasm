# `@mizarjp/yaneuraou.k-p`

- [YaneuraOu](https://github.com/yaneurao/YaneuraOu) is the World's Strongest Shogi engine(AI player) , WCSC29 1st winner , educational and [USI](http://shogidokoro.starfree.jp/usi.html) compliant engine.
- Evaluation type: NNUE K-P
- Evaluation file has built in SuishoPetite(2021-11) by [たややん＠水匠(将棋AI)](https://twitter.com/tayayan_ts)
- License: GPLv3

This project is based on the following repository.

- https://github.com/yaneurao/YaneuraOu
- https://github.com/arashigaoka/YaneuraOu.wasm
- https://github.com/lichess-org/stockfish.wasm
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

### TypeScript

TypeScript sample code `example/example.k-p.ts`:

```typescript
import YaneuraOu_K_P = require("@mizarjp/yaneuraou.k-p");
import { YaneuraOuModule } from "@mizarjp/yaneuraou.k-p/lib/yaneuraou.module";

(async () => {
    // engine
    const yaneuraou: YaneuraOuModule = await YaneuraOu_K_P();
    // utils
    const wCache: { [index: string]: boolean } = {};
    const rCache: { [index: string]: string } = {};
    yaneuraou.addMessageListener((line: string) => {
        console.log(`< ${line}`);
        Object.keys(rCache)
        .filter((v) => line.startsWith(v))
        .forEach((v) => { rCache[v] = line; })
        Object.keys(wCache)
        .filter((v) => line.startsWith(v))
        .forEach((v) => { rCache[v] = line; wCache[v] = false; });
    });
    const postMessage = (command: string) => {
        console.log(`> ${command}`);
        yaneuraou.postMessage(command);
    };
    const postMessageWait = (command: string, waitResp: string, ...gatherResps: string[]) => {
        wCache[waitResp] = true;
        for (const gatherResp of gatherResps) {
            rCache[gatherResp] = "";
        }
        postMessage(command);
        return new Promise<{[index: string]: string}>((resolve) => {
            const poll = () => {
                if (wCache[waitResp]) {
                    setTimeout(poll, 1);
                } else {
                    delete wCache[waitResp];
                    const res: { [index: string]: string } = {};
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
    postMessage("setoption name USI_Hash value 1024");
    postMessage("setoption name PvInterval value 0");
    postMessage("setoption name Threads value 4");
    await postMessageWait("isready", "readyok");
    // search positions
    const resList = [];
    for (const pos of ["position startpos", "position startpos moves 2g2f 8c8d"]) {
        postMessage(pos);
        const res = await postMessageWait("go movetime 1000", "bestmove", "info");
        res.position = pos;
        resList.push(res);
    }
    // output results
    for (const res of resList) {
        console.log(JSON.stringify(res));
    }
    // terminate engine
    yaneuraou.terminate();
})();
```

Execute TypeScript:

```
ts-node example/example.k-p.ts
```

### JavaScript

JavaScript sample code `example/example.k-p.js`:

```javascript
#!/usr/bin/env node

const YaneuraOu_K_P = require("@mizarjp/yaneuraou.k-p");

(async () => {
    // engine
    const yaneuraou = await YaneuraOu_K_P();
    // utils
    const wCache = {};
    const rCache = {};
    yaneuraou.addMessageListener((line) => {
        console.log(`< ${line}`);
        Object.keys(rCache)
        .filter((v) => line.startsWith(v))
        .forEach((v) => { rCache[v] = line; })
        Object.keys(wCache)
        .filter((v) => line.startsWith(v))
        .forEach((v) => { rCache[v] = line; wCache[v] = false; });
    });
    const postMessage = (command) => {
        console.log(`> ${command}`);
        yaneuraou.postMessage(command);
    };
    const postMessageWait = (command, waitResp, ...gatherResps) => {
        wCache[waitResp] = true;
        for (const gatherResp of gatherResps) {
            rCache[gatherResp] = "";
        }
        postMessage(command);
        return new Promise((resolve) => {
            const poll = () => {
                if (wCache[waitResp]) {
                    setTimeout(poll, 1);
                } else {
                    delete wCache[waitResp];
                    const res = {};
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
    postMessage("setoption name USI_Hash value 1024");
    postMessage("setoption name PvInterval value 0");
    postMessage("setoption name Threads value 4");
    await postMessageWait("isready", "readyok");
    // search positions
    const resList = [];
    for (const pos of ["position startpos", "position startpos moves 2g2f 8c8d"]) {
        postMessage(pos);
        const res = await postMessageWait("go movetime 1000", "bestmove", "info");
        res.position = pos;
        resList.push(res);
    }
    // output results
    for (const res of resList) {
        console.log(JSON.stringify(res));
    }
    // terminate engine
    yaneuraou.terminate();
})();
```

Execute JavaScript:

```
node example/example.k-p.js
```

### Output

Output Example:

```
> usi
< id name YaneuraOu NNUE KP256 7.00 32WASM TOURNAMENT
< id author by yaneurao
< option name Threads type spin default 1 min 1 max 32
< option name USI_Hash type spin default 16 min 1 max 1024
< option name USI_Ponder type check default false
< option name Stochastic_Ponder type check default false
< option name MultiPV type spin default 1 min 1 max 800
< option name NetworkDelay type spin default 120 min 0 max 10000
< option name NetworkDelay2 type spin default 1120 min 0 max 10000
< option name MinimumThinkingTime type spin default 2000 min 1000 max 100000
< option name SlowMover type spin default 100 min 1 max 1000
< option name MaxMovesToDraw type spin default 0 min 0 max 100000
< option name DepthLimit type spin default 0 min 0 max 2147483647
< option name NodesLimit type spin default 0 min 0 max 9223372036854775807
< option name EvalDir type string default <internal>
< option name EvalFile type string default nn.bin
< option name WriteDebugLog type string default 
< option name GenerateAllLegalMoves type check default false
< option name EnteringKingRule type combo default CSARule27 var NoEnteringKing var CSARule24 var CSARule24H var CSARule27 var CSARule27H var TryRule
< option name USI_OwnBook type check default true
< option name NarrowBook type check default false
< option name BookMoves type spin default 16 min 0 max 10000
< option name BookIgnoreRate type spin default 0 min 0 max 100
< option name BookFile type combo default no_book var no_book var standard_book.db var yaneura_book1.db var yaneura_book2.db var yaneura_book3.db var yaneura_book4.db var user_book1.db var user_book2.db var user_book3.db var book.bin
< option name BookDir type string default .
< option name BookEvalDiff type spin default 30 min 0 max 99999
< option name BookEvalBlackLimit type spin default 0 min -99999 max 99999
< option name BookEvalWhiteLimit type spin default -140 min -99999 max 99999
< option name BookDepthLimit type spin default 16 min 0 max 99999
< option name BookOnTheFly type check default false
< option name ConsiderBookMoveCount type check default false
< option name BookPvMoves type spin default 8 min 1 max 246
< option name IgnoreBookPly type check default false
< option name SkillLevel type spin default 20 min 0 max 20
< option name DrawValueBlack type spin default -2 min -30000 max 30000
< option name DrawValueWhite type spin default -2 min -30000 max 30000
< option name PvInterval type spin default 300 min 0 max 100000
< option name ResignValue type spin default 99999 min 0 max 99999
< option name ConsiderationMode type check default false
< option name OutputFailLHPV type check default true
< option name FV_SCALE type spin default 24 min 1 max 128
< usiok
> setoption name USI_Hash value 1024
> setoption name PvInterval value 0
> setoption name Threads value 4
> isready
< info string loading eval file : <internal>
< readyok
> position startpos
> go movetime 1000
< info depth 1 seldepth 1 score mate 1 nodes 63 nps 63000 time 1 pv 1g1f
< info depth 2 seldepth 2 score cp 28 nodes 8859 nps 305482 time 29 pv 2g2f 3c3d
< info depth 3 seldepth 3 score cp 33 nodes 11762 nps 336057 time 35 pv 7g7f 4a3b 6i7h
< info depth 4 seldepth 4 score cp 36 nodes 13954 nps 332238 time 42 pv 7g7f 4a3b 6i7h 3c3d
< info depth 5 seldepth 5 score cp 36 nodes 14351 nps 333744 time 43 pv 7g7f 4a3b 6i7h 3c3d 3i4h
< info depth 6 seldepth 6 score cp 25 nodes 15767 nps 315340 time 50 pv 7g7f 4a3b 6i7h 1c1d 3i4h 8c8d
< info depth 7 seldepth 7 score cp 38 nodes 18669 nps 321879 time 58 pv 7g7f 4a3b 7i7h 3c3d 8h7g 2b7g+ 7h7g
< info depth 8 seldepth 8 score cp 21 nodes 37571 nps 375710 time 100 pv 7g7f 4a3b 2g2f 3c3d 3i4h 1c1d 8h2b+ 3a2b
< info depth 9 seldepth 9 score cp 38 nodes 45487 nps 392129 time 116 pv 7g7f 4a3b 2g2f 3c3d 2f2e 1c1d 8h2b+ 3a2b 2e2d
< info depth 10 seldepth 12 score cp 68 nodes 113084 nps 507103 time 223 pv 2g2f 8c8d 7g7f 8d8e 8h7g 3c3d 7i8h 4a3b 3i4h
< info depth 11 seldepth 13 score cp 44 nodes 243357 nps 486714 time 500 pv 7g7f 4a3b 6i7h 3c3d 2g2f 8b5b 3i4h 7a6b 2f2e 2b8h+ 7i8h 3a2b
< info depth 12 seldepth 15 score cp 18 nodes 490432 nps 593743 time 826 pv 7g7f 3c3d 2g2f 8c8d 2f2e 4a3b 2e2d 2c2d 2h2d 8d8e 8h7g 2b7g+ 8i7g B*3c 2d2a+
< info depth 13 seldepth 17 score cp 7 nodes 569135 nps 610659 time 932 pv 7g7f 3c3d 2g2f 8c8d 2f2e 8d8e 6i7h 8e8f 8g8f 8b8f 2e2d 2c2d 2h2d 4a3b 2d2h P*2c 8h2b+
< info depth 14 seldepth 17 score cp 20 nodes 594668 nps 614961 time 967 pv 7g7f 3c3d 2g2f 8c8d 2f2e 4a3b 6i7h 2b3c 8h3c+ 3b3c 6g6f 8d8e 7i8h 8e8f 8g8f
< info depth 14 seldepth 17 score cp 20 nodes 615683 nps 615067 hashfull 1 time 1001 pv 7g7f 3c3d 2g2f 8c8d 2f2e 4a3b 6i7h 2b3c 8h3c+ 3b3c 6g6f 8d8e 7i8h 8e8f 8g8f
< bestmove 7g7f ponder 3c3d
> position startpos moves 2g2f 8c8d
> go movetime 1000
< info depth 1 seldepth 1 score cp 55 nodes 125 nps 62500 time 2 pv 7g7f
< info depth 2 seldepth 2 score cp 44 nodes 4168 nps 1042000 time 4 pv 7g7f 4a3b
< info depth 3 seldepth 3 score cp 44 nodes 4568 nps 913600 time 5 pv 7g7f 4a3b 2f2e
< info depth 4 seldepth 4 score cp 44 nodes 4881 nps 976200 time 5 pv 7g7f 4a3b 2f2e 8d8e
< info depth 5 seldepth 5 score cp 52 nodes 5363 nps 1072600 time 5 pv 7g7f 4a3b 2f2e 8d8e 8h7g
< info depth 6 seldepth 6 score cp 36 nodes 5840 nps 1168000 time 5 pv 7g7f 4a3b 2f2e 3c3d 2e2d 2c2d
< info depth 7 seldepth 7 score cp 36 nodes 6398 nps 1066333 time 6 pv 7g7f 4a3b 2f2e 3c3d 2e2d 2c2d 2h2d
< info depth 8 seldepth 8 score cp 36 nodes 6968 nps 1161333 time 6 pv 7g7f 4a3b 2f2e 3c3d 2e2d 2c2d 2h2d 8d8e
< info depth 9 seldepth 9 score cp 36 nodes 7630 nps 1090000 time 7 pv 7g7f 4a3b 2f2e 3c3d 2e2d 2c2d 2h2d 8d8e 2d2h
< info depth 10 seldepth 13 score cp 36 nodes 9325 nps 1165625 time 8 pv 7g7f 4a3b 2f2e 3c3d 2e2d 2c2d 2h2d 8d8e 2d2h P*2c 8h7g 2b7g+ 8i7g 3a2b
< info depth 11 seldepth 15 score cp 20 nodes 26670 nps 808181 time 33 pv 7g7f 3c3d 2f2e 4a3b 6i7h 2b3c 8h3c+ 3b3c 6g6f 8d8e 7i8h 8e8f
< info depth 12 seldepth 14 score cp 25 nodes 39600 nps 649180 time 61 pv 7g7f 3c3d 2f2e 4a3b 6i7h 2b3c 8h3c+ 3b3c 6g6f 8d8e 4i5h 8e8f 8g8f 8b8f
< info depth 13 seldepth 14 score cp 23 nodes 52401 nps 609313 time 86 pv 7g7f 3c3d 2f2e 4a3b 6i7h 8d8e 8h7g 2b3c 3i4h 7a6b 9g9f 3a2b 4i5h 3c7g+ 8i7g 2b3c
< info depth 14 seldepth 21 score cp 24 nodes 166290 nps 649570 time 256 pv 7g7f 3c3d 2f2e 8d8e 6i7h 4a3b 8h7g 2b3c 3i4h 3a4b 6g6f 1c1d 9g9f 4c4d 4i5h 9c9d
< info depth 15 seldepth 18 score cp 42 nodes 226351 nps 669677 time 338 pv 7g7f 3c3d 6i7h 8d8e 2f2e 4a3b 2e2d 2c2d 2h2d 8e8f 8g8f 8b8f 2d3d 2b8h+ 7i8h 3b3c
< info depth 16 seldepth 17 score cp 27 nodes 283832 nps 679023 time 418 pv 7g7f 3c3d 2f2e 8d8e 6i7h 4a3b 2e2d 2c2d 2h2d P*2c 2d2h 8e8f 8g8f 8b8f P*8g 8f8b 4i5h
< info depth 17 seldepth 21 score cp 24 nodes 400597 nps 705276 time 568 pv 7g7f 3c3d 2f2e 8d8e 6i7h 4a3b 2e2d 2c2d 2h2d P*2c 2d2h 8e8f 8g8f 8b8f P*8g 8f8b 3i4h 7a6b
< info depth 18 seldepth 24 score cp 26 nodes 508120 nps 709664 time 716 pv 7g7f 3c3d 2f2e 8d8e 6i7h 4a3b 2e2d 2c2d 2h2d P*2c 2d2h 8e8f 8g8f 8b8f P*8g 8f8b 3i4h 1c1d P*2d 2c2d 2h2d P*8f 8g8f
< info depth 18 seldepth 24 score cp 26 nodes 717948 nps 715800 hashfull 5 time 1003 pv 7g7f 3c3d 2f2e 8d8e 6i7h 4a3b 2e2d 2c2d 2h2d P*2c 2d2h 8e8f 8g8f 8b8f P*8g 8f8b 3i4h 
1c1d P*2d 2c2d 2h2d P*8f 8g8f
< bestmove 7g7f ponder 3c3d
{"bestmove":"bestmove 7g7f ponder 3c3d","info":"info depth 14 seldepth 17 score cp 20 nodes 615683 nps 615067 hashfull 1 time 1001 pv 7g7f 3c3d 2g2f 8c8d 2f2e 4a3b 6i7h 2b3c 8h3c+ 3b3c 6g6f 8d8e 7i8h 8e8f 8g8f","position":"position startpos"}
{"bestmove":"bestmove 7g7f ponder 3c3d","info":"info depth 18 seldepth 24 score cp 26 nodes 717948 nps 715800 hashfull 5 time 1003 pv 7g7f 3c3d 2f2e 8d8e 6i7h 4a3b 2e2d 2c2d 2h2d P*2c 2d2h 8e8f 8g8f 8b8f P*8g 8f8b 3i4h 1c1d P*2d 2c2d 2h2d P*8f 8g8f","position":"position startpos moves 2g2f 8c8d"}
```
