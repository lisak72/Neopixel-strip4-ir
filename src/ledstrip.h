#define NUMBER_OF_PARTS 10
//                                                  0       1     2       3       4       5             6       7       8         9
const int parts_begins_ends[NUMBER_OF_PARTS][2]={{0,60},{61,120},{121,180},{181,240},{241,300},{301,360},{361,420},{421,480},{481,540},{541,600}};
const int neopixel_led_count=601; //max number of programmed leds (for memory allocation), be sure set maximal value or more!
