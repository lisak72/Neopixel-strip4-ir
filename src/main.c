#include "main.h"
#include "pwds.h"
#include "ledstrip.h"

#ifdef MGOS_HAVE_WIFI
#include "mgos_wifi.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct mgos_neopixel *mystrip;
const int neopixel_pin=15;
const int ir_pin=21;
static int ir_code=0;
static bool ir_changed=false;
static double ir_lasttime=0;
static bool ir_odd=false;
uint8_t modulo=10;
int red = 0, green = 0, blue=0, white=0;
static bool rpcset=false;
static bool stripProgrammed=true;
#define debug false
//struct part_strip arr_part_strips[NUMBER_OF_PARTS];
part_strip *arr_part_strips;
#define wait_before_programming 0.5 //waittime in seconds for time_check
static bool color_changed=false;
static struct mgos_irrecv_nec_s *irrcvr;
static bool evaluatecode_running;
bool maxminrange_exceeded=false;
bool in_ir_isr_context=false;
const char* fnames[10]={"cust0.bin","cust1.bin","cust2.bin","cust3.bin","cust4.bin","cust5.bin","cust6.bin","cust7.bin","cust8.bin","cust9.bin" };
//<code definitions>
#define red_down 0x00ffa25d
#define red_up 0x00ff629d
#define green_down 0x00ff22dd
#define green_up 0x00ff02fd
#define blue_down 0x00ffe01f
#define blue_up 0x00ffa857
#define white_down 0x00ff9867
#define white_up 0x00ffb04f
#define zero 0x00ff6897
#define one 0x00ff30cf
#define two 0x00ff18e7
#define three 0x00ff7a85
#define four 0x00ff10ef
#define five 0x00ff38c7 
#define six 0x00ff5aa5
#define seven 0x00ff42bd
#define eight 0x00ff4ab5
#define nine 0x00ff52ad
#define greenbtn 0x00ffc23d
#define eqbtn 0x00ff906f
#define chplusbtn 0x00ffe21d

/*
const int red_down=0x00ffa25d;
const int red_up=0x00ff629d;
const int green_down=0x00ff22dd;
const int green_up=0x00ff02fd;
const int blue_down=0x00ffe01f;
const int blue_up=0x00ffa857;
const int white_down=0x00ff9867;
const int white_up=0x00ffb04f;
const int zero=0x00ff6897;
const int one=0x00ff30cf;
const int two=0x00ff18e7;
const int three=0x00ff7a85;
const int four=0x00ff10ef;
const int five=0x00ff38c7;
const int six=0x00ff5aa5;
const int seven=0x00ff42bd;
const int eight=0x00ff4ab5;
const int nine=0x00ff52ad;
const int greenbtn=0x00ffc23d;
const int eqbtn=0x00ff906f;
const int chplusbtn=0x00ffe21d;
*/
//</code definitions>
//<parts definition in absolute led numbers>

//parts_begin_ends array and NUMBER_OF_PARTS and neopixel_led_count newly defined in ledstrip.h
//const int parts_begins_ends[NUMBER_OF_PARTS][2]={{0,20},{21,40},{41,60},{61,80},{81,100},{101,120},{121,140},{141,160},{161,180},{181,290}};
//int neopixel_led_count=291; //max number of programmed leds (for memory allocation), be sure set maximal value or more!
//</parts definition>




static void timer_cb() {
  static bool s_tick_tock = false;
  LOG(LL_INFO,
      ("%s uptime: %.2lf, RAM: %lu, %lu free", (s_tick_tock ? "Tick" : "Tock"),
       mgos_uptime(), (unsigned long) mgos_get_heap_size(),
       (unsigned long) mgos_get_free_heap_size()));
  s_tick_tock = !s_tick_tock;
}


static void net_cb(int ev, void *evd, void *arg) {
  switch (ev) {
    case MGOS_NET_EV_DISCONNECTED:
      LOG(LL_INFO, ("%s", "Net disconnected"));
      break;
    case MGOS_NET_EV_CONNECTING:
      LOG(LL_INFO, ("%s", "Net connecting..."));
      break;
    case MGOS_NET_EV_CONNECTED:
      LOG(LL_INFO, ("%s", "Net connected"));
      break;
    case MGOS_NET_EV_IP_ACQUIRED:
      LOG(LL_INFO, ("%s", "Net got IP address"));
      break;
  }

  (void) evd;
  (void) arg;
}


