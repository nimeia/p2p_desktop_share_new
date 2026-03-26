param(
  [ValidateSet("Debug","Release")] [string]$Config = "Debug",
  [string]$BindHost = "127.0.0.1",
  [int]$Port = 9653,
  [int]$HttpRequests = 2000,
  [int]$HttpConcurrency = 64,
  [int]$Rooms = 20,
  [ValidateRange(1,10)] [int]$ViewersPerRoom = 8,
  [int]$SignalRounds = 1,
  [int]$HoldSeconds = 5,
  [int]$TimeoutSeconds = 20,
  [string]$ServerExe = "",
  [string]$WwwRoot = "",
  [string]$AdminWww = ""
)

. (Join-Path $PSScriptRoot "common.ps1")

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Add-Type -ReferencedAssemblies @(
  "System.dll",
  "System.Core.dll",
  "System.Net.Http.dll"
) -TypeDefinition @'
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.WebSockets;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

public sealed class HttpLoadResult {
  public int TotalRequests { get; set; }
  public int SuccessCount { get; set; }
  public int FailureCount { get; set; }
  public long TotalBytes { get; set; }
  public double ElapsedMs { get; set; }
  public double AverageLatencyMs { get; set; }
  public double P95LatencyMs { get; set; }
  public double MaxLatencyMs { get; set; }
  public string StatusCounts { get; set; }
  public string ErrorSummary { get; set; }
  public double AverageCpuPercent { get; set; }
  public double PeakCpuPercent { get; set; }
  public long InitialWorkingSetBytes { get; set; }
  public long FinalWorkingSetBytes { get; set; }
  public long MaxWorkingSetBytes { get; set; }
  public int MaxThreads { get; set; }
  public int MaxHandles { get; set; }
}

public sealed class WebSocketLoadResult {
  public int RequestedRooms { get; set; }
  public int RequestedViewersPerRoom { get; set; }
  public int ConnectedHosts { get; set; }
  public int ConnectedViewers { get; set; }
  public int FailedRooms { get; set; }
  public int SignalSuccessCount { get; set; }
  public int SignalFailureCount { get; set; }
  public double ConnectPhaseMs { get; set; }
  public double TotalPhaseMs { get; set; }
  public int ObservedRoomsAtPeak { get; set; }
  public int ObservedViewersAtPeak { get; set; }
  public bool SettledToZero { get; set; }
  public string ErrorSummary { get; set; }
  public double AverageCpuPercent { get; set; }
  public double PeakCpuPercent { get; set; }
  public long InitialWorkingSetBytes { get; set; }
  public long FinalWorkingSetBytes { get; set; }
  public long MaxWorkingSetBytes { get; set; }
  public int MaxThreads { get; set; }
  public int MaxHandles { get; set; }
}

internal sealed class ProcessMetrics {
  public long InitialWorkingSetBytes = -1;
  public long FinalWorkingSetBytes;
  public long MaxWorkingSetBytes;
  public int MaxThreads;
  public int MaxHandles;
  public double CpuPercentTotal;
  public int CpuPercentSamples;
  public double PeakCpuPercent;
  public long LastSampleTicks;
  public long LastCpuTicks;
  public bool LastCpuSampleReady;
}

internal sealed class ViewerSession {
  public ClientWebSocket Socket;
  public string PeerId;
}

internal sealed class RoomSession {
  public string RoomId;
  public string HostToken;
  public ClientWebSocket HostSocket;
  public List<ViewerSession> Viewers = new List<ViewerSession>();
}

internal sealed class RoomConnectResult {
  public RoomSession Session;
  public string Error;
}

internal sealed class SignalPhaseResult {
  public int SuccessCount;
  public int FailureCount;
  public string Error;
}

internal sealed class TeardownPhaseResult {
  public string Error;
}

internal sealed class StatusSnapshot {
  public int Rooms;
  public int Viewers;
}

public static class LanStressHarness {
  private static readonly Regex StringFieldPattern = new Regex("\"(?<key>[^\"]+)\"\\s*:\\s*\"(?<value>(?:\\\\.|[^\"])*)\"", RegexOptions.Compiled);
  private static readonly Regex NumberFieldPattern = new Regex("\"(?<key>[^\"]+)\"\\s*:\\s*(?<value>\\d+)", RegexOptions.Compiled);

  static LanStressHarness() {
    ServicePointManager.DefaultConnectionLimit = 2048;
    ServicePointManager.Expect100Continue = false;
  }

