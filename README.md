## Description

I use YubiKeys for second-factor in my macOS.
I felt the need to lock the mac when i remove the yubikey from the USB.
I havent found a tool to do this (maybe i was searching in the wrong side of the internet).

This (working) solution is based on [USBNotification Example](https://opensource.apple.com/tarballs/IOUSBFamily/ "from Apple")

I've tried other approaches, without success.
 - [BritishKeyboardDetector](https://github.com/coverprice/britishkeyboarddetector/ "from James Russell") : detach don't work
 - [HardwareGrowl](https://github.com/cirrusj/HardwareGrowler "from cirrusj") : i've simplified but stopped when i was looking to match vendor and product IDs
 - [Lock Me Now](https://github.com/IGRSoft/LockMeNow "from IGR Software") : they are locked to iDevices hope they see my ticket and implement YubiKey support

## Install

#### Double check if the Product and Vendor IDs are the same as i'm using. Run this command:
```bash
$ system_profiler SPUSBDataType | grep -B 1 0x1050

   Product ID: 0x0010
   Vendor ID: 0x1050
```


   If not the same, edit main.c and change according. (open a GH issue so i can support it)


#### Install everything in you mac. Run this:
```bash
$ ./install
```

#### Check if everything is OK. Run:
- insert your YubiKey
- remove it from the USB
- ... your mac should be locked

## How it works

The  plist file will launch the daemon 'yklock' which simply consumes the USB event, and then calls
`CGSession -suspend` to lock your mac.

## Notes

GCSession should be changed to native code
LaunchEvents and com.apple.iokit.matching should have the detach event

### DEBUG
You can run the daemon, and see the output
```bash
$ $HOME/.yklock/yklock
````