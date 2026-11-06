// Plays the core's 44.1 kHz stereo float PCM. The worker posts chunks straight to this processor over a
// MessageChannel; a small jitter buffer absorbs the worker's irregular step timing. If the context runs at
// another rate (some browsers ignore the requested 44100) the stream is resampled by linear interpolation.
const SOURCE_RATE = 44100;
const CAPACITY = 32768;              // frames of ring storage
const START_FRAMES = 1800;           // buffered frames before (re)starting playback (~41 ms)
const MAX_FRAMES = 7000;             // above this the oldest audio is dropped to bound latency (~160 ms)

class IdasPcmPlayer extends AudioWorkletProcessor {
  constructor() {
    super();
    this.left = new Float32Array(CAPACITY);
    this.right = new Float32Array(CAPACITY);
    this.read = 0;                   // fractional read position in source frames
    this.write = 0;                  // absolute frames written
    this.step = SOURCE_RATE / sampleRate;
    this.playing = false;
    this.muted = false;
    this.gain = 1;
    this.stats = { played: 0, underruns: 0, dropped: 0, received: 0, rms: 0 };
    this.sumSquares = 0; this.sumCount = 0; this.lastReport = 0;
    this.port.onmessage = (e) => {
      const m = e.data;
      if (m.type === 'pcm-port') m.port.onmessage = (ev) => this.push(ev.data);
      else if (m.type === 'mute') this.muted = !!m.value;
    };
  }
  push(pcm) {
    const frames = pcm.length >> 1;
    for (let i = 0; i < frames; ++i) {
      const at = (this.write + i) % CAPACITY;
      this.left[at] = pcm[i * 2]; this.right[at] = pcm[i * 2 + 1];
    }
    this.write += frames;
    this.stats.received += frames;
    const queued = this.write - this.read;
    if (queued > MAX_FRAMES) {       // fell behind: skip forward rather than add latency
      const drop = Math.floor(queued - START_FRAMES);
      this.read += drop; this.stats.dropped += drop;
    }
  }
  process(inputs, outputs) {
    const out = outputs[0], l = out[0], r = out[1] || out[0], n = l.length;
    const need = Math.ceil(n * this.step) + 2;
    if (!this.playing && this.write - this.read >= START_FRAMES) this.playing = true;
    if (this.playing && this.write - this.read < need) { this.playing = false; ++this.stats.underruns; }
    const target = this.muted ? 0 : 1;
    if (this.playing) {
      for (let i = 0; i < n; ++i) {
        const p = this.read, i0 = Math.floor(p), f = p - i0;
        const a = i0 % CAPACITY, b = (i0 + 1) % CAPACITY;
        this.gain += (target - this.gain) * 0.002;
        const sl = (this.left[a] + (this.left[b] - this.left[a]) * f) * this.gain;
        const sr = (this.right[a] + (this.right[b] - this.right[a]) * f) * this.gain;
        l[i] = sl; if (r !== l) r[i] = sr;
        this.sumSquares += sl * sl + sr * sr; ++this.sumCount;
        this.read += this.step;
      }
      this.stats.played += n;
    } else {
      l.fill(0); if (r !== l) r.fill(0);
    }
    if (currentTime - this.lastReport > 1) {
      this.lastReport = currentTime;
      this.stats.rms = this.sumCount ? Math.sqrt(this.sumSquares / this.sumCount / 2) : 0;
      this.sumSquares = 0; this.sumCount = 0;
      this.port.postMessage({ type: 'stats', ...this.stats, buffered: this.write - this.read, sampleRate });
    }
    return true;
  }
}
registerProcessor('idas-pcm-player', IdasPcmPlayer);
