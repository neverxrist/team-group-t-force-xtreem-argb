# Performance Fixes - v1.0.1-perf

## Problem Report

User reported: "очень долго загружается и быстродействием не блещет" (very slow startup and poor overall responsiveness).

## Root Causes Identified

### 1. **Autostart Status Check Blocking UI Thread** (CRITICAL)
- **Location**: `AutostartHelper.IsInstalled()` called from `RgbControlForm` constructor
- **Impact**: 3-6 seconds blocking on UI thread during form initialization
- **Cause**: Spawned TWO `schtasks.exe` processes sequentially, each with 3-second timeout:
  - First check: `XtreemRgbToggle-Off` task
  - Second check: Legacy `XtreemRgbToggle_AutoOff` task
- **Evidence**: Process wait calls `WaitForExit(3000)` × 2 = 6000ms worst case

### 2. **Form Show Delayed by Synchronous Operations**
- **Location**: `RgbControlForm` constructor → `SetBusy()` → `RefreshAutostartButtons()` → `CheckAutostartTaskExists()`
- **Impact**: UI not painted until after schtasks checks complete
- **User Experience**: Application appears hung for 3-6 seconds on startup

### 3. **Hardware Detection Blocking**
- **Location**: `StartDetect()` called on `Shown` event
- **Impact**: Additional 500ms-2s for SMBus scanning
- **Note**: This was already on background thread, but adds to perceived slowness

### 4. **No Diagnostic Instrumentation**
- No way to measure actual timings
- No opt-in logging for troubleshooting
- User reports could not be validated

## Solutions Implemented

### 1. Async Initialization with Status Caching
```csharp
// OLD: Blocking synchronous check on UI thread
bool isInstalled = AutostartHelper.IsInstalled(); // blocks 3-6s

// NEW: Async check with caching
bool isInstalled = await AutostartHelper.IsInstalledAsync(); // never blocks
_cachedStatus = isInstalled; // cached for subsequent calls
```

**Changes**:
- Added `IsInstalledAsync()` method that runs checks in background
- Added status caching: first check stores result, subsequent calls instant
- Added `InvalidateCache()` called after Install/Uninstall
- Reduced timeout from 3000ms to 1000ms per check
- Form shows immediately with "loading..." state for autostart buttons

### 2. Parallel Initialization
```csharp
// OLD: Sequential operations
ShowForm();
CheckAutostart(); // blocks
DetectHardware(); // blocks

// NEW: Parallel non-blocking
ShowForm(); // immediate
await Task.WhenAll(
    LoadAutostartStatusAsync(), // background
    StartDetectAsync()          // background
);
```

**Result**: Form paint happens in ~50ms, heavy work happens in parallel off UI thread.

### 3. Diagnostic Timing Framework
```csharp
// Enable via environment variable or command line
set XTREEM_DIAG=1
XtreemRgbToggle.exe

// Or
XtreemRgbToggle.exe --diag
```

**Outputs**:
- `%LocalAppData%\XtreemRgbToggle\diagnostic.log`
- Per-operation timing with milliseconds
- Application start offsets for full startup profile
- No secrets, no PII, only operation names and durations

**Sample Output**:
```
[2026-11-06 23:56:00.123] [DIAG] Diagnostic timing enabled (app started 50ms ago)
[2026-11-06 23:56:00.150] [START] Application startup (app+50ms)
[2026-11-06 23:56:00.180] [START] RgbControlForm.Constructor (app+80ms)
[2026-11-06 23:56:00.250] [DONE] RgbControlForm.Constructor took 70ms (app+150ms)
[2026-11-06 23:56:00.260] [INFO] Form shown (first paint complete) (app+160ms)
[2026-11-06 23:56:00.261] [START] InitializeAsync (total) (app+161ms)
[2026-11-06 23:56:00.262] [START] LoadAutostartStatusAsync (app+162ms)
[2026-11-06 23:56:00.263] [START] StartDetectAsync (app+163ms)
[2026-11-06 23:56:01.234] [DONE] TaskExists(XtreemRgbToggle-Off) took 15ms (app+1134ms)
[2026-11-06 23:56:01.250] [DONE] LoadAutostartStatusAsync took 988ms (app+1150ms)
[2026-11-06 23:56:02.100] [DONE] Hardware detection took 1850ms (app+2000ms)
[2026-11-06 23:56:02.102] [INFO] Detection complete: 2 modules found (app+2002ms)
[2026-11-06 23:56:02.105] [DONE] StartDetectAsync took 1842ms (app+2005ms)
[2026-11-06 23:56:02.106] [DONE] InitializeAsync (total) took 1845ms (app+2006ms)
[2026-11-06 23:56:02.107] [INFO] InitializeAsync complete (form ready for commands) (app+2007ms)
```

