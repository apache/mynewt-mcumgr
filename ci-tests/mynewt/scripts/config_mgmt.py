from util import*
from shlex import split

def config_read(var_name="test/str", conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr config")
    cmd.append(var_name)
    cmd.extend(split(conn_name))
    return run_cmd(cmd)

def config_write(var_name="test/str",
                 val="hello world",
                 type_str=true,
                 conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr config")
    cmd.append(var_name)
    cmd.append("\"" + val + "\"")
    cmd.extend(split(conn_name))
    return run_cmd(cmd)

def config_save(conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr config save")
    cmd.extend(split(conn_name))
    return run_cmd(cmd)
