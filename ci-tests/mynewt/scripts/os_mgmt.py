from util import*
from shlex import split

def test_echo(data = "hello world", conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr echo")
    cmd.append(data)
    cmd.extend(split(conn_name))
    out, err = run_cmd(cmd)
    return out == data

def test_console_echo(data, conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr echo")
    cmd.append(data)
    cmd.extend(split(conn_name))
    out, err = run_cmd(cmd)
    return out == dat

def task_info(task_idx=0, conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr task info")
    cmd.append(task_idx)
    cmd.extend(split(conn_name))
    return run_cmd(cmd)

def mpstat_read(conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr task info")
    cmd.append(task_idx)
    cmd.extend(split(conn_name))
    return run_cmd(cmd)

def datetime_get(conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr task info")
    cmd.append(task_idx)
    cmd.extend(split(conn_name))
    return run_cmd(cmd)

def datetime_set(datetime, conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr datetime set")
    cmd.append(datetime)
    cmd.extend(split(conn_name))
    return run_cmd(cmd)

def reset(conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr reset")
    cmd.extend(split(conn_name))
    return run_cmd(cmd)
