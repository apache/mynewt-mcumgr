from img_mgmt import *
from os_mgmt import *
from util import *
from time import sleep

def main():
    out, err = nrf52_clean_device()
    out, err = load_img("0.0.1", "mcuboot_nrf52dk")
    out, err = create_img()
    out, err = load_img()
    sleep(1) #let the device boot before anything else

    if not test_echo():
        print("echo failed")

    hashes, out, err = img_list()

    '''Testing image upload with pre-erase.'''
    out, err = create_img(img_ver="0.0.2")
    path = "bin/targets/slinky_nrf52dk/app/apps/slinky/slinky.img"
    out, err = img_upload(path)
    hashes, out, err = img_list()
    print(hashes)

    img_settest(hashes[1])
    reset()
    sleep(5)
    hashes, out, err = img_list()
    print(hashes)

    img_setconfirm(hashes[0])
    img_erase(hashes[1], conn_name="-c conn_ttyusb0")
    hashes, out, err = img_list()
    print(hashes)

    '''Testing image upload with lazy-erase.'''
    out, err = create_img(img_ver="0.0.3")
    out, err = img_upload(path, True)
    hashes, out, err = img_list()
    print(hashes)

    img_settest(hashes[1])
    reset()
    sleep(5)
    hashes, out, err = img_list()
    print(hashes)

    img_setconfirm(hashes[0])
    img_erase(hashes[1], conn_name="-c conn_ttyusb0")
    hashes, out, err = img_list()
    print(hashes)


if __name__ == "__main__":
    main()
