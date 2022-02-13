import subprocess
import os
import shutil
import time
import signal
from datetime import datetime

def rm_dir(dir_name):
    if os.path.exists(dir_name):
        try:
            shutil.rmtree(dir_name)
        except OSError as e:
            print("remove directory error: %s - %s." % (e.filename, e.strerror))

def build_project(source_dir, build_dir):
    print(source_dir)
    print(build_dir)
    start_cwd = os.getcwd()
    os.chdir(build_dir)
    process = subprocess.Popen(['cmake', source_dir], stdout=subprocess.PIPE)
    output, error = process.communicate()
    process = subprocess.Popen(['make'], stdout=subprocess.PIPE)
    output, error = process.communicate()
    os.chdir(start_cwd)

def generate_test_dir(parent_dir, files_count):
    test_dir = os.path.join(parent_dir, 'test')
    rm_dir(test_dir)
    os.mkdir(test_dir)
    #create files
    for i in range(1, files_count + 1):
        with open(os.path.join(test_dir, str(i) + '.txt'), 'w') as f:
            f.write(str(i) * i)
    return test_dir

def run_daemon(daemon_dir, daemon_exe, watch_dir, timeout):
    run_cmd = os.path.join(daemon_dir, daemon_exe) + ' -d ' + watch_dir + ' -t ' + str(timeout)
    process = subprocess.Popen(run_cmd.split(), stdout=subprocess.PIPE)
    process.communicate()

def get_daemon_pid(daemon_exe):
    process = subprocess.Popen(['ps', '-A', 'ww'], stdout=subprocess.PIPE)
    output, error = process.communicate()
    for line in output.splitlines():
        if daemon_exe in str(line):
            return int(line.split(None, 1)[0])
    return -1

def request_dir_check(daemon_exe):
    pid = get_daemon_pid(daemon_exe)
    if pid > 0:
        os.kill(pid, signal.SIGUSR1)

def stop_daemon(daemon_exe, kill = False):
    pid = get_daemon_pid(daemon_exe)
    if pid > 0:
        if kill:
            os.kill(pid, signal.SIGKILL)
        else:
            os.kill(pid, signal.SIGTERM)

def kill_daemon(daemon_exe):
    stop_daemon(daemon_exe, True)

def daemon_exists(daemon_exe):
    if get_daemon_pid(daemon_exe) > 0:
        return True
    return False

def check_daemons_log(from_time, check_str, times):
    p1 = subprocess.Popen(['journalctl', '-q', '--since', from_time], stdout=subprocess.PIPE)
    p2 = subprocess.Popen(["grep", "crc"], stdin=p1.stdout, stdout=subprocess.PIPE)
    p1.stdout.close()
    output, error = p2.communicate()
    return output.decode('utf-8').count(check_str) == times

if __name__ == "__main__":
    source_dir = os.path.dirname(os.path.realpath(__file__))
    build_dir = os.path.join(os.getcwd(), 'build')
    if os.path.exists(build_dir):
        try:
            shutil.rmtree(build_dir)
        except OSError as e:
            print("remove build directory error: %s - %s." % (e.filename, e.strerror))
    os.mkdir(build_dir)
    build_project(source_dir, build_dir)

    daemon_exe = 'crc32_check_daemon'
    #
    print('TEST 1: start daemon with args... ')
    run_daemon(build_dir, daemon_exe, '/proc', 1)
    result = daemon_exists(daemon_exe)
    kill_daemon(daemon_exe)
    assert result == True, 'failed: ' + daemon_exe + " process doesn't exist"
    print('Ok')
    #
    print('TEST 2: stop daemon with SIGTERM... ')
    run_daemon(build_dir, daemon_exe, '/proc', 1)
    time.sleep(1)
    stop_daemon(daemon_exe)
    time.sleep(1)
    result = daemon_exists(daemon_exe)
    assert result == False, (kill_daemon(daemon_exe) and False) or 'failed: ' + daemon_exe + " process still exists"
    print('Ok')
    #
    print("TEST 3: run daemon for 3 seconds without dir's modification... ")
    test_dir = generate_test_dir(build_dir, 5)
    start_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    run_daemon(build_dir, daemon_exe, test_dir, 1)
    time.sleep(3)
    kill_daemon(daemon_exe)
    rm_dir(test_dir)
    result = check_daemons_log(start_time, "Integrity check: OK", 3)
    assert result == True, 'failed: ' + daemon_exe + " wrong logs"
    print('Ok')
    #
    time.sleep(2) # wait old daemon stops
    print("TEST 4: request check with SIGUSR1... ")
    test_dir = generate_test_dir(build_dir, 5)
    start_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    run_daemon(build_dir, daemon_exe, test_dir, 100) #BIG timeout
    for _ in range(10): request_dir_check(daemon_exe)
    kill_daemon(daemon_exe)
    rm_dir(test_dir)
    result = check_daemons_log(start_time, "Integrity check: OK", 10)
    assert result == True, 'failed: ' + daemon_exe + " wrong logs"
    print('Ok')
    #
    time.sleep(2) # wait old daemon stops
    print("TEST 5: add new file... ")
    test_dir = generate_test_dir(build_dir, 5)
    start_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    run_daemon(build_dir, daemon_exe, test_dir, 100) #BIG timeout
    #create new file
    time.sleep(1)
    with open(os.path.join(test_dir, '6.txt'), 'w') as f:
        f.write('6'*6)
    time.sleep(1)
    request_dir_check(daemon_exe)
    kill_daemon(daemon_exe)
    rm_dir(test_dir)
    result = check_daemons_log(start_time, "Integrity check: FAIL (6.txt - new file)", 1)
    assert result == True, 'failed: ' + daemon_exe + " wrong logs"
    print('Ok')
    #
    time.sleep(2) # wait old daemon stops
    print("TEST 6: change existing file... ")
    test_dir = generate_test_dir(build_dir, 5)
    start_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    run_daemon(build_dir, daemon_exe, test_dir, 100) #BIG timeout
    #create new file
    time.sleep(1)
    with open(os.path.join(test_dir, '3.txt'), 'w') as f:
        f.write('7'*7)
    time.sleep(1)
    request_dir_check(daemon_exe)
    kill_daemon(daemon_exe)
    rm_dir(test_dir)
    result = check_daemons_log(start_time, "Integrity check: FAIL (3.txt - new crc", 1)
    assert result == True, 'failed: ' + daemon_exe + " wrong logs"
    print('Ok')
    #
    time.sleep(2) # wait old daemon stops
    print("TEST 7: delete existing file... ")
    test_dir = generate_test_dir(build_dir, 5)
    start_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    run_daemon(build_dir, daemon_exe, test_dir, 100) #BIG timeout
    #create new file
    time.sleep(1)
    os.remove(os.path.join(test_dir, '2.txt'))
    time.sleep(1)
    request_dir_check(daemon_exe)
    kill_daemon(daemon_exe)
    rm_dir(test_dir)
    result = check_daemons_log(start_time, "Integrity check: FAIL (2.txt - file was deleted)", 1)
    assert result == True, 'failed: ' + daemon_exe + " wrong logs"
    print('Ok')

    #TODO check crc32
    #change file and check logs