  public static HttpLoadResult RunHttpBurst(
      string baseUrl,
      string[] targets,
      int totalRequests,
      int concurrency,
      int timeoutSeconds,
      int processId) {
    var result = new HttpLoadResult();
    result.TotalRequests = totalRequests;
    result.StatusCounts = string.Empty;
    result.ErrorSummary = string.Empty;

    var metrics = new ProcessMetrics();
    using (var samplerCts = new CancellationTokenSource()) {
      var samplerTask = SampleProcessLoopAsync(processId, metrics, samplerCts.Token);
      var latencies = new ConcurrentBag<double>();
      var statusCounts = new ConcurrentDictionary<int, int>();
      var errors = new ConcurrentQueue<string>();

      int nextRequest = -1;
      int successCount = 0;
      int failureCount = 0;
      long totalBytes = 0;

      var handler = new HttpClientHandler();
      handler.AutomaticDecompression = DecompressionMethods.GZip | DecompressionMethods.Deflate;

      using (handler)
      using (var client = new HttpClient(handler)) {
        client.BaseAddress = new Uri(baseUrl);
        client.Timeout = TimeSpan.FromSeconds(timeoutSeconds);

        var totalStopwatch = Stopwatch.StartNew();
        var tasks = new List<Task>();
        for (int worker = 0; worker < concurrency; ++worker) {
          tasks.Add(Task.Run(async delegate {
            while (true) {
              int requestIndex = Interlocked.Increment(ref nextRequest);
              if (requestIndex >= totalRequests) {
                break;
              }

              string target = targets[requestIndex % targets.Length];
              var requestStopwatch = Stopwatch.StartNew();
              try {
                using (var response = await client.GetAsync(target).ConfigureAwait(false))
                using (var payload = response.Content) {
                  byte[] bytes = await payload.ReadAsByteArrayAsync().ConfigureAwait(false);
                  requestStopwatch.Stop();
                  latencies.Add(requestStopwatch.Elapsed.TotalMilliseconds);
                  Interlocked.Add(ref totalBytes, bytes.LongLength);

                  int code = (int)response.StatusCode;
                  statusCounts.AddOrUpdate(code, 1, delegate(int key, int current) { return current + 1; });
                  if (code >= 200 && code < 300) {
                    Interlocked.Increment(ref successCount);
                  } else {
                    Interlocked.Increment(ref failureCount);
                    if (errors.Count < 8) {
                      errors.Enqueue("HTTP " + code.ToString() + " @ " + target);
                    }
                  }
                }
              } catch (Exception ex) {
                requestStopwatch.Stop();
                latencies.Add(requestStopwatch.Elapsed.TotalMilliseconds);
                Interlocked.Increment(ref failureCount);
                if (errors.Count < 8) {
                  errors.Enqueue(ex.GetType().Name + ": " + ex.Message + " @ " + target);
                }
              }
            }
          }));
        }

        Task.WaitAll(tasks.ToArray());
        totalStopwatch.Stop();

        result.ElapsedMs = totalStopwatch.Elapsed.TotalMilliseconds;
      }

      samplerCts.Cancel();
      try {
        samplerTask.Wait(2000);
      } catch {
      }

      result.SuccessCount = successCount;
      result.FailureCount = failureCount;
      result.TotalBytes = totalBytes;

      double[] orderedLatencies = latencies.ToArray();
      Array.Sort(orderedLatencies);
      if (orderedLatencies.Length > 0) {
        double totalLatency = 0;
        for (int i = 0; i < orderedLatencies.Length; ++i) {
          totalLatency += orderedLatencies[i];
        }

        result.AverageLatencyMs = totalLatency / orderedLatencies.Length;
        int p95Index = (int)Math.Ceiling(orderedLatencies.Length * 0.95) - 1;
        if (p95Index < 0) {
          p95Index = 0;
        }
        if (p95Index >= orderedLatencies.Length) {
          p95Index = orderedLatencies.Length - 1;
        }
        result.P95LatencyMs = orderedLatencies[p95Index];
        result.MaxLatencyMs = orderedLatencies[orderedLatencies.Length - 1];
      }

      if (statusCounts.Count > 0) {
        result.StatusCounts = string.Join(", ", statusCounts.OrderBy(delegate(KeyValuePair<int, int> pair) { return pair.Key; })
          .Select(delegate(KeyValuePair<int, int> pair) { return pair.Key.ToString() + "=" + pair.Value.ToString(); }));
      }

      if (errors.Count > 0) {
        result.ErrorSummary = string.Join(" | ", errors.ToArray());
      }

      result.AverageCpuPercent = (metrics.CpuPercentSamples > 0) ? (metrics.CpuPercentTotal / metrics.CpuPercentSamples) : 0.0;
      result.PeakCpuPercent = metrics.PeakCpuPercent;
      result.InitialWorkingSetBytes = metrics.InitialWorkingSetBytes;
      result.FinalWorkingSetBytes = metrics.FinalWorkingSetBytes;
      result.MaxWorkingSetBytes = metrics.MaxWorkingSetBytes;
      result.MaxThreads = metrics.MaxThreads;
      result.MaxHandles = metrics.MaxHandles;
    }

    return result;
  }

