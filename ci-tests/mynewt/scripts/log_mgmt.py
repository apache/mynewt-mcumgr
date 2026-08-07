from util import*
from shlex import split

def log_clear(log_name="log", conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr log clear")
    cmd.append(log_name)
    cmd.extend(split(conn_name))
    out, err = run_cmd(cmd)
    return out == data

def log_lvl_list(log_name="log", conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr log level_list")
    cmd.append(log_name)
    cmd.extend(split(conn_name))
    return run_cmd(cmd)

def log_list(conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr log list")
    cmd.extend(split(conn_name))
    return run_cmd(cmd)

def log_module_list(log_name="log", conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr log module_list")
    cmd.append(log_name)
    cmd.extend(split(conn_name))
    return run_cmd(cmd)

def log_show(data = "", conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr log show")
    if (data != ""):
        cmd.append(data)
    cmd.extend(split(conn_name))
    return run_cmd(cmd)

