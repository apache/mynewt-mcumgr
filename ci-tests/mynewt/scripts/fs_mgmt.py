from util import*
from shlex import split

def fs_upload(path="file_up", conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr fs upload")
    cmd.append(path)
    cmd.extend(split(conn_name))
    out, err = run_cmd(cmd)
    return out == data

def fs_download(path="file_down", conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr fs download")
    cmd.append(path)
    cmd.extend(split(conn_name))
    out, err = run_cmd(cmd)
    return out == data