  public static WebSocketLoadResult RunWebSocketScenario(
      string baseUrl,
      string wsUrl,
      int rooms,
      int viewersPerRoom,
      int signalRounds,
      int holdSeconds,
      int timeoutSeconds,
      int processId) {
    var result = new WebSocketLoadResult();
    result.RequestedRooms = rooms;
    result.RequestedViewersPerRoom = viewersPerRoom;
    result.ErrorSummary = string.Empty;

    var metrics = new ProcessMetrics();
    using (var samplerCts = new CancellationTokenSource()) {
      var samplerTask = SampleProcessLoopAsync(processId, metrics, samplerCts.Token);
      var allStopwatch = Stopwatch.StartNew();
      var errors = new ConcurrentQueue<string>();

      var connectTasks = new List<Task<RoomConnectResult>>();
      var connectStopwatch = Stopwatch.StartNew();
      for (int roomIndex = 0; roomIndex < rooms; ++roomIndex) {
        string roomId = "stress-room-" + roomIndex.ToString("D3");
        string hostToken = "stress-host-" + roomIndex.ToString("D3");
        connectTasks.Add(ConnectRoomSafeAsync(wsUrl, roomId, hostToken, viewersPerRoom, timeoutSeconds));
      }

      var connectResults = Task.WhenAll(connectTasks).GetAwaiter().GetResult();
      connectStopwatch.Stop();
      result.ConnectPhaseMs = connectStopwatch.Elapsed.TotalMilliseconds;

      var sessions = new List<RoomSession>();
      for (int i = 0; i < connectResults.Length; ++i) {
        RoomConnectResult connectResult = connectResults[i];
        if (connectResult.Session != null) {
          sessions.Add(connectResult.Session);
        } else {
          result.FailedRooms += 1;
          if (!string.IsNullOrEmpty(connectResult.Error) && errors.Count < 8) {
            errors.Enqueue(connectResult.Error);
          }
        }
      }

      result.ConnectedHosts = sessions.Count;
      int connectedViewers = 0;
      for (int i = 0; i < sessions.Count; ++i) {
        connectedViewers += sessions[i].Viewers.Count;
      }
      result.ConnectedViewers = connectedViewers;

      if (signalRounds > 0 && sessions.Count > 0) {
        var signalTasks = new List<Task<SignalPhaseResult>>();
        for (int i = 0; i < sessions.Count; ++i) {
          signalTasks.Add(RunSignalPhaseSafeAsync(sessions[i], signalRounds, timeoutSeconds));
        }
        var signalResults = Task.WhenAll(signalTasks).GetAwaiter().GetResult();
        for (int i = 0; i < signalResults.Length; ++i) {
          result.SignalSuccessCount += signalResults[i].SuccessCount;
          result.SignalFailureCount += signalResults[i].FailureCount;
          if (!string.IsNullOrEmpty(signalResults[i].Error) && errors.Count < 8) {
            errors.Enqueue(signalResults[i].Error);
          }
        }
      }

      StatusSnapshot peak = GetStatusSnapshot(baseUrl, timeoutSeconds);
      result.ObservedRoomsAtPeak = peak.Rooms;
      result.ObservedViewersAtPeak = peak.Viewers;

      if (holdSeconds > 0 && sessions.Count > 0) {
        Task.Delay(TimeSpan.FromSeconds(holdSeconds)).Wait();
      }

      if (sessions.Count > 0) {
        var teardownTasks = new List<Task<TeardownPhaseResult>>();
        for (int i = 0; i < sessions.Count; ++i) {
          teardownTasks.Add(TeardownRoomSafeAsync(sessions[i], timeoutSeconds));
        }
        var teardownResults = Task.WhenAll(teardownTasks).GetAwaiter().GetResult();
        for (int i = 0; i < teardownResults.Length; ++i) {
          if (!string.IsNullOrEmpty(teardownResults[i].Error) && errors.Count < 8) {
            errors.Enqueue(teardownResults[i].Error);
          }
        }
      }

      result.SettledToZero = WaitForZeroStatus(baseUrl, timeoutSeconds);
      allStopwatch.Stop();
      result.TotalPhaseMs = allStopwatch.Elapsed.TotalMilliseconds;

      samplerCts.Cancel();
      try {
        samplerTask.Wait(2000);
      } catch {
      }

      if (errors.Count > 0) {
        result.ErrorSummary = string.Join(" | ", errors.ToArray());
      }

      result.AverageCpuPercent = (metrics.CpuPercentSamples > 0) ? (metrics.CpuPercentTotal / metrics.CpuPercentSamples) : 0.0;
      result.PeakCpuPercent = metrics.PeakCpuPercent;
      result.InitialWorkingSetBytes = metrics.InitialWorkingSetBytes;
      result.FinalWorkingSetBytes = metrics.FinalWorkingSetBytes;
      result.MaxWorkingSetBytes = metrics.MaxWorkingSetBytes;
      result.MaxThreads = metrics.MaxThreads;
      result.MaxHandles = metrics.MaxHandles;
    }

    return result;
  }