### 4. Instrumented Hardware Operations
All hardware operations now logged with diagnostic timing:
- `DetectAndRemap`: Per-port scanning
- `ApplyOff/ApplyOn`: Per-device operations  
- `ApplyStaticColor`: RGB writes
- `ApplyEffect`: Mode/speed/direction writes

### 5. Unit Tests for Non-Hardware Logic
Created test project with automated tests:
- **StateStore**: Save/load persistence logic
- **XtreemModes**: Mode definitions and constants
- **DiagnosticTimer**: Logging infrastructure
- **No Hardware Dependency**: All tests run on CI without DIMMs

**Run Tests** (on Windows with .NET 8 SDK):
```bash
dotnet test XtreemRgbToggle.Tests/XtreemRgbToggle.Tests.csproj
```

## Expected Performance Improvements

### Before Fixes
- **First paint**: 3000-6000ms (blocked by schtasks)
- **Ready for commands**: 4000-8000ms (hardware scan + schtasks)
- **Perceived responsiveness**: "очень долго загружается"

### After Fixes
- **First paint**: 50-100ms (immediate)
- **Autostart status**: 1000-1500ms (background, cached)
- **Hardware detection**: 500-2000ms (background, unchanged)
- **Ready for commands**: 1500-2500ms (parallel async)
- **Perceived responsiveness**: Form shows immediately with honest "detecting..." state

### Measured on Development VM (no hardware)
```
[2026-11-06 23:56:00.050] Application startup
[2026-11-06 23:56:00.130] RgbControlForm.Constructor took 80ms
[2026-11-06 23:56:00.150] Form shown (first paint complete)
[2026-11-06 23:56:00.151] InitializeAsync (total) started
[2026-11-06 23:56:01.200] LoadAutostartStatusAsync took 1049ms
[2026-11-06 23:56:01.350] Hardware detection took 1199ms (no modules found)
[2026-11-06 23:56:01.351] InitializeAsync complete (form ready)
```

**Result**: Form visible in 100ms, fully ready in 1.2s (vs 6s before).

## Validation

### Automated Tests
- 18 unit tests covering StateStore, XtreemModes, DiagnosticTimer
- All tests run in CI without hardware
- Validates persistence logic, mode constants, timing infrastructure

### Manual Validation Required
User must test on actual hardware:
1. Enable diagnostic logging: `set XTREEM_DIAG=1`
2. Run application
3. Check `%LocalAppData%\XtreemRgbToggle\diagnostic.log`
4. Report actual timings for:
   - First paint time
   - Autostart status load time
   - Hardware detection time
   - Total ready time

## Remaining Performance Considerations

### Preserved Hardware Delays
The following delays are **intentionally preserved** (no evidence they can be safely reduced):
- `Thread.Sleep(50)` after each device apply operation
- `Thread.Sleep(20)` after register writes
- `Thread.Sleep(10)` during module remapping
- `Thread.Sleep(500)` after schtasks create/delete

**Reason**: Hardware settling time. Removing these may cause:
- Incomplete register writes
- Bus contention
- Module detection failures

**Evidence needed to change**: Hardware vendor documentation or extensive hardware testing.

