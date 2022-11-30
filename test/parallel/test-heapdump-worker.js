// Flags: --expose-internals --experimental-worker
'use strict';
require('../common');
const { validateSnapshotNodes } = require('../common/heap');
const { Worker } = require('worker_threads');

validateSnapshotNodes('Node / Worker', []);
const worker = new Worker('setInterval(() => {}, 100);', { eval: true });
validateSnapshotNodes('Node / Worker', [
  {
    children: [
      { node_name: 'Node / uv_async_t', edge_name: 'thread_exit_async' },
      { node_name: 'Node / Environment', edge_name: 'env' },
      { node_name: 'Node / MessagePort', edge_name: 'parent_port' },
      { node_name: 'Worker', edge_name: 'wrapped' }
    ]
  }
]);
validateSnapshotNodes('Node / MessagePort', [
  {
    children: [
      { node_name: 'Node / MessagePortData', edge_name: 'data' }
    ]
  }
], { loose: true });
worker.terminate();