  private static async Task<RoomConnectResult> ConnectRoomSafeAsync(
      string wsUrl,
      string roomId,
      string hostToken,
      int viewersPerRoom,
      int timeoutSeconds) {
    try {
      RoomSession session = await ConnectRoomAsync(wsUrl, roomId, hostToken, viewersPerRoom, timeoutSeconds).ConfigureAwait(false);
      return new RoomConnectResult {
        Session = session,
        Error = string.Empty
      };
    } catch (Exception ex) {
      return new RoomConnectResult {
        Session = null,
        Error = roomId + ": " + ex.GetType().Name + ": " + ex.Message
      };
    }
  }

  private static async Task<SignalPhaseResult> RunSignalPhaseSafeAsync(
      RoomSession session,
      int signalRounds,
      int timeoutSeconds) {
    var result = new SignalPhaseResult();
    try {
      for (int round = 0; round < signalRounds; ++round) {
        for (int viewerIndex = 0; viewerIndex < session.Viewers.Count; ++viewerIndex) {
          ViewerSession viewer = session.Viewers[viewerIndex];
          string offerSdp = "offer-" + round.ToString() + "-" + viewer.PeerId;
          string answerSdp = "answer-" + round.ToString() + "-" + viewer.PeerId;
          string offerMessage = "{\"type\":\"webrtc.offer\",\"to\":\"" + viewer.PeerId + "\",\"token\":\"" +
                                session.HostToken + "\",\"sdp\":\"" + offerSdp + "\"}";

          await SendTextAsync(session.HostSocket, offerMessage, timeoutSeconds).ConfigureAwait(false);
          string offerAck = await ReceiveTextAsync(viewer.Socket, timeoutSeconds).ConfigureAwait(false);
          EnsureFieldValue(offerAck, "type", "webrtc.offer");
          EnsureFieldValue(offerAck, "from", "host");

          string answerMessage = "{\"type\":\"webrtc.answer\",\"to\":\"host\",\"sdp\":\"" + answerSdp + "\"}";
          await SendTextAsync(viewer.Socket, answerMessage, timeoutSeconds).ConfigureAwait(false);
          string answerAck = await ReceiveTextAsync(session.HostSocket, timeoutSeconds).ConfigureAwait(false);
          EnsureFieldValue(answerAck, "type", "webrtc.answer");
          EnsureFieldValue(answerAck, "from", viewer.PeerId);
          result.SuccessCount += 1;
        }
      }
    } catch (Exception ex) {
      result.FailureCount += 1;
      result.Error = session.RoomId + ": signal failure: " + ex.GetType().Name + ": " + ex.Message;
    }

    return result;
  }

  private static async Task<TeardownPhaseResult> TeardownRoomSafeAsync(RoomSession session, int timeoutSeconds) {
    var result = new TeardownPhaseResult();
    try {
      string endMessage = "{\"type\":\"session.end\",\"room\":\"" + session.RoomId + "\",\"token\":\"" +
                          session.HostToken + "\",\"reason\":\"stress\"}";
      await SendTextAsync(session.HostSocket, endMessage, timeoutSeconds).ConfigureAwait(false);

      for (int viewerIndex = 0; viewerIndex < session.Viewers.Count; ++viewerIndex) {
        string ended = await ReceiveTextAsync(session.Viewers[viewerIndex].Socket, timeoutSeconds).ConfigureAwait(false);
        EnsureFieldValue(ended, "type", "session.ended");
      }

      string ack = await ReceiveTextAsync(session.HostSocket, timeoutSeconds).ConfigureAwait(false);
      EnsureFieldValue(ack, "type", "session.end.ack");
    } catch (Exception ex) {
      result.Error = session.RoomId + ": teardown failure: " + ex.GetType().Name + ": " + ex.Message;
    }

    try {
      CloseRoomAsync(session).GetAwaiter().GetResult();
    } catch (Exception ex) {
      if (string.IsNullOrEmpty(result.Error)) {
        result.Error = session.RoomId + ": close failure: " + ex.GetType().Name + ": " + ex.Message;
      }
    }

    return result;
  }

