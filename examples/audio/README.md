Audio analysis example for libPCA9685

DEPENDENCIES

```
$ sudo apt-get install libasound2
$ sudo apt-get install libasound2-dev
$ sudo apt-get install libfftw3-3
$ sudo apt-get install libfftw3-dev
```

DEFAULT AUDIO

On a debian system, to use the first USB sound card as the default,
create an /etc/asound.conf with:
```
pcm.!default {
        type plug
        slave {
                pcm "hw:1,0"
        }
}

ctl.!default {
        type hw
        card 1
}
```

NOTES

For an automated network build and install, download netinst.sh
and execute as root.
```
sudo ./netinst.sh
```
