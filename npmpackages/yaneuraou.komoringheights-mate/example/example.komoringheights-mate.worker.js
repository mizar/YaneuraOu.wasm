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
  await postMessageWait("d", "sfen");
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