  private static async Task<RoomSession> ConnectRoomAsync(
      string wsUrl,
      string roomId,
      string hostToken,
      int viewersPerRoom,
      int timeoutSeconds) {
    var session = new RoomSession();
    session.RoomId = roomId;
    session.HostToken = hostToken;

    try {
      session.HostSocket = await ConnectSocketAsync(wsUrl, timeoutSeconds).ConfigureAwait(false);
      string hostRegister = "{\"type\":\"host.register\",\"room\":\"" + roomId + "\",\"token\":\"" + hostToken + "\"}";
      await SendTextAsync(session.HostSocket, hostRegister, timeoutSeconds).ConfigureAwait(false);

      string hostAck = await ReceiveTextAsync(session.HostSocket, timeoutSeconds).ConfigureAwait(false);
      EnsureFieldValue(hostAck, "type", "host.registered");

      for (int viewerIndex = 0; viewerIndex < viewersPerRoom; ++viewerIndex) {
        ClientWebSocket viewerSocket = await ConnectSocketAsync(wsUrl, timeoutSeconds).ConfigureAwait(false);
        string viewerJoin = "{\"type\":\"room.join\",\"room\":\"" + roomId + "\"}";
        await SendTextAsync(viewerSocket, viewerJoin, timeoutSeconds).ConfigureAwait(false);

        string joined = await ReceiveTextAsync(viewerSocket, timeoutSeconds).ConfigureAwait(false);
        EnsureFieldValue(joined, "type", "room.joined");
        string peerId = ExtractStringField(joined, "peerId");
        if (string.IsNullOrEmpty(peerId)) {
          throw new InvalidOperationException("room.joined missing peerId");
        }

        string peerJoined = await ReceiveTextAsync(session.HostSocket, timeoutSeconds).ConfigureAwait(false);
        EnsureFieldValue(peerJoined, "type", "peer.joined");
        EnsureFieldValue(peerJoined, "peerId", peerId);

        session.Viewers.Add(new ViewerSession {
          Socket = viewerSocket,
          PeerId = peerId
        });
      }

      return session;
    } catch {
      CloseRoomAsync(session).GetAwaiter().GetResult();
      throw;
    }
  }

  private static async Task<ClientWebSocket> ConnectSocketAsync(string wsUrl, int timeoutSeconds) {
    var socket = new ClientWebSocket();
    socket.Options.KeepAliveInterval = TimeSpan.FromSeconds(20);
    using (var cts = new CancellationTokenSource(TimeSpan.FromSeconds(timeoutSeconds))) {
      await socket.ConnectAsync(new Uri(wsUrl), cts.Token).ConfigureAwait(false);
    }
    return socket;
  }

  private static async Task SendTextAsync(ClientWebSocket socket, string text, int timeoutSeconds) {
    using (var cts = new CancellationTokenSource(TimeSpan.FromSeconds(timeoutSeconds))) {
      byte[] payload = Encoding.UTF8.GetBytes(text);
      var segment = new ArraySegment<byte>(payload);
      await socket.SendAsync(segment, WebSocketMessageType.Text, true, cts.Token).ConfigureAwait(false);
    }
  }

  private static async Task<string> ReceiveTextAsync(ClientWebSocket socket, int timeoutSeconds) {
    using (var cts = new CancellationTokenSource(TimeSpan.FromSeconds(timeoutSeconds)))
    using (var stream = new MemoryStream()) {
      byte[] buffer = new byte[8192];
      while (true) {
        var segment = new ArraySegment<byte>(buffer);
        WebSocketReceiveResult result = await socket.ReceiveAsync(segment, cts.Token).ConfigureAwait(false);
        if (result.MessageType == WebSocketMessageType.Close) {
          throw new InvalidOperationException("remote websocket closed");
        }

        stream.Write(buffer, 0, result.Count);
        if (result.EndOfMessage) {
          break;
        }
      }

      return Encoding.UTF8.GetString(stream.ToArray());
    }
  }

  private static async Task CloseRoomAsync(RoomSession session) {
    if (session == null) {
      return;
    }

    if (session.Viewers != null) {
      for (int i = 0; i < session.Viewers.Count; ++i) {
        ViewerSession viewer = session.Viewers[i];
        if (viewer != null) {
          await CloseSocketAsync(viewer.Socket).ConfigureAwait(false);
        }
      }
    }

    await CloseSocketAsync(session.HostSocket).ConfigureAwait(false);
  }

  private static async Task CloseSocketAsync(ClientWebSocket socket) {
    if (socket == null) {
      return;
    }

    try {
      if (socket.State == WebSocketState.Open || socket.State == WebSocketState.CloseReceived) {
        using (var cts = new CancellationTokenSource(TimeSpan.FromSeconds(2))) {
          await socket.CloseAsync(WebSocketCloseStatus.NormalClosure, "stress", cts.Token).ConfigureAwait(false);
        }
      } else if (socket.State == WebSocketState.CloseSent) {
        using (var cts = new CancellationTokenSource(TimeSpan.FromSeconds(2))) {
          await socket.CloseOutputAsync(WebSocketCloseStatus.NormalClosure, "stress", cts.Token).ConfigureAwait(false);
        }
      }
    } catch {
    } finally {
      socket.Dispose();
    }
  }

