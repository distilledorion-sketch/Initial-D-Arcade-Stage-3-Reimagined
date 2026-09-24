using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.Rendering;

// Stable recovered item IDs are save values. Zero always means no ornament.
// Geometry is loaded only for the selected item, never for the whole picker.
public static class Idas3OrnamentCatalog
{
    [Serializable] public sealed class Catalog
    {
        public int version;
        public Entry[] ornaments = Array.Empty<Entry>();
        public Entry[] straps = Array.Empty<Entry>();
    }

    [Serializable] public sealed class Entry
    {
        public int id, strapId, swingType, vertices, triangles;
        public string name, sourceName, mesh, sourceModel;
        public Vector3 boundsMin, boundsMax;
        public MaterialData[] materials = Array.Empty<MaterialData>();
        public bool recoveredSocket, attachmentInferred, bindPose;
        public float socketScale = 1f;
    }

    [Serializable] public sealed class MaterialData
    {
        public string name, texture;
        public float alphaCutoff;
        public bool doubleSided, transparent;
        public Color tint = Color.white;
    }

    public sealed class Part
    {
        public Mesh mesh;
        public MaterialData material;
    }

    // The caller owns these generated meshes. Texture/material ownership belongs
    // to the renderer, so preview and gameplay can share resource texture leases.
    public sealed class MeshParts : IDisposable
    {
        public Part[] Parts { get; internal set; } = Array.Empty<Part>();
        public Vector3 BoundsMin { get; internal set; }
        public Vector3 BoundsMax { get; internal set; }

        public void Dispose()
        {
            foreach (var part in Parts)
                if (part?.mesh)
                {
                    if (Application.isPlaying) UnityEngine.Object.Destroy(part.mesh);
                    else UnityEngine.Object.DestroyImmediate(part.mesh);
                }
            Parts = Array.Empty<Part>();
        }
    }

    static readonly List<int> ids = new List<int>();
    static readonly Dictionary<int, Entry> ornaments = new Dictionary<int, Entry>();
    static readonly Dictionary<int, Entry> straps = new Dictionary<int, Entry>();
    static bool loaded;

    static void Load()
    {
        if (loaded) return;
        loaded = true;
        ids.Add(0);
        var asset = Resources.Load<TextAsset>("ArcadeOrnaments/catalog");
        if (!asset) return;
        try
        {
            var catalog = JsonUtility.FromJson<Catalog>(asset.text);
            if (catalog == null || catalog.version != 1) return;
            Array.Sort(catalog.ornaments, (a, b) => a.id.CompareTo(b.id));
            foreach (var entry in catalog.ornaments)
                if (ValidEntry(entry) && !ornaments.ContainsKey(entry.id))
                {
                    ornaments.Add(entry.id, entry);
                    ids.Add(entry.id);
                }
            foreach (var entry in catalog.straps)
                if (ValidEntry(entry) && !straps.ContainsKey(entry.id)) straps.Add(entry.id, entry);
        }
        finally { Resources.UnloadAsset(asset); }
    }

    static bool ValidEntry(Entry entry)
    {
        return entry != null && entry.id > 0 && !string.IsNullOrEmpty(entry.name)
            && !string.IsNullOrEmpty(entry.mesh) && entry.materials != null && entry.materials.Length > 0;
    }

    public static int Count { get { Load(); return ids.Count; } }
    public static int IdAt(int index) { Load(); return index >= 0 && index < ids.Count ? ids[index] : 0; }
    public static int IndexOf(int id) { Load(); return Math.Max(0, ids.IndexOf(id)); }
    public static bool IsValid(int id) { if (id == 0) return true; Load(); return ornaments.ContainsKey(id); }
    public static string Name(int id) { return id == 0 ? "Off" : Get(id)?.name ?? "Off"; }
    public static Entry Get(int id) { Load(); ornaments.TryGetValue(id, out var entry); return entry; }
    public static Entry GetStrap(int id) { Load(); straps.TryGetValue(id, out var entry); return entry; }
    public static MeshParts LoadModel(int id) { return LoadMesh(Get(id)); }
    public static MeshParts LoadStrap(int id) { return LoadMesh(GetStrap(id)); }

    static MeshParts LoadMesh(Entry entry)
    {
        if (entry == null) return null;
        var asset = Resources.Load<TextAsset>(entry.mesh);
        if (!asset) return null;
        var created = new List<Part>();
        try
        {
            using (var stream = new MemoryStream(asset.bytes, false))
            using (var reader = new BinaryReader(stream))
            {
                if (reader.ReadUInt32() != 0x314e524f) throw new InvalidDataException("Invalid ornament mesh header.");
                int count = reader.ReadInt32();
                if (count < 1 || count > 128) throw new InvalidDataException("Invalid ornament mesh part count.");
                for (int n = 0; n < count; n++)
                {
                    int materialIndex = reader.ReadInt32(), vertexCount = reader.ReadInt32(), indexCount = reader.ReadInt32();
                    if (materialIndex < 0 || materialIndex >= entry.materials.Length || vertexCount < 3 || vertexCount > 1000000
                        || indexCount < 3 || indexCount > 3000000 || indexCount % 3 != 0
                        || stream.Length - stream.Position < (long)vertexCount * 32 + (long)indexCount * 4)
                        throw new InvalidDataException("Invalid ornament mesh dimensions.");
                    var positions = new Vector3[vertexCount];
                    var normals = new Vector3[vertexCount];
                    var uv = new Vector2[vertexCount];
                    var indices = new int[indexCount];
                    for (int i = 0; i < vertexCount; i++)
                    {
                        positions[i] = new Vector3(reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle());
                        normals[i] = new Vector3(reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle());
                        uv[i] = new Vector2(reader.ReadSingle(), reader.ReadSingle());
                    }
                    for (int i = 0; i < indexCount; i++)
                    {
                        indices[i] = reader.ReadInt32();
                        if (indices[i] < 0 || indices[i] >= vertexCount) throw new InvalidDataException("Invalid ornament triangle index.");
                    }
                    var mesh = new Mesh { name = entry.name + " / " + n, hideFlags = HideFlags.HideAndDontSave,
                        indexFormat = vertexCount > 65535 ? IndexFormat.UInt32 : IndexFormat.UInt16 };
                    created.Add(new Part { mesh = mesh, material = entry.materials[materialIndex] });
                    mesh.vertices = positions;
                    mesh.normals = normals;
                    mesh.uv = uv;
                    mesh.triangles = indices;
                    mesh.RecalculateBounds();
                }
                if (stream.Position != stream.Length) throw new InvalidDataException("Unexpected ornament mesh data.");
            }
            return new MeshParts { Parts = created.ToArray(), BoundsMin = entry.boundsMin, BoundsMax = entry.boundsMax };
        }
        catch (Exception error)
        {
            new MeshParts { Parts = created.ToArray() }.Dispose();
            Debug.LogError("Could not load ornament " + entry.id + ": " + error.Message);
            return null;
        }
        finally { Resources.UnloadAsset(asset); }
    }
}
