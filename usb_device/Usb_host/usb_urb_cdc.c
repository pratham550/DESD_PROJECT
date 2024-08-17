#include <linux/usb.h>
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define USB_CDC_VENDOR_ID   0x0483
#define USB_CDC_PRODUCT_ID  0xc1b0
#define USB_CDC_EP_IN       0x81    // Endpoint address for IN direction
#define USB_CDC_EP_OUT      0x01    // Endpoint address for OUT direction
#define USB_CDC_BUFSIZE     512     // Buffer size for data transfers

// USB structures
static struct usb_device *device;
static struct usb_class_driver class;
struct urb *read_urb; // Define URB for read operation
struct urb *write_urb; // Define URB for write operation
struct usb_anchor *anchor;
DECLARE_COMPLETION(read_completion);
DECLARE_COMPLETION(write_completion);

// Function to handle read completion
static void read_complete(struct urb *urb)
{
    // Signal completion event for read operation
    complete(&read_completion);
}

// Function to handle write completion
static void write_complete(struct urb *urb)
{
    // Signal completion event for write operation
    complete(&write_completion);
}

static int cdc_open(struct inode *pinode, struct file *pfile);
static int cdc_close(struct inode *pinode, struct file *pfile);
static char data[USB_CDC_BUFSIZE] = "LED";
static ssize_t cdc_read(struct file *pfile, char __user *ubuf, size_t usize, loff_t *pfpos);
static ssize_t cdc_write(struct file *pfile, const char __user *ubuf, size_t usize, loff_t *pfpos);

static int cdc_probe(struct usb_interface *interface, const struct usb_device_id *id);
static void cdc_disconnect(struct usb_interface *interface);

// Device file operations structure variables
static struct file_operations cdc_fops = {
    .owner = THIS_MODULE,
    .open = cdc_open,
    .release = cdc_close,
    .read = cdc_read,
    .write = cdc_write
};

// Device ID table for entry
static struct usb_device_id cdc_device_table[] = {
    { USB_DEVICE(USB_CDC_VENDOR_ID, USB_CDC_PRODUCT_ID) },
    {}
};

// USB device functionalities
static struct usb_driver cdc_driver = {
    .name = "usb_cdc_driver",
    .probe = cdc_probe,
    .disconnect = cdc_disconnect,
    .id_table = cdc_device_table,
};

// Kernel module init
static int __init cdc_init(void)
{   
    int ret;
    
    // Allocating memory for the anchor
    anchor = kzalloc(sizeof(struct usb_anchor), GFP_KERNEL);
    if (!anchor) 
    {
        printk(KERN_ERR "Failed to allocate memory for anchor.\n");
        return -ENOMEM;
    }

    // Initialization of the spinlock inside the anchor structure
    spin_lock_init(&anchor->lock);
    // Initialization of the list head inside the anchor structure
    INIT_LIST_HEAD(&anchor->urb_list);

    ret = usb_register(&cdc_driver);
    if (ret < 0)
    {
        printk(KERN_ERR "Failed to register USB CDC driver: %d\n", ret);
        kfree(anchor);
        return ret;
    }
    printk(KERN_INFO "USB driver registered.\n");
    return 0;
}

// Kernel module exit
static void __exit cdc_exit(void)
{
    usb_deregister(&cdc_driver);
    kfree(anchor);
    printk(KERN_INFO "USB Driver Deregistered.\n");
}

module_init(cdc_init);
module_exit(cdc_exit);

// USB device function definitions
static int cdc_probe(struct usb_interface *interface, const struct usb_device_id *id)
{   

    int ret;
    struct usb_host_interface *iface_desc;

    iface_desc = interface->cur_altsetting;
    printk(KERN_INFO "USB CDC device plugged with vendor id: %x & product id: %x.\n",
           id->idVendor, id->idProduct);
    device = interface_to_usbdev(interface);
    if (!device)
    {
        printk(KERN_ERR "%s : interface_to_usbdev() failed !\n", THIS_MODULE->name);
        return -ENOMEM;
    }
    printk(KERN_INFO "New USB Device connected: %s.\n", device->devpath);
    class.name = "usb_cdc%d";
    class.fops = &cdc_fops;
    ret = usb_register_dev(interface, &class);
    if (ret < 0) {
        printk(KERN_ERR "usb_register_dev() failed.\n");
        return ret;
    }
    printk(KERN_INFO "Device Registered successful.\n");
    return 0;
}

static void cdc_disconnect(struct usb_interface *interface)
{ 
    usb_deregister_dev(interface, &class);
    printk(KERN_INFO "USB CDC device disconnected\n");
}

