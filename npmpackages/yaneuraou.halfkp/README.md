# `@mizarjp/yaneuraou.halfkp`

- [YaneuraOu](https://github.com/yaneurao/YaneuraOu) is the World's Strongest Shogi engine(AI player) , WCSC29 1st winner , educational and [USI](http://shogidokoro.starfree.jp/usi.html) compliant engine.
- Evaluation type: NNUE HalfKP
- Evaluation file has built in Suisho5(水匠5, 2021-11) by [たややん＠水匠(将棋AI)](https://twitter.com/tayayan_ts)
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

TypeScript sample code `example/example.halfkp.ts`:

```typescript
import YaneuraOu_HalfKP = require("@mizarjp/yaneuraou.halfkp");
import { YaneuraOuModule } from "@mizarjp/yaneuraou.halfkp/lib/yaneuraou.module";

(async () => {
    // engine
    const yaneuraou: YaneuraOuModule = await YaneuraOu_HalfKP();
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
ts-node example/example.halfkp.ts
```

### JavaScript

JavaScript sample code `example/example.halfkp.js`:

```javascript
#!/usr/bin/env node

const YaneuraOu_HalfKP = require("@mizarjp/yaneuraou.halfkp");

(async () => {
    // engine
    const yaneuraou = await YaneuraOu_HalfKP();
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
node example/example.halfkp.js
```

### Output

Output Example:

```
> usi
< id name Suisho5+YaneuraOu NNUE 7.00 32WASM TOURNAMENT
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
< info depth 1 seldepth 1 score mate 1 nodes 126 nps 126000 time 1 pv 1g1f
< info depth 2 seldepth 2 score cp 98 nodes 1688 nps 844000 time 2 pv 2g2f 8c8d 2f2e
< info depth 3 seldepth 3 score cp 107 nodes 2359 nps 786333 time 3 pv 2g2f 8c8d 2f2e
< info depth 4 seldepth 4 score cp 70 nodes 4540 nps 1135000 time 4 pv 2g2f 8c8d 2f2e 3c3d 7g7f
< info depth 5 seldepth 5 score cp 70 nodes 6885 nps 1377000 time 5 pv 2g2f 3c3d 2f2e 2b3c 7g7f
< info depth 6 seldepth 6 score cp 75 nodes 8708 nps 1451333 time 6 pv 2g2f 8c8d 2f2e 8d8e 9g9f 1c1d
< info depth 7 seldepth 7 score cp 58 nodes 14396 nps 1599555 time 9 pv 7g7f 3c3d 6i7h 8c8d 2g2f 8d8e 2f2e 8e8f
< info depth 8 seldepth 8 score cp 108 nodes 26125 nps 1632812 time 16 pv 2g2f 8c8d 2f2e 4a3b 2e2d 2c2d
< info depth 9 seldepth 10 score cp 85 nodes 44325 nps 1641666 time 27 pv 2g2f 8c8d 2f2e 4a3b 7g7f 8d8e 8h7g 3c3d 7i8h 2b3c
< info depth 10 seldepth 11 score cp 98 nodes 69685 nps 1659166 time 42 pv 7g7f 8c8d 6i7h 8d8e 8h7g 3c3d 7i8h 9c9d 9g9f 7a6b 2g2f
< info depth 11 seldepth 13 score cp 120 nodes 128480 nps 1668571 time 77 pv 2g2f 8c8d 2f2e 8d8e 7g7f 4a3b 8h7g 3c3d 7i8h 2b7g+ 8h7g 3a2b
< info depth 12 seldepth 19 score cp 124 nodes 157639 nps 1659357 time 95 pv 2g2f 8c8d 2f2e 8d8e 7g7f 4a3b 8h7g 3c3d 7i8h 2b7g+ 8h7g 3a2b 2e2d 2c2d
< info depth 13 seldepth 24 score cp 115 nodes 272829 nps 1633706 time 167 pv 2g2f 8c8d 7g7f 4a3b 2f2e 1c1d 8h7g 7a6b 7i8h 6c6d 2e2d 2c2d 2h2d 6b6c 3i4h 3c3d
< info depth 14 seldepth 20 score cp 91 nodes 431601 nps 1610451 time 268 pv 2g2f 8c8d 7g7f 4a3b 2f2e 8d8e 8h7g 3c3d 7i8h 2b7g+ 8h7g 3a2b 5i6h 2b3c 3i3h
< info depth 15 seldepth 19 score cp 88 nodes 646662 nps 1550748 time 417 pv 2g2f 8c8d 7g7f 4a3b 6i7h 8d8e 8h7g 3c3d 7i6h 3a4b 7g2b+ 3b2b 6h7g 6c6d 5i6h 7a6b 3i3h
< info depth 16 seldepth 17 score cp 83 nodes 677062 nps 1549340 time 437 pv 2g2f 8c8d 7g7f 4a3b 6i7h 8d8e 8h7g 3c3d 7i6h 3a4b 7g2b+ 3b2b 6h7g 4b3c 5i6h 7a6b 3i4h
< info depth 17 seldepth 22 score cp 110 nodes 805484 nps 1543072 time 522 pv 2g2f 8c8d 7g7f 4a3b 6i7h 7a6b 2f2e 1c1d 3i4h 6c6d 2e2d 2c2d 2h2d 3c3d 5i6i 8d8e 2d2h
< info depth 18 seldepth 24 score cp 87 nodes 1329192 nps 1503610 time 884 pv 2g2f 8c8d 7g7f 4a3b 6i7h 1c1d 2f2e 7a6b 2e2d 2c2d 2h2d P*2c 2d2h 6c6d 7i6h 6b6c 4i5h 3c3d 8h2b+ 3a2b 6h7g 2b3c 5i6h 5a4b
< info depth 19 seldepth 22 score cp 112 nodes 1485999 nps 1504047 time 988 pv 2g2f 8c8d 7g7f 4a3b 6i7h 1c1d 2f2e 7a6b 2e2d 2c2d 2h2d P*2c 2d2h 6c6d 7i6h 9c9d 8h7g 6b6c 5i6i 6c5d 4g4f 9d9e 3i4h
< info depth 19 seldepth 22 score cp 112 nodes 1506475 nps 1500473 hashfull 9 time 1004 pv 2g2f 8c8d 7g7f 4a3b 6i7h 1c1d 2f2e 7a6b 2e2d 2c2d 2h2d P*2c 2d2h 6c6d 7i6h 9c9d 8h7g 6b6c 5i6i 6c5d 4g4f 9d9e 3i4h
< bestmove 2g2f ponder 8c8d
> position startpos moves 2g2f 8c8d
> go movetime 1000
< info depth 1 seldepth 1 score cp 94 nodes 87 nps 43500 time 2 pv 7g7f
< info depth 2 seldepth 2 score cp 110 nodes 1096 nps 548000 time 2 pv 7g7f 8d8e
< info depth 3 seldepth 3 score cp 112 nodes 1689 nps 844500 time 2 pv 7g7f 8d8e 8h7g
< info depth 4 seldepth 4 score cp 102 nodes 2118 nps 1059000 time 2 pv 7g7f 8d8e 8h7g 3c3d
< info depth 5 seldepth 5 score cp 112 nodes 5358 nps 1339500 time 4 pv 7g7f 8d8e 8h7g 3c3d 7i6h
< info depth 6 seldepth 6 score cp 112 nodes 6838 nps 1367600 time 5 pv 7g7f 8d8e 8h7g 4a3b 7i6h 3c3d
< info depth 7 seldepth 8 score cp 111 nodes 7500 nps 1500000 time 5 pv 7g7f 8d8e 8h7g 3c3d 7i6h 2b7g+ 6h7g 3a2b
< info depth 8 seldepth 8 score cp 108 nodes 9351 nps 1558500 time 6 pv 7g7f 4a3b 6i7h 8d8e 8h7g 3c3d 7i6h 6c6d
< info depth 9 seldepth 10 score cp 108 nodes 10249 nps 1464142 time 7 pv 7g7f 4a3b 6i7h 8d8e 8h7g 3c3d 7i6h 6c6d 2f2e 2b7g+
< info depth 10 seldepth 10 score cp 108 nodes 11403 nps 1629000 time 7 pv 7g7f 4a3b 6i7h 8d8e 8h7g 3c3d 7i6h 6c6d 2f2e 2b7g+
< info depth 11 seldepth 12 score cp 110 nodes 13028 nps 1628500 time 8 pv 7g7f 4a3b 6i7h 8d8e 8h7g 3c3d 7i6h 6c6d 1g1f 7a6b 3i3h 2b7g+
< info depth 12 seldepth 14 score cp 98 nodes 28976 nps 1811000 time 16 pv 7g7f 7a6b 2f2e 4a3b 8h7g 1c1d 7i8h 6c6d 5i6h 6b6c 3g3f 8d8e 3f3e 6c7d
< info depth 13 seldepth 13 score cp 103 nodes 30928 nps 1819294 time 17 pv 7g7f 4a3b 6i7h 1c1d 2f2e 7a6b 2e2d 2c2d 2h2d P*2c 2d2h 6c6d 7i6h
< info depth 14 seldepth 16 score cp 103 nodes 33720 nps 1873333 time 18 pv 7g7f 4a3b 6i7h 1c1d 2f2e 7a6b 2e2d 2c2d 2h2d P*2c 2d2h 6c6d 7i6h 6b6c 6h7g 9c9d
< info depth 15 seldepth 20 score cp 95 nodes 47192 nps 1815076 time 26 pv 7g7f 7a6b 2f2e 4a3b 6i7h 1c1d 2e2d 2c2d 2h2d P*2c 2d2h 6c6d 4i5h 3c3d 8h2b+ 3a2b 7i8h 8d8e 8h7g
< info depth 16 seldepth 20 score cp 106 nodes 77564 nps 1686173 time 46 pv 7g7f 7a6b 2f2e 4a3b 6i7h 1c1d 2e2d 2c2d 2h2d P*2c 2d2h 6c6d 7i6h 6b6c 3i4h 8d8e 6h7g 9c9d 9g9f 3c3d
< info depth 17 seldepth 21 score cp 102 nodes 148647 nps 1633483 time 91 pv 7g7f 7a6b 2f2e 4a3b 6i7h 1c1d 2e2d 2c2d 2h2d P*2c 2d2h 6c6d 7i6h 6b6c 5i6i 3c3d 6h7g 9c9d 9g9f
< info depth 18 seldepth 24 score cp 113 nodes 478733 nps 1539334 time 311 pv 7g7f 7a6b 6i7h 1c1d 2f2e 4a3b 2e2d 2c2d 2h2d P*2c 2d2h 6c6d 7i6h 6b6c 3i4h 8d8e 6h7g 3c3d 5i6i 7c7d
< info depth 19 seldepth 24 score cp 61 nodes 1560121 nps 1577473 time 989 pv 7g7f 8d8e 8h7g 3c3d 7i8h 2b7g+ 8h7g 3a2b 6i7h 2b3c 3i3h 7c7d 4g4f 7a7b 3h4g 7b7c 6g6f 7c6d 4g5f 7d7e 6f6e 7e7f
< info depth 19 seldepth 24 score cp 61 nodes 1584659 nps 1581496 hashfull 6 time 1002 pv 7g7f 8d8e 8h7g 3c3d 7i8h 2b7g+ 8h7g 3a2b 6i7h 2b3c 3i3h 7c7d 4g4f 7a7b 3h4g 7b7c 6g6f 7c6d 4g5f 7d7e 6f6e 7e7f
< bestmove 7g7f ponder 8d8e
{"bestmove":"bestmove 2g2f ponder 8c8d","info":"info depth 19 seldepth 22 score cp 112 nodes 1506475 nps 1500473 hashfull 9 time 1004 pv 2g2f 8c8d 7g7f 4a3b 6i7h 1c1d 2f2e 7a6b 2e2d 2c2d 2h2d P*2c 2d2h 6c6d 7i6h 9c9d 8h7g 6b6c 5i6i 6c5d 4g4f 9d9e 3i4h","position":"position startpos"}
{"bestmove":"bestmove 7g7f ponder 8d8e","info":"info depth 19 seldepth 24 score cp 61 nodes 1584659 nps 1581496 hashfull 6 time 1002 pv 7g7f 8d8e 8h7g 3c3d 7i8h 2b7g+ 8h7g 3a2b 6i7h 2b3c 3i3h 7c7d 4g4f 7a7b 3h4g 7b7c 6g6f 7c6d 4g5f 7d7e 6f6e 7e7f","position":"position startpos moves 2g2f 8c8d"}
```
