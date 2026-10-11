// Preserve existing local build invocations while the implementation and probe
// use provider-neutral names. Both methods build the currently selected provider.
public static class Idas3UnityOnlineBuild
{
    public static void BuildProbe()=>Idas3OnlineBuild.BuildProbe();
    public static void BuildCurrent()=>Idas3OnlineBuild.BuildCurrent();
}
