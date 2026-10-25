# ==============================================================================
# Metin2 MT2009 - Kaynak Sinirli (CPU Hard-Cap / Affinity / BelowNormal) Baslatici
# Yalnizca Windows yerel gelistirme ortami icindir. Uretim veya CI koduna dokunmaz.
# ==============================================================================
param(
    [string]$Command = "",
    [switch]$Build,
    [switch]$CleanFirst,
    [switch]$CTest,
    [ValidateRange(10, 40)]
    [int]$CpuCapPercent = 35,
    [ValidateRange(1, 4)]
    [int]$MaxLogicalProcessors = 4,
    [ValidateRange(1, 2)]
    [int]$ParallelJobs = 2
)

$ErrorActionPreference = "Stop"
$RootDir = (Resolve-Path "$PSScriptRoot\..").Path
Set-Location $RootDir

if ($CleanFirst) {
    $Command = "cmake --build --preset windows-gcc-ucrt --clean-first -- -j $ParallelJobs"
} elseif ($Build) {
    $Command = "cmake --build --preset windows-gcc-ucrt -- -j $ParallelJobs"
} elseif ($CTest) {
    $Command = "ctest --preset windows-gcc-ucrt --parallel 1 --output-on-failure"
}

if ([string]::IsNullOrWhiteSpace($Command)) {
    throw "Calistirilacak komut belirtilmedi (-Command, -Build, -CleanFirst veya -CTest kullanin)."
}

# Zorunlu paralellik ortam degiskenleri ve calisma zamani DLL yollari
$env:CMAKE_BUILD_PARALLEL_LEVEL = "$ParallelJobs"
$env:CTEST_PARALLEL_LEVEL = "1"
$env:NINJAFLAGS = "-j$ParallelJobs"
$env:PATH = "$RootDir\build\windows-gcc-ucrt\tests;$RootDir\extern\common\mariadb-connector-c\lib\win64;$RootDir\extern\server\devil\lib\win64;C:\msys64\ucrt64\bin;$env:PATH"

$csharpSource = @'
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

public static class Mt2JobRunner
{
    private const uint JOB_OBJECT_LIMIT_ACTIVE_PROCESS = 0x00000008;
    private const uint JOB_OBJECT_LIMIT_AFFINITY = 0x00000010;
    private const uint JOB_OBJECT_LIMIT_PRIORITY_CLASS = 0x00000020;
    private const uint JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x00002000;

    private const uint BELOW_NORMAL_PRIORITY_CLASS = 0x00004000;
    private const uint CREATE_SUSPENDED = 0x00000004;
    private const uint CREATE_NO_WINDOW = 0x08000000;

    private const uint JOB_OBJECT_CPU_RATE_CONTROL_ENABLE = 0x00000001;
    private const uint JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP = 0x00000004;

