"""Exercise the real Game Capture source on an unselected Windows desktop."""
import ctypes as c
from ctypes import wintypes as w
import os
import sys
import subprocess
import time
import xml.etree.ElementTree as ET
from pathlib import Path

class Startup(c.Structure):
    _fields_ = [('cb', w.DWORD), ('reserved', w.LPWSTR), ('desktop', w.LPWSTR), ('title', w.LPWSTR),
                ('x', w.DWORD), ('y', w.DWORD), ('cx', w.DWORD), ('cy', w.DWORD),
                ('charsx', w.DWORD), ('charsy', w.DWORD), ('fill', w.DWORD), ('flags', w.DWORD),
                ('show', w.WORD), ('reserved2', w.WORD), ('data', c.c_void_p),
                ('stdin', w.HANDLE), ('stdout', w.HANDLE), ('stderr', w.HANDLE)]
class Process(c.Structure):
    _fields_ = [('process', w.HANDLE), ('thread', w.HANDLE), ('pid', w.DWORD), ('tid', w.DWORD)]
class Security(c.Structure):
    _fields_ = [('size', w.DWORD), ('descriptor', c.c_void_p), ('inherit', w.BOOL)]

u = c.WinDLL('user32', use_last_error=True)
k = c.WinDLL('kernel32', use_last_error=True)
u.CreateDesktopW.argtypes = [w.LPCWSTR, c.c_void_p, c.c_void_p, w.DWORD, w.DWORD, c.c_void_p]
u.CreateDesktopW.restype = w.HANDLE
u.CloseDesktop.argtypes = [w.HANDLE]
k.CreateFileW.argtypes = [w.LPCWSTR,w.DWORD,w.DWORD,c.POINTER(Security),w.DWORD,w.DWORD,w.HANDLE]
k.CreateFileW.restype = w.HANDLE
k.CreateProcessW.argtypes = [w.LPCWSTR,w.LPWSTR,c.c_void_p,c.c_void_p,w.BOOL,w.DWORD,c.c_void_p,w.LPCWSTR,c.POINTER(Startup),c.POINTER(Process)]
k.WaitForSingleObject.argtypes = [w.HANDLE,w.DWORD]
k.GetExitCodeProcess.argtypes = [w.HANDLE,c.POINTER(w.DWORD)]
k.TerminateProcess.argtypes = [w.HANDLE,w.UINT]
k.CloseHandle.argtypes = [w.HANDLE]

obs = Path(__file__).resolve().parents[2]
cbz = obs.parent/'CheatBreakerZ'
evidence = obs/'.target/Testing/client-capture'
name = 'CBZCaptureTest-' + str(os.getpid())
desktop = u.CreateDesktopW(name,None,None,0,0x01ff,None)
if not desktop: raise c.WinError(c.get_last_error())
# Never SwitchDesktop or SetThreadDesktop: user's input desktop is untouched.
owned = []
mode = ['disconnect'] if '--disconnect' in sys.argv else []
label = 'disconnect' if mode else 'source'
def launch(args, cwd, logfile):
    sa = Security(c.sizeof(Security),None,True)
    out = k.CreateFileW(str(logfile),0x40000000,3,c.byref(sa),2,0x80,None)
    null = k.CreateFileW('NUL',0x80000000,3,c.byref(sa),3,0x80,None)
    if out == c.c_void_p(-1).value or null == c.c_void_p(-1).value: raise c.WinError(c.get_last_error())
    si = Startup(); si.cb = c.sizeof(si); si.desktop = name
    si.flags = 0x100; si.stdin = null; si.stdout = out; si.stderr = out
    pi = Process()
    try:
        command = c.create_unicode_buffer(subprocess.list2cmdline(args))
        if not k.CreateProcessW(None,command,None,None,True,0x08000000,None,str(cwd),c.byref(si),c.byref(pi)):
            raise c.WinError(c.get_last_error())
    finally:
        k.CloseHandle(out); k.CloseHandle(null)
    k.CloseHandle(pi.thread); owned.append(pi.process)
    return pi

try:
    report = ET.parse(cbz/'.target/maven/surefire-reports/TEST-com.cheatbreakerz.capture.CaptureSessionTest.xml')
    cp = next(p.attrib['value'] for p in report.findall('./properties/property') if p.attrib['name']=='java.class.path')
    producer = launch(['C:/Program Files/Java/jdk1.8.0_251/jre/bin/java.exe',
                       '-Djava.library.path='+str(cbz/'.target/capture-native/RelWithDebInfo'),
                       '-cp',cp,'com.cheatbreakerz.capture.CaptureProducerFixture'] + mode,
                      cbz/'.target/maven/test-work',evidence/(label+'-producer.log'))
    for _ in range(200):
        if b'FIXTURE_READY' in (evidence/(label+'-producer.log')).read_bytes(): break
        if k.WaitForSingleObject(producer.process,0)==0: raise RuntimeError('Producer exited')
        time.sleep(.05)
    else: raise RuntimeError('Producer startup timeout')
    os.environ['PATH']=str(obs/'.target/rundir/RelWithDebInfo/bin')+';'+os.environ['PATH']
    test=launch([str(obs/'.target/test/obspt-client-capture/source-test/RelWithDebInfo/OBS-PT.exe'), str(evidence/(label+'-recording.mp4'))] + mode,
                obs/'.target/rundir/RelWithDebInfo/bin',evidence/('game-'+label+'.log'))
    if k.WaitForSingleObject(test.process,45000)!=0: raise RuntimeError('Source test timed out')
    code=w.DWORD(); k.GetExitCodeProcess(test.process,c.byref(code))
    print((evidence/('game-'+label+'.log')).read_text(encoding='utf-8',errors='replace')[-5000:])
    if code.value: raise RuntimeError('Source test failed: '+str(code.value))
finally:
    for handle in reversed(owned):
        if k.WaitForSingleObject(handle,0)!=0: k.TerminateProcess(handle,1)
        k.WaitForSingleObject(handle,5000); k.CloseHandle(handle)
    u.CloseDesktop(desktop)
print('PASS isolated desktop source test; no input desktop switch; owned processes cleaned up')