static void irrecv_cb(int code, void *arg)
{
    //forbidden to use LOG macro in interrupt-callback functions
  in_ir_isr_context=true;
  ir_changed=true;
  ir_odd=!ir_odd;
  color_changed=false;
  time_refresh();
  ir_code=code;
  //(void) arg;
  if(!evaluatecode_running) mgos_invoke_cb(ISRinvoke_evaluatecode_cb,NULL,false);
  //pq_invoke_cb(&pqh, ISRinvoke_evaluatecode_cb, NULL, (void *)"CB ARG", false, false);
  in_ir_isr_context=false;
}

void ISRinvoke_evaluatecode_cb(){
  if(debug) LOG(LL_INFO, ("in ISRinvoke_evaluatecode_cb begin code %08X", ir_code));
    evaluatecode_running=true;
  if (code_evaluate_color(ir_code)){
    if(debug) LOG(LL_INFO, ("in ISRinvoke_evaluatecode_cb color evaluated do repeat color_changed: %i, ir_changed: %i, ir_odd: %i", color_changed, ir_changed, ir_odd)); 
    ir_changed=false;
    repeat_last_code_loop_cb();
   
  }
  else
    code_evaluate_noncolor(ir_code);
  ir_changed=false;
  if(debug) LOG(LL_INFO, ("in ISRinvoke_evaluatecode_cb 003"));
  evaluatecode_running=false;
}

void color_up(int *color){
  maxminrange_exceeded=false;
  if(*color<255) {
    (*color)++;
    color_changed=true;
    stripProgrammed=false;
    time_refresh();
  }
  else maxminrange_exceeded=true;
}

void color_dn(int *color){
  maxminrange_exceeded=false;
  if(*color>0) {
    (*color)--;
    color_changed=true;
    stripProgrammed=false;
    time_refresh();
  }
  else maxminrange_exceeded=true;
}

bool time_check(double waittime){
  if((mgos_uptime()-ir_lasttime)>waittime) return true;
  else return false;
}

void time_refresh(){
  ir_lasttime=mgos_uptime();
}

void delay_sd(double secs){
  double actTime;
  actTime=mgos_uptime();
  while ((actTime+secs)>mgos_uptime()) {
    if(ir_changed) return;
  }
}

bool code_evaluate_color(int evcode){
  switch (evcode)
  {
  case red_up:
    color_up(&red); show_color();
    break;
  case red_down:
    color_dn(&red); show_color();
    break;
  case green_up:
    color_up(&green); show_color();
    break;
  case green_down:
    color_dn(&green); show_color();
    break;
  case blue_up:
    color_up(&blue); show_color();
    break;
  case blue_down:
    color_dn(&blue); show_color();
    break;
  case white_up:
    color_up(&white); show_color();
    break;
  case white_down:
    color_dn(&white); show_color();
    break;
  default:
    return false;
  }
  return true;
}

/*
void repeat_last_code_cb(){
  if(color_changed && (!ir_changed) && ir_odd){
    code_evaluate_color(ir_code);
  }
} 
*/

void repeat_last_code_loop_cb(){
  double delays=0.1;
  maxminrange_exceeded=false;
  while(color_changed && (!ir_changed) && ir_odd && (!maxminrange_exceeded)){
    delay_sd(delays);
    mgos_wdt_feed();
    code_evaluate_color(ir_code);
  }
  ir_changed=false;
} 

bool code_evaluate_noncolor(int evcode){
  switch (evcode){
    case zero:
      switch_part(arr_part_strips,0);
      break;
    case one:
      switch_part(arr_part_strips,1);
      break;
    case two:
      switch_part(arr_part_strips,2);
      break;
    case three:
      switch_part(arr_part_strips,3);
      break;
    case four:
      switch_part(arr_part_strips,4);
      break;
    case five:
      switch_part(arr_part_strips,5);
      break;
    case six:
      switch_part(arr_part_strips,6);
      break;
    case seven:
      switch_part(arr_part_strips,7);
      break;
    case eight:
      switch_part(arr_part_strips,8);
      break;
    case nine:
      switch_part(arr_part_strips,9);
      break;
    case greenbtn:
      save_custom_setting_to_file();
      break;
    case eqbtn:
      read_custom_setting_from_file();
      break;
    default:
      return false;
  }
  return true;
}

