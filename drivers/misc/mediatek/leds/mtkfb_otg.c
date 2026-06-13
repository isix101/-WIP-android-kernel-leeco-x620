#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/list.h>

#define LED_RED_NAME    "red"
#define LED_GREEN_NAME  "green"

struct led_flash_data {
    struct timer_list timer;
    int is_red;
    struct led_classdev *led_red;
    struct led_classdev *led_green;
};

static struct led_flash_data *flash_data;

extern struct list_head leds_list;
extern struct mutex leds_list_lock;

static struct led_classdev *find_led_by_name(const char *name) {
    struct led_classdev *led_cdev;
    
    mutex_lock(&leds_list_lock);
    list_for_each_entry(led_cdev, &leds_list, node) {
        if (!strcmp(led_cdev->name, name)) {
            mutex_unlock(&leds_list_lock);
            return led_cdev;
        }
    }
    mutex_unlock(&leds_list_lock);
    return NULL;
}

static void set_led_color(struct led_classdev *led, int brightness) {
    if (led && led->brightness_set)
        led->brightness_set(led, brightness);
}

static void led_timer_callback(unsigned long data) {
    if (!flash_data) return;
    
    if (flash_data->is_red) {
        set_led_color(flash_data->led_red, LED_FULL);
        set_led_color(flash_data->led_green, LED_OFF);
    } else {
        set_led_color(flash_data->led_red, LED_OFF);
        set_led_color(flash_data->led_green, LED_FULL);
    }
    
    flash_data->is_red = !flash_data->is_red;
    mod_timer(&flash_data->timer, jiffies + msecs_to_jiffies(1000));
}

static int __init led_flash_init(void) {
    flash_data = kzalloc(sizeof(*flash_data), GFP_KERNEL);
    if (!flash_data) return -ENOMEM;
    
    // Получаем доступ к LED устройствам
    flash_data->led_red = find_led_by_name(LED_RED_NAME);
    flash_data->led_green = find_led_by_name(LED_GREEN_NAME);
    
    if (!flash_data->led_red || !flash_data->led_green) {
        pr_err("Failed to find LED devices\n");
        kfree(flash_data);
        return -ENODEV;
    }
    
    init_timer(&flash_data->timer);
    flash_data->timer.function = led_timer_callback;
    flash_data->timer.data = (unsigned long)flash_data;
    flash_data->is_red = 1;
    
    mod_timer(&flash_data->timer, jiffies + msecs_to_jiffies(1000));
    pr_info("LED Flash Driver initialized\n");
    return 0;
}

static void __exit led_flash_exit(void) {
    if (flash_data) {
        del_timer_sync(&flash_data->timer);
        set_led_color(flash_data->led_red, LED_OFF);
        set_led_color(flash_data->led_green, LED_OFF);
        kfree(flash_data);
        flash_data = NULL;
    }
    pr_info("LED Flash Driver unloaded\n");
}

module_init(led_flash_init);
module_exit(led_flash_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Direct LED Control Driver");