  private static StatusSnapshot GetStatusSnapshot(string baseUrl, int timeoutSeconds) {
    using (var client = new HttpClient()) {
      client.BaseAddress = new Uri(baseUrl);
      client.Timeout = TimeSpan.FromSeconds(timeoutSeconds);
      string json = client.GetStringAsync("/api/status").GetAwaiter().GetResult();
      return new StatusSnapshot {
        Rooms = ExtractNumberField(json, "rooms"),
        Viewers = ExtractNumberField(json, "viewers")
      };
    }
  }

  private static bool WaitForZeroStatus(string baseUrl, int timeoutSeconds) {
    for (int attempt = 0; attempt < 25; ++attempt) {
      StatusSnapshot snapshot = GetStatusSnapshot(baseUrl, timeoutSeconds);
      if (snapshot.Rooms == 0 && snapshot.Viewers == 0) {
        return true;
      }
      Thread.Sleep(200);
    }
    return false;
  }

  private static void EnsureFieldValue(string json, string key, string expected) {
    string actual = ExtractStringField(json, key);
    if (!string.Equals(actual, expected, StringComparison.Ordinal)) {
      throw new InvalidOperationException("expected " + key + "=" + expected + " but got " + actual + " from " + json);
    }
  }

  private static string ExtractStringField(string json, string key) {
    MatchCollection matches = StringFieldPattern.Matches(json);
    for (int i = 0; i < matches.Count; ++i) {
      Match match = matches[i];
      if (string.Equals(match.Groups["key"].Value, key, StringComparison.Ordinal)) {
        return Regex.Unescape(match.Groups["value"].Value);
      }
    }
    return string.Empty;
  }

  private static int ExtractNumberField(string json, string key) {
    MatchCollection matches = NumberFieldPattern.Matches(json);
    for (int i = 0; i < matches.Count; ++i) {
      Match match = matches[i];
      if (string.Equals(match.Groups["key"].Value, key, StringComparison.Ordinal)) {
        return int.Parse(match.Groups["value"].Value);
      }
    }
    return 0;
  }

  private static async Task SampleProcessLoopAsync(int processId, ProcessMetrics metrics, CancellationToken token) {
    if (processId <= 0) {
      return;
    }

    while (!token.IsCancellationRequested) {
      try {
        using (Process process = Process.GetProcessById(processId)) {
          process.Refresh();
          long workingSet = process.WorkingSet64;
          if (metrics.InitialWorkingSetBytes < 0) {
            metrics.InitialWorkingSetBytes = workingSet;
          }
          metrics.FinalWorkingSetBytes = workingSet;
          if (process.WorkingSet64 > metrics.MaxWorkingSetBytes) {
            metrics.MaxWorkingSetBytes = process.WorkingSet64;
          }
          try {
            int handles = process.HandleCount;
            if (handles > metrics.MaxHandles) {
              metrics.MaxHandles = handles;
            }
          } catch {
          }

          try {
            int threads = process.Threads.Count;
            if (threads > metrics.MaxThreads) {
              metrics.MaxThreads = threads;
            }
          } catch {
          }

          try {
            long nowTicks = Stopwatch.GetTimestamp();
            long cpuTicks = process.TotalProcessorTime.Ticks;
            if (metrics.LastCpuSampleReady) {
              double wallSeconds = (nowTicks - metrics.LastSampleTicks) / (double)Stopwatch.Frequency;
              double cpuSeconds = (cpuTicks - metrics.LastCpuTicks) / (double)TimeSpan.TicksPerSecond;
              if (wallSeconds > 0) {
                double cpuPercent = (cpuSeconds / wallSeconds / Environment.ProcessorCount) * 100.0;
                if (cpuPercent < 0) {
                  cpuPercent = 0;
                }
                metrics.CpuPercentTotal += cpuPercent;
                metrics.CpuPercentSamples += 1;
                if (cpuPercent > metrics.PeakCpuPercent) {
                  metrics.PeakCpuPercent = cpuPercent;
                }
              }
            }
            metrics.LastSampleTicks = nowTicks;
            metrics.LastCpuTicks = cpuTicks;
            metrics.LastCpuSampleReady = true;
          } catch {
          }
        }
      } catch {
      }

      try {
        await Task.Delay(250, token).ConfigureAwait(false);
      } catch (TaskCanceledException) {
        break;
      }
    }
  }
}
'@