    private enum JOBOBJECTINFOCLASS
    {
        JobObjectBasicAccountingInformation = 1,
        JobObjectBasicLimitInformation = 2,
        JobObjectBasicProcessIdList = 3,
        JobObjectExtendedLimitInformation = 9,
        JobObjectCpuRateControlInformation = 15
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct JOBOBJECT_BASIC_ACCOUNTING_INFORMATION
    {
        public long TotalUserTime;
        public long TotalKernelTime;
        public long ThisPeriodTotalUserTime;
        public long ThisPeriodTotalKernelTime;
        public uint TotalPageFaultCount;
        public uint TotalProcesses;
        public uint ActiveProcesses;
        public uint TotalTerminatedProcesses;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct JOBOBJECT_BASIC_LIMIT_INFORMATION
    {
        public long PerProcessUserTimeLimit;
        public long PerJobUserTimeLimit;
        public uint LimitFlags;
        public UIntPtr MinimumWorkingSetSize;
        public UIntPtr MaximumWorkingSetSize;
        public uint ActiveProcessLimit;
        public UIntPtr Affinity;
        public uint PriorityClass;
        public uint SchedulingClass;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct IO_COUNTERS
    {
        public ulong ReadOperationCount;
        public ulong WriteOperationCount;
        public ulong OtherOperationCount;
        public ulong ReadTransferCount;
        public ulong WriteTransferCount;
        public ulong OtherTransferCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct JOBOBJECT_EXTENDED_LIMIT_INFORMATION
    {
        public JOBOBJECT_BASIC_LIMIT_INFORMATION BasicLimitInformation;
        public IO_COUNTERS IoInfo;
        public UIntPtr ProcessMemoryLimit;
        public UIntPtr JobMemoryLimit;
        public UIntPtr PeakProcessMemoryUsed;
        public UIntPtr PeakJobMemoryUsed;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct JOBOBJECT_CPU_RATE_CONTROL_INFORMATION
    {
        public uint ControlFlags;
        public uint CpuRate;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct STARTUPINFOW
    {
        public int cb;
        public string lpReserved;
        public string lpDesktop;
        public string lpTitle;
        public int dwX;
        public int dwY;
        public int dwXSize;
        public int dwYSize;
        public int dwXCountChars;
        public int dwYCountChars;
        public int dwFillAttribute;
        public int dwFlags;
        public short wShowWindow;
        public short cbReserved2;
        public IntPtr lpReserved2;
        public IntPtr hStdInput;
        public IntPtr hStdOutput;
        public IntPtr hStdError;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct PROCESS_INFORMATION
    {
        public IntPtr hProcess;
        public IntPtr hThread;
        public uint dwProcessId;
        public uint dwThreadId;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct FILETIME
    {
        public uint dwLowDateTime;
        public uint dwHighDateTime;
        public long ToLong()
        {
            return ((long)dwHighDateTime << 32) | (uint)dwLowDateTime;
        }
    }

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr CreateJobObjectW(IntPtr lpJobAttributes, string lpName);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool SetInformationJobObject(IntPtr hJob, JOBOBJECTINFOCLASS infoClass, IntPtr lpJobObjectInfo, uint cbJobObjectInfoLength);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool QueryInformationJobObject(IntPtr hJob, JOBOBJECTINFOCLASS infoClass, IntPtr lpJobObjectInfo, uint cbJobObjectInfoLength, out uint lpReturnLength);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool AssignProcessToJobObject(IntPtr hJob, IntPtr hProcess);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool CreateProcessW(
        string lpApplicationName,
        StringBuilder lpCommandLine,
        IntPtr lpProcessAttributes,
        IntPtr lpThreadAttributes,
        bool bInheritHandles,
        uint dwCreationFlags,
        IntPtr lpEnvironment,
        string lpCurrentDirectory,
        ref STARTUPINFOW lpStartupInfo,
        out PROCESS_INFORMATION lpProcessInformation);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern uint ResumeThread(IntPtr hThread);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GetExitCodeProcess(IntPtr hProcess, out uint lpExitCode);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool CloseHandle(IntPtr hObject);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GetProcessAffinityMask(IntPtr hProcess, out UIntPtr lpProcessAffinityMask, out UIntPtr lpSystemAffinityMask);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern uint GetPriorityClass(IntPtr hProcess);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GetSystemTimes(out FILETIME lpIdleTime, out FILETIME lpKernelTime, out FILETIME lpUserTime);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GetLogicalProcessorInformationEx(int RelationshipType, IntPtr Buffer, ref uint ReturnedLength);

    public static void DetectHybridTopology(out ulong pCoreMask, out ulong eCoreMask, out int pCoreCount, out int eCoreCount)
    {
        pCoreMask = 0;
        eCoreMask = 0;
        pCoreCount = 0;
        eCoreCount = 0;
        uint len = 0;
        GetLogicalProcessorInformationEx(0 /* RelationProcessorCore */, IntPtr.Zero, ref len);
        if (len == 0) return;

        IntPtr buf = Marshal.AllocHGlobal((int)len);
        try
        {
            if (!GetLogicalProcessorInformationEx(0, buf, ref len)) return;
            int offset = 0;
            List<Tuple<byte, ulong>> cores = new List<Tuple<byte, ulong>>();
            while (offset < (int)len)
            {
                IntPtr cur = IntPtr.Add(buf, offset);
                int size = Marshal.ReadInt32(cur, 4);
                byte effClass = Marshal.ReadByte(cur, 9);
                ulong mask = (ulong)Marshal.ReadInt64(cur, 32);
                cores.Add(Tuple.Create(effClass, mask));
                offset += size;
            }
            bool hasMultipleClasses = false;
            for (int i = 1; i < cores.Count; ++i)
            {
                if (cores[i].Item1 != cores[0].Item1) hasMultipleClasses = true;
            }
            foreach (var c in cores)
            {
                if (!hasMultipleClasses || c.Item1 > 0)
                {
                    pCoreMask |= c.Item2;
                    pCoreCount++;
                }
                else
                {
                    eCoreMask |= c.Item2;
                    eCoreCount++;
                }
            }
        }
        finally
        {
            Marshal.FreeHGlobal(buf);
        }
    }

    private static int CountBits(ulong v)
    {
        int c = 0;
        while (v != 0) { v &= (v - 1); c++; }
        return c;
    }

    private static ulong SelectLowestBits(ulong mask, int maxBits)
    {
        ulong result = 0;
        int count = 0;
        for (int bit = 0; bit < 64 && count < maxBits; ++bit)
        {
            ulong b = 1UL << bit;
            if ((mask & b) != 0)
            {
                result |= b;
                count++;
            }
        }
        return result;
    }

    public static int ExecuteLimited(string workingDir, string commandText, int cpuCapPercent, int maxLogicalProcessors, int parallelJobs)
    {
        int totalLogical = Environment.ProcessorCount;
        ulong pCoreMask, eCoreMask;
        int pCoreCount, eCoreCount;
        DetectHybridTopology(out pCoreMask, out eCoreMask, out pCoreCount, out eCoreCount);

        UIntPtr procAff, sysAff;
        GetProcessAffinityMask(Process.GetCurrentProcess().Handle, out procAff, out sysAff);
        ulong sysMask = sysAff.ToUInt64();

        // Tercihen E-core mantiksal islemcilerini (LP 8..11) secerek P-core'lari (LP 0..7) kullaniciya serbest birak
        ulong candidatePool = (eCoreMask != 0 && CountBits(eCoreMask & sysMask) >= maxLogicalProcessors)
            ? (eCoreMask & sysMask)
            : sysMask;
        ulong targetAffinity = SelectLowestBits(candidatePool, maxLogicalProcessors);
        if (targetAffinity == 0)
        {
            targetAffinity = SelectLowestBits(sysMask, maxLogicalProcessors);
        }

        IntPtr hJob = CreateJobObjectW(IntPtr.Zero, null);
        if (hJob == IntPtr.Zero)
        {
            throw new Win32Exception(Marshal.GetLastWin32Error(), "CreateJobObjectW failed");
        }

        try
        {
            // 1. Extended Limit: BelowNormal Priority + Affinity (max 4 LP) + Kill on close (NO BREAKAWAY)
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION extLimit = new JOBOBJECT_EXTENDED_LIMIT_INFORMATION();
            extLimit.BasicLimitInformation.LimitFlags =
                JOB_OBJECT_LIMIT_PRIORITY_CLASS |
                JOB_OBJECT_LIMIT_AFFINITY |
                JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            extLimit.BasicLimitInformation.PriorityClass = BELOW_NORMAL_PRIORITY_CLASS;
            extLimit.BasicLimitInformation.Affinity = new UIntPtr(targetAffinity);

            int extSize = Marshal.SizeOf(typeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION));
            IntPtr pExt = Marshal.AllocHGlobal(extSize);
            try
            {
                Marshal.StructureToPtr(extLimit, pExt, false);
                if (!SetInformationJobObject(hJob, JOBOBJECTINFOCLASS.JobObjectExtendedLimitInformation, pExt, (uint)extSize))
                {
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "SetInformationJobObject(ExtendedLimit) failed");
                }
            }
            finally
            {
                Marshal.FreeHGlobal(pExt);
            }

            // 2. CPU Rate Control: HARD_CAP
            string hardCapStatus = "FAILED";
            uint verifiedCpuRate = 0;
            uint verifiedControlFlags = 0;
            JOBOBJECT_CPU_RATE_CONTROL_INFORMATION cpuInfo = new JOBOBJECT_CPU_RATE_CONTROL_INFORMATION();
            cpuInfo.ControlFlags = JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP;
            cpuInfo.CpuRate = (uint)(cpuCapPercent * 100); // 35% -> 3500

            int cpuSize = Marshal.SizeOf(typeof(JOBOBJECT_CPU_RATE_CONTROL_INFORMATION));
            IntPtr pCpu = Marshal.AllocHGlobal(cpuSize);
            try
            {
                Marshal.StructureToPtr(cpuInfo, pCpu, false);
                if (SetInformationJobObject(hJob, JOBOBJECTINFOCLASS.JobObjectCpuRateControlInformation, pCpu, (uint)cpuSize))
                {
                    uint retLen = 0;
                    if (QueryInformationJobObject(hJob, JOBOBJECTINFOCLASS.JobObjectCpuRateControlInformation, pCpu, (uint)cpuSize, out retLen))
                    {
                        var readBack = (JOBOBJECT_CPU_RATE_CONTROL_INFORMATION)Marshal.PtrToStructure(pCpu, typeof(JOBOBJECT_CPU_RATE_CONTROL_INFORMATION));
                        verifiedControlFlags = readBack.ControlFlags;
                        verifiedCpuRate = readBack.CpuRate;
                        if ((verifiedControlFlags & JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP) != 0 && verifiedCpuRate == (uint)(cpuCapPercent * 100))
                        {
                            hardCapStatus = "ACTIVE";
                        }
                    }
                }
            }
            finally
            {
                Marshal.FreeHGlobal(pCpu);
            }

            // Read back BasicLimitInformation to prove Affinity & Priority on Job Object
            ulong verifiedAffinity = 0;
            uint verifiedPriority = 0;
            int basicSize = Marshal.SizeOf(typeof(JOBOBJECT_BASIC_LIMIT_INFORMATION));
            IntPtr pBasic = Marshal.AllocHGlobal(basicSize);
            try
            {
                uint retLen = 0;
                if (QueryInformationJobObject(hJob, JOBOBJECTINFOCLASS.JobObjectBasicLimitInformation, pBasic, (uint)basicSize, out retLen))
                {
                    var readBasic = (JOBOBJECT_BASIC_LIMIT_INFORMATION)Marshal.PtrToStructure(pBasic, typeof(JOBOBJECT_BASIC_LIMIT_INFORMATION));
                    verifiedAffinity = readBasic.Affinity.ToUInt64();
                    verifiedPriority = readBasic.PriorityClass;
                }
            }
            finally
            {
                Marshal.FreeHGlobal(pBasic);
            }

            Console.WriteLine("==================================================================");
            Console.WriteLine(" [MT2009 CPU-LIMITED RUNNER] Donanim & Job Object Dogrulamasi");
            Console.WriteLine(string.Format("  * Sistem Mantiksal Islemci : {0} (P-core: {1} [mask=0x{2:X}], E-core: {3} [mask=0x{4:X}])",
                totalLogical, pCoreCount, pCoreMask, eCoreCount, eCoreMask));
            Console.WriteLine(string.Format("  * CPU_RATE_CONTROL_HARD_CAP: {0} (Hedef: %{1}, Okunan CpuRate: {2}/10000, Flags: 0x{3:X})",
                hardCapStatus, cpuCapPercent, verifiedCpuRate, verifiedControlFlags));
            Console.WriteLine(string.Format("  * Job Affinity Mask        : 0x{0:X} ({1} mantiksal islemci, Secilen: {2})",
                verifiedAffinity, CountBits(verifiedAffinity), (verifiedAffinity & eCoreMask) == verifiedAffinity ? "E-core izole (LP 8..11)" : "Hibrit maske"));
            Console.WriteLine(string.Format("  * Job Priority Class       : 0x{0:X} ({1})",
                verifiedPriority, verifiedPriority == BELOW_NORMAL_PRIORITY_CLASS ? "BelowNormal" : "Other"));
            Console.WriteLine(string.Format("  * Paralel Sinirlar         : CMAKE_BUILD_PARALLEL_LEVEL={0}, CTEST_PARALLEL_LEVEL=1", parallelJobs));
            Console.WriteLine(string.Format("  * Calistirilan Komut       : {0}", commandText));
            Console.WriteLine("==================================================================");

            // 3. Start PowerShell child process SUSPENDED, assign to Job Object, then resume
            string psExe = Environment.GetEnvironmentVariable("SystemRoot") + "\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
            string wrappedScript = "$ProgressPreference = 'SilentlyContinue'\r\n" + commandText + "\r\nif ($LASTEXITCODE -ne $null -and $LASTEXITCODE -ne 0) { exit $LASTEXITCODE }\r\n";
            string encodedCmd = Convert.ToBase64String(Encoding.Unicode.GetBytes(wrappedScript));

            StringBuilder cmdLine = new StringBuilder();
            cmdLine.AppendFormat("\"{0}\" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand {1}", psExe, encodedCmd);

            STARTUPINFOW si = new STARTUPINFOW();
            si.cb = Marshal.SizeOf(typeof(STARTUPINFOW));
            PROCESS_INFORMATION pi;

            if (!CreateProcessW(
                null,
                cmdLine,
                IntPtr.Zero,
                IntPtr.Zero,
                true,
                CREATE_SUSPENDED | BELOW_NORMAL_PRIORITY_CLASS,
                IntPtr.Zero,
                workingDir,
                ref si,
                out pi))
            {
                throw new Win32Exception(Marshal.GetLastWin32Error(), "CreateProcessW failed");
            }

            try
            {
                if (!AssignProcessToJobObject(hJob, pi.hProcess))
                {
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "AssignProcessToJobObject failed");
                }
                ResumeThread(pi.hThread);

                // Verify child process affinity and priority immediately after resume
                UIntPtr childProcAff, childSysAff;
                GetProcessAffinityMask(pi.hProcess, out childProcAff, out childSysAff);
                uint childPrio = GetPriorityClass(pi.hProcess);

                Stopwatch sw = Stopwatch.StartNew();
                long prevJobTicks = 0;
                long prevWallTicks = 0;
                FILETIME ftIdle0, ftKernel0, ftUser0;
                GetSystemTimes(out ftIdle0, out ftKernel0, out ftUser0);
                long prevSysIdle = ftIdle0.ToLong();
                long prevSysTotal = ftKernel0.ToLong() + ftUser0.ToLong();

                double maxJobCpuPct = 0.0;
                double sumJobCpuPct = 0.0;
                double maxSysCpuPct = 0.0;
                double sumSysCpuPct = 0.0;
                int sampleCount = 0;
                uint maxActiveProcs = 0;
                uint totalSpawnedProcs = 0;
                HashSet<string> observedNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                HashSet<int> verifiedChildPids = new HashSet<int>();
                int affinityMismatchCount = 0;
                int priorityMismatchCount = 0;

                int acctSize = Marshal.SizeOf(typeof(JOBOBJECT_BASIC_ACCOUNTING_INFORMATION));
                IntPtr pAcct = Marshal.AllocHGlobal(acctSize);
                int pidBufSize = 4096;
                IntPtr pPidList = Marshal.AllocHGlobal(pidBufSize);

                try
                {
                    while (true)
                    {
                        uint waitRes = WaitForSingleObject(pi.hProcess, 500);
                        uint retLen = 0;
                        if (QueryInformationJobObject(hJob, JOBOBJECTINFOCLASS.JobObjectBasicAccountingInformation, pAcct, (uint)acctSize, out retLen))
                        {
                            var acct = (JOBOBJECT_BASIC_ACCOUNTING_INFORMATION)Marshal.PtrToStructure(pAcct, typeof(JOBOBJECT_BASIC_ACCOUNTING_INFORMATION));
                            long curJobTicks = acct.TotalUserTime + acct.TotalKernelTime;
                            long curWallTicks = sw.Elapsed.Ticks;
                            long dWall = curWallTicks - prevWallTicks;
                            if (dWall >= 1000000) // >= 100ms
                            {
                                long dJob = curJobTicks - prevJobTicks;
                                double jobPct = (double)dJob / ((double)dWall * totalLogical) * 100.0;
                                if (jobPct < 0.0) jobPct = 0.0;
                                if (jobPct > 100.0) jobPct = 100.0;

                                FILETIME ftIdle1, ftKernel1, ftUser1;
                                GetSystemTimes(out ftIdle1, out ftKernel1, out ftUser1);
                                long curSysIdle = ftIdle1.ToLong();
                                long curSysTotal = ftKernel1.ToLong() + ftUser1.ToLong();
                                long dSysTotal = curSysTotal - prevSysTotal;
                                long dSysIdle = curSysIdle - prevSysIdle;
                                double sysPct = dSysTotal > 0 ? (double)(dSysTotal - dSysIdle) / (double)dSysTotal * 100.0 : 0.0;

                                prevJobTicks = curJobTicks;
                                prevWallTicks = curWallTicks;
                                prevSysIdle = curSysIdle;
                                prevSysTotal = curSysTotal;

                                if (jobPct > maxJobCpuPct) maxJobCpuPct = jobPct;
                                sumJobCpuPct += jobPct;
                                if (sysPct > maxSysCpuPct) maxSysCpuPct = sysPct;
                                sumSysCpuPct += sysPct;
                                sampleCount++;
                            }
                            if (acct.ActiveProcesses > maxActiveProcs) maxActiveProcs = acct.ActiveProcesses;
                            totalSpawnedProcs = acct.TotalProcesses;
                        }

                        if (QueryInformationJobObject(hJob, JOBOBJECTINFOCLASS.JobObjectBasicProcessIdList, pPidList, (uint)pidBufSize, out retLen))
                        {
                            int assignedCount = Marshal.ReadInt32(pPidList, 4);
                            for (int i = 0; i < assignedCount && i < 256; ++i)
                            {
                                int pid = (int)Marshal.ReadIntPtr(pPidList, 8 + i * IntPtr.Size).ToInt64();
                                try
                                {
                                    using (Process p = Process.GetProcessById(pid))
                                    {
                                        observedNames.Add(p.ProcessName);
                                        if (verifiedChildPids.Add(pid))
                                        {
                                            UIntPtr pAff, sAff;
                                            if (GetProcessAffinityMask(p.Handle, out pAff, out sAff))
                                            {
                                                if ((pAff.ToUInt64() & ~verifiedAffinity) != 0)
                                                    affinityMismatchCount++;
                                            }
                                            uint prio = GetPriorityClass(p.Handle);
                                            if (prio != 0 && prio != BELOW_NORMAL_PRIORITY_CLASS)
                                                priorityMismatchCount++;
                                        }
                                    }
                                }
                                catch { }
                            }
                        }

                        if (waitRes == 0) break; // Process exited
                    }

                    // Final accounting read
                    uint finalRet = 0;
                    long finalJobTicks = 0;
                    if (QueryInformationJobObject(hJob, JOBOBJECTINFOCLASS.JobObjectBasicAccountingInformation, pAcct, (uint)acctSize, out finalRet))
                    {
                        var acct = (JOBOBJECT_BASIC_ACCOUNTING_INFORMATION)Marshal.PtrToStructure(pAcct, typeof(JOBOBJECT_BASIC_ACCOUNTING_INFORMATION));
                        finalJobTicks = acct.TotalUserTime + acct.TotalKernelTime;
                        totalSpawnedProcs = acct.TotalProcesses;
                        if (acct.ActiveProcesses > maxActiveProcs) maxActiveProcs = acct.ActiveProcesses;
                    }
                    sw.Stop();
                    double overallJobAvgPct = sw.Elapsed.Ticks > 0
                        ? ((double)finalJobTicks / ((double)sw.Elapsed.Ticks * totalLogical)) * 100.0
                        : 0.0;
                    double sampledJobAvgPct = sampleCount > 0 ? (sumJobCpuPct / sampleCount) : overallJobAvgPct;
                    double sampledSysAvgPct = sampleCount > 0 ? (sumSysCpuPct / sampleCount) : 0.0;

                    uint exitCode = 1;
                    GetExitCodeProcess(pi.hProcess, out exitCode);

                    string[] namesArr = new string[observedNames.Count];
                    observedNames.CopyTo(namesArr);
                    Array.Sort(namesArr, StringComparer.OrdinalIgnoreCase);

                    Console.WriteLine("==================================================================");
                    Console.WriteLine(" [MT2009 CPU-LIMITED RUNNER] Calisma Sonu Olcum Raporu");
                    Console.WriteLine(string.Format("  * Cikis Kodu (Exit Code)   : {0}", (int)exitCode));
                    Console.WriteLine(string.Format("  * Toplam Sure              : {0:F2} sn", sw.Elapsed.TotalSeconds));
                    Console.WriteLine(string.Format("  * CPU Hard-Cap Durumu      : {0} (Sinir: %{1}.00)", hardCapStatus, cpuCapPercent));
                    Console.WriteLine(string.Format("  * Cocugun Gercek Affinity  : 0x{0:X} | Priority: 0x{1:X} (Ihlal: aff={2}, prio={3} / Denetlenen PID={4})",
                        childProcAff.ToUInt64(), childPrio, affinityMismatchCount, priorityMismatchCount, verifiedChildPids.Count));
                    Console.WriteLine(string.Format("  * Job Agaci CPU Kullanimi  : Ort={0:F2}% (Orneklem Ort={1:F2}%, Tepe={2:F2}%)",
                        overallJobAvgPct, sampledJobAvgPct, maxJobCpuPct));
                    Console.WriteLine(string.Format("  * Sistem Geneli CPU        : Ort={0:F2}%, Tepe={1:F2}% (Orneklem Sayisi={2})",
                        sampledSysAvgPct, maxSysCpuPct, sampleCount));
                    Console.WriteLine(string.Format("  * Job Icindeki Surecler    : Toplam Baslatilan={0}, Maks Eszamanli Aktif={1}",
                        totalSpawnedProcs, maxActiveProcs));
                    Console.WriteLine(string.Format("  * Gozlenen Alt Surec Adlari: {0}",
                        namesArr.Length > 0 ? string.Join(", ", namesArr) : "powershell"));
                    Console.WriteLine("==================================================================");

                    return (int)exitCode;
                }
                finally
                {
                    Marshal.FreeHGlobal(pAcct);
                    Marshal.FreeHGlobal(pPidList);
                }
            }
            finally
            {
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            }
        }
        finally
        {
            CloseHandle(hJob);
        }
    }
}
'@

if (-not ("Mt2JobRunner" -as [type])) {
    Add-Type -TypeDefinition $csharpSource -Language CSharp
}

$rc = [Mt2JobRunner]::ExecuteLimited($RootDir, $Command, $CpuCapPercent, $MaxLogicalProcessors, $ParallelJobs)
exit $rc
