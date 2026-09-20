using System;
using System.Runtime.InteropServices;
using System.Threading;
using UnityEngine;

// The main-thread native owner creates the original44100Hz stereo mix. Unity's
// DSP callback only drains a permanent lock-free PCM ring, never EngineAudio.
[DisallowMultipleComponent]
[RequireComponent(typeof(AudioSource))]
public sealed class Idas3UnityAudio : MonoBehaviour
{
    [Serializable, StructLayout(LayoutKind.Sequential, Pack = 8)]
    public struct Statistics
    {
        public uint size, version;
        public ulong producedFrames, consumedFrames, droppedFrames;
        public ulong underrunFrames, primingFrames, discardedFrames, queuedFrames;
    }
    [Serializable] public struct CallbackStatistics
    {
        public long calls, requestedFrames, returnedFrames;
        public int minimumFrames, maximumFrames, lastFrames;
        public int deviceSampleRate, channels;
    }
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Idas3UnityReadAudioDevice([Out] float[] output, int frameCount, int channels, int sampleRate);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)]
    private static extern void Idas3UnitySetAudioRunning(int running);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Idas3UnityGetAudioStatistics(ref Statistics statistics);

    public static Statistics ReadStatistics()
    {
        var result = new Statistics { size = (uint)Marshal.SizeOf<Statistics>() };
        if (result.size != 64 || Idas3UnityGetAudioStatistics(ref result) != 1 || result.version != 1)
            throw new InvalidOperationException("Unity audio statistics ABI mismatch.");
        return result;
    }

    private AudioSource output;
    private AudioClip stream;
    private GameObject ownedListenerObject;
    private int reading, restartRequested, callbackFailed;
    private long callbackCalls, callbackRequested, callbackReturned;
    private int callbackMinimum = int.MaxValue, callbackMaximum, callbackLast;
    private int deviceSampleRate, callbackChannels;
    private bool initialized;
    public bool IsOutputRunning => initialized && Volatile.Read(ref reading) != 0;
    public CallbackStatistics ReadCallbackStatistics() => new CallbackStatistics {
        calls = Interlocked.Read(ref callbackCalls), requestedFrames = Interlocked.Read(ref callbackRequested),
        returnedFrames = Interlocked.Read(ref callbackReturned), minimumFrames = Volatile.Read(ref callbackMinimum),
        maximumFrames = Volatile.Read(ref callbackMaximum), lastFrames = Volatile.Read(ref callbackLast),
        deviceSampleRate = Volatile.Read(ref deviceSampleRate), channels = Volatile.Read(ref callbackChannels)
    };

    // Call once AFTER native scene initialization. No Unity mixer/global sample
    // rate changes. Native consumer interpolation converts the unchanged44100Hz
    // source mix into the actual device rate, without changing its pitch.
    public void Initialize()
    {
        if (initialized) return;
        output = GetComponent<AudioSource>();
        if (FindAnyObjectByType<AudioListener>() == null)
        {
            // Keep the source filter off the listener's GameObject: Unity can
            // otherwise try to attach OnAudioFilterRead to both DSP chains.
            ownedListenerObject = new GameObject("Initial D audio listener");
            ownedListenerObject.transform.SetParent(transform, false);
            ownedListenerObject.AddComponent<AudioListener>();
        }
        output.playOnAwake = false;
        output.loop = true;
        output.spatialBlend = 0;
        output.dopplerLevel = 0;
        output.volume = 1;
        output.pitch = 1;
        output.priority = 0;
        output.bypassEffects = false; // this component IS the source DSP filter.
        output.bypassListenerEffects = true;
        output.bypassReverbZones = true;
        output.ignoreListenerPause = true; // source owner controls pause/mute.
        output.outputAudioMixerGroup = null;
        Volatile.Write(ref callbackFailed, 0);
        Idas3UnitySetAudioRunning(1); // initialize permanent ring before callback.
        Volatile.Write(ref deviceSampleRate, AudioSettings.outputSampleRate);
        // A tiny silent carrier keeps AudioSource playback/lifecycle explicit.
        // Its DSP block is replaced below. A streaming PCMReader requests large
        // read-ahead bursts, unsuitable for a low-latency real-time producer.
        stream = AudioClip.Create("Initial D DSP output carrier", 2048, 2, deviceSampleRate, false);
        output.clip = stream;
        initialized = true;
        AudioSettings.OnAudioConfigurationChanged += AudioConfigurationChanged;
        Volatile.Write(ref reading, 1);
        output.Play();
    }

    private void OnAudioFilterRead(float[] data, int channels)
    {
        if (channels <= 0) { Array.Clear(data, 0, data.Length); return; }
        int frames = data.Length / channels;
        Volatile.Write(ref callbackChannels, channels);
        Interlocked.Increment(ref callbackCalls); Interlocked.Add(ref callbackRequested, frames);
        Volatile.Write(ref callbackLast, frames);
        int observed;
        while (frames > (observed = Volatile.Read(ref callbackMaximum)) && Interlocked.CompareExchange(ref callbackMaximum, frames, observed) != observed) { }
        while (frames < (observed = Volatile.Read(ref callbackMinimum)) && Interlocked.CompareExchange(ref callbackMinimum, frames, observed) != observed) { }
        if (Volatile.Read(ref reading) == 0) { Array.Clear(data, 0, data.Length); return; }
        try
        {
            Interlocked.Add(ref callbackReturned, Idas3UnityReadAudioDevice(data, frames, channels, Volatile.Read(ref deviceSampleRate)));
            for (int i = frames * channels; i < data.Length; ++i) data[i] = 0;
        }
        catch (Exception)
        {
            // Unity API/logging and error-string allocation stay on main thread.
            Array.Clear(data, 0, data.Length);
            Volatile.Write(ref callbackFailed, 1);
            Volatile.Write(ref reading, 0);
        }
    }

    private void AudioConfigurationChanged(bool deviceWasChanged)
    {
        // Device changes may stop a streaming source. Recreate it on main thread.
        if (initialized) { Volatile.Write(ref reading, 0); Volatile.Write(ref restartRequested, 1); }
    }
    private void Update()
    {
        if (Interlocked.Exchange(ref callbackFailed, 0) != 0)
        {
            Debug.LogError("Initial D Unity audio callback failed; output stopped. Check the native plugin and its audio exports.");
            StopOutput();
        }
        if (initialized && Interlocked.Exchange(ref restartRequested, 0) != 0)
        {
            StopOutput();
            Initialize();
        }
    }

    // Call BEFORE native scene shutdown/domain unload. A callback already in
    // progress can only touch the permanent PCM ring, never the destroyed App.
    public void StopOutput()
    {
        Volatile.Write(ref reading, 0);
        AudioSettings.OnAudioConfigurationChanged -= AudioConfigurationChanged;
        if (!initialized && stream == null) return;
        Idas3UnitySetAudioRunning(0);
        if (output != null) { output.Stop(); output.clip = null; }
        if (stream != null) { Destroy(stream); stream = null; }
        initialized = false;
        Volatile.Write(ref restartRequested, 0);
    }
    private void OnDisable() { StopOutput(); }
    private void OnDestroy() { StopOutput(); if (ownedListenerObject != null) Destroy(ownedListenerObject); }
    private void OnApplicationQuit() { StopOutput(); }
}