function Get-ResolvedServerLayout {
  param(
    [string]$RepoRoot,
    [string]$Config,
    [string]$ServerExe,
    [string]$WwwRoot,
    [string]$AdminWww
  )

  $exePath = if ($ServerExe) { $ServerExe } else { Get-ServerExePath $RepoRoot $Config }
  Assert-PathExists $exePath "server executable"

  $resolvedWwwRoot = if ($WwwRoot) { $WwwRoot } else { Join-Path (Split-Path -Parent $exePath) "www" }
  $resolvedAdminWww = if ($AdminWww) { $AdminWww } else { Join-Path (Split-Path -Parent $exePath) "webui" }
  Assert-PathExists $resolvedWwwRoot "www root"
  Assert-PathExists $resolvedAdminWww "admin webui root"

  return @{
    Exe = $exePath
    WwwRoot = $resolvedWwwRoot
    AdminWww = $resolvedAdminWww
  }
}

function Wait-ForHealth {
  param(
    [string]$Url,
    [int]$TimeoutSeconds
  )

  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
  while ((Get-Date) -lt $deadline) {
    try {
      $response = Invoke-WebRequest -Uri $Url -UseBasicParsing -TimeoutSec 5
      if ($response.StatusCode -eq 200 -and $response.Content -eq "ok") {
        return $true
      }
    }
    catch {
    }
    Start-Sleep -Milliseconds 200
  }

  return $false
}

function Test-ServerHealth {
  param([string]$Url)

  try {
    $response = Invoke-WebRequest -Uri $Url -UseBasicParsing -TimeoutSec 5
    return ($response.StatusCode -eq 200 -and $response.Content -eq "ok")
  }
  catch {
    return $false
  }
}

function Format-Bytes {
  param([long]$Bytes)

  if ($Bytes -lt 0) { return "n/a" }
  if ($Bytes -lt 1KB) { return "$Bytes B" }
  if ($Bytes -lt 1MB) { return ("{0:N2} KiB" -f ($Bytes / 1KB)) }
  if ($Bytes -lt 1GB) { return ("{0:N2} MiB" -f ($Bytes / 1MB)) }
  return ("{0:N2} GiB" -f ($Bytes / 1GB))
}

function Write-HttpResult {
  param([HttpLoadResult]$Result)

  Write-Section "HTTP stress"
  Write-Host ("Requests: {0} total, {1} ok, {2} failed" -f $Result.TotalRequests, $Result.SuccessCount, $Result.FailureCount)
  Write-Host ("Latency: avg {0:N2} ms, p95 {1:N2} ms, max {2:N2} ms" -f $Result.AverageLatencyMs, $Result.P95LatencyMs, $Result.MaxLatencyMs)
  Write-Host ("Throughput window: {0:N2} s, payload {1}, statuses [{2}]" -f ($Result.ElapsedMs / 1000.0), (Format-Bytes $Result.TotalBytes), $Result.StatusCounts)
  Write-Host ("CPU during HTTP: avg {0:N2}%, peak {1:N2}%" -f $Result.AverageCpuPercent, $Result.PeakCpuPercent)
  Write-Host ("Memory during HTTP: start {0}, end {1}, peak {2}" -f (Format-Bytes $Result.InitialWorkingSetBytes), (Format-Bytes $Result.FinalWorkingSetBytes), (Format-Bytes $Result.MaxWorkingSetBytes))
  Write-Host ("Server peak during HTTP: handles {0}, threads {1}" -f $Result.MaxHandles, $Result.MaxThreads)
  if ($Result.ErrorSummary) {
    Write-Host ("HTTP errors: " + $Result.ErrorSummary) -ForegroundColor Yellow
  }
}

function Write-WebSocketResult {
  param([WebSocketLoadResult]$Result)

  Write-Section "WebSocket stress"
  Write-Host ("Rooms: requested {0}, connected {1}, failed {2}" -f $Result.RequestedRooms, $Result.ConnectedHosts, $Result.FailedRooms)
  Write-Host ("Viewers: target/room {0}, connected total {1}" -f $Result.RequestedViewersPerRoom, $Result.ConnectedViewers)
  Write-Host ("Signals: ok {0}, failed {1}" -f $Result.SignalSuccessCount, $Result.SignalFailureCount)
  Write-Host ("Peak status: rooms {0}, viewers {1}" -f $Result.ObservedRoomsAtPeak, $Result.ObservedViewersAtPeak)
  Write-Host ("Timing: connect {0:N2} s, total {1:N2} s" -f ($Result.ConnectPhaseMs / 1000.0), ($Result.TotalPhaseMs / 1000.0))
  Write-Host ("CPU during WebSocket phase: avg {0:N2}%, peak {1:N2}%" -f $Result.AverageCpuPercent, $Result.PeakCpuPercent)
  Write-Host ("Memory during WebSocket phase: start {0}, end {1}, peak {2}" -f (Format-Bytes $Result.InitialWorkingSetBytes), (Format-Bytes $Result.FinalWorkingSetBytes), (Format-Bytes $Result.MaxWorkingSetBytes))
  Write-Host ("Server peak during WebSocket phase: handles {0}, threads {1}" -f $Result.MaxHandles, $Result.MaxThreads)
  Write-Host ("Settled to zero after teardown: {0}" -f ($(if ($Result.SettledToZero) { "yes" } else { "no" })))
  if ($Result.ErrorSummary) {
    Write-Host ("WebSocket errors: " + $Result.ErrorSummary) -ForegroundColor Yellow
  }
}

