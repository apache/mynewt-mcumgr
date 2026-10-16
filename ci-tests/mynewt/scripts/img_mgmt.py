from util import *

def img_list(conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr image list")
    cmd.extend(split(conn_name))
    out, err = run_cmd(cmd)

    '''Extracting image hashes.'''
    out_lst = out.decode("utf-8").strip('\\').split()
    try:
        hash0_idx = out_lst.index("hash:") + 1
    except ValueError:
        return None, out, err

    try:
        hash1_idx = out_lst.index("hash:", hash0_idx + 1) + 1
    except ValueError:
        return [out_lst[hash0_idx]], out, err

    return [out_lst[hash0_idx], out_lst[hash1_idx]], out, err

def img_upload(img_path, lazy=False, conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr image upload")
    cmd.append(img_path)
    if lazy:
        cmd.append("-e")
    cmd.extend(split(conn_name))
    return run_cmd(cmd)

def img_settest(img_hash, conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr image test")
    cmd.append(img_hash)
    cmd.extend(split(conn_name))
    return run_cmd(cmd)

def img_setconfirm(img_hash, conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr image confirm")
    cmd.append(img_hash)
    cmd.extend(split(conn_name))
    return run_cmd(cmd)

def img_erase(img_hash, conn_name="-c conn_ttyusb0"):
    cmd = split("newtmgr image erase")
    cmd.append(img_hash)
    cmd.extend(split(conn_name))
    return run_cmd(cmd)
