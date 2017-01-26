// based on USBNotification Example found in IOUSBFamily-630.4.5.tar.gz, available at https://opensource.apple.com

#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

#include <mach/mach.h>


#define kOurVendorID 0x1050
#define kOurProductID 0x0010
//#define kOurReleaseID 0x0223

// globals
static IONotificationPortRef	gNotifyPort;
static io_iterator_t		gRawAddedIter;
static io_iterator_t		gRawRemovedIter;

void RawDeviceAdded(void *refCon, io_iterator_t iterator) {
    kern_return_t		kr;
    io_service_t		usbDevice;
    IOCFPlugInInterface 	**plugInInterface=NULL;
    IOUSBDeviceInterface245 	**dev=NULL;
    HRESULT 			res;
    SInt32 			score;
    UInt16			vendor;
    UInt16			product;
//    UInt16			release;

    while ( (usbDevice = IOIteratorNext(iterator)) )
    {
        printf("Raw device added.\n");
        
        kr = IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);
        kr = IOObjectRelease(usbDevice);				// done with the device object now that I have the plugin
        if ((kIOReturnSuccess != kr) || !plugInInterface)
        {
            printf("unable to create a plugin (%08x)\n", kr);
            continue;
        }
        
        // I have the device plugin, I need the device interface
        res = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID245), (LPVOID)&dev);
        IODestroyPlugInInterface(plugInInterface);			// done with this
        
        if (res || !dev)
        {
            printf("couldn't create a device interface (%08x)\n", (int) res);
            continue;
        }
        // technically should check these kr values
        kr = (*dev)->GetDeviceVendor(dev, &vendor);
        kr = (*dev)->GetDeviceProduct(dev, &product);
//        kr = (*dev)->GetDeviceReleaseNumber(dev, &release);
//        if ((vendor != kOurVendorID) || (product != kOurProductID) || (release != kOurReleaseID))
        if ((vendor != kOurVendorID) || (product != kOurProductID))
        {
            // We should never get here because the matching criteria we specified above
            // will return just those devices with our vendor and product IDs
            printf("found device i didn't want (vendor = %d, product = %d)\n", vendor, product);
            (void) (*dev)->Release(dev);
            continue;
        }
        
        // need to open the device in order to change its state
        kr = (*dev)->USBDeviceOpen(dev);
        if (kIOReturnSuccess != kr)
        {
            printf("unable to open device: %08x\n", kr);
            (void) (*dev)->Release(dev);
            continue;
        }

        kr = (*dev)->USBDeviceClose(dev);
        kr = (*dev)->Release(dev);
    }
}

