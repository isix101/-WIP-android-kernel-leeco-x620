/*
 * FAKE tpd driver - Simple version with GPIO only
 * - Toggles GPIO pin (reset) only
 * - No I2C, no timers, no workqueues, no interrupts, no loops
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include "tpd.h"

#define DRIVER_NAME "fake_tpd"
#define FAKE_GPIO_RESET     57

/* ============================================================
 * GPIO TOGGLE - ONCE
 * ============================================================ */
static void fake_gpio_toggle(void)
{
    int ret;
    
    if (!gpio_is_valid(FAKE_GPIO_RESET)) {
        printk("[FAKE_TPD] GPIO %d is invalid\n", FAKE_GPIO_RESET);
        return;
    }
    
    ret = gpio_request(FAKE_GPIO_RESET, "fake_reset");
    if (ret < 0) {
        printk("[FAKE_TPD] Failed to request GPIO %d, ret=%d\n", FAKE_GPIO_RESET, ret);
        return;
    }
    
    gpio_direction_output(FAKE_GPIO_RESET, 1);
    printk("[FAKE_TPD] GPIO %d set HIGH\n", FAKE_GPIO_RESET);
    
    msleep(50);
    
    gpio_set_value(FAKE_GPIO_RESET, 0);
    printk("[FAKE_TPD] GPIO %d set LOW\n", FAKE_GPIO_RESET);
    
    msleep(50);
    
    gpio_set_value(FAKE_GPIO_RESET, 1);
    printk("[FAKE_TPD] GPIO %d set HIGH again\n", FAKE_GPIO_RESET);
    
    /* Don't free GPIO - keep it for resume */
}

/* ============================================================
 * LOCAL INIT
 * ============================================================ */
static int fake_tpd_local_init(void)
{
    printk("[FAKE_TPD] ========== INIT START ==========\n");
    
    /* Toggle GPIO */
    fake_gpio_toggle();
    
    /* Mark as loaded */
    tpd_load_status = 1;
    
    printk("[FAKE_TPD] ========== INIT DONE ==========\n");
    return 0;
}

/* ============================================================
 * SUSPEND - DO NOTHING
 * ============================================================ */
static void fake_tpd_suspend(struct device *dev)
{
    printk("[FAKE_TPD] Suspend\n");
}

/* ============================================================
 * RESUME - TOGGLE GPIO AGAIN
 * ============================================================ */
static void fake_tpd_resume(struct device *dev)
{
    printk("[FAKE_TPD] Resume\n");
    
    /* Toggle GPIO again */
    if (gpio_is_valid(FAKE_GPIO_RESET)) {
        gpio_set_value(FAKE_GPIO_RESET, 0);
        msleep(20);
        gpio_set_value(FAKE_GPIO_RESET, 1);
        printk("[FAKE_TPD] GPIO %d toggled on resume\n", FAKE_GPIO_RESET);
    }
}

/* ============================================================
 * DRIVER STRUCTURE
 * ============================================================ */
static struct tpd_driver_t fake_tpd_driver = {
    .tpd_device_name = DRIVER_NAME,
    .tpd_local_init = fake_tpd_local_init,
    .suspend = fake_tpd_suspend,
    .resume = fake_tpd_resume,
};

/* ============================================================
 * EXIT
 * ============================================================ */
static void __exit fake_tpd_exit(void)
{
    printk("[FAKE_TPD] Exit\n");
    
    if (gpio_is_valid(FAKE_GPIO_RESET))
        gpio_free(FAKE_GPIO_RESET);
    
    tpd_driver_remove(&fake_tpd_driver);
}

static int __init fake_tpd_init(void)
{
    printk("[FAKE_TPD] Registering driver\n");
    return tpd_driver_add(&fake_tpd_driver);
}

module_init(fake_tpd_init);
module_exit(fake_tpd_exit);

MODULE_AUTHOR("Kernel Debug");
MODULE_DESCRIPTION("FAKE tpd driver - GPIO only");
MODULE_LICENSE("GPL v2");