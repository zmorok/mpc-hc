"""Windows playback benchmark using MPC-HC's documented WM_COPYDATA API.

Use a separate portable copy with an INI; the script opens files and changes
playback state in its own /new instance. No third-party Python packages needed.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import json
from pathlib import Path
import statistics
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('videos', type=Path, nargs='+')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--cpu-seconds', type=float, default=10)
    args = parser.parse_args()
    if args.rounds < 1 or args.cpu_seconds <= 0:
        parser.error('rounds and cpu-seconds must be positive')
    if not args.exe.with_suffix('.ini').is_file():
        parser.error('use a separate portable copy with an INI beside the EXE')
    for path in [args.exe, *args.videos]:
        if not path.is_file():
            parser.error(f'file not found: {path}')

    user = C.WinDLL('user32', use_last_error=True)
    kernel = C.WinDLL('kernel32', use_last_error=True)
    callback = C.WINFUNCTYPE(C.c_ssize_t, W.HWND, W.UINT, C.c_size_t, C.c_ssize_t)

    class WindowClass(C.Structure):
        _fields_ = [('style', W.UINT), ('proc', callback), ('cls', C.c_int),
                    ('wnd', C.c_int), ('inst', W.HINSTANCE), ('icon', W.HICON),
                    ('cursor', W.HANDLE), ('bg', W.HBRUSH),
                    ('menu', W.LPCWSTR), ('name', W.LPCWSTR)]

    class CopyData(C.Structure):
        _fields_ = [('tag', C.c_size_t), ('size', W.DWORD), ('data', C.c_void_p)]

    user.DefWindowProcW.argtypes = [W.HWND, W.UINT, C.c_size_t, C.c_ssize_t]
    user.DefWindowProcW.restype = C.c_ssize_t
    user.CreateWindowExW.argtypes = [W.DWORD, W.LPCWSTR, W.LPCWSTR, W.DWORD,
        C.c_int, C.c_int, C.c_int, C.c_int, W.HWND, W.HMENU, W.HINSTANCE, C.c_void_p]
    user.CreateWindowExW.restype = W.HWND
    user.SendMessageTimeoutW.argtypes = [W.HWND, W.UINT, C.c_size_t, C.c_ssize_t,
        W.UINT, W.UINT, C.POINTER(C.c_size_t)]
    user.SendMessageTimeoutW.restype = C.c_ssize_t
    user.PeekMessageW.argtypes = [C.POINTER(W.MSG), W.HWND, W.UINT, W.UINT, W.UINT]
    user.DispatchMessageW.argtypes = [C.POINTER(W.MSG)]
    user.DestroyWindow.argtypes = [W.HWND]
    kernel.GetModuleHandleW.restype = W.HMODULE
    kernel.GetProcessTimes.argtypes = [W.HANDLE, *([C.POINTER(W.FILETIME)] * 4)]
    player = None
    responses = []

    @callback
    def wndproc(hwnd, msg, wp, lp):
        nonlocal player
        if msg == 0x4a:
            data = CopyData.from_address(lp)
            value = C.wstring_at(data.data) if data.data and data.size else ''
            responses.append((time.perf_counter(), data.tag, value))
            if data.tag == 0x50000000:
                player = wp or int(value)
            return 1
        return user.DefWindowProcW(hwnd, msg, wp, lp)

    cls = WindowClass()
    cls.proc, cls.inst, cls.name = wndproc, kernel.GetModuleHandleW(None), 'MpcPlaybackBenchmark'
    if not user.RegisterClassW(C.byref(cls)):
        raise C.WinError(C.get_last_error())
    master = user.CreateWindowExW(0, cls.name, cls.name, 0, 0, 0, 0, 0,
                                 None, None, cls.inst, None)
    if not master:
        raise C.WinError(C.get_last_error())

    def pump():
        msg = W.MSG()
        while user.PeekMessageW(C.byref(msg), None, 0, 0, 1):
            user.TranslateMessage(C.byref(msg))
            user.DispatchMessageW(C.byref(msg))

    def wait_for(predicate, timeout=15):
        end = time.perf_counter() + timeout
        while time.perf_counter() < end:
            pump()
            value = predicate()
            if value:
                return value
            if proc.poll() is not None:
                raise RuntimeError(f'player exited: {proc.returncode}')
            time.sleep(.005)
        raise TimeoutError('player did not reach the requested state')

    def delay(seconds):
        end = time.perf_counter() + seconds
        while time.perf_counter() < end:
            pump()
            time.sleep(.005)

    def send(command, value=''):
        buf = C.create_unicode_buffer(value)
        data = CopyData(command, C.sizeof(buf), C.cast(buf, C.c_void_p))
        result = C.c_size_t()
        started = time.perf_counter()
        if not user.SendMessageTimeoutW(player, 0x4a, master, C.addressof(data),
                                        2, 8000, C.byref(result)):
            raise TimeoutError(f'unresponsive player, command {command:#x}')
        return (time.perf_counter() - started) * 1000

    def position():
        before = len(responses)
        send(0xA0003004)
        return wait_for(lambda: [float(v) for _, c, v in responses[before:]
                                if c == 0x50000007])[-1]

    def duration():
        before = len(responses)
        send(0xA0003002)
        return wait_for(lambda: [float(v.split('|')[-1])
                                for _, c, v in responses[before:]
                                if c == 0x50000003 and v])[-1]

    def cpu_seconds():
        values = [W.FILETIME() for _ in range(4)]
        if not kernel.GetProcessTimes(W.HANDLE(proc._handle), *map(C.byref, values)):
            raise C.WinError(C.get_last_error())
        return sum((v.dwHighDateTime << 32) | v.dwLowDateTime for v in values[2:]) / 1e7

    result = {'exe': str(args.exe.resolve()), 'files': [],
              'note': 'API completion and graph-clock advance, not screen presentation latency; '
                      'CPU is percent of one logical CPU. Warm local files, muted portable instance.'}
    proc = subprocess.Popen([str(args.exe.resolve()), '/new', '/minimized',
                             '/slave', str(master)])
    try:
        wait_for(lambda: player)
        for video in args.videos:
            row = {'path': str(video.resolve()), 'open_loaded_ms': [],
                   'seek_command_ms': [], 'seek_advancing_ms': []}
            for _ in range(args.rounds):
                before = len(responses)
                started = time.perf_counter()
                send(0xA0000000, str(video.resolve()))
                stamp = wait_for(lambda: [t for t, c, v in responses[before:]
                                          if c == 0x50000001 and v == '2'])[0]
                row['open_loaded_ms'].append((stamp - started) * 1000)
                send(0xA0000004)
                length = duration()
                delay(.3)
                for fraction in [.5, .02, .8, .1]:
                    target = max(.1, length * fraction)
                    current = position()
                    started = time.perf_counter()
                    row['seek_command_ms'].append(send(0xA0003005, str(target - current)))
                    observed = wait_for(lambda: (p if abs((p := position()) - target) < 2 else None))
                    wait_for(lambda: position() > observed + .05)
                    row['seek_advancing_ms'].append((time.perf_counter() - started) * 1000)
                    delay(.1)
            start_cpu, start_time = cpu_seconds(), time.perf_counter()
            delay(args.cpu_seconds)
            row['playing_cpu_one_core_percent'] = 100 * (cpu_seconds() - start_cpu) / (time.perf_counter() - start_time)
            send(0xA0000005)
            start_cpu, start_time = cpu_seconds(), time.perf_counter()
            delay(args.cpu_seconds)
            row['paused_cpu_one_core_percent'] = 100 * (cpu_seconds() - start_cpu) / (time.perf_counter() - start_time)
            for key in ['open_loaded_ms', 'seek_command_ms', 'seek_advancing_ms']:
                row[key] = {'samples': row[key], 'median': statistics.median(row[key]), 'max': max(row[key])}
            result['files'].append(row)
            print(json.dumps(row, ensure_ascii=False), flush=True)
    finally:
        args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding='utf-8')
        # Close only the instance launched above, even if a measurement failed.
        if proc.poll() is None:
            try:
                send(0xA0004006)
                # Shutdown sends synchronous notifications to the master HWND.
                # Keep servicing them instead of blocking in Popen.wait().
                wait_for(lambda: proc.poll() is not None, timeout=10)
            except (TimeoutError, subprocess.TimeoutExpired):
                proc.terminate()
                proc.wait(timeout=10)
        user.DestroyWindow(master)


if __name__ == '__main__':
    main()
