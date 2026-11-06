// Read-only, lazily fetched files for the Emscripten filesystem.
//
// The game reads its data with ordinary blocking file I/O (ifstream/fopen), and a race session
// touches ~260 MB out of 2.8 GB. Running the core in a Web Worker lets those reads block on a
// synchronous HTTP Range request, so only what is actually read is downloaded.
//
// Emscripten's own FS.createLazyFile does the same but asks the server for every file's size
// (one blocking HEAD per file), copies byte-by-byte in JS and never evicts. This version takes the
// sizes from a manifest, copies chunks with TypedArray.set, keeps a global LRU byte budget (large
// files are re-fetchable) and lets callers prefetch files asynchronously (parallel, HTTP-cacheable
// whole-file requests) so the blocking path is only a safety net.
//
// Must run in a dedicated worker: synchronous XMLHttpRequest is not allowed on the main thread.

const MIB = 1024 * 1024;

export class RangeFiles {
  /**
   * @param {object} FS the Emscripten FS object
   * @param {object} options
   * @param {string} options.base URL prefix the manifest paths are appended to (ends with '/')
   * @param {number} [options.chunkSize] bytes per range request for files larger than one chunk
   * @param {number} [options.budget] max bytes of chunk data kept in memory
   */
  constructor(FS, { base, chunkSize = 2 * MIB, budget = 384 * MIB } = {}) {
    this.FS = FS;
    this.base = base;
    this.chunkSize = chunkSize;
    this.budget = budget;
    this.used = 0;
    this.lru = new Map();           // "fileIndex:chunk" -> true, oldest first
    this.files = [];                // index -> { url, size, chunks: Map(chunk -> Uint8Array) }
    this.byPath = new Map();        // path inside the mount -> file
    this.stats = { requests: 0, bytes: 0, blockedMs: 0, evictions: 0, prefetched: 0 };
    this.accessLog = [];            // manifest paths in order of first read (used to build prefetch lists)
  }

  /** Create every manifest entry under `mountPoint` (e.g. '/idas'): {"data/a/b.bin": size, ...}. */
  mount(mountPoint, files) {
    const { FS } = this;
    FS.mkdirTree(mountPoint);
    const made = new Set();
    for (const [rel, size] of Object.entries(files)) {
      const slash = rel.lastIndexOf('/');
      const dir = slash < 0 ? mountPoint : `${mountPoint}/${rel.slice(0, slash)}`;
      if (!made.has(dir)) { FS.mkdirTree(dir); made.add(dir); }
      this.#createFile(dir, slash < 0 ? rel : rel.slice(slash + 1), rel, size);
    }
  }