int code_evaluate_number(int evcode){
  switch (evcode){
    case zero:
      return 0;
    case one:
      return 1;
    case two:
      return 2;
    case three:
      return 3;
    case four:
      return 4;
    case five:
      return 5;
    case six:
      return 6;
    case seven:
      return 7;
    case eight:
      return 8;
    case nine:
      return 9;
    default:
      return -1;
  }
}

void switch_part(part_strip *arr_strip, int number_of_part){
    arr_strip[number_of_part].active=(!(arr_strip[number_of_part].active));
    stripProgrammed=false;
}

void begin_end_set_parts(part_strip *arr_strip){
  for(int i=0;i<NUMBER_OF_PARTS;i++){
   arr_strip[i].first_led=parts_begins_ends[i][0];
   arr_strip[i].last_led=parts_begins_ends[i][1]; 
   if(debug) LOG(LL_INFO, ("end of part %i be set %i currently set %i, global %i",i,parts_begins_ends[i][1],arr_strip[i].last_led, arr_part_strips[i].last_led));
      }
    stripProgrammed=false;
  }


void colorize_parts(part_strip *arr_strip){
  for(int i=0;i<NUMBER_OF_PARTS;i++){
    if(arr_strip[i].active){
      arr_strip[i].r=red;
      arr_strip[i].g=green;
      arr_strip[i].b=blue;
      arr_strip[i].w=white;
 }
 stripProgrammed=false;
  }
}

void array_to_strip(struct mgos_neopixel *strip, part_strip arr_strip){
  neopixel_set_from_to(strip,arr_strip.first_led,arr_strip.last_led,arr_strip.r,arr_strip.g,arr_strip.b,arr_strip.w);
}

void array_to_strip_b(struct mgos_neopixel *strip, part_strip arr_strip, bool active_part){
  if(active_part) neopixel_set_from_to(strip,arr_strip.first_led,arr_strip.last_led,arr_strip.r,arr_strip.g,arr_strip.b,arr_strip.w);
    else neopixel_set_from_to(strip,arr_strip.first_led,arr_strip.last_led,0,0,0,0);

}

static void array_to_strip_cb(){
  for(int i=0;i<NUMBER_OF_PARTS;i++){
    if(arr_part_strips[i].active) array_to_strip(mystrip,arr_part_strips[i]);
      else array_to_strip_b(mystrip,arr_part_strips[i],0);
  }
}

static void ircodeShow(){
  LOG(LL_INFO, ("IR: %08X, lasttime %f, address=%X", ir_code, ir_lasttime, (int)irrcvr));
  LOG(LL_INFO, ("red: %i, green: %i, blue: %i, white: %i, changed %i", red, green, blue, white, color_changed));
  for(int i=0;i<NUMBER_OF_PARTS;i++){
    LOG(LL_INFO, ("part: %i, active %i, endvalue: %i", i, arr_part_strips[i].active, arr_part_strips[i].last_led));
  }
  timer_cb();
}

static void neopixel_program_cb(){
  if(stripProgrammed) return;
  if(!time_check(wait_before_programming)) return;
  //if(stripProgrammed || !time_check()) return;
  if(debug) LOG(LL_INFO, ("in neopixel_program_cb 01"));
  if(color_changed) colorize_parts(arr_part_strips);
  color_changed=false;
  if(debug) LOG(LL_INFO, ("in neopixel_program_cb 02"));
  array_to_strip_cb();
  stripProgrammed=true;
  if(debug) LOG(LL_INFO, ("in neopixel_program_cb 03"));
  save_settings_to_file("settings.bin");
}

#ifdef MGOS_HAVE_WIFI
static void wifi_cb(int ev, void *evd, void *arg) {
  switch (ev) {
    case MGOS_WIFI_EV_STA_DISCONNECTED: {
      struct mgos_wifi_sta_disconnected_arg *da =
          (struct mgos_wifi_sta_disconnected_arg *) evd;
      LOG(LL_INFO, ("WiFi STA disconnected, reason %d", da->reason));
      break;
    }
    case MGOS_WIFI_EV_STA_CONNECTING:
      LOG(LL_INFO, ("WiFi STA connecting %p", arg));
      break;
    case MGOS_WIFI_EV_STA_CONNECTED:
      LOG(LL_INFO, ("WiFi STA connected %p", arg));
      break;
    case MGOS_WIFI_EV_STA_IP_ACQUIRED:
      LOG(LL_INFO, ("WiFi STA IP acquired %p", arg));
      break;
    case MGOS_WIFI_EV_AP_STA_CONNECTED: {
      struct mgos_wifi_ap_sta_connected_arg *aa =
          (struct mgos_wifi_ap_sta_connected_arg *) evd;
      LOG(LL_INFO, ("WiFi AP STA connected MAC %02x:%02x:%02x:%02x:%02x:%02x",
                    aa->mac[0], aa->mac[1], aa->mac[2], aa->mac[3], aa->mac[4],
                    aa->mac[5]));
      break;
    }
    case MGOS_WIFI_EV_AP_STA_DISCONNECTED: {
      struct mgos_wifi_ap_sta_disconnected_arg *aa =
          (struct mgos_wifi_ap_sta_disconnected_arg *) evd;
      LOG(LL_INFO,
          ("WiFi AP STA disconnected MAC %02x:%02x:%02x:%02x:%02x:%02x",
           aa->mac[0], aa->mac[1], aa->mac[2], aa->mac[3], aa->mac[4],
           aa->mac[5]));
      break;
    }
  }
  (void) arg;
}
#endif /* MGOS_HAVE_WIFI */

