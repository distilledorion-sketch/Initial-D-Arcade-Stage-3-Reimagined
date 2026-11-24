using System.Reflection;
public static class Idas3PrecisePedalChecks
{
    public static void Run() => typeof(Idas3ControlBindings).Assembly
        .GetType("Idas3PrecisePedalRuntimeChecks",true)
        .GetMethod("Run",BindingFlags.Public|BindingFlags.Static).Invoke(null,null);
}