  #createFile(dir, name, rel, size) {
    const { FS } = this;
    const file = { url: this.base + encodeURI(rel), rel, size, chunks: new Map(), index: this.files.length, touched: false };
    this.files.push(file);
    this.byPath.set(rel, file);
    const node = FS.createFile(dir, name, {}, true, false);   // MEMFS node, mode 0444-ish
    node.usedBytes = size;                                      // what stat() and lseek(SEEK_END) report
    const self = this;
    const ops = Object.assign({}, node.stream_ops);
    ops.read = (stream, buffer, offset, length, position) => self.#read(file, buffer, offset, length, position);
    ops.write = () => { throw new FS.ErrnoError(63); };          // EPERM: data is read-only
    ops.mmap = () => { throw new FS.ErrnoError(19); };           // ENODEV: the core reads, it never maps
    node.stream_ops = ops;
    return node;
  }

  // ---- chunk cache ---------------------------------------------------------------------------
  #chunkSpan(file, chunk) {
    // Small files are one whole-file chunk (plain GET: cacheable by the browser, no Range needed).
    if (file.size <= this.chunkSize) return [0, file.size];
    const start = chunk * this.chunkSize;
    return [start, Math.min(file.size, start + this.chunkSize)];
  }
  #chunkIndex(file, position) {
    return file.size <= this.chunkSize ? 0 : Math.floor(position / this.chunkSize);
  }
  #touch(file, chunk) {
    const key = file.index * 1e6 + chunk;                       // < 1e6 chunks per file
    this.lru.delete(key);
    this.lru.set(key, file);
  }
  #install(file, chunk, bytes) {
    file.chunks.set(chunk, bytes);
    this.used += bytes.length;
    this.#touch(file, chunk);
    // Evict least-recently-used chunks (never the one just installed).
    while (this.used > this.budget && this.lru.size > 1) {
      const [key, owner] = this.lru.entries().next().value;
      const c = key - owner.index * 1e6;
      const dropped = owner.chunks.get(c);
      if (owner === file && c === chunk) break;
      this.lru.delete(key);
      if (dropped) { owner.chunks.delete(c); this.used -= dropped.length; ++this.stats.evictions; }
    }
  }
  #fetchBlocking(file, chunk) {
    const [from, to] = this.#chunkSpan(file, chunk);
    const t0 = performance.now();
    const xhr = new XMLHttpRequest();
    xhr.open('GET', file.url, false);
    if (to - from !== file.size) xhr.setRequestHeader('Range', `bytes=${from}-${to - 1}`);
    xhr.responseType = 'arraybuffer';          // allowed for sync XHR in workers
    xhr.send(null);
    this.stats.blockedMs += performance.now() - t0;
    ++this.stats.requests;
    if (!(xhr.status === 200 || xhr.status === 206)) throw new Error(`HTTP ${xhr.status} for ${file.url}`);
    let bytes = new Uint8Array(xhr.response);
    // A server that ignores Range answers 200 with the whole body: take our slice of it.
    if (xhr.status === 200 && bytes.length === file.size && to - from !== file.size) bytes = bytes.subarray(from, to);
    if (bytes.length !== to - from) throw new Error(`short response (${bytes.length}/${to - from}) for ${file.url}`);
    this.stats.bytes += bytes.length;
    return bytes;
  }
  #chunk(file, chunk) {
    let bytes = file.chunks.get(chunk);
    if (!bytes) { bytes = this.#fetchBlocking(file, chunk); this.#install(file, chunk, bytes); }
    else this.#touch(file, chunk);
    return bytes;
  }
  #read(file, buffer, offset, length, position) {
    if (position >= file.size) return 0;
    if (!file.touched) { file.touched = true; this.accessLog.push(file.rel); }
    const n = Math.min(length, file.size - position);
    // `buffer` is the wasm heap (an Int8Array); copy through a Uint8Array view so set() is a memmove.
    const target = new Uint8Array(buffer.buffer, buffer.byteOffset + offset, n);
    let done = 0;
    while (done < n) {
      const pos = position + done;
      const chunk = this.#chunkIndex(file, pos);
      const bytes = this.#chunk(file, chunk);
      const within = pos - this.#chunkSpan(file, chunk)[0];
      const take = Math.min(n - done, bytes.length - within);
      target.set(bytes.subarray(within, within + take), done);
      done += take;
    }
    return n;
  }

  // ---- prefetch ------------------------------------------------------------------------------
  /**
   * Download whole files ahead of use, in parallel, and keep them in the chunk cache so the
   * blocking path never has to. Unknown paths are ignored. Resolves when all are done.
   * @param {string[]} paths manifest-relative paths
   * @param {object} [o] {concurrency, onProgress(doneBytes, totalBytes)}
   */
  async prefetch(paths, { concurrency = 6, onProgress } = {}) {
    const wanted = [];
    let total = 0;
    for (const p of paths) {
      const file = this.byPath.get(p);
      if (file && !this.#cached(file)) { wanted.push(file); total += file.size; }
    }
    let done = 0, next = 0;
    const worker = async () => {
      while (next < wanted.length) {
        const file = wanted[next++];
        const response = await fetch(file.url);
        if (!response.ok) throw new Error(`HTTP ${response.status} for ${file.url}`);
        const all = new Uint8Array(await response.arrayBuffer());
        if (all.length !== file.size) throw new Error(`size mismatch for ${file.url}: ${all.length} != ${file.size}`);
        if (file.size <= this.chunkSize) this.#install(file, 0, all);
        else for (let c = 0; c * this.chunkSize < file.size; ++c) {
          const [from, to] = this.#chunkSpan(file, c);
          this.#install(file, c, all.slice(from, to));       // slice: do not pin the whole buffer
        }
        done += file.size; ++this.stats.prefetched;
        onProgress?.(done, total);
      }
    };
    await Promise.all(Array.from({ length: Math.min(concurrency, wanted.length) }, worker));
  }
  #cached(file) {
    const chunks = file.size <= this.chunkSize ? 1 : Math.ceil(file.size / this.chunkSize);
    return file.chunks.size === chunks;
  }
}