static void cloud_cb(int ev, void *evd, void *arg) {
  struct mgos_cloud_arg *ca = (struct mgos_cloud_arg *) evd;
  switch (ev) {
    case MGOS_EVENT_CLOUD_CONNECTED: {
      LOG(LL_INFO, ("Cloud connected (%d)", ca->type));
      break;
    }
    case MGOS_EVENT_CLOUD_DISCONNECTED: {
      LOG(LL_INFO, ("Cloud disconnected (%d)", ca->type));
      break;
    }
  }

  (void) arg;
}

void cloud_config_external(){
  mgos_wifi_setup_sta(&sta_config);
  mgos_sys_config_set_dash_token(dash_token);
  mgos_sys_config_set_dash_enable(dash_config_enable);
   char *err = NULL;
  mgos_sys_config_save(&mgos_sys_config, false, &err);
}

void save_settings_to_file(const char* fname){
  mgos_gpio_disable_int(ir_pin);
  //while(in_ir_isr_context) {}
  if(debug) LOG(LL_INFO, ("Save settings to file started"));
  FILE *fsettings=NULL;
  fsettings=fopen(fname,"wb"); //wb binary write, refer to https://cplusplus.com/reference/cstdio/fopen/
  if(fsettings==NULL) {
    mgos_gpio_enable_int(ir_pin);
    return;
  }
  if(debug) LOG(LL_INFO, ("Save settings to file before fwrite"));
  fwrite(arr_part_strips,sizeof(part_strip),NUMBER_OF_PARTS,fsettings);
  fclose(fsettings); 
  if(debug) LOG(LL_INFO, ("Save settings to file finished"));
  mgos_gpio_enable_int(ir_pin);
  return;
}

bool read_settings_from_file(const char* fname){
  if(debug) LOG(LL_INFO, ("Read settings from file started"));
  FILE* fw;
  fw=fopen(fname,"rb"); 
  if(debug) LOG(LL_INFO, ("settings.bin opened"));
  if(fw==NULL){
    //fclose(fw);
    if(debug) LOG(LL_INFO, ("settings.bin closed null"));
    return 0; //using default settings
  }
  fread(arr_part_strips,sizeof(part_strip),NUMBER_OF_PARTS,fw);
  fclose(fw); 
  return(1);
}

void show_color(){
  neopixel_set_from_to_quick(mystrip,0,1,red, green, blue, white);
}

void neopixel_set_from_to(struct mgos_neopixel *strip, int first, int last, int r, int g, int b, int w){
  if(color_changed) mgos_neopixel_clear(strip);
  for(int i=first; i<=last; i++){
    mgos_neopixel_set(strip, i, r, g, b, w);   
  }
  mgos_neopixel_show(strip);
}

void neopixel_set_from_to_quick(struct mgos_neopixel *strip, int first, int last, int r, int g, int b, int w){
  for(int i=first; i<=last; i++){
    mgos_neopixel_set(strip, i, r, g, b, w);
  }
   mgos_neopixel_show(strip);
}

void initialize_array(){
  arr_part_strips=(part_strip*) (malloc((sizeof(part_strip))*NUMBER_OF_PARTS));
  if(debug) LOG(LL_INFO, ("sizeof part_strip array: %i, first_element_addr: %08X", (sizeof(part_strip)*NUMBER_OF_PARTS), (int) arr_part_strips));
  initialize_array_values();
  read_settings_from_file("settings.bin");
  begin_end_set_parts(arr_part_strips);
}

