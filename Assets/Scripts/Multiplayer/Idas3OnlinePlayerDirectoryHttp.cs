using System;
using System.Collections.Generic;
using System.Text;
using System.Threading.Tasks;
using Epic.OnlineServices;
using Epic.OnlineServices.Connect;
using UnityEngine;
using UnityEngine.Networking;

namespace Idas3.Multiplayer
{
    internal sealed class Idas3OnlinePlayerDirectoryHttp : IDisposable
    {
        [Serializable] sealed class Request { public string username, build, targetPlayerId; }
        readonly Dictionary<UnityWebRequest, TaskCompletionSource<bool>> pending = new Dictionary<UnityWebRequest, TaskCompletionSource<bool>>();
        bool disposed;
        internal Idas3OnlinePlayerDirectoryHttp()
        {
            Application.quitting += Dispose;
#if UNITY_EDITOR
            UnityEditor.AssemblyReloadEvents.beforeAssemblyReload += Dispose;
#endif
        }
        internal async Task<Idas3OnlinePlayerDirectory.Reply> Query(string self, string target, string name)
        {
            if (disposed || !Idas3EosPlatform.SignedIn || Idas3EosPlatform.User.ToString() != self)
                throw new InvalidOperationException(Idas3EosSanctions.Unavailable);
            var options = new CopyIdTokenOptions { LocalUserId = Idas3EosPlatform.User };
            var result = Idas3EosPlatform.Handle.GetConnectInterface().CopyIdToken(ref options, out IdToken? token);
            // The managed SDK wrapper releases the native token allocation.
            if (result != Result.Success || !token.HasValue || token.Value.ProductUserId?.ToString() != self ||
                string.IsNullOrEmpty(token.Value.JsonWebToken?.ToString()))
                throw new InvalidOperationException(Idas3EosSanctions.Unavailable);
            var payload = new Request { username = name, build = Application.version, targetPlayerId = target };
            using (var web = new UnityWebRequest(Idas3CommunityTimes.ServiceUrl + "/api/v1/online/player", "POST")) {
                web.uploadHandler = new UploadHandlerRaw(Encoding.UTF8.GetBytes(JsonUtility.ToJson(payload)));
                web.downloadHandler = new DownloadHandlerBuffer();
                web.timeout = 15; web.redirectLimit = 0;
                web.SetRequestHeader("Content-Type", "application/json");
                web.SetRequestHeader("Authorization", "Bearer " + token.Value.JsonWebToken.ToString());
                var complete = new TaskCompletionSource<bool>();
                pending.Add(web, complete);
                try {
                    var operation = web.SendWebRequest();
                    operation.completed += _ => {
                        if (disposed || Idas3EosPlatform.IsStopping) complete.TrySetCanceled();
                        else Idas3EosPlatform.Defer(() => complete.TrySetResult(true));
                    };
                    if (operation.isDone) complete.TrySetResult(true);
                    await complete.Task;
                } finally { pending.Remove(web); }
                if (web.result != UnityWebRequest.Result.Success || web.responseCode != 200 ||
                    web.downloadHandler.data == null || web.downloadHandler.data.Length > 8192)
                    throw new InvalidOperationException(Idas3EosSanctions.Unavailable);
                // Tokens, response bodies and service errors never enter logs.
                return JsonUtility.FromJson<Idas3OnlinePlayerDirectory.Reply>(web.downloadHandler.text);
            }
        }
        public void Dispose()
        {
            if (disposed) return;
            disposed = true;
            Application.quitting -= Dispose;
#if UNITY_EDITOR
            UnityEditor.AssemblyReloadEvents.beforeAssemblyReload -= Dispose;
#endif
            foreach (var item in new List<KeyValuePair<UnityWebRequest, TaskCompletionSource<bool>>>(pending)) {
                item.Key.Abort(); item.Value.TrySetCanceled(); item.Key.Dispose();
            }
            pending.Clear();
        }
    }
}