$root = Get-RepoRoot
$layout = Get-ResolvedServerLayout -RepoRoot $root -Config $Config -ServerExe $ServerExe -WwwRoot $WwwRoot -AdminWww $AdminWww

$baseUrl = "http://$BindHost`:$Port"
$wsUrl = "ws://$BindHost`:$Port/ws"
$healthUrl = "$baseUrl/health"
$httpTargets = @(
  "/health",
  "/api/status",
  "/admin/",
  "/host?room=http-stress&token=http-token",
  "/view?room=http-stress",
  "/assets/common.js"
)

Write-Section "Start server"
$serverArgs = @(
  "--bind", $BindHost,
  "--port", "$Port",
  "--www", $layout.WwwRoot,
  "--admin-www", $layout.AdminWww,
  "--no-stdin"
)
$serverProc = Start-Process -FilePath $layout.Exe -ArgumentList $serverArgs -PassThru -WindowStyle Hidden

try {
  if (-not (Wait-ForHealth -Url $healthUrl -TimeoutSeconds 20)) {
    Fail "Server did not become healthy at $healthUrl"
  }

  Write-Host ("Base URL: " + $baseUrl)
  Write-Host ("WS URL:   " + $wsUrl)
  Write-Host ("PID:      " + $serverProc.Id)

  $httpResult = [LanStressHarness]::RunHttpBurst(
    $baseUrl,
    $httpTargets,
    $HttpRequests,
    $HttpConcurrency,
    $TimeoutSeconds,
    $serverProc.Id
  )
  Write-HttpResult -Result $httpResult

  if (-not (Test-ServerHealth -Url $healthUrl)) {
    Fail "Server health degraded after the HTTP phase."
  }

  $webSocketResult = [LanStressHarness]::RunWebSocketScenario(
    $baseUrl,
    $wsUrl,
    $Rooms,
    $ViewersPerRoom,
    $SignalRounds,
    $HoldSeconds,
    $TimeoutSeconds,
    $serverProc.Id
  )
  Write-WebSocketResult -Result $webSocketResult

  if (-not (Test-ServerHealth -Url $healthUrl)) {
    Fail "Server health degraded after the WebSocket phase."
  }

  $peakRoomsExpected = $webSocketResult.ConnectedHosts
  $peakViewersExpected = $webSocketResult.ConnectedViewers
  $hadFailures =
    ($httpResult.FailureCount -gt 0) -or
    ($webSocketResult.FailedRooms -gt 0) -or
    ($webSocketResult.SignalFailureCount -gt 0) -or
    (-not [string]::IsNullOrEmpty($webSocketResult.ErrorSummary)) -or
    (-not $webSocketResult.SettledToZero) -or
    ($webSocketResult.ObservedRoomsAtPeak -lt $peakRoomsExpected) -or
    ($webSocketResult.ObservedViewersAtPeak -lt $peakViewersExpected)

  Write-Section "Summary"
  Write-Host ("HTTP p95: {0:N2} ms" -f $httpResult.P95LatencyMs)
  Write-Host ("WS connected: hosts {0}, viewers {1}" -f $webSocketResult.ConnectedHosts, $webSocketResult.ConnectedViewers)
  Write-Host ("Peak CPU: HTTP {0:N2}%, WebSocket {1:N2}%" -f $httpResult.PeakCpuPercent, $webSocketResult.PeakCpuPercent)
  Write-Host ("Peak memory: HTTP {0}, WebSocket {1}" -f (Format-Bytes $httpResult.MaxWorkingSetBytes), (Format-Bytes $webSocketResult.MaxWorkingSetBytes))
  Write-Host ("Server stayed healthy: yes")

  if ($hadFailures) {
    Fail "Stress test completed with one or more failures. Review the metrics above."
  }

  Write-Host "Stress test passed." -ForegroundColor Green
}
finally {
  if ($serverProc -and -not $serverProc.HasExited) {
    try { Stop-Process -Id $serverProc.Id -Force } catch {}
  }
}
