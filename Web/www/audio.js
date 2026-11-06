// Main-thread side of the sound output: owns the AudioContext, loads the worklet, and hands the worker a
// MessagePort so PCM goes worker -> worklet without touching the main thread.
export class AudioOutput {
  constructor() {
    this.ctx = null; this.node = null; this.muted = false; this.stats = null;
    try { this.muted = localStorage.getItem('idas-muted') === '1'; } catch { /* storage may be blocked */ }
  }
  /** Create the context (call from startup; browsers may keep it suspended until a user gesture). */
  async start(worker) {
    const Ctx = window.AudioContext || window.webkitAudioContext;
    if (!Ctx || !window.AudioWorkletNode) return false;
    try {
      this.ctx = new Ctx({ sampleRate: 44100, latencyHint: 'interactive' });
      await this.ctx.audioWorklet.addModule(new URL('./audio-worklet.js', import.meta.url));
      this.node = new AudioWorkletNode(this.ctx, 'idas-pcm-player', { numberOfInputs: 0, outputChannelCount: [2] });
      this.node.connect(this.ctx.destination);
      this.node.port.onmessage = (e) => { if (e.data.type === 'stats') { this.stats = e.data; window.__audio = e.data; } };
      const channel = new MessageChannel();
      this.node.port.postMessage({ type: 'pcm-port', port: channel.port1 }, [channel.port1]);
      this.node.port.postMessage({ type: 'mute', value: this.muted });
      worker.postMessage({ type: 'audio-port', port: channel.port2 }, [channel.port2]);
      // Autoplay rules: resume on the first interaction (and right away where allowed).
      const resume = () => { if (this.ctx.state !== 'running') this.ctx.resume().catch(() => {}); };
      resume();
      for (const type of ['pointerdown', 'keydown', 'touchend']) addEventListener(type, resume, { passive: true });
      document.addEventListener('visibilitychange', () => { if (!document.hidden) resume(); });
      return true;
    } catch (e) {
      console.warn('audio unavailable:', e);
      return false;
    }
  }
  setMuted(value) {
    this.muted = value;
    this.node?.port.postMessage({ type: 'mute', value });
    try { localStorage.setItem('idas-muted', value ? '1' : '0'); } catch { /* optional */ }
  }
  get running() { return this.ctx?.state === 'running'; }
}
