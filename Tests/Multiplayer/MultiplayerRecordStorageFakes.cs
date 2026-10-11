using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Newtonsoft.Json;
using Newtonsoft.Json.Serialization;

// Real Newtonsoft serialization with the public-field shape used by these
// Unity DTOs. Native progression alone is replaced by a deterministic delegate.
// The golden v1 fixture separately verifies original serialized field order.
namespace UnityEngine
{
#if !IDAS3_RECORD_STORAGE_INTEGRATION
    internal static class Application { public static string version=>"record-storage-test"; }
#endif
    internal static class JsonUtility
    {
        private sealed class FieldsOnlyResolver : DefaultContractResolver
        {
            protected override IList<JsonProperty> CreateProperties(Type type,MemberSerialization memberSerialization)
            {
                return type.GetFields(BindingFlags.Instance|BindingFlags.Public).OrderBy(field=>field.MetadataToken)
                    .Select(field=>base.CreateProperty(field,MemberSerialization.Fields)).ToList();
            }
        }
        private static readonly JsonSerializerSettings settings=new JsonSerializerSettings{
            ContractResolver=new FieldsOnlyResolver(),ConstructorHandling=ConstructorHandling.AllowNonPublicDefaultConstructor};
        public static string ToJson(object value,bool pretty=false)=>JsonConvert.SerializeObject(value,pretty?Formatting.Indented:Formatting.None,settings);
        public static T FromJson<T>(string value)
        {
            try{return JsonConvert.DeserializeObject<T>(value,settings);}
            catch(JsonException error){throw new ArgumentException("Invalid fixture JSON.",error);}
        }
    }
}
#if !IDAS3_RECORD_STORAGE_INTEGRATION
internal static class Idas3Native { internal static string Error()=>throw new InvalidOperationException("Native progression is not part of storage checks."); }
#endif