void initialize_array_values(){
  for(int i=0; i<NUMBER_OF_PARTS; i++){
    arr_part_strips[i].active=false;
    arr_part_strips[i].first_led=0;
    arr_part_strips[i].last_led=0;
    arr_part_strips[i].r=0;
    arr_part_strips[i].g=0;
    arr_part_strips[i].b=0;
    arr_part_strips[i].w=0;
  }
}

static void sys_restart_cb(){
  mgos_system_restart();
}

void save_custom_setting_to_file(){
  int choice=-1;
  ir_changed=false;
  time_refresh();
  do{
    choice=code_evaluate_number(ir_code);
    led_blinking();
    if(time_check(10.0)) return;
  }
  while(choice==-1);
  save_settings_to_file(fnames[choice]);
  stripProgrammed=false;
}

void read_custom_setting_from_file(){
  int choice=-1;
  ir_changed=false;
  time_refresh();
  do{
    choice=code_evaluate_number(ir_code);
    led_blinking();
    if(time_check(10.0)) return;
  }
  while(choice==-1);
  read_settings_from_file(fnames[choice]);
  stripProgrammed=false;
}

void custom_setting_from_RPC(int choice_r){
  if((choice_r==-1) || (choice_r>9) || (choice_r<-1)) return;
  read_settings_from_file(fnames[choice_r]);
  stripProgrammed=false;
}

void led_blinking(){
  neopixel_set_from_to_quick(mystrip,0/*from*/,1/*to*/,0/*red*/,255/*green*/,0/*blue*/,0/*white*/);
/*
  if(ir_changed) {
    ir_changed=false;
    return;
  }
*/
  delay_sd(0.5);
  neopixel_set_from_to_quick(mystrip,0/*from*/,1/*to*/,255/*red*/,0/*green*/,0/*blue*/,0/*white*/);
 /*
  if(ir_changed) {
    ir_changed=false;
    return;
  }
  */
  delay_sd(0.5);
}


static void choose_cust_sett_cb(struct mg_rpc_request_info *ri, void *cb_arg,
                   struct mg_rpc_frame_info *fi, struct mg_str args) {
int choice=-1;
  if (json_scanf(args.p, args.len, ri->args_fmt, &choice) == 1) {
    mg_rpc_send_responsef(ri, "OK");
    custom_setting_from_RPC(choice);
    } else {
    mg_rpc_send_errorf(ri, -1, "Bad request. ");
  }
  (void) cb_arg;
  (void) fi;
}

void cust_set_from_RPC_auto_cb(){
  for(int i=0; i<100; i++){
  custom_setting_from_RPC(0);
  neopixel_program_cb();
  custom_setting_from_RPC(1);
  neopixel_program_cb();
  custom_setting_from_RPC(2);
  neopixel_program_cb();
  }
}

//INIT FUNCT HERE
enum mgos_app_init_result mgos_app_init(void) {
  cs_log_set_level(2);
  //  mgos_wdt_set_timeout(20);
  //  mgos_wdt_enable();

  mystrip= mgos_neopixel_create(neopixel_pin, neopixel_led_count, MGOS_NEOPIXEL_ORDER_GRB);
  irrcvr= mgos_irrecv_nec_create(ir_pin, irrecv_cb, NULL);
  //sysconfigget();
  initialize_array();
  cloud_config_external();
  if(debug) mgos_set_timer(20000, MGOS_TIMER_REPEAT, ircodeShow, NULL);
    

  // mgos_set_timer(500, MGOS_TIMER_REPEAT,neopixelRunRandom, NULL);

  /* Network connectivity events */
  mgos_event_add_group_handler(MGOS_EVENT_GRP_NET, net_cb, NULL);

  //RPC
  mg_rpc_add_handler(mgos_rpc_get_global(), "CustSet", "{Setting: %i}", choose_cust_sett_cb, NULL);
 
  #ifdef MGOS_HAVE_WIFI
    mgos_event_add_group_handler(MGOS_WIFI_EV_BASE, wifi_cb, NULL);
  #endif

  mgos_event_add_handler(MGOS_EVENT_CLOUD_CONNECTED, cloud_cb, NULL);
  mgos_event_add_handler(MGOS_EVENT_CLOUD_DISCONNECTED, cloud_cb, NULL);
 //NEOPIXEL
  mgos_set_timer(150, MGOS_TIMER_REPEAT, neopixel_program_cb, NULL);

  return MGOS_APP_INIT_SUCCESS;
}
