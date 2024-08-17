#include <linux/module.h>
#include <linux/usb.h>

#define USB_CDC_VENDOR_ID   0x0483
#define USB_CDC_PRODUCT_ID  0xc1b0
#define USB_CDC_EP_IN       0x82    // Endpoint address for IN direction
#define USB_CDC_EP_OUT      0x01    // Endpoint address for OUT direction
#define USB_CDC_BUFSIZE     32      // Buffer size for data transfers



// USB CDC device id table
static struct usb_device_id cdc_device_table[] = {
    { USB_DEVICE(USB_CDC_VENDOR_ID, USB_CDC_PRODUCT_ID) },
    {} 
};

static struct usb_device *device;
static struct usb_class_driver class;

static int cdc_open(struct inode *pinode, struct file *pfile) 
{
    printk(KERN_INFO "%s: cdc_open() called.\n", THIS_MODULE->name);
    return 0;
}

static int cdc_close(struct inode *pinode, struct file *pfile) 
{
    printk(KERN_INFO "%s: cdc_close() called.\n", THIS_MODULE->name);
    return 0;
}

static char data[USB_CDC_BUFSIZE] = "";
static ssize_t cdc_read(struct file *pfile, char __user *ubuf, size_t usize, loff_t *pfpos) 
{
	int length=0;
	printk(KERN_INFO "%s : cdc_read() called.\n",THIS_MODULE->name);
	
	
	return length;
}


static ssize_t cdc_write(struct file *pfile, const char __user *ubuf, size_t usize, loff_t *pfpos) 
{	
	int pipe,ret_val,length=0;
	printk(KERN_INFO "%s : cdc_write() called.\n",THIS_MODULE->name);
	//data = "LED";
	//initializing the pipe in direction host -> client
	pipe = usb_sndbulkpipe(device, USB_CDC_EP_OUT);
	//sending data over the initialized pipe
	ret_val = usb_bulk_msg(device, pipe, data, sizeof("LED"), &length, 5000);
	if(ret_val < 0)
	{
		printk(KERN_ERR "%s: failed to send LED request to the client.\n", THIS_MODULE->name);
        	return ret_val;
	}
	printk(KERN_INFO "%s : cdc_write Executed.\n", THIS_MODULE->name);	
    	return length;
}

static struct file_operations cdc_fops = {
    .owner = THIS_MODULE,
    .open = cdc_open,
    .release = cdc_close,
    .read = cdc_read,
    .write = cdc_write
};

MODULE_DEVICE_TABLE(usb, cdc_device_table); 



static int cdc_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    int ret;
    struct usb_host_interface *iface_desc;
	
    iface_desc = interface->cur_altsetting;
    printk(KERN_INFO "%s: stm device plugged with vendor id :- %d & product id :- %d.\n ", THIS_MODULE->name, id->idVendor, id->idProduct);
    device = interface_to_usbdev(interface);
    printk(KERN_INFO "%s: New USB Device connected: %s.\n", THIS_MODULE->name, device->devpath);
    class.name = "usb_cdc%d";
    class.fops = &cdc_fops;
    ret = usb_register_dev(interface, &class);
    if(ret < 0) {
        printk(KERN_ERR "%s: usb_register_dev() failed.\n", THIS_MODULE->name);
        return ret;
    }
    printk(KERN_INFO "%s: usb_register_dev() successful.\n", THIS_MODULE->name);
    return 0;
    
}


static void cdc_disconnect(struct usb_interface *interface)
{

    usb_deregister_dev(interface, &class);
    printk(KERN_INFO "%s: USB CDC device disconnected\n", THIS_MODULE->name);
    
}

// USB driver structure
static struct usb_driver cdc_driver = 
{
    .name = "usb_cdc_driver",
    .probe = cdc_probe,
    .disconnect = cdc_disconnect,
    .id_table = cdc_device_table,
};

// Initialization function
static __init int cdc_init(void)
{
    int ret = usb_register(&cdc_driver);
    if (ret < 0)
    {
        printk(KERN_ERR "Failed to register USB CDC driver: %d\n", ret);
    	return ret;
    }
    printk(KERN_INFO "%s: USB driver registered.\n", THIS_MODULE->name);
    return 0;
}

// Exit function
static __exit void cdc_exit(void)
{
    usb_deregister(&cdc_driver);
    printk(KERN_INFO "%s: USB Driver Deregistered.\n", THIS_MODULE->name);
}

// Module initialization
module_init(cdc_init);
module_exit(cdc_exit);

// Mod info
MODULE_LICENSE("GPL");
MODULE_AUTHOR("USB_DD_PG-DESD");
MODULE_DESCRIPTION("USB_Device-Driver_Prototype");
