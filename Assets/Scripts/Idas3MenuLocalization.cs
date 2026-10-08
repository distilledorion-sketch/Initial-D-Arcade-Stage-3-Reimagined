using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text.RegularExpressions;
using System.Runtime.CompilerServices;
using UnityEngine;

// Presentation-only language for remake menus. Never used by the recovered
// arcade artwork, simulation, network messages, song metadata or save names.
public static class Idas3MenuLocalization
{
    public static readonly string[] LanguageNames={"English","日本語","简体中文"};
    public static int Language {get;private set;}
    private static Dictionary<string,string[]> catalog;
    private static Font japanese,chinese;
    private static readonly ConditionalWeakTable<GUIStyle,GUIStyle[]> styles=new ConditionalWeakTable<GUIStyle,GUIStyle[]>();
    private sealed class Pattern {public Regex regex;public string[] translations;public int count;}
    private static readonly List<Pattern> patterns=new List<Pattern>();
    private static readonly Dictionary<string,string[]> dynamicText=new Dictionary<string,string[]>();
    public static void SetLanguage(int language){Language=language>=0&&language<3?language:0;}
    public static int LoadLanguage(string saveRoot)
    {
        try{
            string path=Path.Combine(saveRoot,"game-options.json");
            if(!File.Exists(path))return 0;
            var saved=new Idas3GameOptions.Values{version=0};JsonUtility.FromJsonOverwrite(File.ReadAllText(path),saved);
            return saved.version==1&&saved.customMenuLanguage>=0&&saved.customMenuLanguage<3?saved.customMenuLanguage:0;
        }catch(Exception){return 0;}
    }
    private static void Load()
    {
        if(catalog!=null)return;
        catalog=new Dictionary<string,string[]>(StringComparer.OrdinalIgnoreCase);
        var file=Resources.Load<TextAsset>("Localization/CustomMenus");
        if(file==null){Debug.LogError("Custom menu translations are missing.");return;}
        foreach(string line in file.text.Split('\n')){
            if(string.IsNullOrWhiteSpace(line)||line.StartsWith("#",StringComparison.Ordinal))continue;
            var fields=line.TrimEnd('\r').Split('\t');
            if(fields.Length!=3)throw new InvalidDataException("Invalid custom menu translation row.");
            for(int i=0;i<fields.Length;++i)fields[i]=fields[i].Replace("\\n","\n");
            catalog.Add(fields[0],new[]{fields[1],fields[2]});
            if(fields[0].Contains("{0}")){
                int count=0;
                string expression=Regex.Replace(Regex.Escape(fields[0]),@"\\\{(\d+)}",m=>{count=Math.Max(count,int.Parse(m.Groups[1].Value)+1);return "(?<p"+m.Groups[1].Value+">.*?)";});
                patterns.Add(new Pattern{regex=new Regex("\\A"+expression+"\\z",RegexOptions.CultureInvariant|RegexOptions.Singleline),translations=new[]{fields[1],fields[2]},count=count});
            }
        }
    }
    public static IEnumerable<KeyValuePair<string,string[]>> Entries {get{Load();return catalog;}}
    public static string T(string text)
    {
        if(string.IsNullOrEmpty(text)||Language==0)return text??"";
        Load();if(catalog.TryGetValue(text,out var translated))return translated[Language-1];
        if(dynamicText.TryGetValue(text,out var cached)&&cached[Language-1]!=null)return cached[Language-1];
        string result=text;
        foreach(var pattern in patterns){
            var match=pattern.regex.Match(text);if(!match.Success)continue;
            var args=new object[pattern.count];
            for(int i=0;i<args.Length;++i)args[i]=match.Groups["p"+i].Value;
            result=string.Format(CultureInfo.CurrentCulture,pattern.translations[Language-1],args);break;
        }
        if(result==text&&text.IndexOf('\n')>=0){
            var lines=text.Split('\n');for(int i=0;i<lines.Length;++i)if(catalog.TryGetValue(lines[i],out var line))lines[i]=line[Language-1];
            result=string.Join("\n",lines);
        }
        if(dynamicText.Count>=2048)dynamicText.Clear();
        if(!dynamicText.TryGetValue(text,out cached)){cached=new string[2];dynamicText.Add(text,cached);}cached[Language-1]=result;
        return result;
    }
    // Translate the template before inserting opaque data. A driver called
    // READY or a song called PAUSE must not become a translated UI label.
    public static string Format(string template,params object[] args)=>string.Format(CultureInfo.CurrentCulture,T(template),args);
    public static Font Font {
        get{
            if(japanese==null)japanese=Resources.Load<Font>("Fonts/NotoSansJP-Regular");
            if(Language==2&&chinese==null)chinese=Resources.Load<Font>("Fonts/NotoSansSC-Regular");
            return Language==2&&chinese!=null?chinese:japanese;
        }
    }
    public static GUIStyle Style(GUIStyle source)
    {
        if(!styles.TryGetValue(source,out var localized)){localized=new GUIStyle[3];styles.Add(source,localized);}
        var result=localized[Language]??(localized[Language]=new GUIStyle(source));
        result.font=Font;result.fontSize=source.fontSize;result.wordWrap=source.wordWrap;
        result.fontStyle=Language==0?source.fontStyle:(source.fontStyle==FontStyle.Bold||source.fontStyle==FontStyle.BoldAndItalic?FontStyle.Bold:FontStyle.Normal);
        return result;
    }
    public static void Label(Rect rect,string text,GUIStyle style,bool translate=true)
    {
        var localized=Style(style);string value=translate?T(text):text??"";
        int original=localized.fontSize;
        // Fixed menu rows keep their anchors; fit translated labels rather than
        // allowing glyphs to run into arrows, adjacent columns or buttons.
        if(!localized.wordWrap&&original>0)
            while(localized.fontSize>9&&localized.CalcSize(new GUIContent(value)).x>rect.width) --localized.fontSize;
        try{GUI.Label(rect,value,localized);}finally{localized.fontSize=original;}
    }
}

// Explicitly opt custom GUILayout screens in. No global GUI skin changes can
// leak into the arcade HUD, the original menus or the meter preview.
internal static class Idas3MenuGui
{
    public static void Label(string text,params GUILayoutOption[] options)=>GUILayout.Label(Idas3MenuLocalization.T(text),Idas3MenuLocalization.Style(GUI.skin.label),options);
    public static void Label(string text,GUIStyle style,params GUILayoutOption[] options)=>GUILayout.Label(Idas3MenuLocalization.T(text),Idas3MenuLocalization.Style(style),options);
    public static bool Button(string text,params GUILayoutOption[] options)=>GUILayout.Button(Idas3MenuLocalization.T(text),Idas3MenuLocalization.Style(GUI.skin.button),options);
    public static bool Toggle(bool value,string text,GUIStyle style,params GUILayoutOption[] options)=>GUILayout.Toggle(value,Idas3MenuLocalization.T(text),Idas3MenuLocalization.Style(style),options);
    public static void RawLabel(string text,params GUILayoutOption[] options)=>GUILayout.Label(text,Idas3MenuLocalization.Style(GUI.skin.label),options);
    public static bool RawButton(string text,params GUILayoutOption[] options)=>GUILayout.Button(text,Idas3MenuLocalization.Style(GUI.skin.button),options);
}
