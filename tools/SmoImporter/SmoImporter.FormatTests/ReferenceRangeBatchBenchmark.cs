using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;

internal static class ReferenceRangeBatchBenchmark
{
    private const int RangeCount = 20;
    private const int RoundCount = 5;
    private static readonly JsonSerializerOptions PrettyJson = new() { WriteIndented = true };

    // A host CaptureRanges microbenchmark, not a whole-import performance claim.
    // One immutable document and one actual loaded trace serve both measured paths.
    public static int Run(string sourcePath, string outputFolder)
    {
        var overall = Stopwatch.StartNew();
        var report = new Dictionary<string, object?>
        {
            ["kind"] = "reference-range-batch-benchmark",
            ["startedUtc"] = DateTimeOffset.UtcNow,
            ["status"] = "failed",
            ["scope"] = "CaptureRanges versus repeated CaptureRange; not whole-import speed",
            ["allocationMetric"] = "GC.GetAllocatedBytesForCurrentThread; synchronous captures only",
            ["budget"] = "55-second cooperative stop leaves reporting time inside a 60-second worker budget"
        };
        int exitCode = 1;
        Directory.CreateDirectory(outputFolder);
        string reportPath = Path.Combine(outputFolder, "report.json");
        if (File.Exists(reportPath))
            throw new IOException("Benchmark evidence already exists: " + reportPath);

        void CheckBudget()
        {
            if (overall.Elapsed.TotalSeconds >= 55)
                throw new TimeoutException("Bounded benchmark stopped before its next capture.");
        }

        try
        {
            sourcePath = Path.GetFullPath(sourcePath);
            long sourceBytes = new FileInfo(sourcePath).Length;
            if (sourceBytes <= 0 || sourceBytes > 64L * 1024 * 1024)
                throw new InvalidDataException("Input exceeds the unchanged 64 MiB host limit.");
            report["sourcePath"] = sourcePath;
            report["sourceBytes"] = sourceBytes;
            var loadWatch = Stopwatch.StartNew();
            var document = SmoDocument.Load(sourcePath);
            if (document.HasErrors)
                throw new InvalidDataException("Source container has error diagnostics.");
            report["sourceSha256"] = Hash(document.Data.Span);
            var loaded = SmoLoadedResources.Get(document);
            if (loaded.LoadIssue is not null || loaded.ReferenceTrace is null)
                throw new InvalidDataException(loaded.ReferenceTraceIssue ?? loaded.LoadIssue ?? "Missing actual reference trace.");
            var trace = loaded.ReferenceTrace;
            report["documentAndActualLoadMilliseconds"] = loadWatch.Elapsed.TotalMilliseconds;
            report["actualLoadCount"] = 1;
            report["sourceObjectCount"] = document.Objects.Count;
            report["traceReferenceCount"] = trace.ReferenceCount;
            report["traceCoveredExtentCount"] = trace.CoveredExtentCount;
            string nativePath = Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll");
            report["nativeDll"] = new { path = nativePath, sha256 = HashFile(nativePath) };
            report["coreAssembly"] = new { path = typeof(SmoDocument).Assembly.Location,
                sha256 = HashFile(typeof(SmoDocument).Assembly.Location) };
            report["benchmarkAssembly"] = new { path = typeof(ReferenceRangeBatchBenchmark).Assembly.Location,
                sha256 = HashFile(typeof(ReferenceRangeBatchBenchmark).Assembly.Location) };

            var requests = new List<(int PhysicalOffset, int Length)>(RangeCount);
            var selection = new List<object>(RangeCount);
            var rejected = new List<object>();
            report["selection"] = selection;
            report["rejectedCandidates"] = rejected;
            report["selectionRule"] = "First 20 successful whole FAT extents sorted by serialized size, physical offset, then object ID; includes each 8-byte object header";
            foreach (var entry in document.Objects.OrderBy(value => value.SerializedSize)
                         .ThenBy(value => value.PhysicalOffset).ThenBy(value => value.Id))
            {
                CheckBudget();
                var request = (PhysicalOffset: checked((int)entry.PhysicalOffset),
                    Length: checked((int)entry.SerializedSize));
                try
                {
                    _ = trace.CaptureRange(document, request.PhysicalOffset, request.Length);
                }
                catch (InvalidDataException error)
                {
                    rejected.Add(new { entry.Id, entry.TypeHash, entry.Name, request.PhysicalOffset,
                        request.Length, reason = error.Message });
                    continue;
                }
                requests.Add(request);
                selection.Add(new { entry.Id, entry.TypeHash, entry.Name, request.PhysicalOffset,
                    request.Length, contentSha256 = Hash(document.Data.Span.Slice(request.PhysicalOffset, request.Length)) });
                if (requests.Count == RangeCount) break;
            }
            if (requests.Count != RangeCount)
                throw new InvalidDataException($"Only {requests.Count} complete covered extents were available.");
            var ranges = requests.ToArray();

            IReadOnlyList<SmoFileReferenceRange> CaptureSingles()
            {
                var result = new SmoFileReferenceRange[ranges.Length];
                for (int index = 0; index < result.Length; index++)
                {
                    var range = ranges[index];
                    result[index] = trace.CaptureRange(document, range.PhysicalOffset, range.Length);
                }
                return Array.AsReadOnly(result);
            }
            IReadOnlyList<SmoFileReferenceRange> CaptureBatch() => trace.CaptureRanges(document, ranges);

            // Compare full exported metadata before any timing. Independent export
            // contexts include the same complete catalog in every comparison.
            CheckBudget();
            var singleProof = CaptureSingles();
            var batchProof = CaptureBatch();
            var equivalence = new List<object>(RangeCount);
            report["equivalence"] = equivalence;
            for (int index = 0; index < ranges.Length; index++)
            {
                var single = singleProof[index].ExportWorkerTransport();
                var batch = batchProof[index].ExportWorkerTransport();
                byte[] singleJson = JsonSerializer.SerializeToUtf8Bytes(single);
                byte[] batchJson = JsonSerializer.SerializeToUtf8Bytes(batch);
                if (!singleJson.AsSpan().SequenceEqual(batchJson))
                    throw new InvalidDataException($"Batch metadata differs from single capture at range {index}.");
                equivalence.Add(new
                {
                    index, exactJsonMatch = true, metadataJsonSha256 = Hash(singleJson),
                    single.ContentSha256, single.CatalogSha256,
                    sites = single.Sites.Length, sitesJsonSha256 = Hash(JsonSerializer.SerializeToUtf8Bytes(single.Sites)),
                    objects = single.Objects.Length, objectsJsonSha256 = Hash(JsonSerializer.SerializeToUtf8Bytes(single.Objects))
                });
            }
            report["exactMetadataMatches"] = equivalence.Count;

            var samples = new List<Sample>(RoundCount * 2);
            report["samples"] = samples;
            Sample Measure(string mode, int round, int repetitions, Func<IReadOnlyList<SmoFileReferenceRange>> capture)
            {
                int generation0 = GC.CollectionCount(0), generation1 = GC.CollectionCount(1), generation2 = GC.CollectionCount(2);
                long allocated = GC.GetAllocatedBytesForCurrentThread();
                long started = Stopwatch.GetTimestamp();
                for (int repetition = 0; repetition < repetitions; repetition++)
                {
                    CheckBudget();
                    GC.KeepAlive(capture());
                }
                long elapsed = Stopwatch.GetTimestamp() - started;
                long allocatedBytes = GC.GetAllocatedBytesForCurrentThread() - allocated;
                return new(mode, round, repetitions, elapsed, elapsed * 1000d / Stopwatch.Frequency,
                    allocatedBytes, GC.CollectionCount(0) - generation0, GC.CollectionCount(1) - generation1,
                    GC.CollectionCount(2) - generation2);
            }

            // Exactly one additional warmup per path; proof/selection are untimed setup.
            var singleWarmup = Measure("single", -1, 1, CaptureSingles);
            var batchWarmup = Measure("batch", -1, 1, CaptureBatch);
            report["warmups"] = new[] { singleWarmup, batchWarmup };
            int repetitions = 10;
            double estimatedTenRepetitionSeconds = (singleWarmup.Milliseconds + batchWarmup.Milliseconds)
                * RoundCount * repetitions / 1000d;
            if (estimatedTenRepetitionSeconds * 1.5 > 50 - overall.Elapsed.TotalSeconds)
                repetitions = 5;
            report["repetitionsPerRound"] = repetitions;
            report["roundCount"] = RoundCount;
            report["timingIncludes"] = "Captures and returned collections only; no load, selection, JSON, SHA reporting or file IO";
            report["structuralSourceValidation"] = new
            {
                rangesPerOperation = ranges.Length,
                repeatedSingleFullFileHashBytes = sourceBytes * ranges.Length,
                batchFullFileHashBytes = sourceBytes,
                sourceValidationHashByteRatio = ranges.Length,
                rangeContentHashBytesPerPath = ranges.Sum(value => (long)value.Length),
                note = "Derived from one ValidateSource call per API call; per-range content hashes and coverage checks remain"
            };
            for (int round = 0; round < RoundCount; round++)
            {
                if ((round & 1) == 0)
                {
                    samples.Add(Measure("single", round, repetitions, CaptureSingles));
                    samples.Add(Measure("batch", round, repetitions, CaptureBatch));
                }
                else
                {
                    samples.Add(Measure("batch", round, repetitions, CaptureBatch));
                    samples.Add(Measure("single", round, repetitions, CaptureSingles));
                }
            }
            var singleSamples = samples.Where(value => value.Mode == "single").ToArray();
            var batchSamples = samples.Where(value => value.Mode == "batch").ToArray();
            double singleMedian = Median(singleSamples.Select(value => value.Milliseconds / value.Repetitions));
            double batchMedian = Median(batchSamples.Select(value => value.Milliseconds / value.Repetitions));
            report["summary"] = new
            {
                singleMedianMillisecondsPer20Ranges = singleMedian,
                batchMedianMillisecondsPer20Ranges = batchMedian,
                observedSpeedup = batchMedian > 0 ? singleMedian / batchMedian : (double?)null,
                singleMedianAllocatedBytesPer20Ranges = Median(singleSamples.Select(value => (double)value.AllocatedBytes / value.Repetitions)),
                batchMedianAllocatedBytesPer20Ranges = Median(batchSamples.Select(value => (double)value.AllocatedBytes / value.Repetitions)),
                performanceThresholdAsserted = false
            };
            report["status"] = "passed";
            GC.KeepAlive(document);
            GC.KeepAlive(loaded);
            exitCode = 0;
        }
        catch (TimeoutException error)
        {
            report["status"] = "bounded";
            report["error"] = error.Message;
            exitCode = 2;
        }
        catch (Exception error)
        {
            report["error"] = error.ToString();
        }
        report["elapsedSecondsBeforeReporting"] = overall.Elapsed.TotalSeconds;
        using (var process = Process.GetCurrentProcess())
        {
            process.Refresh();
            report["processPeakWorkingSetBytes"] = process.PeakWorkingSet64;
            report["processPrivateBytesAtEnd"] = process.PrivateMemorySize64;
        }
        report["managedHeapBytesAtEnd"] = GC.GetTotalMemory(false);
        using (var output = new FileStream(reportPath, FileMode.CreateNew, FileAccess.Write, FileShare.Read))
            JsonSerializer.Serialize(output, report, PrettyJson);
        Console.WriteLine($"Reference range batch benchmark: {report["status"]}; {reportPath}");
        return exitCode;
    }

    private static string Hash(ReadOnlySpan<byte> bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    private static string HashFile(string path)
    {
        using var input = File.OpenRead(path);
        return Convert.ToHexString(SHA256.HashData(input));
    }
    private static double Median(IEnumerable<double> values)
    {
        var sorted = values.OrderBy(value => value).ToArray();
        return sorted[sorted.Length / 2];
    }
    private sealed record Sample(string Mode, int Round, int Repetitions, long StopwatchTicks,
        double Milliseconds, long AllocatedBytes, int Gen0Collections, int Gen1Collections, int Gen2Collections);
}