void RawDeviceRemoved(void *refCon, io_iterator_t iterator)
{
    kern_return_t	kr;
    io_service_t	obj;
    
    while ( (obj = IOIteratorNext(iterator)) )
    {
        printf("Raw device removed.\n");
 
        pid_t p = fork();
        int status;
        if (p == 0) {
            execl("/System/Library/CoreServices/Menu Extras/User.menu/Contents/Resources/CGSession", "/System/Library/CoreServices/Menu Extras/User.menu/Contents/Resources/CGSession", "-suspend", NULL);
            if (errno == ENOENT)
                _exit(-1);
            _exit(-2);
        }
        wait(&status);
        if (WIFEXITED(status)) {
            printf("exited, status=%d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("killed by signal %d\n", WTERMSIG(status));
        } else if (WIFSTOPPED(status)) {
            printf("stopped by signal %d\n", WSTOPSIG(status));
        } else if (WIFCONTINUED(status)) {
            printf("continued\n");
        }
        
        kr = IOObjectRelease(obj);
    }
}

void SignalHandler(int sigraised)
{
    printf("\nInterrupted\n");
    
    // Clean up here
    IONotificationPortDestroy(gNotifyPort);
    
    if (gRawAddedIter)
    {
        IOObjectRelease(gRawAddedIter);
        gRawAddedIter = 0;
    }
    
    if (gRawRemovedIter)
    {
        IOObjectRelease(gRawRemovedIter);
        gRawRemovedIter = 0;
    }
    
    // exit(0) should not be called from a signal handler.  Use _exit(0) instead
    //
    _exit(0);
}

int main (int argc, const char *argv[])
{
    mach_port_t 		masterPort;
    CFMutableDictionaryRef 	matchingDict;
    CFRunLoopSourceRef		runLoopSource;
    kern_return_t		kr;
    long			usbVendor = kOurVendorID;
    long			usbProduct = kOurProductID;
//    SInt32			usbRelease = kOurReleaseID;
    sig_t			oldHandler;
    
    // pick up command line arguments
    if (argc > 1)
        usbVendor = atoi(argv[1]);
    if (argc > 2)
        usbProduct = atoi(argv[2]);
    
    // Set up a signal handler so we can clean up when we're interrupted from the command line
    // Otherwise we stay in our run loop forever.
    oldHandler = signal(SIGINT, SignalHandler);
    if (oldHandler == SIG_ERR)
        printf("Could not establish new signal handler");
    
    // first create a master_port for my task
    kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (kr || !masterPort)
    {
        printf("ERR: Couldn't create a master IOKit Port(%08x)\n", kr);
        return -1;
    }
    
//    printf("Looking for devices matching vendor ID=%ld, product ID=%ld and release=%ld\n", usbVendor, usbProduct, usbRelease);
    printf("Looking for devices matching vendor ID=%ld and product ID=%ld\n", usbVendor, usbProduct);

    // Set up the matching criteria for the devices we're interested in
    matchingDict = IOServiceMatching(kIOUSBDeviceClassName);	// Interested in instances of class IOUSBDevice and its subclasses
    if (!matchingDict)
    {
        printf("Can't create a USB matching dictionary\n");
        mach_port_deallocate(mach_task_self(), masterPort);
        return -1;
    }
    
    // Add our vendor and product IDs to the matching criteria
    CFDictionarySetValue(
                         matchingDict,
                         CFSTR(kUSBVendorID),
                         CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbVendor));
    CFDictionarySetValue(
                         matchingDict,
                         CFSTR(kUSBProductID),
                         CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbProduct));
    
    // Create a notification port and add its run loop event source to our run loop
    // This is how async notifications get set up.
    gNotifyPort = IONotificationPortCreate(masterPort);
    runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);
    
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopDefaultMode);
    
    // Retain additional references because we use this same dictionary with four calls to
    // IOServiceAddMatchingNotification, each of which consumes one reference.
    matchingDict = (CFMutableDictionaryRef) CFRetain( matchingDict );
    matchingDict = (CFMutableDictionaryRef) CFRetain( matchingDict );
    matchingDict = (CFMutableDictionaryRef) CFRetain( matchingDict );
    
    // Now set up two notifications, one to be called when a raw device is first matched by I/O Kit, and the other to be
    // called when the device is terminated.
    kr = IOServiceAddMatchingNotification(  gNotifyPort,
                                          kIOFirstMatchNotification,
                                          matchingDict,
                                          RawDeviceAdded,
                                          NULL,
                                          &gRawAddedIter );
    
    RawDeviceAdded(NULL, gRawAddedIter);	// Iterate once to get already-present devices and
    // arm the notification
    
    kr = IOServiceAddMatchingNotification(  gNotifyPort,
                                          kIOTerminatedNotification,
                                          matchingDict,
                                          RawDeviceRemoved,
                                          NULL,
                                          &gRawRemovedIter );
    
    RawDeviceRemoved(NULL, gRawRemovedIter);	// Iterate once to arm the notification

    // Now done with the master_port
    mach_port_deallocate(mach_task_self(), masterPort);
    masterPort = 0;
    
    // Start the run loop. Now we'll receive notifications.
    CFRunLoopRun();
    
    // We should never get here
    return 0;
}
