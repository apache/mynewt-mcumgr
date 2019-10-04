from subprocess import Popen, PIPE
from shlex import split

def run_cmd(cmd):
    proc = Popen(cmd, stdout=PIPE, stderr=PIPE)
    out, err = proc.communicate()

    if proc.returncode:
        print(str(err))
        exit(-1)

    return out, err

def nrf52_clean_device():
    cmd = split("nrfjprog -e -f NRF52")
    return run_cmd(cmd)

def create_img(img_ver="0.0.1", tgt_name="slinky_nrf52dk"):
    cmd = split("newt create-image")
    cmd.append(tgt_name)
    cmd.append(img_ver)
    return run_cmd(cmd)

def load_img(img_ver="0.0.1", tgt_name="slinky_nrf52dk"):
    cmd = split("newt load")
    cmd.append(tgt_name)
    cmd.append(img_ver)
    return run_cmd(cmd)
