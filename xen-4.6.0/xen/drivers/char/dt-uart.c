/*
 * xen/drivers/char/dt-uart.c
 *
 * Generic uart retrieved via the device tree
 *
 * Julien Grall <julien.grall@linaro.org>
 * Copyright (c) 2013 Linaro Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <asm/device.h>
#include <asm/types.h>
#include <xen/console.h>
#include <xen/device_tree.h>
#include <xen/serial.h>
#include <xen/errno.h>

/*
 * Configure UART port with a string:
 * path:options
 *
 * @path: full path used in the device tree for the UART. If the path
 * doesn't start with '/', we assuming that it's an alias.
 * @options: UART speficic options (see in each UART driver)
 */
static char __initdata opt_dtuart[256] = "";
string_param("dtuart", opt_dtuart);

void __init dt_uart_init(void)
{
    struct dt_device_node *dev;
    int ret;
    const char *devpath = opt_dtuart;
    char *options;

    if ( !console_has("dtuart") )
        return; /* Not for us */

    if ( !strcmp(opt_dtuart, "") )
    {
        const struct dt_device_node *chosen = dt_find_node_by_path("/chosen");

        if ( chosen )
        {
            const char *stdout;

            ret = dt_property_read_string(chosen, "stdout-path", &stdout);
            if ( ret >= 0 )
            {
                printk("Taking dtuart configuration from /chosen/stdout-path\n");
                if ( strlcpy(opt_dtuart, stdout, sizeof(opt_dtuart))
                     >= sizeof(opt_dtuart) )
                    printk("WARNING: /chosen/stdout-path too long, truncated\n");
            }
            else if ( ret != -EINVAL /* Not present */ )
                printk("Failed to read /chosen/stdout-path (%d)\n", ret);
        }
    }

    if ( !strcmp(opt_dtuart, "") )
    {
        printk("No dtuart path configured\n");
        return;
    }

    options = strchr(opt_dtuart, ':');
    if ( options != NULL )
        *(options++) = '\0';
    else
        options = "";

    printk("Looking for dtuart at \"%s\", options \"%s\"\n", devpath, options);
    if ( *devpath == '/' )
        dev = dt_find_node_by_path(devpath);
    else
        dev = dt_find_node_by_alias(devpath);

    if ( !dev )
    {
        printk("Unable to find device \"%s\"\n", devpath);
        return;
    }

    ret = device_init(dev, DEVICE_SERIAL, options);

    if ( ret )
        printk("Unable to initialize dtuart: %d\n", ret);
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
