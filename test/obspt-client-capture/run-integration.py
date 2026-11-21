"""Run the real producer/receiver seam without a visible or activating window."""
import os
import subprocess
import time
import xml.etree.ElementTree as ET
from pathlib import Path

obs = Path(__file__).resolve().parents[2]
cbz = obs.parent / 'CheatBreakerZ'
evidence = obs / '.target/Testing/client-capture'
evidence.mkdir(parents=True, exist_ok=True)
report = ET.parse(cbz / '.target/maven/surefire-reports/TEST-com.cheatbreakerz.capture.CaptureSessionTest.xml')
classpath = next(p.attrib['value'] for p in report.findall('./properties/property') if p.attrib['name'] == 'java.class.path')
args = ['C:/Program Files/Java/jdk1.8.0_251/jre/bin/java.exe',
        '-Djava.library.path=' + str(cbz / '.target/capture-native/RelWithDebInfo'),
        '-cp', classpath, 'com.cheatbreakerz.capture.CaptureProducerFixture']
with (evidence / 'producer.log').open('wb') as log:
    producer = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT,
                                cwd=cbz / '.target/maven/test-work', creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        for _ in range(200):
            if b'FIXTURE_READY' in (evidence / 'producer.log').read_bytes():
                break
            if producer.poll() is not None:
                raise RuntimeError('Producer exited; inspect producer.log')
            time.sleep(.05)
        else:
            raise RuntimeError('Producer startup timeout')
        env = dict(os.environ)
        env['PATH'] = str(obs / '.target/rundir/RelWithDebInfo/bin') + ';' + env['PATH']
        with (evidence / 'cross-process.log').open('wb') as out:
            result = subprocess.run([str(obs / '.target/test/obspt-client-capture/RelWithDebInfo/OBS-PT.exe'), str(producer.pid)],
                                    cwd=obs / '.target/rundir/RelWithDebInfo/bin', env=env, stdout=out,
                                    stderr=subprocess.STDOUT, timeout=20, creationflags=subprocess.CREATE_NO_WINDOW)
        print((evidence / 'cross-process.log').read_text(encoding='utf-8', errors='replace')[-1800:])
        result.check_returncode()
    finally:
        if producer.poll() is None:
            producer.terminate()
        producer.wait(timeout=5)
print('PASS cross-process pixel and resize integration; owned processes cleaned up')
