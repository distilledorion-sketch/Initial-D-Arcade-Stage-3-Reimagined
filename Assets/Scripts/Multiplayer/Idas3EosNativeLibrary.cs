using System;
#if UNITY_EDITOR
using System.IO;
using System.Runtime.InteropServices;
#endif

namespace Idas3.Multiplayer
{
    // The official SDK uses dynamic bindings in the Editor so the native SDK
    // can be unloaded between play sessions. Players use its normal P/Invoke.
    internal static class Idas3EosNativeLibrary
    {
#if UNITY_EDITOR
        private static IntPtr library;

        public static void Initialize()
        {
            if (library != IntPtr.Zero) return;
            var package = UnityEditor.PackageManager.PackageInfo.FindForAssetPath("Packages/com.initiald.eos-sdk");
            if (package == null || string.IsNullOrEmpty(package.resolvedPath))
                throw new InvalidOperationException("The EOS desktop SDK package is not installed.");

#if UNITY_EDITOR_WIN
            var path = Path.Combine(package.resolvedPath, "Runtime", "Windows", "x64", "EOSSDK-Win64-Shipping.dll");
#elif UNITY_EDITOR_LINUX
            var path = Path.Combine(package.resolvedPath, "Runtime", "Linux", "libEOSSDK-Linux-Shipping.so");
#else
            throw new PlatformNotSupportedException("EOS online currently supports the Windows and Linux x64 editors.");
#endif
#if UNITY_EDITOR_WIN || UNITY_EDITOR_LINUX
            if (IntPtr.Size != 8 || !File.Exists(path))
                throw new InvalidOperationException("The EOS x64 runtime could not be found for this editor.");
            var loaded = Load(path);
            try
            {
                Epic.OnlineServices.Bindings.Hook(loaded, Resolve);
#if UNITY_EDITOR_WIN
                Epic.OnlineServices.WindowsBindings.Hook(loaded, Resolve);
#endif
                library = loaded;
            }
            catch
            {
#if UNITY_EDITOR_WIN
                Epic.OnlineServices.WindowsBindings.Unhook();
#endif
                Epic.OnlineServices.Bindings.Unhook();
                Unload(loaded);
                throw;
            }
#endif
        }

        // The owner must release every platform and call SDK Shutdown first.
        public static void Shutdown()
        {
            if (library == IntPtr.Zero) return;
#if UNITY_EDITOR_WIN
            Epic.OnlineServices.WindowsBindings.Unhook();
#endif
            Epic.OnlineServices.Bindings.Unhook();
            var loaded = library;
            library = IntPtr.Zero;
            Unload(loaded);
        }

#if UNITY_EDITOR_WIN
        [DllImport("kernel32", EntryPoint = "LoadLibraryExW", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr LoadLibraryEx(string path, IntPtr reserved, uint flags);
        [DllImport("kernel32", EntryPoint = "GetProcAddress", CharSet = CharSet.Ansi, ExactSpelling = true)]
        private static extern IntPtr GetProcAddress(IntPtr handle, string name);
        [DllImport("kernel32", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool FreeLibrary(IntPtr handle);

        private static IntPtr Load(string path)
        {
            // Search the SDK directory and standard safe Windows directories.
            var handle = LoadLibraryEx(path, IntPtr.Zero, 0x00000100u | 0x00001000u);
            if (handle == IntPtr.Zero)
                throw new InvalidOperationException("The EOS native SDK failed to load (Windows error " + Marshal.GetLastWin32Error() + ").");
            return handle;
        }
        private static IntPtr Resolve(IntPtr handle, string name) { return GetProcAddress(handle, name); }
        private static void Unload(IntPtr handle) { FreeLibrary(handle); }
#elif UNITY_EDITOR_LINUX
        [DllImport("libdl.so.2", EntryPoint = "dlopen")]
        private static extern IntPtr Dlopen(string path, int flags);
        [DllImport("libdl.so.2", EntryPoint = "dlsym")]
        private static extern IntPtr Dlsym(IntPtr handle, string name);
        [DllImport("libdl.so.2", EntryPoint = "dlclose")]
        private static extern int Dlclose(IntPtr handle);
        [DllImport("libdl.so.2", EntryPoint = "dlerror")]
        private static extern IntPtr Dlerror();

        private static IntPtr Load(string path)
        {
            var handle = Dlopen(path, 2); // RTLD_NOW | RTLD_LOCAL
            if (handle == IntPtr.Zero)
                throw new InvalidOperationException("The EOS native SDK failed to load: " + Marshal.PtrToStringAnsi(Dlerror()));
            return handle;
        }
        private static IntPtr Resolve(IntPtr handle, string name) { return Dlsym(handle, name); }
        private static void Unload(IntPtr handle) { Dlclose(handle); }
#else
        private static void Unload(IntPtr handle) { }
#endif
#else
        public static void Initialize() { }
        public static void Shutdown() { }
#endif
    }
}
