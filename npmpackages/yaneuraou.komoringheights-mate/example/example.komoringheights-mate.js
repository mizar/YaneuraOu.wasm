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
  // number of worker generate : (number of logical cores) or (free memory size / 3Gbyte)
  const num_worker_gen = Math.max(Math.min(cpus().length, freemem() / 3221225472 | 0), 1);
  console.log(`generate ${num_worker_gen} workers`);
  for (let i = 0; i < num_worker_gen; ++i) {
    addTask();
  }
}