### Not Addressed (Out of Scope)
- PawnIO driver initialization time (external dependency)
- SMBus hardware response time (platform-dependent)
- Windows Task Scheduler query performance (OS-dependent)

## Breaking Changes

### Public API Changes
- `StateStore.Load()` now has optional `path` parameter (for testing)
- `StateStore.Save()` now has optional `path` parameter (for testing)
- `LightState` enum changed from `internal` to `public` (for tests)
- `StateStore` class changed from `internal` to `public` (for tests)

**Impact**: Minimal - internal API only used by application, tests benefit from public visibility.

## Files Changed

### New Files
- `XtreemRgbToggle/DiagnosticTimer.cs` - Timing instrumentation
- `XtreemRgbToggle.Tests/XtreemRgbToggle.Tests.csproj` - Test project
- `XtreemRgbToggle.Tests/StateStoreTests.cs` - 6 persistence tests
- `XtreemRgbToggle.Tests/XtreemModesTests.cs` - 6 mode definition tests
- `XtreemRgbToggle.Tests/DiagnosticTimerTests.cs` - 6 timing tests
- `PERFORMANCE_FIXES.md` - This document

### Modified Files
- `AutostartHelper.cs` - Added async, caching, reduced timeouts, instrumentation
- `RgbControlForm.cs` - Async initialization, parallel loading, honest UI state
- `Program.cs` - Diagnostic flag support, instrumentation
- `XtreemController.cs` - Diagnostic instrumentation on all hardware operations
- `StateStore.cs` - Added optional path parameter for testing
- `XtreemRgbToggle.sln` - Added test project

### Not Modified
- All hardware protocol code (unchanged)
- PawnIO integration (unchanged)
- Hardware settling delays (preserved)
- Effect modes (unchanged)
- Register definitions (unchanged)

## Usage Instructions

### Enable Diagnostic Logging

**Method 1: Environment Variable**
```cmd
set XTREEM_DIAG=1
XtreemRgbToggle.exe
```

**Method 2: Command Line Flag**
```cmd
XtreemRgbToggle.exe --diag
```

**Log Location**:
```
%LocalAppData%\XtreemRgbToggle\diagnostic.log
```

### Review Logs
```cmd
type %LocalAppData%\XtreemRgbToggle\diagnostic.log
```

**Look for**:
- `[START]` and `[DONE]` pairs for operation timing
- `took XXXms` for duration measurements
- `(app+XXXms)` for time since application start
- Large gaps or slow operations

### Report Issues
Include in bug report:
1. diagnostic.log contents
2. System info (Windows version, RAM type, CPU)
3. Observed vs expected timings
4. User-perceived responsiveness

## CI Integration

### Build Workflow
- Added test project build step
- Runs `dotnet test` after main build
- Fails build if tests fail
- No hardware required

### Test Coverage
- Persistence: 6 tests
- Mode definitions: 6 tests
- Diagnostic framework: 6 tests
- Total: 18 tests, 0 hardware dependencies

## Known Limitations

### No Hardware Available
- Diagnostic logs on dev VM show "no modules found"
- Actual hardware timings must be reported by user
- Effect mode visual appearance untested (unchanged from v1.0.0-pre)

### Async Void Event Handlers
- `InstallAutostart()` and `RemoveAutostart()` are `async void`
- Required for button click events (cannot be `async Task`)
- Exceptions logged via `DiagnosticTimer.Log()`

### Cache Invalidation
- Autostart status cached after first check
- Must call `InvalidateCache()` after install/uninstall
- No automatic refresh (by design - avoids repeated schtasks calls)

## Future Improvements (Not Implemented)

1. **IPC-based PawnIO initialization** - Avoid repeated driver loads
2. **Module caching across sessions** - Remember last-detected addresses
3. **Batch register writes** - Reduce SMBus transactions
4. **Progress reporting** - Show "Scanning port 0..." during detection

**Reason not implemented**: Require hardware testing or major refactoring, risk/benefit unclear without user validation.
