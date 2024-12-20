#include "mgos.h"
#include "mgos_mqtt.h"
#include "mgos_init.h"
#include "mgos_sys_config.h"
#include "mgos_neopixel_four.h"
#include "mgos_rpc.h"
#include "mgos_ir.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct {
    bool active;
    int first_led;
    int last_led;
    int r;
    int g;
    int b;
    int w;
} part_strip;

static void timer_cb();

static void net_cb(int ev, void *evd, void *arg) ;

//callback interrupt f after infrared code received
static void irrecv_cb(int code, void *arg);

//callback called from irrecv_cb to evaluate code etc
void ISRinvoke_evaluatecode_cb();

static void ircodeShow();

#ifdef MGOS_HAVE_WIFI
static void wifi_cb(int ev, void *evd, void *arg) ;
#endif /* MGOS_HAVE_WIFI */

static void cloud_cb(int ev, void *evd, void *arg) ;

//use variables from pwds.h to configure cloud and wifi access
void cloud_config_external();

//save array arr_part_strips actual settings to binary file settings.bin
void save_settings_to_file(const char* fname);

//read saved settings of arr_part_strips from file settings.bin and returned 1, if not exists do nothing and returned 0
bool read_settings_from_file(const char* fname);

//program strip leds from first to last led
void neopixel_set_from_to(struct mgos_neopixel *strip, int first, int last, int r, int g, int b, int w);

//program strip leds from first to last led without clear strip and show after while
void neopixel_set_from_to_quick(struct mgos_neopixel *strip, int first, int last, int r, int g, int b, int w);

//send data to neopixel strip
//void neopixelRun();

//time check, if  time from last calling time_refresh() is longer than waittime return true, else return false. See time_refresh() too. 
bool time_check(double waittime);

//time refresh, refresh variable ir_lasttime by number from mgos_uptime used in time_check()
void time_refresh();

//add delay in seconds (double - ex. 0.5)
void delay_sd(double secs);

//evaluate code for color change, return true if code color, false if not
bool code_evaluate_color(int evcode);

//evaluate code noncolor (changing active part or any)
bool code_evaluate_noncolor(int evcode);

//evaluate numbers only, return number which was pressed or -1 if non number button pressed
int code_evaluate_number(int evcode);

//repeating last color code until new button pressed
//void repeat_last_code_cb();

//repeating last color code inside of f. (loop),, pay attention on use in combination of mgos_ints_disable!
void repeat_last_code_loop_cb();

//set only on begin of strip, for quick view
void show_color();

//change color variable ++ (global)
void color_up(int *color);
//change color variable -- (global)
void color_dn(int *color);

//change .active state of part number number_of_part in arr_strip_parts and set stripProgrammed to false
void switch_part(part_strip *arr_strip, int number_of_part);

//initialize array structures by zeros
void initialize_array_values();

//initialization of array by zeros or values from file
void initialize_array();

//set begin and ends of parts on depend of array parts_begins_ends defined on begining (const)
void begin_end_set_parts(part_strip *arr_strip);

//set colors in active parts from arr_strip_parts using global color variables
void colorize_parts(part_strip *arr_strip);

//propagate array setting to strip, parameters *strip and part pointer to array
void array_to_strip(struct mgos_neopixel *strip, part_strip arr_strip);

//overloaded array to strip/ last part if active_part false, send all colours to 0
void array_to_strip_b(struct mgos_neopixel *strip, part_strip arr_strip, bool active_part);

//callback f for calling by timer to propagate all array to strip
static void array_to_strip_cb();

//called by timer, colorize parts and propagate all to strip 
static void neopixel_program_cb();

//start saving custom settings to file 
void save_custom_setting_to_file();

//read saved custom setting from file
void read_custom_setting_from_file();

//read saved custom setting from file, int value coming from RPC, filtered to numbers from 0 to 9 and -1 like abort
void custom_setting_from_RPC(int choice_r);

//cb f for RPC handler used for custom_setting_from_RPC
static void choose_cust_sett_cb(struct mg_rpc_request_info *ri, void *cb_arg,
                   struct mg_rpc_frame_info *fi, struct mg_str args);

//blinking first leds
void led_blinking();

//INIT FUNCT
enum mgos_app_init_result mgos_app_init(void);