// Device file operations definitions
static int cdc_open(struct inode *pinode, struct file *pfile)
{   
    printk(KERN_INFO "cdc_open() called.\n");
    return 0;
}

static int cdc_close(struct inode *pinode, struct file *pfile)
{
    printk(KERN_INFO "cdc_close() called.\n");
    return 0;
}

static ssize_t cdc_read(struct file *pfile, char __user *ubuf, size_t usize, loff_t *pfpos)
{
    int pipe, ret_val, length = 0;
    char *buffer;
    
    printk(KERN_INFO "%s : cdc_read() called.\n", THIS_MODULE->name);
    
    // Initialize the pipe in the direction client -> host
    pipe = usb_rcvbulkpipe(device, USB_CDC_EP_IN);

    // Allocating buffer to receive data
    buffer = kmalloc(USB_CDC_BUFSIZE, GFP_KERNEL);
    if (!buffer) 
    {
        printk(KERN_ERR "Failed to allocate memory for receive buffer.\n");
        return -ENOMEM;
    }
    printk("%s : Buffer allocated Successfully.\n", THIS_MODULE->name);
    
    // Allocating the URB
    read_urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!read_urb) 
    {
        printk(KERN_ERR "Failed to allocate URB.\n");
        kfree(buffer);
        return -ENOMEM;
    }
	printk(KERN_INFO "urb in read allocated successfully.\n");
    // Filling the URB for transfer
    usb_fill_bulk_urb(read_urb, device, pipe, buffer, USB_CDC_BUFSIZE, read_complete, NULL);
    
    // Submitting the URB for transfer
    ret_val = usb_submit_urb(read_urb, GFP_KERNEL);
    if (ret_val < 0) 
    {
        printk(KERN_ERR "Failed to submit URB for bulk transfer.\n");
        kfree(buffer);
        usb_free_urb(read_urb);
        return ret_val;
    }
    
    // Waiting for transfer
    wait_for_completion(&read_completion);
    
    // Actual length of data received
    length = read_urb->actual_length;
    
    // Copying the data to user buffer
    if (copy_to_user(ubuf, buffer, min((unsigned long)length, (unsigned long)usize))) 
    {
        printk(KERN_ERR "Failed to copy data to user buffer.\n");
        kfree(buffer);
        usb_free_urb(read_urb);
        return -EFAULT;
    }
    
    kfree(buffer);
    usb_free_urb(read_urb);
    
    printk(KERN_INFO "%s : cdc_read() executed.\n", THIS_MODULE->name);

    return length;
}

static ssize_t cdc_write(struct file *pfile, const char __user *ubuf, size_t usize, loff_t *pfpos)
{
    int pipe, ret_val, length = 0;
    
    printk(KERN_INFO "%s : cdc_write() called.\n", THIS_MODULE->name);
    
    // Initialize the pipe in the direction host -> client
    pipe = usb_sndbulkpipe(device, USB_CDC_EP_OUT);
    
    // Allocating an URB
    write_urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!write_urb) 
    {
        printk(KERN_ERR "Failed to allocate URB.\n");
        return -ENOMEM;
    }
	printk(KERN_INFO "urb in write allocated successfully.\n");

    // Copy data from user buffer
    if (copy_from_user(data, ubuf, min((unsigned long)usize, (unsigned long)USB_CDC_BUFSIZE))) 
    {
        printk(KERN_ERR "Failed to copy data from user buffer.\n");
        usb_free_urb(write_urb);
        return -EFAULT;
    }
    
    // Filling the URB for transfer
    usb_fill_bulk_urb(write_urb, device, pipe, data, min((unsigned long)usize, (unsigned long)USB_CDC_BUFSIZE), write_complete, NULL);
    
    // Submitting the URB for transfer
    ret_val = usb_submit_urb(write_urb, GFP_KERNEL);
    if (ret_val < 0) 
    {
        printk(KERN_ERR "Failed to submit URB for bulk transfer.\n");
        usb_free_urb(write_urb);
        return ret_val;
    }
    
    // Waiting for transfer to complete
    wait_for_completion(&write_completion);

    // Actual length of the data
    length = write_urb->actual_length;

    usb_free_urb(write_urb);
   
   printk(KERN_INFO "%s : cdc_write() executed.\n", THIS_MODULE->name);
   
   return length;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Project_PG-DESD : Linux Device - Driver");
MODULE_DESCRIPTION("Prototyping a USB_CDC linux Device - Driver using a Microcontroller config device");

