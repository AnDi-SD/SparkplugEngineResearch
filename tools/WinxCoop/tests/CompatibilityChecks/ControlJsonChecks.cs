using System.Reflection;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace WinxCoop.CompatibilityChecks;

internal static class ControlJsonChecks
{
    private const BindingFlags StaticInternal = BindingFlags.Static | BindingFlags.NonPublic;
    internal static void Run(Assembly actor)
    {
        Type helper = ChecksProgram.Type(actor, "WinxCoop.RuntimeProbe.RuntimeControlJson");
        MethodInfo createOptions = helper.GetMethod("CreateOptions", StaticInternal)!;
        MethodInfo deserialize = helper.GetMethod("Deserialize", StaticInternal)!;
        const string identity = """{"MappingName":"Local\\WinxCoop.fixture","Role":1,"OwnerPid":1,"OwnerCreationFileTime":2,"Nonce":3,"SessionId":4,"ScenarioId":5,"LevelEpoch":1,"Generation":1}""";
        string input = identity.Replace("\"Generation\"", "\"BodyGeneration\"");
        string rebind = """{"Schema":1,"RequestOrdinal":1,"CapabilityNonce":"0123456789ABCDEF0123456789ABCDEF","TargetActorPid":1,"TargetActorCreationFileTime":2,"PreviousSessionId":3,"NextInput":INPUT,"NextWorld":WORLD,"NextInputConnectionToken":6,"NextWorldConnectionToken":7,"IssuedAtMilliseconds":0}"""
            .Replace("INPUT", input).Replace("WORLD", identity);
        Type request = ChecksProgram.Type(actor, "WinxCoop.RuntimeProbe.HostTransportRebindRequest");
        object options = ChecksProgram.Invoke(createOptions, 8)!;
        MethodInfo read = deserialize.MakeGenericMethod(request);
        ChecksProgram.Require(ChecksProgram.Invoke(read, Encoding.UTF8.GetBytes(rebind), options) is not null,
            "Known nested identity record failed to deserialize.");
        Reject(Changed(rebind, value => value["Unexpected"] = true));
        Reject(Changed(rebind, value => value["NextInput"]!.AsObject()["Unexpected"] = 1));
        Reject(Changed(rebind, value => value["NextWorld"]!.AsObject()["NonceTypo"] = 2));
        Reject(Changed(rebind, value => { value["schema"] = value["Schema"]!.GetValue<uint>(); value.Remove("Schema"); }));

        Type command = ChecksProgram.Type(actor, "WinxCoop.RuntimeProbe.ProgressionActorCommand");
        MethodInfo commandRead = deserialize.MakeGenericMethod(command);
        object smallOptions = ChecksProgram.Invoke(createOptions, 4)!;
        byte[] valid = Encoding.UTF8.GetBytes("""{"Version":1,"CapabilityNonce":"0123456789ABCDEF0123456789ABCDEF","LevelEpoch":1,"Generation":1,"Sequence":1,"Operation":0,"Action":"Hold","IssuedAtMilliseconds":0}""");
        ChecksProgram.Require(ChecksProgram.Invoke(commandRead, valid, smallOptions) is not null, "Known progression record rejected.");
        bool rejected = false;
        try { ChecksProgram.Invoke(commandRead, Encoding.UTF8.GetBytes("{\"Unknown\":true}"), smallOptions); }
        catch (JsonException) { rejected = true; }
        ChecksProgram.Require(rejected, "Progression reader accepted an unknown member.");

        void Reject(byte[] bytes)
        {
            bool rejected = false;
            try { ChecksProgram.Invoke(read, bytes, options); }
            catch (JsonException) { rejected = true; }
            ChecksProgram.Require(rejected, "Strict reader admitted an unknown root/nested/case-mismatched member.");
        }
    }

    private static byte[] Changed(string source, Action<JsonObject> change)
    {
        JsonObject value = JsonNode.Parse(source)!.AsObject(); change(value);
        return Encoding.UTF8.GetBytes(value.ToJsonString());
    }

    internal static void FrameworkWrappers(Assembly actor)
    {
        Type helper = ChecksProgram.Type(actor, "WinxCoop.RuntimeProbe.RuntimeFramework");
        MethodInfo ascii = helper.GetMethod("IsAsciiLetterOrDigit", StaticInternal)!;
        foreach (char value in new[] { 'A', 'Z', 'a', 'z', '0', '9', '@', '[', '`', '{', '/', ':', '\u00C0', '\u0661' })
        {
            bool expected = value is >= 'A' and <= 'Z' or >= 'a' and <= 'z' or >= '0' and <= '9';
            ChecksProgram.Require((bool)ChecksProgram.Invoke(ascii, value)! == expected, "ASCII classification broadened the mapping alphabet.");
        }
        using var stream = new MemoryStream(Encoding.ASCII.GetBytes("xxabc")); stream.Position = 2;
        byte[] hash = (byte[])ChecksProgram.Invoke(helper.GetMethod("HashSha256", StaticInternal)!, stream)!;
        ChecksProgram.Require(Convert.ToHexString(hash) == "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD" && stream.Position == 5,
            "Stream hashing changed the current-position or digest contract.");
    }

    internal static void FrameworkShims(Assembly actor)
    {
        Type? helper = actor.GetType("WinxCoop.Compatibility.Net6FrameworkShims");
        if (helper is null)
        {
            ChecksProgram.Require(Environment.Version.Major >= 8, "NET6 assembly is missing its required BCL shims.");
            return; // NET8 deliberately binds to its BCL, without replacement shims.
        }
        MethodInfo order = helper.GetMethod("Order", StaticInternal)!.MakeGenericMethod(typeof(int));
        var sorted = (IEnumerable<int>)ChecksProgram.Invoke(order, new[] { 3, 1, 1, -1 })!;
        ChecksProgram.Require(sorted.SequenceEqual(new[] { -1, 1, 1, 3 }), "Order shim changed default ordering/duplicates.");
        MethodInfo exact = helper.GetMethod("ReadExactly", StaticInternal)!;
        byte[] output = new byte[4];
        using var shortReads = new ShortReadStream(new byte[] { 1, 2, 3, 4 });
        ChecksProgram.Invoke(exact, shortReads, output);
        ChecksProgram.Require(output.SequenceEqual(new byte[] { 1, 2, 3, 4 }), "ReadExactly skipped partial reads.");
        using var ended = new MemoryStream(new byte[] { 1 });
        bool rejected = false;
        try { ChecksProgram.Invoke(exact, ended, new byte[2]); }
        catch (EndOfStreamException) { rejected = true; }
        ChecksProgram.Require(rejected, "ReadExactly accepted an incomplete read.");
    }

    internal static void FrameworkGuards(Assembly core)
    {
        Type helper = ChecksProgram.Type(core, "SmoNativeValidator.Core.FrameworkGuards");
        MethodInfo text = helper.GetMethod("ThrowIfNullOrWhiteSpace", StaticInternal)!;
        MethodInfo positive = helper.GetMethod("ThrowIfNegativeOrZero", StaticInternal)!;
        MethodInfo disposed = helper.GetMethod("ThrowIfDisposed", StaticInternal)!;
        ChecksProgram.Invoke(text, "value", "source");
        ChecksProgram.Invoke(positive, 1, "count");
        ChecksProgram.Invoke(disposed, false, new object());
        CheckFailure(() => ChecksProgram.Invoke(text, null, "source"), typeof(ArgumentNullException), "source", null);
        CheckFailure(() => ChecksProgram.Invoke(text, " \t", "source"), typeof(ArgumentException), "source", null);
        CheckFailure(() => ChecksProgram.Invoke(positive, 0, "count"), typeof(ArgumentOutOfRangeException), "count", 0);
        CheckFailure(() => ChecksProgram.Invoke(positive, -1, "count"), typeof(ArgumentOutOfRangeException), "count", -1);
        object instance = new object();
        bool rejected = false;
        try { ChecksProgram.Invoke(disposed, true, instance); }
        catch (ObjectDisposedException error) { rejected = error.ObjectName == instance.GetType().FullName; }
        ChecksProgram.Require(rejected, "Disposed guard changed its exception/object-name contract.");

        static void CheckFailure(Action action, Type expected, string name, object? actual)
        {
            bool rejected = false;
            try { action(); }
            catch (ArgumentException error)
            {
                rejected = error.GetType() == expected && error.ParamName == name &&
                    (error is not ArgumentOutOfRangeException range || Equals(range.ActualValue, actual));
            }
            ChecksProgram.Require(rejected, "Framework guard changed type, original parameter name or actual value.");
        }
    }

    private sealed class ShortReadStream(byte[] source) : MemoryStream(source)
    {
        public override int Read(byte[] buffer, int offset, int count) => base.Read(buffer, offset, Math.Min(count, 1));
    }
}
