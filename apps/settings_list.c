/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2007 Jonathan Gordon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include "config.h"
#include <stdbool.h>
#include "string-extra.h"
#include "system.h"
#include "storage.h"
#include "lang.h"
#include "talk.h"
#include "lcd.h"
#include "scroll_engine.h"
#include "button.h"
#include "backlight.h"
#include "sound.h"
#include "settings.h"
#include "rbpaths.h"
#include "settings_list.h"
#include "usb.h"
#include "audio.h"
#include "power.h"
#include "powermgmt.h"
#include "kernel.h"
#include "open_plugin.h"
#include "misc.h"
#include "playback.h"
#ifdef HAVE_REMOTE_LCD
#include "lcd-remote.h"
#endif
#include "list.h"
#include "rbunicode.h"
#include "peakmeter.h"
#include "menus/eq_menu.h"
#if CONFIG_TUNER
#include "radio.h"
#endif
#ifdef IPOD_ACCESSORY_PROTOCOL
#include "iap.h"
#endif
#include "statusbar.h"
#ifdef HAVE_TOUCHSCREEN
#include "touchscreen.h"
#include "ctype.h" /* For isspace() */
#endif
#ifdef HAVE_HOTKEY
#include "onplay.h"
#include "misc.h" /* current activity */
#endif
#include "playlist.h"
#include "tree.h"

#include "voice_thread.h"

#if defined(DX50) || defined(DX90)
#include "governor-ibasso.h"
#include "usb-ibasso.h"
#endif

#define UNUSED {.RESERVED=NULL}
#define INT(a) {.int_ = a}
#define UINT(a) {.uint_ = a}
#define BOOL(a) {.bool_ = a}
#define CHARPTR(a) {.charptr = a}
#define UCHARPTR(a) {.ucharptr = a}
#define FUNCTYPE(a) {.func = a}
#define NODEFAULT INT(0)

/* in all the following macros the args are:
    - flags: bitwise | or the F_ bits in settings_list.h
    - var: pointer to the variable being changed (usually in global_settings)
    - lang_id: LANG_* id to display in menus and setting screens for the setting
    - default: the default value for the variable, set if settings are reset
    - name: the name of the setting in config files
    - cfg_vals: comma separated list of legal values to write to cfg files.
                The values correspond to the values 0,1,2,etc. of the setting.
                NULL if just the number itself should be written to the file.
                No spaces between the values and the commas!
    - cb: the callback used by the setting screen.
*/

/* Use for int settings which use the set_sound() function to set them */
#define SOUND_SETTING(flags,var,lang_id,name,setting)                      \
            {flags|F_T_INT|F_T_SOUND|F_SOUNDSETTING|F_ALLOW_ARBITRARY_VALS, &global_settings.var, \
                lang_id, NODEFAULT,name,                              \
                {.sound_setting=(struct sound_setting[]){{setting}}} }

/* Use for bool variables which don't use LANG_SET_BOOL_YES and LANG_SET_BOOL_NO
      or dont save as "off" or "on" in the cfg.
   cfgvals are comma separated values (without spaces after the comma!) to write
      for 'false' and 'true' (in this order)
   yes_id is the lang_id for the 'yes' (or 'on') option in the menu
   no_id is the lang_id for the 'no' (or 'off') option in the menu
 */
#define BOOL_SETTING(flags,var,lang_id,default,name,cfgvals,yes_id,no_id,cb)\
            {flags|F_BOOL_SETTING, &global_settings.var,                    \
                lang_id, BOOL(default),name,                                \
                {.bool_setting=(struct bool_setting[]){{cb,yes_id,no_id,cfgvals}}} }

/* bool setting which does use LANG_YES and _NO and save as "off,on" */
#define OFFON_SETTING(flags,var,lang_id,default,name,cb)                    \
            BOOL_SETTING(flags,var,lang_id,default,name,off_on,             \
                LANG_SET_BOOL_YES,LANG_SET_BOOL_NO,cb)

/*system_status int variable which is saved to resume.cfg */
#define SYSTEM_STATUS(flags,var,default,name)                \
            {flags|F_RESUMESETTING|F_T_INT, &global_status.var,-1, \
             INT(default), name, UNUSED}
/* system_status settings items will be saved to resume.cfg
   Use for int which use the set_sound() function to set them
   These items WILL be included in the users exported settings files
 */
#define SYSTEM_STATUS_SOUND(flags,var,lang_id,name,setting)                 \
            {flags|F_T_INT|F_T_SOUND|F_SOUNDSETTING|F_ALLOW_ARBITRARY_VALS| \
             F_RESUMESETTING, &global_status.var, lang_id, NODEFAULT,name,  \
                {.sound_setting=(struct sound_setting[]){{setting}}} }

/* setting which stores as a filename (or another string) in the .cfgvals
    The string must be a char array (which all of our string settings are),
    not just a char pointer.
    prefix: The absolute path to not save in the variable, ex /.rockbox/wps_file
    suffix: The file extention (usually...) e.g .wps_file
    If the prefix is set (not NULL), then the suffix must be set as well.
 */
#define TEXT_SETTING(flags,var,name,default,prefix,suffix)      \
            {flags|F_T_UCHARPTR, &global_settings.var,-1,           \
                CHARPTR(default),name,                              \
                {.filename_setting=                                 \
                    (struct filename_setting[]){                    \
                        {prefix,suffix,sizeof(global_settings.var)}}} }

#define DIRECTORY_SETTING(flags,var,lang_id,name,default) \
    {flags|F_DIRNAME|F_T_UCHARPTR, &global_settings.var, lang_id, \
     CHARPTR(default), name, \
     {.filename_setting=(struct filename_setting[]){ \
         {NULL, NULL, sizeof(global_settings.var)}}}}

/*  Used for settings which use the set_option() setting screen.
    The ... arg is a list of pointers to strings to display in the setting
    screen. These can either be literal strings, or ID2P(LANG_*) */
#define CHOICE_SETTING(flags,var,lang_id,default,name,cfg_vals,cb,count,...)   \
            {flags|F_CHOICE_SETTING|F_T_INT,  &global_settings.var, lang_id,   \
                INT(default), name,                                  \
                {.choice_setting = (struct choice_setting[]){                  \
                    {cb, count, cfg_vals, {.desc = (const unsigned char*[])              \
                        {__VA_ARGS__}}}}}}

/* Similar to above, except the strings to display are taken from cfg_vals,
   the ... arg is a list of ID's to talk for the strings, can use TALK_ID()'s */
#define STRINGCHOICE_SETTING(flags,var,lang_id,default,name,cfg_vals,          \
                                                                cb,count,...)  \
            {flags|F_CHOICE_SETTING|F_T_INT|F_CHOICETALKS,                     \
                &global_settings.var, lang_id,                                 \
                INT(default), name,                                            \
                {.choice_setting = (struct choice_setting[]){                  \
                    {cb, count, cfg_vals, {.talks = (const int[]){__VA_ARGS__}}}}}}

/*  for settings which use the set_int() setting screen.
    unit is the UNIT_ define to display/talk.
    the first one saves a string to the config file,
    the second one saves the variable value to the config file */
#define INT_SETTING(flags, var, lang_id, default, name,                 \
                    unit, min, max, step, formatter, get_talk_id, cb)   \
            {flags|F_INT_SETTING|F_T_INT, &global_settings.var,         \
                lang_id, INT(default), name,                            \
                 {.int_setting = (struct int_setting[]){                \
                    {cb, unit, step, min, max, formatter, get_talk_id}}}}
#define INT_SETTING_NOWRAP(flags, var, lang_id, default, name,             \
                    unit, min, max, step, formatter, get_talk_id, cb)      \
            {flags|F_INT_SETTING|F_T_INT|F_NO_WRAP, &global_settings.var,  \
                lang_id, INT(default), name,                               \
                 {.int_setting = (struct int_setting[]){                   \
                    {cb, unit, step, min, max, formatter, get_talk_id}}}}

#define TABLE_SETTING(flags, var, lang_id, default, name, cfg_vals, \
                      unit, formatter, get_talk_id, cb, count, ...) \
            {flags|F_TABLE_SETTING|F_T_INT, &global_settings.var,   \
                lang_id, INT(default), name,                        \
                {.table_setting = (struct table_setting[]) {        \
                    {cb, formatter, get_talk_id, unit, count,       \
                    cfg_vals, (const int[]){__VA_ARGS__}}}}}

#define TABLE_SETTING_LIST(flags, var, lang_id, default, name, cfg_vals, \
                      unit, formatter, get_talk_id, cb, count, list) \
            {flags|F_TABLE_SETTING|F_T_INT, &global_settings.var,   \
                lang_id, INT(default), name,                        \
                {.table_setting = (struct table_setting[]) {        \
                    {cb, formatter, get_talk_id, unit, count, cfg_vals, list}}}}

#define CUSTOM_SETTING(flags, var, lang_id, default, name,              \
                       load_from_cfg, write_to_cfg,                     \
                       is_change, set_default)                          \
            {flags|F_CUSTOM_SETTING|F_T_CUSTOM|F_BANFROMQS,             \
                &global_settings.var, lang_id,                          \
                {.custom = (void*)default}, name,                       \
            {.custom_setting = (struct custom_setting[]){               \
        {load_from_cfg, write_to_cfg, is_change, set_default}}}}

#define VIEWPORT_SETTING(var,name)      \
        TEXT_SETTING(F_THEMESETTING|F_NEEDAPPLY,var,name,"-", NULL, NULL)

/* some sets of values which are used more than once, to save memory */
static const char off[] = "off";
static const char off_on[] = "off,on";
static const char off_on_ask[] = "off,on,ask";
static const char off_number_spell[] = "off,number,spell";
static const int timeout_sec_common[] = {-1,0,1,2,3,4,5,6,7,8,9,10,15,20,25,30,
                                        45,60,90,120,180,240,300,600,900,1200,
                                        1500,1800,2700,3600,4500,5400,6300,7200};
#if defined(HAVE_RECORDING)
static const int time_recording_trigger[] = {0,1,2,5,10,15,20,25,30,60,120,300,600};
#endif
#if defined(HAVE_BACKLIGHT_FADING_INT_SETTING)
static const int backlight_fade[] = {0,100,200,300,500,1000,2000,3000,5000,10000};
#endif

static const char graphic_numeric[] = "graphic,numeric";

/* Default theme settings */
#define DEFAULT_WPSNAME  "cabbiev2"
#define DEFAULT_SBSNAME  "-"
#define DEFAULT_FMS_NAME "cabbiev2"

#if LCD_HEIGHT <= 64
  #define DEFAULT_FONT_HEIGHT 8
  #define DEFAULT_FONTNAME "08-Rockfont"
#elif LCD_HEIGHT <= 80
  #define DEFAULT_FONT_HEIGHT 11
  #define DEFAULT_FONTNAME "11-Sazanami-Mincho"
/* sandisk sansa clip zip and samsung yh-820 */
#elif (LCD_HEIGHT == 96) && ((LCD_WIDTH == 96) || (LCD_WIDTH == 128))
  #define DEFAULT_FONT_HEIGHT 8
  #define DEFAULT_FONTNAME "08-Rockfont"
#elif LCD_HEIGHT <= 220
  #define DEFAULT_FONT_HEIGHT 12
#elif LCD_HEIGHT <= 240
  #define DEFAULT_FONT_HEIGHT 15
#elif LCD_HEIGHT <= 320
  #define DEFAULT_FONT_HEIGHT 18
#elif defined(SHANLING_Q1)
  /* 16pt font looks pretty aliased & ugly */
  #define DEFAULT_FONT_HEIGHT 18
#elif LCD_HEIGHT <= 400
  #define DEFAULT_FONT_HEIGHT 16
#elif LCD_HEIGHT <= 480 && LCD_WIDTH < 800
  #define DEFAULT_FONT_HEIGHT 27
#else
  #define DEFAULT_FONT_HEIGHT 35
#endif
#define DEFAULT_GLYPHS 250
#define MIN_GLYPHS 50
#define MAX_GLYPHS 65540

#ifndef DEFAULT_FONTNAME
/* ugly expansion needed */
#define _EXPAND2(x) #x
#define _EXPAND(x) _EXPAND2(x)
#define DEFAULT_FONTNAME _EXPAND(DEFAULT_FONT_HEIGHT) "-Adobe-Helvetica"
#endif

#ifdef HAVE_LCD_COLOR
  #if DEFAULT_FONT_HEIGHT >= 31 || defined(SHANLING_Q1)
    #define DEFAULT_ICONSET "tango_icons.32x32"
    #define DEFAULT_VIEWERS_ICONSET "tango_icons_viewers.32x32"
  #elif DEFAULT_FONT_HEIGHT >= 23
    #define DEFAULT_ICONSET "tango_icons.24x24"
    #define DEFAULT_VIEWERS_ICONSET "tango_icons_viewers.24x24"
  #elif DEFAULT_FONT_HEIGHT >= 15
    #define DEFAULT_ICONSET "tango_icons.16x16"
    #define DEFAULT_VIEWERS_ICONSET "tango_icons_viewers.16x16"
  #elif DEFAULT_FONT_HEIGHT >= 11
    #define DEFAULT_ICONSET "tango_icons.12x12"
    #define DEFAULT_VIEWERS_ICONSET "tango_icons_viewers.12x12"
  #elif DEFAULT_FONT_HEIGHT >= 7
    #define DEFAULT_ICONSET "tango_icons.8x8"
    #define DEFAULT_VIEWERS_ICONSET "tango_icons_viewers.8x8"
  #endif
#elif LCD_DEPTH > 1 /* greyscale */
  #define DEFAULT_ICONSET "tango_small_mono"
  #define DEFAULT_VIEWERS_ICONSET "tango_small_viewers_mono"
#else /* monochrome */
  #define DEFAULT_ICONSET ""
  #define DEFAULT_VIEWERS_ICONSET ""
#endif

#ifdef HAVE_REMOTE_LCD
#if LCD_REMOTE_HEIGHT <= 64
  #define DEFAULT_REMOTE_FONTNAME "08-Rockfont"
#else
  #define DEFAULT_REMOTE_FONTNAME "-"
#endif
#endif /* HAVE_REMOTE_LCD */

#define DEFAULT_THEME_FOREGROUND LCD_RGBPACK(0xce, 0xcf, 0xce)
#define DEFAULT_THEME_BACKGROUND LCD_RGBPACK(0x00, 0x00, 0x00)
#define DEFAULT_THEME_SELECTOR_START LCD_RGBPACK(0xff, 0xeb, 0x9c)
#define DEFAULT_THEME_SELECTOR_END LCD_RGBPACK(0xb5, 0x8e, 0x00)
#define DEFAULT_THEME_SELECTOR_TEXT LCD_RGBPACK(0x00, 0x00, 0x00)
#define DEFAULT_THEME_SEPARATOR  LCD_RGBPACK(0x80, 0x80, 0x80)

#define DEFAULT_BACKDROP    BACKDROP_DIR "/cabbiev2.bmp"

#ifdef HAVE_RECORDING
/* these should be in the config.h files */
# ifdef HAVE_UDA1380
#  define DEFAULT_REC_MIC_GAIN 16
#  define DEFAULT_REC_LEFT_GAIN 0
#  define DEFAULT_REC_RIGHT_GAIN 0
# elif defined(HAVE_TLV320)
#  define DEFAULT_REC_MIC_GAIN 0
#  define DEFAULT_REC_LEFT_GAIN 0
#  define DEFAULT_REC_RIGHT_GAIN 0
# elif defined(HAVE_WM8975)
#  define DEFAULT_REC_MIC_GAIN 16
#  define DEFAULT_REC_LEFT_GAIN 0
#  define DEFAULT_REC_RIGHT_GAIN 0
# elif defined(HAVE_WM8758)
#  define DEFAULT_REC_MIC_GAIN 16
#  define DEFAULT_REC_LEFT_GAIN 0
#  define DEFAULT_REC_RIGHT_GAIN 0
# elif defined(HAVE_WM8731)
#  define DEFAULT_REC_MIC_GAIN 16
#  define DEFAULT_REC_LEFT_GAIN 0
#  define DEFAULT_REC_RIGHT_GAIN 0
# endif

#endif /* HAVE_RECORDING */

#if (CONFIG_PLATFORM & PLATFORM_ANDROID)
#define DEFAULT_TAGCACHE_SCAN_PATHS "/sdcard"
#else
#define DEFAULT_TAGCACHE_SCAN_PATHS "/"
#endif

#ifdef HAVE_BACKLIGHT
#ifdef SIMULATOR
#define DEFAULT_BACKLIGHT_TIMEOUT 0
#else
#define DEFAULT_BACKLIGHT_TIMEOUT 15
#endif
#endif /* HAVE_BACKLIGHT */

#if defined(HAVE_USB_CHARGING_ENABLE)
# if !defined(TARGET_USB_CHARGING_DEFAULT)
#  define TARGET_USB_CHARGING_DEFAULT USB_CHARGING_ENABLE
# endif
#endif

#ifdef AUDIOHW_HAVE_POWER_MODE
# ifndef TARGET_DEFAULT_DAC_POWER_MODE
#   define TARGET_DEFAULT_DAC_POWER_MODE SOUND_HIGH_POWER
# endif
#endif

/*
 * Total buffer size due to this setting = max files in dir * 52 bytes
 * Keep this in mind when selecting the maximum - if the maximum is too
 * high it's possible rockbox could hit OOM and become unusable until
 * the config file is deleted manually.
 *
 * Note the FAT32 limit is 65534 files per directory, but this limit
 * also applies to the database browser so it makes sense to support
 * larger maximums.
 */
#if MEMORYSIZE >= 16
# define MAX_FILES_IN_DIR_DEFAULT   5000
# define MAX_FILES_IN_DIR_MAX       100000
# define MAX_FILES_IN_DIR_STEP      1000
#elif MEMORYSIZE >= 8
# define MAX_FILES_IN_DIR_DEFAULT   5000
# define MAX_FILES_IN_DIR_MAX       40000
# define MAX_FILES_IN_DIR_STEP      500
#else
/* historical defaults, only for 2 MiB targets these days */
# define MAX_FILES_IN_DIR_DEFAULT   1000
# define MAX_FILES_IN_DIR_MAX       10000
# define MAX_FILES_IN_DIR_STEP      50
#endif

#if LCD_DEPTH > 1
static const char* list_pad_formatter(char *buffer, size_t buffer_size,
                                    int val, const char *unit)
{
    switch (val)
    {
        case -1: return str(LANG_AUTO);
        case  0: return str(LANG_OFF);
        default: break;
    }
    snprintf(buffer, buffer_size, "%d %s", val, unit);
    return buffer;
}

static int32_t list_pad_getlang(int value, int unit)
{
    switch (value)
    {
        case -1: return LANG_AUTO;
        case  0: return LANG_OFF;
        default: return TALK_ID(value, unit);
    }
}
#endif

static const char* formatter_time_unit_0_is_off(char *buffer, size_t buffer_size,
                                    int val, const char *unit)
{
    (void) buffer_size;
    (void) unit;
    if (val == 0)
        return str(LANG_OFF);
    return buffer;
}

static int32_t getlang_time_unit_0_is_off(int value, int unit)
{
    if (value == 0)
        return LANG_OFF;
    else
        return talk_time_intervals(value, unit, false);
}

#if defined(HAVE_BACKLIGHT) || defined(HAVE_LCD_SLEEP_SETTING)
static const char* formatter_time_unit_0_is_always(char *buffer, size_t buffer_size,
                                    int val, const char *unit)
{
    (void) buffer_size;
    (void) unit;
    if (val == -1)
        return str(LANG_NEVER);
    else if (val == 0)
        return str(LANG_ALWAYS);
    return buffer;
}

static int32_t getlang_time_unit_0_is_always(int value, int unit)
{
    if (value == -1)
        return LANG_NEVER;
    else if (value == 0)
        return LANG_ALWAYS;
    else
        return talk_time_intervals(value, unit, false);
}
#endif /* HAVE_BACKLIGHT || HAVE_LCD_SLEEP_SETTING */

static const char* formatter_time_unit_0_is_skip_track(char *buffer,
                                size_t buffer_size, int val, const char *unit)
{
    (void)unit;
    (void)buffer_size;
    if (val == -1)
        return str(LANG_SKIP_OUTRO);
    else if (val == 0)
        return str(LANG_SKIP_TRACK);
    return buffer;
}

static int32_t getlang_time_unit_0_is_skip_track(int value, int unit)
{
    (void)unit;
    if (value == -1)
        return LANG_SKIP_OUTRO;
    else if (value == 0)
        return LANG_SKIP_TRACK;
    else
        return talk_time_intervals(value, unit, false);
}

static const char* formatter_time_unit_0_is_eternal(char *buffer,
                                size_t buffer_size, int val, const char *unit)
{
    (void) buffer_size;
    (void) unit;
    if (val == 0)
        return str(LANG_PM_ETERNAL);
    return buffer;
}
static int32_t getlang_time_unit_0_is_eternal(int value, int unit)
{
    if (value == 0)
        return LANG_PM_ETERNAL;
    else
        return talk_time_intervals(value, unit, false);
}

#ifndef HAVE_WHEEL_ACCELERATION
static const char* scanaccel_formatter(char *buffer, size_t buffer_size,
        int val, const char *unit)
{
    (void)unit;
    if (val == 0)
        return str(LANG_OFF);
    else
        snprintf(buffer, buffer_size, "Speed up every %d s", val);
    return buffer;
}
#endif

static const char* formatter_unit_0_is_off(char *buffer, size_t buffer_size,
                                    int val, const char *unit)
{
    if (val == 0)
        return str(LANG_OFF);
    else
        snprintf(buffer, buffer_size, "%d %s", val, unit);
    return buffer;
}

static int32_t getlang_unit_0_is_off(int value, int unit)
{
    if (value == 0)
        return LANG_OFF;
    else
        return TALK_ID(value,unit);
}

static void crossfeed_cross_set(int val)
{
   (void)val;
   dsp_set_crossfeed_cross_params(global_settings.crossfeed_cross_gain,
                                  global_settings.crossfeed_hf_attenuation,
                                  global_settings.crossfeed_hf_cutoff);
}

static void surround_set_factor(int val)
{
    (void)val;
    dsp_surround_set_cutoff(global_settings.surround_fx1, global_settings.surround_fx2);
}

static void compressor_set(int val)
{
    (void)val;
    dsp_set_compressor(&global_settings.compressor_settings);
}

static const char* db_format(char* buffer, size_t buffer_size, int value,
                      const char* unit)
{
    int v = abs(value);

    snprintf(buffer, buffer_size, "%s%d.%d %s", value < 0 ? "-" : "",
             v / 10, v % 10, unit);
    return buffer;
}

static int32_t get_dec_talkid(int value, int unit)
{
    return TALK_ID_DECIMAL(value, 1, unit);
}

static int32_t get_precut_talkid(int value, int unit)
{
    return TALK_ID_DECIMAL(-value, 1, unit);
}

struct eq_band_setting eq_defaults[EQ_NUM_BANDS] = {
    { 32, 7, 0 },
    { 64, 10, 0 },
    { 125, 10, 0 },
    { 250, 10, 0 },
    { 500, 10, 0 },
    { 1000, 10, 0 },
    { 2000, 10, 0 },
    { 4000, 10, 0 },
    { 8000, 10, 0 },
    { 16000, 7, 0 },
};

static void eq_load_from_cfg(void *setting, char *value)
{
    struct eq_band_setting *eq = setting;
    char *val_end, *end;

    val_end = value + strlen(value);

    /* cutoff/center */
    end = strchr(value, ',');
    if (!end) return;
    *end = '\0';
    eq->cutoff = atoi(value);

    /* q */
    value = end + 1;
    if (value > val_end) return;
    end = strchr(value, ',');
    if (!end) return;
    *end = '\0';
    eq->q = atoi(value);

    /* gain */
    value = end + 1;
    if (value > val_end) return;
    eq->gain = atoi(value);
}

static char* eq_write_to_cfg(void *setting, char *buf, int buf_len)
{
    struct eq_band_setting *eq = setting;

    snprintf(buf, buf_len, "%d, %d, %d", eq->cutoff, eq->q, eq->gain);
    return buf;
}

static bool eq_is_changed(void *setting, void *defaultval)
{
    struct eq_band_setting *eq = setting;

    return memcmp(eq, defaultval, sizeof(struct eq_band_setting));
}

static void eq_set_default(void* setting, void* defaultval)
{
    memcpy(setting, defaultval, sizeof(struct eq_band_setting));
}

#ifdef HAVE_PLAY_FREQ
static const char* formatter_freq_unit_0_is_auto(char *buffer, size_t buffer_size,
                                    int value, const char *unit)
{
    if (value == 0)
        return str(LANG_AUTO);
    else
        return db_format(buffer, buffer_size, value / 100, unit);
}

static int32_t getlang_freq_unit_0_is_auto(int value, int unit)
{
    if (value == 0)
        return LANG_AUTO;
    else
        return talk_value_decimal(value, unit, 3, false);
}

static void playback_frequency_callback(int sample_rate_hz)
{
    audio_set_playback_frequency(sample_rate_hz);
}
#endif /* HAVE_PLAY_FREQ */

#ifdef HAVE_ALBUMART
static void albumart_callback(int mode)
{
    set_albumart_mode(mode);
}
#endif

/* perform shuffle/unshuffle of the current playlist based on the boolean provided */
static void shuffle_playlist_callback(bool shuffle)
{
    struct playlist_info *playlist = playlist_get_current();
    if (playlist->started)
    {
        if ((audio_status() & AUDIO_STATUS_PLAY) == AUDIO_STATUS_PLAY)
        {
            replaygain_update();
            if (shuffle)
            {
                playlist_randomise(playlist, current_tick, true);
            }
            else
            {
                playlist_sort(playlist, true);
            }
        }
    }
}

static void repeat_mode_callback(int repeat)
{
    if ((audio_status() & AUDIO_STATUS_PLAY) == AUDIO_STATUS_PLAY)
    {
        audio_flush_and_reload_tracks();
    }
    (void)repeat;
}

static void treesort_callback(int value)
{
    (void) value;
    reload_directory();
}

#ifdef HAVE_QUICKSCREEN
static void qs_load_from_cfg(void *var, char *value)
{
    const struct settings_list **item = var;

    if (*value == '-')
        *item = NULL;
    else
        *item = find_setting_by_cfgname(value);
}

static char* qs_write_to_cfg(void *var, char *buf, int buf_len)
{
    const struct settings_list *setting = *(const struct settings_list **)var;

    strmemccpy(buf, setting ? setting->cfg_name : "-", buf_len);
    return buf;
}

static bool qs_is_changed(void* var, void* defaultval)
{
    const struct settings_list *defaultsetting = find_setting(defaultval);

    return var != defaultsetting;
}

static void qs_set_default(void* var, void* defaultval)
{
    *(const struct settings_list **)var = find_setting(defaultval);
}
#endif
#ifdef HAVE_TOUCHSCREEN

#if defined(APPLICATION) \
 || defined(ONDA_VX747)  \
 || defined(ONDA_VX767)  \
 || defined(ONDA_VX747P) \
 || defined(ONDA_VX777)

#define DEFAULT_TOUCHSCREEN_MODE TOUCHSCREEN_POINT
#else
#define DEFAULT_TOUCHSCREEN_MODE TOUCHSCREEN_BUTTON
#endif

static void tsc_load_from_cfg(void* setting, char*value)
{
    struct touchscreen_parameter *var = (struct touchscreen_parameter*) setting;

    /* Replacement for sscanf(value, "%d ..., &var->A, ...); */
    int vals[7], count = 0;
    while(*value != 0 && count < 7)
    {
        if(isspace(*value))
            value++;
        else
        {
            vals[count++] = atoi(value);
            while(!isspace(*value))
                value++;
        }
    }
    var->A = vals[0];
    var->B = vals[1];
    var->C = vals[2];
    var->D = vals[3];
    var->E = vals[4];
    var->F = vals[5];
    var->divider = vals[6];
}

static char* tsc_write_to_cfg(void* setting, char*buf, int buf_len)
{
    const struct touchscreen_parameter *var = (const struct touchscreen_parameter*) setting;
    snprintf(buf, buf_len, "%d %d %d %d %d %d %d", var->A, var->B, var->C, var->D, var->E, var->F, var->divider);
    return buf;
}
static bool tsc_is_changed(void* setting, void* defaultval)
{
    return memcmp(setting, defaultval, sizeof(struct touchscreen_parameter)) != 0;
}
static void tsc_set_default(void* setting, void* defaultval)
{
    memcpy(setting, defaultval, sizeof(struct touchscreen_parameter));
}
#endif
#ifdef HAVE_HOTKEY
static void hotkey_callback(int var)
{
    if (get_current_activity() != ACTIVITY_QUICKSCREEN)
    {
        if (get_hotkey(var)->action == HOTKEY_PLUGIN)
            open_plugin_browse(ID2P(LANG_HOTKEY_WPS));
    }
}
static const char* hotkey_formatter(char* buffer, size_t buffer_size, int value,
                              const char* unit)
{
    (void)buffer;
    (void)buffer_size;
    (void)unit;
    return str(get_hotkey(value)->lang_id);
}
static int32_t hotkey_getlang(int value, int unit)
{
    (void)unit;
    return get_hotkey(value)->lang_id;
}
#endif /* HAVE_HOTKEY */

static void start_in_callback(int var)
{
    if (var - 2 == GO_TO_PLUGIN)
        open_plugin_browse(ID2P(LANG_START_SCREEN));
}

/* volume limiter */
static void volume_limit_load_from_cfg(void* var, char*value)
{
    *(int*)var = atoi(value);
}
static char* volume_limit_write_to_cfg(void* setting, char*buf, int buf_len)
{
    int current = *(int*)setting;
    itoa_buf(buf, buf_len, current);
    return buf;
}
static bool volume_limit_is_changed(void* setting, void* defaultval)
{
    (void)defaultval;
    int current = *(int*)setting;
    return (current != sound_max(SOUND_VOLUME));
}
static void volume_limit_set_default(void* setting, void* defaultval)
{
    (void)defaultval;
    *(int*)setting = sound_max(SOUND_VOLUME);
}

#if defined(HAVE_EROS_QN_CODEC)
static void hp_lo_select_apply(int arg)
{
    (void)arg;

    sound_settings_apply();
}
#endif

const struct settings_list settings[] = {
/* system_status settings .resume.cfg */
    SYSTEM_STATUS_SOUND(F_NO_WRAP, volume, LANG_VOLUME, "volume", SOUND_VOLUME),
#ifdef HAVE_PITCHCONTROL
    SYSTEM_STATUS(F_SOUNDSETTING, resume_pitch, PITCH_SPEED_100, "pitch"),
    SYSTEM_STATUS(F_SOUNDSETTING, resume_speed, PITCH_SPEED_100, "speed"),
#endif
    SYSTEM_STATUS(0, resume_index,   -1,     "IDX"),
    SYSTEM_STATUS(0, resume_crc32,   -1,     "CRC"),
    SYSTEM_STATUS(0, resume_elapsed, -1,     "ELA"),
    SYSTEM_STATUS(0, resume_offset,  -1,     "OFF"),
    SYSTEM_STATUS(0, resume_modified, false, "PLM"),
    SYSTEM_STATUS(0, runtime,         0,     "CRT"),
    SYSTEM_STATUS(0, topruntime,      0,     "TRT"),
    SYSTEM_STATUS(0, last_screen,    -1,     "PVS"),
/* sound settings */
    CUSTOM_SETTING(F_NO_WRAP, volume_limit, LANG_VOLUME_LIMIT,
                  NULL, "volume limit",
                  volume_limit_load_from_cfg, volume_limit_write_to_cfg,
                  volume_limit_is_changed, volume_limit_set_default),
    SOUND_SETTING(0, balance, LANG_BALANCE, "balance", SOUND_BALANCE),
/* Tone controls */
#ifdef AUDIOHW_HAVE_BASS
    SOUND_SETTING(F_NO_WRAP,bass, LANG_BASS, "bass", SOUND_BASS),
#endif
#ifdef AUDIOHW_HAVE_TREBLE
    SOUND_SETTING(F_NO_WRAP,treble, LANG_TREBLE, "treble", SOUND_TREBLE),
#endif
/* Hardware EQ tone controls */
#ifdef AUDIOHW_HAVE_EQ
/* Band gain is generic */
    SOUND_SETTING(F_NO_WRAP, hw_eq_bands[AUDIOHW_EQ_BAND1].gain,
                  LANG_HW_EQ_GAIN, "tone band1 gain", SOUND_EQ_BAND1_GAIN),
#ifdef AUDIOHW_HAVE_EQ_BAND2
    SOUND_SETTING(F_NO_WRAP, hw_eq_bands[AUDIOHW_EQ_BAND2].gain,
                  LANG_HW_EQ_GAIN, "tone band2 gain", SOUND_EQ_BAND2_GAIN),
#endif /* AUDIOHW_HAVE_EQ_BAND2 */
#ifdef AUDIOHW_HAVE_EQ_BAND3
    SOUND_SETTING(F_NO_WRAP, hw_eq_bands[AUDIOHW_EQ_BAND3].gain,
                  LANG_HW_EQ_GAIN, "tone band3 gain", SOUND_EQ_BAND3_GAIN),
#endif /* AUDIOHW_HAVE_EQ_BAND3 */
#ifdef AUDIOHW_HAVE_EQ_BAND4
    SOUND_SETTING(F_NO_WRAP, hw_eq_bands[AUDIOHW_EQ_BAND4].gain,
                  LANG_HW_EQ_GAIN, "tone band4 gain", SOUND_EQ_BAND4_GAIN),
#endif /* AUDIOHW_HAVE_EQ_BAND4 */
#ifdef AUDIOHW_HAVE_EQ_BAND5
    SOUND_SETTING(F_NO_WRAP, hw_eq_bands[AUDIOHW_EQ_BAND5].gain,
                  LANG_HW_EQ_GAIN, "tone band5 gain", SOUND_EQ_BAND5_GAIN),
#endif /* AUDIOHW_HAVE_EQ_BAND5 */
#ifdef HAVE_WM8978
    /* Frequencies vary with samplerate but at least the user has an idea
     * about the bands and it will be correct with normal playback rates. */
/* Band 1 */
    STRINGCHOICE_SETTING(F_SOUNDSETTING,
                         hw_eq_bands[AUDIOHW_EQ_BAND1].frequency,
                         LANG_HW_EQ_FREQUENCY, 0,"tone band1 frequency",
                         "80 Hz,105 Hz,135 Hz,175 Hz",
                         sound_set_hw_eq_band1_frequency, 4,
                         TALK_ID(80, UNIT_HERTZ), TALK_ID(105, UNIT_HERTZ),
                         TALK_ID(135, UNIT_HERTZ), TALK_ID(175, UNIT_HERTZ)),
/* Band 2 */
    STRINGCHOICE_SETTING(F_SOUNDSETTING,
                         hw_eq_bands[AUDIOHW_EQ_BAND2].frequency,
                         LANG_HW_EQ_FREQUENCY, 0,"tone band2 frequency",
                         "230 Hz,300 Hz,385 Hz,500 Hz",
                         sound_set_hw_eq_band2_frequency, 4,
                         TALK_ID(230, UNIT_HERTZ), TALK_ID(300, UNIT_HERTZ),
                         TALK_ID(385, UNIT_HERTZ), TALK_ID(500, UNIT_HERTZ)),
    CHOICE_SETTING(F_SOUNDSETTING, hw_eq_bands[AUDIOHW_EQ_BAND2].width,
                   LANG_HW_EQ_WIDTH, 0, "tone band2 width", "narrow,wide",
                   sound_set_hw_eq_band2_width, 2,
                   ID2P(LANG_HW_EQ_WIDTH_NARROW), ID2P(LANG_HW_EQ_WIDTH_WIDE)),
/* Band 3 */
    STRINGCHOICE_SETTING(F_SOUNDSETTING,
                         hw_eq_bands[AUDIOHW_EQ_BAND3].frequency,
                         LANG_HW_EQ_FREQUENCY, 0, "tone band3 frequency",
                         "650 Hz,850 Hz,1.1 kHz,1.4 kHz",
                         sound_set_hw_eq_band3_frequency, 4,
                         TALK_ID(650, UNIT_HERTZ), TALK_ID(850, UNIT_HERTZ),
                         TALK_ID_DECIMAL(11, 1, UNIT_KHZ),
                         TALK_ID_DECIMAL(14, 1, UNIT_KHZ)),
    CHOICE_SETTING(F_SOUNDSETTING,hw_eq_bands[AUDIOHW_EQ_BAND3].width,
                   LANG_HW_EQ_WIDTH, 0, "tone band3 width", "narrow,wide",
                   sound_set_hw_eq_band3_width, 2,
                   ID2P(LANG_HW_EQ_WIDTH_NARROW), ID2P(LANG_HW_EQ_WIDTH_WIDE)),
/* Band 4 */
    STRINGCHOICE_SETTING(F_SOUNDSETTING,
                         hw_eq_bands[AUDIOHW_EQ_BAND4].frequency,
                         LANG_HW_EQ_FREQUENCY, 0, "tone band4 frequency",
                         "1.8 kHz,2.4 kHz,3.2 kHz,4.1 kHz",
                         sound_set_hw_eq_band4_frequency, 4,
                         TALK_ID_DECIMAL(18, 1, UNIT_KHZ),
                         TALK_ID_DECIMAL(24, 1, UNIT_KHZ),
                         TALK_ID_DECIMAL(32, 1, UNIT_KHZ),
                         TALK_ID_DECIMAL(41, 1, UNIT_KHZ)),
    CHOICE_SETTING(F_SOUNDSETTING, hw_eq_bands[AUDIOHW_EQ_BAND4].width,
                   LANG_HW_EQ_WIDTH, 0, "tone band4 width", "narrow,wide",
                   sound_set_hw_eq_band4_width, 2,
                   ID2P(LANG_HW_EQ_WIDTH_NARROW), ID2P(LANG_HW_EQ_WIDTH_WIDE)),
/* Band 5 */
    STRINGCHOICE_SETTING(F_SOUNDSETTING,
                         hw_eq_bands[AUDIOHW_EQ_BAND5].frequency,
                         LANG_HW_EQ_FREQUENCY, 0, "tone band5 frequency",
                         "5.3 kHz,6.9 kHz,9.0 kHz,11.7 kHz",
                         sound_set_hw_eq_band5_frequency, 4,
                         TALK_ID_DECIMAL(53, 1, UNIT_KHZ),
                         TALK_ID_DECIMAL(69, 1, UNIT_KHZ),
                         TALK_ID_DECIMAL(90, 1, UNIT_KHZ),
                         TALK_ID_DECIMAL(117, 1, UNIT_KHZ)),
#endif /* HAVE_WM8978 */
#endif /* AUDIOHW_HAVE_EQ */
/* 3-d enhancement effect */
    CHOICE_SETTING(0, channel_config, LANG_CHANNEL_CONFIGURATION,
                   0,"channels",
                   "stereo,mono,custom,mono left,mono right,karaoke",
                   sound_set_channels, 6,
                   ID2P(LANG_CHANNEL_STEREO), ID2P(LANG_CHANNEL_MONO),
                   ID2P(LANG_CHANNEL_CUSTOM), ID2P(LANG_CHANNEL_LEFT),
                   ID2P(LANG_CHANNEL_RIGHT), ID2P(LANG_CHANNEL_KARAOKE)),
    SOUND_SETTING(0, stereo_width, LANG_STEREO_WIDTH,
                  "stereo_width", SOUND_STEREO_WIDTH),
#ifdef AUDIOHW_HAVE_DEPTH_3D
    SOUND_SETTING(0,depth_3d, LANG_DEPTH_3D, "3-d enhancement",
                  SOUND_DEPTH_3D),
#endif

#ifdef AUDIOHW_HAVE_FILTER_ROLL_OFF
    CHOICE_SETTING(F_SOUNDSETTING, roll_off, LANG_FILTER_ROLL_OFF, 0,
#if defined(AUDIOHW_HAVE_ES9218_ROLL_OFF)
                   "roll_off", "linear fast,linear slow,minimum fast,minimum slow,apodizing 1,apodizing 2,hybrid fast,brick wall", sound_set_filter_roll_off,
                   8, ID2P(LANG_FILTER_LINEAR_FAST), ID2P(LANG_FILTER_LINEAR_SLOW), ID2P(LANG_FILTER_MINIMUM_FAST), ID2P(LANG_FILTER_MINIMUM_SLOW),
                   ID2P(LANG_FILTER_APODIZING_1), ID2P(LANG_FILTER_APODIZING_2), ID2P(LANG_FILTER_HYBRID_FAST), ID2P(LANG_FILTER_BRICK_WALL)),
#elif defined(AUDIOHW_HAVE_SHORT2_ROLL_OFF)
                   "roll_off", "sharp,slow,short sharp,short slow", sound_set_filter_roll_off,
                   4, ID2P(LANG_FILTER_SHARP), ID2P(LANG_FILTER_SLOW), ID2P(LANG_FILTER_SHORT_SHARP), ID2P(LANG_FILTER_SHORT_SLOW)),
#elif defined(AUDIOHW_HAVE_SS_ROLL_OFF)
                   "roll_off", "sharp,slow,short sharp,short slow,super slow", sound_set_filter_roll_off,
                   5, ID2P(LANG_FILTER_SHARP), ID2P(LANG_FILTER_SLOW), ID2P(LANG_FILTER_SHORT_SHARP), ID2P(LANG_FILTER_SHORT_SLOW), ID2P(LANG_FILTER_SUPER_SLOW)),
#elif defined(AUDIOHW_HAVE_SHORT_ROLL_OFF)
                   "roll_off", "sharp,slow,short,bypass", sound_set_filter_roll_off,
                   4, ID2P(LANG_FILTER_SHARP), ID2P(LANG_FILTER_SLOW), ID2P(LANG_FILTER_SHORT), ID2P(LANG_FILTER_BYPASS)),
#else
                   "roll_off", "sharp,slow", sound_set_filter_roll_off,
                   2, ID2P(LANG_FILTER_SHARP), ID2P(LANG_FILTER_SLOW)),
#endif
#endif

#ifdef AUDIOHW_HAVE_POWER_MODE
    CHOICE_SETTING(F_SOUNDSETTING, power_mode, LANG_DAC_POWER_MODE,
                   TARGET_DEFAULT_DAC_POWER_MODE,
                   "dac_power_mode", "high,low", sound_set_power_mode,
                   2, ID2P(LANG_DAC_POWER_HIGH), ID2P(LANG_DAC_POWER_LOW)),
#endif

    /* playback */
    OFFON_SETTING(F_CB_ON_SELECT_ONLY|F_CB_ONLY_IF_CHANGED, playlist_shuffle,
                  LANG_SHUFFLE, false, "shuffle", shuffle_playlist_callback),

    CHOICE_SETTING(F_CB_ON_SELECT_ONLY|F_CB_ONLY_IF_CHANGED, repeat_mode,
                   LANG_REPEAT, REPEAT_OFF, "repeat", "off,all,one,shuffle"
#ifdef AB_REPEAT_ENABLE
                   ",ab"
#endif
                   , repeat_mode_callback,
#ifdef AB_REPEAT_ENABLE
                   5,
#else
                   4,
#endif
                   ID2P(LANG_OFF), ID2P(LANG_ALL), ID2P(LANG_REPEAT_ONE),
                   ID2P(LANG_SHUFFLE)
#ifdef AB_REPEAT_ENABLE
                   ,ID2P(LANG_REPEAT_AB)
#endif
                  ), /* CHOICE_SETTING( repeat_mode ) */
#ifdef HAVE_PLAY_FREQ
     TABLE_SETTING(F_SOUNDSETTING|F_CB_ON_SELECT_ONLY|F_CB_ONLY_IF_CHANGED,
                  play_frequency, LANG_FREQUENCY, 0, "playback frequency", "auto",
                  UNIT_KHZ, formatter_freq_unit_0_is_auto,
                  getlang_freq_unit_0_is_auto,
                  playback_frequency_callback,
#if HAVE_PLAY_FREQ >= 192
                  7,0,SAMPR_44,SAMPR_48,SAMPR_88,SAMPR_96,SAMPR_176,SAMPR_192),
#elif HAVE_PLAY_FREQ >= 96
                  5,0,SAMPR_44,SAMPR_48,SAMPR_88,SAMPR_96),
#elif HAVE_PLAY_FREQ >= 48
                  3,0,SAMPR_44,SAMPR_48),
#else
      #error "HAVE_PLAY_FREQ < 48???"
#endif
#endif /* HAVE_PLAY_FREQ */

#ifdef HAVE_ALBUMART
    CHOICE_SETTING(F_CB_ON_SELECT_ONLY|F_CB_ONLY_IF_CHANGED, album_art,
                   LANG_ALBUM_ART, 1, "album art",
                   "off,prefer embedded,prefer image file",
                   albumart_callback, 3,
                   ID2P(LANG_OFF), ID2P(LANG_PREFER_EMBEDDED),
                   ID2P(LANG_PREFER_IMAGE_FILE)),
#endif

    /* LCD */
#ifdef HAVE_LCD_CONTRAST
    /* its easier to leave this one un-macro()ed for the time being */
    { F_T_INT|F_DEF_ISFUNC|F_INT_SETTING, &global_settings.contrast,
        LANG_CONTRAST, FUNCTYPE(lcd_default_contrast), "contrast", {
            .int_setting = (struct int_setting[]) {
                { .option_callback = lcd_set_contrast,
                  .unit = UNIT_INT, .step = 1,
                  .min = MIN_CONTRAST_SETTING, .max = MAX_CONTRAST_SETTING,
                  .formatter = NULL, .get_talk_id = NULL }}}},
#endif
#ifdef HAVE_BACKLIGHT
    TABLE_SETTING_LIST(F_TIME_SETTING | F_ALLOW_ARBITRARY_VALS,
                    backlight_timeout, LANG_BACKLIGHT,
                    DEFAULT_BACKLIGHT_TIMEOUT, "backlight timeout",
                    off_on, UNIT_SEC, formatter_time_unit_0_is_always,
                    getlang_time_unit_0_is_always, backlight_set_timeout,
                    23, timeout_sec_common),
#if CONFIG_CHARGING
    TABLE_SETTING_LIST(F_TIME_SETTING | F_ALLOW_ARBITRARY_VALS,
                    backlight_timeout_plugged, LANG_BACKLIGHT_ON_WHEN_CHARGING,
                    DEFAULT_BACKLIGHT_TIMEOUT, "backlight timeout plugged",
                    off_on, UNIT_SEC, formatter_time_unit_0_is_always,
                    getlang_time_unit_0_is_always, backlight_set_timeout_plugged,
                    23, timeout_sec_common),
#endif
#endif /* HAVE_BACKLIGHT */
#ifdef HAVE_LCD_INVERT
    BOOL_SETTING(0, invert, LANG_INVERT, false ,"invert", off_on,
                 LANG_INVERT_LCD_INVERSE, LANG_NORMAL, lcd_set_invert_display),
#endif
#ifdef HAVE_LCD_FLIP
    OFFON_SETTING(0, flip_display, LANG_FLIP_DISPLAY, false, "flip display",
                  NULL),
#endif
    /* display */
     CHOICE_SETTING(F_TEMPVAR|F_THEMESETTING, cursor_style, LANG_INVERT_CURSOR,
 #ifdef HAVE_LCD_COLOR
                    3, "selector type",
                    "pointer,bar (inverse),bar (color),bar (gradient)", NULL, 4,
                    ID2P(LANG_INVERT_CURSOR_POINTER),
                    ID2P(LANG_INVERT_CURSOR_BAR),
                    ID2P(LANG_INVERT_CURSOR_COLOR),
                    ID2P(LANG_INVERT_CURSOR_GRADIENT)),
 #else
                    1, "selector type", "pointer,bar (inverse)", NULL, 2,
                    ID2P(LANG_INVERT_CURSOR_POINTER),
                    ID2P(LANG_INVERT_CURSOR_BAR)),
 #endif
    CHOICE_SETTING(F_THEMESETTING|F_TEMPVAR|F_NEEDAPPLY, statusbar,
                  LANG_STATUS_BAR, STATUSBAR_TOP, "statusbar","off,top,bottom",
                  NULL, 3, ID2P(LANG_OFF), ID2P(LANG_STATUSBAR_TOP),
                  ID2P(LANG_STATUSBAR_BOTTOM)),
#ifdef HAVE_REMOTE_LCD
    CHOICE_SETTING(F_THEMESETTING|F_TEMPVAR|F_NEEDAPPLY, remote_statusbar,
                  LANG_REMOTE_STATUSBAR, STATUSBAR_TOP, "remote statusbar","off,top,bottom",
                  NULL, 3, ID2P(LANG_OFF), ID2P(LANG_STATUSBAR_TOP),
                  ID2P(LANG_STATUSBAR_BOTTOM)),
#endif
    CHOICE_SETTING(F_THEMESETTING|F_TEMPVAR, scrollbar,
                  LANG_SCROLL_BAR, SCROLLBAR_LEFT, "scrollbar","off,left,right",
                  NULL, 3, ID2P(LANG_OFF), ID2P(LANG_LEFT), ID2P(LANG_RIGHT)),
    INT_SETTING(F_THEMESETTING, scrollbar_width, LANG_SCROLLBAR_WIDTH, 6,
                "scrollbar width",UNIT_INT, 3, MAX(LCD_WIDTH/10,25), 1,
                NULL, NULL, NULL),
#ifdef HAVE_TOUCHSCREEN
    TABLE_SETTING(F_ALLOW_ARBITRARY_VALS, list_line_padding, LANG_LIST_LINE_PADDING,
                  -1, "list padding", "auto,off", UNIT_PIXEL, list_pad_formatter,
                  list_pad_getlang, NULL, 16,
                  -1,0,2,4,6,8,10,12,16,20,24,28,32,38,44,50),
#endif
#if LCD_DEPTH > 1
    TABLE_SETTING(F_ALLOW_ARBITRARY_VALS, list_separator_height, LANG_LIST_SEPARATOR,
                  0, "list separator height", "auto,off", UNIT_PIXEL,
                  list_pad_formatter, list_pad_getlang, NULL, 15,
                  -1,0,1,2,3,4,5,7,9,11,13,16,20,25,30),
#ifdef HAVE_LCD_COLOR
    {F_T_INT|F_RGB|F_THEMESETTING ,&global_settings.list_separator_color,-1,
        INT(DEFAULT_THEME_SEPARATOR),"list separator color",UNUSED},
#endif
#endif
    CHOICE_SETTING(F_THEMESETTING, volume_type, LANG_VOLUME_DISPLAY, 0,
                   "volume display", graphic_numeric, NULL, 2,
                   ID2P(LANG_DISPLAY_GRAPHIC),
                   ID2P(LANG_DISPLAY_NUMERIC)),
    CHOICE_SETTING(F_THEMESETTING, battery_display, LANG_BATTERY_DISPLAY, 0,
                   "battery display", graphic_numeric, NULL, 2,
                   ID2P(LANG_DISPLAY_GRAPHIC), ID2P(LANG_DISPLAY_NUMERIC)),
    CHOICE_SETTING(0, timeformat, LANG_TIMEFORMAT, 0,
        "time format", "24hour,12hour", NULL, 2,
        ID2P(LANG_24_HOUR_CLOCK), ID2P(LANG_12_HOUR_CLOCK)),
    OFFON_SETTING(0,show_icons, LANG_SHOW_ICONS ,true,"show icons", NULL),
    /* system */
    INT_SETTING(F_TIME_SETTING, poweroff, LANG_POWEROFF_IDLE, 10,
                "idle poweroff", UNIT_MIN, 0,60,1,
                formatter_time_unit_0_is_off, getlang_time_unit_0_is_off,
                set_poweroff_timeout),
    INT_SETTING(F_BANFROMQS, max_files_in_playlist,
                LANG_MAX_FILES_IN_PLAYLIST,
#if CONFIG_CPU == PP5002 || CONFIG_CPU == PP5020 || CONFIG_CPU == PP5022
                  /** Slow CPU benefits greatly from building smaller playlists
                  On the iPod Mini 2nd gen, creating a playlist of 2000 entries takes around 10 seconds */
                  2000,
#elif MEMORYSIZE > 1
                  10000,
#else
                  400,
#endif
                  "max files in playlist", UNIT_INT, 1000, 32000, 1000,
                  NULL, NULL, NULL),
    INT_SETTING(F_BANFROMQS, max_files_in_dir, LANG_MAX_FILES_IN_DIR,
                MAX_FILES_IN_DIR_DEFAULT, "max files in dir", UNIT_INT,
                MAX_FILES_IN_DIR_STEP /* min */, MAX_FILES_IN_DIR_MAX,
                MAX_FILES_IN_DIR_STEP,
                NULL, NULL, NULL),
#ifdef HAVE_PERCEPTUAL_VOLUME
    CHOICE_SETTING(0, volume_adjust_mode, LANG_VOLUME_ADJUST_MODE,
                   VOLUME_ADJUST_DIRECT, "volume adjustment mode",
                   "direct,perceptual", NULL, 2,
                   ID2P(LANG_DIRECT), ID2P(LANG_PERCEPTUAL)),
    INT_SETTING_NOWRAP(0, volume_adjust_norm_steps, LANG_VOLUME_ADJUST_NORM_STEPS,
                       50, "perceptual volume step count", UNIT_INT,
                       MIN_NORM_VOLUME_STEPS, MAX_NORM_VOLUME_STEPS, 5,
                       NULL, NULL, NULL),
#endif
/* use this setting for user code even if there's no exchangable battery
 * support enabled */
#if BATTERY_CAPACITY_INC > 0
#if defined(IPOD_VIDEO) && !defined(SIMULATOR)
    /* its easier to leave this one un-macro()ed for the time being */
    { F_T_INT|F_DEF_ISFUNC|F_INT_SETTING, &global_settings.battery_capacity,
        LANG_BATTERY_CAPACITY, FUNCTYPE(battery_default_capacity),
        "battery capacity" , {
            .int_setting = (struct int_setting[]) {
                { .option_callback = set_battery_capacity,
                  .unit = UNIT_MAH, .step = BATTERY_CAPACITY_INC,
                  .min = BATTERY_CAPACITY_MIN, .max = BATTERY_CAPACITY_MAX,
                  .formatter = NULL, .get_talk_id = NULL }}}},
#else /* IPOD_VIDEO */
    INT_SETTING(0, battery_capacity, LANG_BATTERY_CAPACITY,
                BATTERY_CAPACITY_DEFAULT, "battery capacity", UNIT_MAH,
                BATTERY_CAPACITY_MIN, BATTERY_CAPACITY_MAX,
                BATTERY_CAPACITY_INC, NULL, NULL, set_battery_capacity),
#endif /* IPOD_VIDEO */
#endif
#if CONFIG_CHARGING
    OFFON_SETTING(0, car_adapter_mode,
                  LANG_CAR_ADAPTER_MODE, false, "car adapter mode", NULL),
    INT_SETTING_NOWRAP(0, car_adapter_mode_delay, LANG_CAR_ADAPTER_MODE_DELAY,
                5, "delay before resume", UNIT_SEC, 5, 30, 5,
                NULL, NULL, NULL),
#endif
#ifdef IPOD_ACCESSORY_PROTOCOL
    CHOICE_SETTING(0, serial_bitrate, LANG_SERIAL_BITRATE, 0, "serial bitrate",
                   "auto,9600,19200,38400,57600", iap_bitrate_set, 5, ID2P(LANG_SERIAL_BITRATE_AUTO),
           ID2P(LANG_SERIAL_BITRATE_9600),ID2P(LANG_SERIAL_BITRATE_19200),
           ID2P(LANG_SERIAL_BITRATE_38400),ID2P(LANG_SERIAL_BITRATE_57600)),
#endif
#ifdef HAVE_ACCESSORY_SUPPLY
    OFFON_SETTING(0, accessory_supply, LANG_ACCESSORY_SUPPLY,
                  true, "accessory power supply", accessory_supply_set),
#endif
#ifdef HAVE_LINEOUT_POWEROFF
    OFFON_SETTING(0, lineout_active, LANG_LINEOUT,
                  true, "lineout", lineout_set),
#endif
    /* tuner */
#if CONFIG_TUNER
    OFFON_SETTING(0, fm_force_mono, LANG_FM_MONO_MODE,
                  false, "force fm mono", toggle_mono_mode),
    SYSTEM_STATUS(0, last_frequency, 0, "PFQ"),
#endif
#if defined(HAVE_RDS_CAP) && defined(CONFIG_RTC)
    OFFON_SETTING(0, sync_rds_time, LANG_FM_SYNC_RDS_TIME, false, "sync_rds_time", NULL),
#endif

#ifdef HAVE_REMOTE_LCD
    /* remote lcd */
    INT_SETTING(0, remote_contrast, LANG_CONTRAST,
                DEFAULT_REMOTE_CONTRAST_SETTING, "remote contrast", UNIT_INT,
                MIN_REMOTE_CONTRAST_SETTING, MAX_REMOTE_CONTRAST_SETTING, 1,
                NULL, NULL, lcd_remote_set_contrast),
    BOOL_SETTING(0, remote_invert, LANG_INVERT, false ,"remote invert", off_on,
                 LANG_INVERT_LCD_INVERSE, LANG_NORMAL,
                 lcd_remote_set_invert_display),
    OFFON_SETTING(0,remote_flip_display, LANG_FLIP_DISPLAY,
                  false,"remote flip display", NULL),
    TABLE_SETTING_LIST(F_ALLOW_ARBITRARY_VALS, remote_backlight_timeout,
                  LANG_BACKLIGHT, 5,
                  "remote backlight timeout", off_on, UNIT_SEC,
                  formatter_time_unit_0_is_always, getlang_time_unit_0_is_always,
                  remote_backlight_set_timeout, 23, timeout_sec_common),
#if CONFIG_CHARGING
    TABLE_SETTING_LIST(F_ALLOW_ARBITRARY_VALS, remote_backlight_timeout_plugged,
                  LANG_BACKLIGHT_ON_WHEN_CHARGING, 10,
                  "remote backlight timeout plugged", off_on, UNIT_SEC,
                  formatter_time_unit_0_is_always, getlang_time_unit_0_is_always,
                  remote_backlight_set_timeout_plugged, 23, timeout_sec_common),
#endif
#ifdef HAVE_REMOTE_LCD_TICKING
    OFFON_SETTING(0, remote_reduce_ticking, LANG_REDUCE_TICKING,
                  false,"remote reduce ticking", NULL),
#endif
#endif

#ifdef HAVE_BACKLIGHT
    OFFON_SETTING(0, bl_filter_first_keypress,
                  LANG_BACKLIGHT_FILTER_FIRST_KEYPRESS, false,
                  "backlight filters first keypress", NULL),
#ifdef HAVE_REMOTE_LCD
    OFFON_SETTING(0, remote_bl_filter_first_keypress,
                  LANG_BACKLIGHT_FILTER_FIRST_KEYPRESS, false,
                  "backlight filters first remote keypress", NULL),
#endif
#endif /* HAVE_BACKLIGHT */

/** End of old RTC config block **/

#ifndef HAS_BUTTON_HOLD
    OFFON_SETTING(F_BANFROMQS, bt_selective_softlock_actions,
                  LANG_ACTION_ENABLED, false,
                  "No Screen Lock For Selected Actions", NULL),
    INT_SETTING(F_BANFROMQS, bt_selective_softlock_actions_mask,
                LANG_SOFTLOCK_SELECTIVE,
                0, "Selective Screen Lock Actions", UNIT_INT,
                0, 2048,2, NULL, NULL, NULL),
#endif /* !HAS_BUTTON_HOLD */

#ifdef HAVE_BACKLIGHT
    OFFON_SETTING(0, caption_backlight, LANG_CAPTION_BACKLIGHT,
                  false, "caption backlight", NULL),

    OFFON_SETTING(F_BANFROMQS, bl_selective_actions,
                  LANG_ACTION_ENABLED, false,
                  "No Backlight On Selected Actions", NULL),

    INT_SETTING(F_BANFROMQS, bl_selective_actions_mask,
                LANG_BACKLIGHT_SELECTIVE,
                0, "Selective Backlight Actions", UNIT_INT,
                0, 2048,2, NULL, NULL, NULL),
#ifdef HAVE_REMOTE_LCD
    OFFON_SETTING(0, remote_caption_backlight, LANG_CAPTION_BACKLIGHT,
                  false, "remote caption backlight", NULL),
#endif
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    INT_SETTING(F_NO_WRAP, brightness, LANG_BRIGHTNESS,
                DEFAULT_BRIGHTNESS_SETTING, "brightness",UNIT_INT,
                MIN_BRIGHTNESS_SETTING, MAX_BRIGHTNESS_SETTING, 1,
                NULL, NULL, backlight_set_brightness),
#endif
    /* backlight fading */
#if defined(HAVE_BACKLIGHT_FADING_INT_SETTING)
    TABLE_SETTING_LIST(F_TIME_SETTING | F_ALLOW_ARBITRARY_VALS, backlight_fade_in,
                  LANG_BACKLIGHT_FADE_IN, 300, "backlight fade in", "off",
                  UNIT_MS, formatter_time_unit_0_is_off, getlang_time_unit_0_is_off,
                  backlight_set_fade_in, 7, backlight_fade),
    TABLE_SETTING_LIST(F_TIME_SETTING | F_ALLOW_ARBITRARY_VALS, backlight_fade_out,
                  LANG_BACKLIGHT_FADE_OUT, 2000, "backlight fade out", "off",
                  UNIT_MS, formatter_time_unit_0_is_off,
                  getlang_time_unit_0_is_off,
                  backlight_set_fade_out, 10, backlight_fade),
#elif defined(HAVE_BACKLIGHT_FADING_BOOL_SETTING)
    OFFON_SETTING(0, backlight_fade_in, LANG_BACKLIGHT_FADE_IN,
                    true, "backlight fade in", backlight_set_fade_in),
    OFFON_SETTING(0, backlight_fade_out, LANG_BACKLIGHT_FADE_OUT,
                    true, "backlight fade out", backlight_set_fade_out),
#endif
#endif /* HAVE_BACKLIGHT */
    INT_SETTING(F_PADTITLE, scroll_speed, LANG_SCROLL_SPEED, 9,"scroll speed",
                UNIT_INT, 0, 17, 1, NULL, NULL, lcd_scroll_speed),
    INT_SETTING(F_TIME_SETTING | F_PADTITLE, scroll_delay, LANG_SCROLL_DELAY,
                1000, "scroll delay", UNIT_MS, 0, 3000, 100,
                formatter_time_unit_0_is_off,
                getlang_time_unit_0_is_off, lcd_scroll_delay),
    INT_SETTING(0, bidir_limit, LANG_BIDIR_SCROLL, 50, "bidir limit",
                UNIT_PERCENT, 0, 200, 25, NULL, NULL, lcd_bidir_scroll),
#ifdef HAVE_REMOTE_LCD
    INT_SETTING(0, remote_scroll_speed, LANG_SCROLL_SPEED, 9,
                "remote scroll speed", UNIT_INT, 0,17, 1,
                NULL, NULL, lcd_remote_scroll_speed),
    INT_SETTING(0, remote_scroll_step, LANG_SCROLL_STEP, 6,
                "remote scroll step", UNIT_PIXEL, 1, LCD_REMOTE_WIDTH, 1, NULL,
                NULL, lcd_remote_scroll_step),
    INT_SETTING(F_TIME_SETTING, remote_scroll_delay, LANG_SCROLL_DELAY, 1000,
                "remote scroll delay", UNIT_MS, 0, 3000, 100,
                formatter_time_unit_0_is_off, getlang_time_unit_0_is_off,
                lcd_remote_scroll_delay),
    INT_SETTING(0, remote_bidir_limit, LANG_BIDIR_SCROLL, 50,
                "remote bidir limit", UNIT_PERCENT, 0, 200, 25, NULL, NULL,
                lcd_remote_bidir_scroll),
#endif
    OFFON_SETTING(0, offset_out_of_view, LANG_SCREEN_SCROLL_VIEW,
                  false, "Screen Scrolls Out Of View", NULL),
    OFFON_SETTING(0, disable_mainmenu_scrolling, LANG_DISABLE_MAINMENU_SCROLLING,
                  false, "Disable main menu scrolling", NULL),
    INT_SETTING(F_PADTITLE, scroll_step, LANG_SCROLL_STEP, 6, "scroll step",
                UNIT_PIXEL, 1, LCD_WIDTH, 1, NULL, NULL, lcd_scroll_step),
    INT_SETTING(F_PADTITLE, screen_scroll_step, LANG_SCREEN_SCROLL_STEP, 16,
                "screen scroll step", UNIT_PIXEL, 1, LCD_WIDTH, 1, NULL, NULL, NULL),
    OFFON_SETTING(0,scroll_paginated,LANG_SCROLL_PAGINATED,
                  false,"scroll paginated",NULL),
    OFFON_SETTING(0,list_wraparound,LANG_LIST_WRAPAROUND,
                  true,"list wraparound",NULL),
    CHOICE_SETTING(0, list_order, LANG_LIST_ORDER,
#if defined(HAVE_SCROLLWHEEL) && !defined(FIIO_M3K)
                   1,
#else
                   0,
#endif
                   /* values are defined by the enum in option_select.h */
                   "list order", "descending,ascending",
                   NULL, 2, ID2P(LANG_DESCENDING), ID2P(LANG_ASCENDING)),
#ifdef HAVE_LCD_COLOR

    {F_T_INT|F_RGB|F_THEMESETTING ,&global_settings.fg_color,-1,
        INT(DEFAULT_THEME_FOREGROUND),"foreground color",UNUSED},
    {F_T_INT|F_RGB|F_THEMESETTING ,&global_settings.bg_color,-1,
        INT(DEFAULT_THEME_BACKGROUND),"background color",UNUSED},
    {F_T_INT|F_RGB|F_THEMESETTING ,&global_settings.lss_color,-1,
        INT(DEFAULT_THEME_SELECTOR_START),"line selector start color",UNUSED},
    {F_T_INT|F_RGB|F_THEMESETTING ,&global_settings.lse_color,-1,
        INT(DEFAULT_THEME_SELECTOR_END),"line selector end color",UNUSED},
    {F_T_INT|F_RGB|F_THEMESETTING ,&global_settings.lst_color,-1,
        INT(DEFAULT_THEME_SELECTOR_TEXT),"line selector text color",UNUSED},

#endif
    /* more playback */
    OFFON_SETTING(0,play_selected,LANG_PLAY_SELECTED,true,"play selected",NULL),
    CHOICE_SETTING(0, single_mode, LANG_SINGLE_MODE, 0,
                  "single mode",
                  "off,track,album,album artist,artist,composer,work,genre",
                  NULL, 8,
                  ID2P(LANG_OFF),
                  ID2P(LANG_TRACK),
                  ID2P(LANG_ID3_ALBUM),
                  ID2P(LANG_ID3_ALBUMARTIST),
                  ID2P(LANG_ID3_ARTIST),
                  ID2P(LANG_ID3_COMPOSER),
                  ID2P(LANG_ID3_GROUPING),
                  ID2P(LANG_ID3_GENRE)),
    OFFON_SETTING(0,party_mode,LANG_PARTY_MODE,false,"party mode",NULL),
    OFFON_SETTING(0,fade_on_stop,LANG_FADE_ON_STOP,true,"volume fade",NULL),
    INT_SETTING(F_TIME_SETTING, ff_rewind_min_step, LANG_FFRW_STEP, 1,
                "scan min step", UNIT_SEC, 1, 60, 1, NULL, NULL, NULL),
    CHOICE_SETTING(0, ff_rewind_accel, LANG_FFRW_ACCEL, 2,
                   "seek acceleration", "very fast,fast,normal,slow,very slow", NULL, 5,
                   ID2P(LANG_VERY_FAST), ID2P(LANG_FAST), ID2P(LANG_NORMAL),
                   ID2P(LANG_SLOW) , ID2P(LANG_VERY_SLOW)),
#if defined(HAVE_DISK_STORAGE)
    TABLE_SETTING(F_TIME_SETTING | F_ALLOW_ARBITRARY_VALS, buffer_margin,
                  LANG_MP3BUFFER_MARGIN, 5, "antiskip", NULL, UNIT_SEC,
                  NULL, NULL,
                  NULL,8, 5,15,30,60,120,180,300,600),
#endif
    /* disk */
#ifdef HAVE_DISK_STORAGE
    INT_SETTING(F_TIME_SETTING, disk_spindown, LANG_SPINDOWN, 5, "disk spindown",
                    UNIT_SEC, 3, 254, 1, NULL, NULL, STORAGE_FUNCTION(spindown)),
#endif /* HAVE_DISK_STORAGE */
    /* browser */
    TEXT_SETTING(0, start_directory, "start directory", "/", NULL, NULL),
    CHOICE_SETTING(0, dirfilter, LANG_FILTER, SHOW_SUPPORTED, "show files",
                   "all,supported,music,playlists", NULL, 4, ID2P(LANG_ALL),
                   ID2P(LANG_FILTER_SUPPORTED), ID2P(LANG_FILTER_MUSIC),
                   ID2P(LANG_PLAYLISTS)),
    /* file sorting */
    OFFON_SETTING(0, sort_case, LANG_SORT_CASE, false, "sort case", NULL),
    CHOICE_SETTING(0, sort_dir, LANG_SORT_DIR, 0 ,
                   "sort dirs", "alpha,oldest,newest", treesort_callback, 3,
                   ID2P(LANG_SORT_ALPHA), ID2P(LANG_SORT_DATE),
                   ID2P(LANG_SORT_DATE_REVERSE)),
    CHOICE_SETTING(0, sort_file, LANG_SORT_FILE, 0 ,
                   "sort files", "alpha,oldest,newest,type", treesort_callback, 4,
                   ID2P(LANG_SORT_ALPHA), ID2P(LANG_SORT_DATE),
                   ID2P(LANG_SORT_DATE_REVERSE) , ID2P(LANG_SORT_TYPE)),
    CHOICE_SETTING(0, sort_playlists, LANG_SORT_PLAYLISTS, 0 ,
                   "sort playlists", "alpha,oldest,newest", treesort_callback, 3,
                   ID2P(LANG_SORT_ALPHA), ID2P(LANG_SORT_DATE),
                   ID2P(LANG_SORT_DATE_REVERSE)),
    CHOICE_SETTING(0, interpret_numbers, LANG_SORT_INTERPRET_NUMBERS, 1,
                    "sort interpret number", "digits,numbers",treesort_callback, 2,
                    ID2P(LANG_SORT_INTERPRET_AS_DIGIT),
                    ID2P(LANG_SORT_INTERPRET_AS_NUMBERS)),
    CHOICE_SETTING(0, show_filename_ext, LANG_SHOW_FILENAME_EXT, 3,
                   "show filename exts", "off,on,unknown,view_all", NULL , 4 ,
                   ID2P(LANG_OFF), ID2P(LANG_ON), ID2P(LANG_UNKNOWN_TYPES),
                   ID2P(LANG_EXT_ONLY_VIEW_ALL)),
    OFFON_SETTING(0,browse_current,LANG_FOLLOW,false,"follow playlist",NULL),
    OFFON_SETTING(0,playlist_viewer_icons,LANG_SHOW_ICONS,true,
                  "playlist viewer icons",NULL),
    OFFON_SETTING(0,playlist_viewer_indices,LANG_SHOW_INDICES,true,
                  "playlist viewer indices",NULL),
    CHOICE_SETTING(0, playlist_viewer_track_display, LANG_TRACK_DISPLAY, 0,
                   "playlist viewer track display",
                   "track name,full path,title and album from tags,title from tags",
                   NULL, 4, ID2P(LANG_DISPLAY_TRACK_NAME_ONLY),
                   ID2P(LANG_DISPLAY_FULL_PATH),ID2P(LANG_DISPLAY_TITLEALBUM_FROMTAGS),
                   ID2P(LANG_DISPLAY_TITLE_FROMTAGS)),
    CHOICE_SETTING(0, recursive_dir_insert, LANG_RECURSE_DIRECTORY , RECURSE_ON,
                   "recursive directory insert", off_on_ask, NULL , 3 ,
                   ID2P(LANG_OFF), ID2P(LANG_ON), ID2P(LANG_ASK)),
    /* bookmarks */
    CHOICE_SETTING(0, autocreatebookmark, LANG_BOOKMARK_SETTINGS_AUTOCREATE,
                   BOOKMARK_NO, "autocreate bookmarks",
                   "off,on,ask,recent only - on,recent only - ask", NULL, 5,
                   ID2P(LANG_SET_BOOL_NO), ID2P(LANG_SET_BOOL_YES),
                   ID2P(LANG_ASK), ID2P(LANG_BOOKMARK_SETTINGS_RECENT_ONLY_YES),
                   ID2P(LANG_BOOKMARK_SETTINGS_RECENT_ONLY_ASK)),
    OFFON_SETTING(0, autoupdatebookmark, LANG_BOOKMARK_SETTINGS_AUTOUPDATE,
                   false, "autoupdate bookmarks", NULL),
    CHOICE_SETTING(0, autoloadbookmark, LANG_BOOKMARK_SETTINGS_AUTOLOAD,
                   BOOKMARK_NO, "autoload bookmarks", off_on_ask, NULL, 3,
                   ID2P(LANG_SET_BOOL_NO), ID2P(LANG_SET_BOOL_YES),
                   ID2P(LANG_ASK)),
    CHOICE_SETTING(0, usemrb, LANG_BOOKMARK_SETTINGS_MAINTAIN_RECENT_BOOKMARKS,
                   BOOKMARK_NO, "use most-recent-bookmarks",
                   "off,on,unique only,one per track", NULL, 4, ID2P(LANG_SET_BOOL_NO),
                   ID2P(LANG_SET_BOOL_YES),
                   ID2P(LANG_BOOKMARK_SETTINGS_ONE_PER_PLAYLIST),
                   ID2P(LANG_BOOKMARK_SETTINGS_ONE_PER_TRACK)),
    /* peak meter */
    TABLE_SETTING_LIST(F_TIME_SETTING | F_ALLOW_ARBITRARY_VALS, peak_meter_clip_hold,
                  LANG_PM_CLIP_HOLD, 60, "peak meter clip hold", "eternal",
                  UNIT_SEC, formatter_time_unit_0_is_eternal,
                  getlang_time_unit_0_is_eternal, peak_meter_set_clip_hold,
                  31, &timeout_sec_common[1]), /* skip -1 entry */
    TABLE_SETTING(F_TIME_SETTING | F_ALLOW_ARBITRARY_VALS, peak_meter_hold,
                  LANG_PM_PEAK_HOLD, 500, "peak meter hold", off, UNIT_MS,
                  formatter_time_unit_0_is_off, getlang_time_unit_0_is_off,NULL,
                  18, 0,200,300,500,1000,2000,3000,4000,5000,6000,7000,8000,
                  9000,10000,15000,20000,30000,60000),
    INT_SETTING(0, peak_meter_release, LANG_PM_RELEASE, 8, "peak meter release",
                UNIT_PM_TICK, 1, 0x7e, 1, NULL, NULL,NULL),
    OFFON_SETTING(0,peak_meter_dbfs,LANG_PM_DBFS,true,"peak meter dbfs",NULL),
    {F_T_INT, &global_settings.peak_meter_min, LANG_PM_MIN,INT(60),
        "peak meter min", UNUSED},
    {F_T_INT, &global_settings.peak_meter_max, LANG_PM_MAX,INT(0),
        "peak meter max", UNUSED},
#ifdef HAVE_RECORDING
    OFFON_SETTING(0, peak_meter_clipcounter, LANG_PM_CLIPCOUNTER, false,
                  "peak meter clipcounter", NULL),
#endif /* HAVE_RECORDING */
    /* voice */
    OFFON_SETTING(F_TEMPVAR, talk_menu, LANG_VOICE_MENU, true, "talk menu", NULL),
    CHOICE_SETTING(0, talk_dir, LANG_VOICE_DIR, 0,
                   "talk dir", off_number_spell, NULL, 3,
                   ID2P(LANG_OFF), ID2P(LANG_VOICE_NUMBER),
                   ID2P(LANG_VOICE_SPELL)),
    OFFON_SETTING(F_TEMPVAR, talk_dir_clip, LANG_VOICE_DIR_TALK, false,
                  "talk dir clip", NULL),
    CHOICE_SETTING(0, talk_file, LANG_VOICE_FILE, 0,
                   "talk file", off_number_spell, NULL, 3,
                   ID2P(LANG_OFF), ID2P(LANG_VOICE_NUMBER),
                   ID2P(LANG_VOICE_SPELL)),
    OFFON_SETTING(F_TEMPVAR, talk_file_clip, LANG_VOICE_FILE_TALK, false,
                  "talk file clip", NULL),
    OFFON_SETTING(F_TEMPVAR, talk_filetype, LANG_VOICE_FILETYPE, false,
                  "talk filetype", NULL),
    OFFON_SETTING(F_TEMPVAR, talk_battery_level, LANG_TALK_BATTERY_LEVEL, false,
                  "Announce Battery Level", NULL),
    INT_SETTING(0, talk_mixer_amp, LANG_TALK_MIXER_LEVEL, 100,
        "talk mixer level", UNIT_PERCENT, 0, 100, 5, NULL, NULL, voice_set_mixer_level),

#ifdef HAVE_RECORDING
     /* recording */
    TABLE_SETTING(F_TIME_SETTING | F_ALLOW_ARBITRARY_VALS | F_RECSETTING,
                  rec_timesplit,
                  LANG_SPLIT_TIME, 0, "rec timesplit", off, UNIT_MIN,
                  formatter_time_unit_0_is_off, getlang_time_unit_0_is_off,NULL,
                  16, 0,5,10,15,30,60,74,80,120,240,360,480,600,720,1080,1440),
    STRINGCHOICE_SETTING(F_RECSETTING, rec_sizesplit, LANG_SPLIT_SIZE, 0,
                         "rec sizesplit",
                         "off,5MB,10MB,15MB,32MB,64MB,75MB,100MB,128MB,"
                         "256MB,512MB,650MB,700MB,1GB,1.5GB,1.75GB",
                         NULL, 16, LANG_OFF,
                         TALK_ID(5, UNIT_MB), TALK_ID(10, UNIT_MB),
                         TALK_ID(15, UNIT_MB), TALK_ID(32, UNIT_MB),
                         TALK_ID(64, UNIT_MB), TALK_ID(75, UNIT_MB),
                         TALK_ID(100, UNIT_MB), TALK_ID(128, UNIT_MB),
                         TALK_ID(256, UNIT_MB), TALK_ID(512, UNIT_MB),
                         TALK_ID(650, UNIT_MB), TALK_ID(700, UNIT_MB),
                         TALK_ID(1024, UNIT_MB), TALK_ID(1536, UNIT_MB),
                         TALK_ID(1792, UNIT_MB)),
    {F_T_INT|F_RECSETTING|F_HAS_CFGVALS, &global_settings.rec_channels, LANG_CHANNELS, INT(0),
     "rec channels",{.cfg_vals="stereo,mono"}},
    {F_T_INT|F_RECSETTING|F_HAS_CFGVALS, &global_settings.rec_mono_mode,
     LANG_RECORDING_MONO_MODE, INT(0), "rec mono mode",{.cfg_vals="L+R,L,R"}},
    CHOICE_SETTING(F_RECSETTING, rec_split_type, LANG_SPLIT_TYPE, 0,
                   "rec split type", "Split,Stop,Shutdown", NULL, 3,
                   ID2P(LANG_START_NEW_FILE), ID2P(LANG_STOP_RECORDING),ID2P(LANG_STOP_RECORDING_AND_SHUTDOWN)),
    CHOICE_SETTING(F_RECSETTING, rec_split_method, LANG_SPLIT_MEASURE, 0,
                   "rec split method", "Time,Filesize", NULL, 2,
                   ID2P(LANG_TIME), ID2P(LANG_FILESIZE)),
    {F_T_INT|F_RECSETTING|F_HAS_CFGVALS, &global_settings.rec_source, LANG_RECORDING_SOURCE,
        INT(0), "rec source",
        {.cfg_vals=&HAVE_MIC_REC_(",mic")
        HAVE_LINE_REC_(",line")
        HAVE_SPDIF_REC_(",spdif")
        HAVE_FMRADIO_REC_(",fmradio")[1]}},
    INT_SETTING(F_TIME_SETTING | F_RECSETTING, rec_prerecord_time,
                LANG_RECORD_PRERECORD_TIME, 0,
                "prerecording time", UNIT_SEC, 0, 30, 1,
                formatter_time_unit_0_is_off, getlang_time_unit_0_is_off, NULL),

    TEXT_SETTING(F_RECSETTING, rec_directory, "rec path",
                     REC_BASE_DIR, NULL, NULL),
#ifdef HAVE_BACKLIGHT
    CHOICE_SETTING(F_RECSETTING, cliplight, LANG_CLIP_LIGHT, 0,
                   "cliplight", "off,main,both,remote", NULL,
#ifdef HAVE_REMOTE_LCD
                   4, ID2P(LANG_OFF), ID2P(LANG_MAIN_UNIT),
                   ID2P(LANG_REMOTE_MAIN), ID2P(LANG_REMOTE_UNIT)
#else
                   2, ID2P(LANG_OFF), ID2P(LANG_ON)
#endif
                  ),
#endif
#ifdef DEFAULT_REC_MIC_GAIN
    {F_T_INT|F_RECSETTING,&global_settings.rec_mic_gain,
        LANG_GAIN,INT(DEFAULT_REC_MIC_GAIN),
        "rec mic gain",UNUSED},
#endif /* DEFAULT_REC_MIC_GAIN */
#ifdef DEFAULT_REC_LEFT_GAIN
    {F_T_INT|F_RECSETTING,&global_settings.rec_left_gain,
        LANG_GAIN_LEFT,INT(DEFAULT_REC_LEFT_GAIN),
        "rec left gain",UNUSED},
#endif /* DEFAULT_REC_LEFT_GAIN */
#ifdef DEFAULT_REC_RIGHT_GAIN
    {F_T_INT|F_RECSETTING,&global_settings.rec_right_gain,LANG_GAIN_RIGHT,
        INT(DEFAULT_REC_RIGHT_GAIN),
        "rec right gain",UNUSED},
#endif /* DEFAULT_REC_RIGHT_GAIN */
    {F_T_INT|F_RECSETTING|F_HAS_CFGVALS,&global_settings.rec_frequency,
        LANG_FREQUENCY,INT(REC_FREQ_DEFAULT),
        "rec frequency",{.cfg_vals=REC_FREQ_CFG_VAL_LIST}},
    {F_T_INT|F_RECSETTING|F_HAS_CFGVALS,&global_settings.rec_format,
        LANG_FORMAT,INT(REC_FORMAT_DEFAULT),
        "rec format",{.cfg_vals=REC_FORMAT_CFG_VAL_LIST}},
    /** Encoder settings start - keep these together **/
    /* aiff_enc */
    /* (no settings yet) */
    /* mp3_enc */
    {F_T_INT|F_RECSETTING|F_HAS_CFGVALS, &global_settings.mp3_enc_config.bitrate,-1,
        INT(MP3_ENC_BITRATE_CFG_DEFAULT),
        "mp3_enc bitrate",{.cfg_vals=MP3_ENC_BITRATE_CFG_VALUE_LIST}},
    /* wav_enc */
    /* (no settings yet) */
    /* wavpack_enc */
    /* (no settings yet) */
    /** Encoder settings end **/
    /* values for the trigger */
    INT_SETTING(F_RECSETTING, rec_start_thres_db, LANG_RECORD_START_THRESHOLD, -35,
        "trigger start threshold dB", UNIT_DB, -89, 0, 1, NULL, NULL, NULL),
    INT_SETTING(F_RECSETTING, rec_start_thres_linear, LANG_RECORD_START_THRESHOLD, 5,
        "trigger start threshold linear", UNIT_PERCENT, 0, 100, 1, NULL, NULL, NULL),
    INT_SETTING(F_RECSETTING, rec_stop_thres_db, LANG_RECORD_STOP_THRESHOLD, -45,
        "trigger stop threshold dB", UNIT_DB, -89, 0, 1, NULL, NULL, NULL),
    INT_SETTING(F_RECSETTING, rec_stop_thres_linear, LANG_RECORD_STOP_THRESHOLD, 10,
        "trigger stop threshold linear", UNIT_PERCENT, 0, 100, 1, NULL, NULL, NULL),
    TABLE_SETTING_LIST(F_TIME_SETTING | F_RECSETTING, rec_start_duration,
        LANG_MIN_DURATION, 0, "trigger start duration",
        off, UNIT_SEC, NULL, NULL, NULL, 13, time_recording_trigger),
    TABLE_SETTING_LIST(F_TIME_SETTING | F_RECSETTING, rec_stop_postrec,
        LANG_MIN_DURATION, 0, "trigger stop duration",
        off, UNIT_SEC, NULL, NULL, NULL, 13, time_recording_trigger),
    TABLE_SETTING_LIST(F_TIME_SETTING | F_RECSETTING, rec_stop_gap,
        LANG_RECORD_STOP_GAP, 1, "trigger min gap",
        off, UNIT_SEC, NULL, NULL, NULL, 13, time_recording_trigger),
    CHOICE_SETTING(F_RECSETTING, rec_trigger_mode, LANG_RECORD_TRIGGER, TRIG_MODE_OFF,
       "trigger mode","off,once,repeat", NULL ,3,
       ID2P(LANG_OFF), ID2P(LANG_RECORD_TRIG_NOREARM), ID2P(LANG_REPEAT)),
    CHOICE_SETTING(F_RECSETTING, rec_trigger_type, LANG_RECORD_TRIGGER_TYPE, TRIG_TYPE_STOP,
        "trigger type","stop,pause,nf stp", NULL ,3,
       ID2P(LANG_RECORD_TRIGGER_STOP), ID2P(LANG_PAUSE), ID2P(LANG_RECORD_TRIGGER_NEWFILESTP)),
#endif /* HAVE_RECORDING */

#ifdef HAVE_HISTOGRAM
     /* TO DO: additional restictions of following REP items? */
    INT_SETTING(F_TIME_SETTING | F_RECSETTING, histogram_interval,
        LANG_HISTOGRAM_INTERVAL, 0,
        "histogram interval", UNIT_SEC, 0,4,1,
        NULL, NULL, NULL),
#endif /* HAVE_HISTOGRAM */

#ifdef HAVE_SPDIF_POWER
    OFFON_SETTING(F_SOUNDSETTING, spdif_enable, LANG_SPDIF_ENABLE, false,
                  "spdif enable", spdif_power_enable),
#endif
    CHOICE_SETTING(0, next_folder, LANG_NEXT_FOLDER, FOLDER_ADVANCE_OFF,
                   "folder navigation", "off,on,random",NULL ,3,
                   ID2P(LANG_SET_BOOL_NO), ID2P(LANG_SET_BOOL_YES),
                   ID2P(LANG_RANDOM)),
    BOOL_SETTING(0, constrain_next_folder, LANG_CONSTRAIN_NEXT_FOLDER, false,
                 "constrain next folder", off_on,
                 LANG_SET_BOOL_YES, LANG_SET_BOOL_NO, NULL),

#ifdef HAVE_TAGCACHE
    BOOL_SETTING(0, autoresume_enable, LANG_AUTORESUME, false,
                 "autoresume enable", off_on,
                 LANG_SET_BOOL_YES, LANG_SET_BOOL_NO, NULL),
    CHOICE_SETTING(0, autoresume_automatic, LANG_AUTORESUME_AUTOMATIC,
                   AUTORESUME_NEXTTRACK_NEVER,
                   "autoresume next track", "never,all,custom",
                   NULL, 3,
                   ID2P(LANG_SET_BOOL_NO),
                   ID2P(LANG_ALWAYS),
                   ID2P(LANG_AUTORESUME_CUSTOM)),
    TEXT_SETTING(0, autoresume_paths, "autoresume next track paths",
                 "/podcast:/podcasts", NULL, NULL),

    OFFON_SETTING(0, runtimedb, LANG_RUNTIMEDB_ACTIVE, false,
                  "gather runtime data", NULL),
    TEXT_SETTING(0, tagcache_scan_paths, "database scan paths",
                 DEFAULT_TAGCACHE_SCAN_PATHS, NULL, NULL),
    TEXT_SETTING(0, tagcache_db_path, "database path",
                 ROCKBOX_DIR, NULL, NULL),
#endif

    /* replay gain */
    CHOICE_SETTING(F_SOUNDSETTING, replaygain_settings.type,
                   LANG_REPLAYGAIN_MODE, REPLAYGAIN_SHUFFLE, "replaygain type",
                   "track,album,track shuffle,off", NULL, 4, ID2P(LANG_TRACK_GAIN),
                   ID2P(LANG_ALBUM_GAIN), ID2P(LANG_SHUFFLE_GAIN), ID2P(LANG_OFF)),
    OFFON_SETTING(F_SOUNDSETTING, replaygain_settings.noclip,
                  LANG_REPLAYGAIN_NOCLIP, false, "replaygain noclip", NULL),
    INT_SETTING_NOWRAP(F_SOUNDSETTING, replaygain_settings.preamp,
                       LANG_REPLAYGAIN_PREAMP, 0, "replaygain preamp",
                       UNIT_DB, -120, 120, 5, db_format, get_dec_talkid, NULL),

    CHOICE_SETTING(0, beep, LANG_BEEP, 0, "beep", "off,weak,moderate,strong",
                   NULL, 4, ID2P(LANG_OFF), ID2P(LANG_WEAK),
                   ID2P(LANG_MODERATE), ID2P(LANG_STRONG)),

#ifdef HAVE_CROSSFADE
    /* crossfade */
    CHOICE_SETTING(F_SOUNDSETTING, crossfade, LANG_CROSSFADE_ENABLE, 0,
                   "crossfade",
                   "off,auto track change,man track skip,shuffle,shuffle or man track skip,always",
                   NULL, 6, ID2P(LANG_OFF), ID2P(LANG_AUTOTRACKSKIP),
                   ID2P(LANG_MANTRACKSKIP), ID2P(LANG_SHUFFLE),
                   ID2P(LANG_SHUFFLE_TRACKSKIP), ID2P(LANG_ALWAYS)),
    INT_SETTING(F_TIME_SETTING | F_SOUNDSETTING, crossfade_fade_in_delay,
                LANG_CROSSFADE_FADE_IN_DELAY, 0,
                "crossfade fade in delay", UNIT_SEC, 0, 7, 1, NULL, NULL, NULL),
    INT_SETTING(F_TIME_SETTING | F_SOUNDSETTING, crossfade_fade_out_delay,
                LANG_CROSSFADE_FADE_OUT_DELAY, 0,
                "crossfade fade out delay", UNIT_SEC, 0, 7, 1, NULL, NULL,NULL),
    INT_SETTING(F_TIME_SETTING | F_SOUNDSETTING, crossfade_fade_in_duration,
                LANG_CROSSFADE_FADE_IN_DURATION, 2,
                "crossfade fade in duration", UNIT_SEC, 0, 15, 1, NULL, NULL, NULL),
    INT_SETTING(F_TIME_SETTING | F_SOUNDSETTING, crossfade_fade_out_duration,
                LANG_CROSSFADE_FADE_OUT_DURATION, 2,
                "crossfade fade out duration", UNIT_SEC, 0, 15, 1, NULL, NULL, NULL),
    CHOICE_SETTING(F_SOUNDSETTING, crossfade_fade_out_mixmode,
                   LANG_CROSSFADE_FADE_OUT_MODE, 0,
                   "crossfade fade out mode", "crossfade,mix", NULL, 2,
                   ID2P(LANG_CROSSFADE), ID2P(LANG_MIX)),
#endif

    /* crossfeed */
    CHOICE_SETTING(F_SOUNDSETTING, crossfeed, LANG_CROSSFEED, 0,"crossfeed",
                   "off,meier,custom", dsp_set_crossfeed_type, 3,
                   ID2P(LANG_OFF), ID2P(LANG_CROSSFEED_MEIER),
                   ID2P(LANG_CROSSFEED_CUSTOM)),
    INT_SETTING_NOWRAP(F_SOUNDSETTING, crossfeed_direct_gain,
                       LANG_CROSSFEED_DIRECT_GAIN, -15,
                       "crossfeed direct gain", UNIT_DB, -60, 0, 5,
                       db_format, get_dec_talkid,dsp_set_crossfeed_direct_gain),
    INT_SETTING_NOWRAP(F_SOUNDSETTING, crossfeed_cross_gain,
                       LANG_CROSSFEED_CROSS_GAIN, -60,
                       "crossfeed cross gain", UNIT_DB, -120, -30, 5,
                       db_format, get_dec_talkid, crossfeed_cross_set),
    INT_SETTING_NOWRAP(F_SOUNDSETTING, crossfeed_hf_attenuation,
                       LANG_CROSSFEED_HF_ATTENUATION, -160,
                       "crossfeed hf attenuation", UNIT_DB, -240, -60, 5,
                       db_format, get_dec_talkid, crossfeed_cross_set),
    INT_SETTING_NOWRAP(F_SOUNDSETTING, crossfeed_hf_cutoff,
                       LANG_CROSSFEED_HF_CUTOFF, 700,
                       "crossfeed hf cutoff", UNIT_HERTZ, 500, 2000, 100,
                       NULL, NULL, crossfeed_cross_set),

    /* equalizer */
    OFFON_SETTING(F_EQSETTING, eq_enabled, LANG_EQUALIZER_ENABLED, false,
                  "eq enabled", eq_enabled_option_callback),

    INT_SETTING_NOWRAP(F_EQSETTING, eq_precut, LANG_EQUALIZER_PRECUT, 0,
                       "eq precut", UNIT_DB, 0, 240, 1, eq_precut_format,
                       get_precut_talkid, dsp_set_eq_precut),

#define EQ_BAND(id, string) \
        CUSTOM_SETTING(F_EQSETTING, eq_band_settings[id], -1,   \
                  &eq_defaults[id], string,                     \
                  eq_load_from_cfg, eq_write_to_cfg,            \
                  eq_is_changed, eq_set_default)
    EQ_BAND(0, "eq low shelf filter"),
    EQ_BAND(1, "eq peak filter 1"),
    EQ_BAND(2, "eq peak filter 2"),
    EQ_BAND(3, "eq peak filter 3"),
    EQ_BAND(4, "eq peak filter 4"),
    EQ_BAND(5, "eq peak filter 5"),
    EQ_BAND(6, "eq peak filter 6"),
    EQ_BAND(7, "eq peak filter 7"),
    EQ_BAND(8, "eq peak filter 8"),
    EQ_BAND(9, "eq high shelf filter"),
#undef EQ_BAND

    /* dithering */
    OFFON_SETTING(F_SOUNDSETTING, dithering_enabled, LANG_DITHERING, false,
                  "dithering enabled", dsp_dither_enable),
    /* surround */
     TABLE_SETTING(F_TIME_SETTING | F_SOUNDSETTING, surround_enabled,
                  LANG_SURROUND, 0, "surround enabled", off,
                  UNIT_MS, formatter_time_unit_0_is_off,
                  getlang_time_unit_0_is_off,
                  dsp_surround_enable, 6,
                  0,5,8,10,15,30),
    INT_SETTING_NOWRAP(F_SOUNDSETTING, surround_balance,
                       LANG_BALANCE, 35,
                       "surround balance", UNIT_PERCENT, 0, 99,
                       1, NULL, NULL, dsp_surround_set_balance),
    INT_SETTING_NOWRAP(F_SOUNDSETTING, surround_fx1,
                       LANG_SURROUND_FX1, 3400,
                       "surround_fx1", UNIT_HERTZ, 600, 8000,
                       200, NULL, NULL, surround_set_factor),
    INT_SETTING_NOWRAP(F_SOUNDSETTING, surround_fx2,
                       LANG_SURROUND_FX2, 320,
                       "surround_fx2", UNIT_HERTZ, 40, 400,
                       40, NULL, NULL, surround_set_factor),
    OFFON_SETTING(F_SOUNDSETTING, surround_method2, LANG_SURROUND_METHOD2, false,
                  "side only", dsp_surround_side_only),
    INT_SETTING_NOWRAP(F_SOUNDSETTING, surround_mix,
                       LANG_SURROUND_MIX, 50,
                       "surround mix", UNIT_PERCENT, 0, 100,
                       5, NULL, NULL, dsp_surround_mix),
    /* auditory fatigue reduction */
    CHOICE_SETTING(F_SOUNDSETTING|F_NO_WRAP, afr_enabled,
                       LANG_AFR, 0,"afr enabled",
                       "off,weak,moderate,strong", dsp_afr_enable, 4,
                       ID2P(LANG_OFF), ID2P(LANG_WEAK),ID2P(LANG_MODERATE),ID2P(LANG_STRONG)),
    /* PBE */
    INT_SETTING_NOWRAP(F_SOUNDSETTING, pbe,
                       LANG_PBE, 0,
                       "pbe", UNIT_PERCENT, 0, 100,
                       25, NULL, NULL, dsp_pbe_enable),
    INT_SETTING_NOWRAP(F_SOUNDSETTING, pbe_precut,
                       LANG_EQUALIZER_PRECUT, -25,
                       "pbe precut", UNIT_DB, -45, 0,
                       1, db_format, NULL, dsp_pbe_precut),
#ifdef HAVE_PITCHCONTROL
    /* timestretch */
    OFFON_SETTING(F_SOUNDSETTING, timestretch_enabled, LANG_TIMESTRETCH, false,
                  "timestretch enabled", dsp_timestretch_enable),
#endif

    /* compressor */
    INT_SETTING_NOWRAP(F_SOUNDSETTING, compressor_settings.threshold,
                       LANG_COMPRESSOR_THRESHOLD, 0,
                       "compressor threshold", UNIT_DB, 0, -24,
                       -3, formatter_unit_0_is_off, getlang_unit_0_is_off,
                       compressor_set),
    CHOICE_SETTING(F_SOUNDSETTING|F_NO_WRAP, compressor_settings.makeup_gain,
                   LANG_COMPRESSOR_GAIN, 1, "compressor makeup gain",
                   "off,auto", compressor_set, 2,
                   ID2P(LANG_OFF), ID2P(LANG_AUTO)),
    CHOICE_SETTING(F_SOUNDSETTING|F_NO_WRAP, compressor_settings.ratio,
                   LANG_COMPRESSOR_RATIO, 1, "compressor ratio",
                   "2:1,4:1,6:1,10:1,limit", compressor_set, 5,
                   ID2P(LANG_COMPRESSOR_RATIO_2), ID2P(LANG_COMPRESSOR_RATIO_4),
                   ID2P(LANG_COMPRESSOR_RATIO_6), ID2P(LANG_COMPRESSOR_RATIO_10),
                   ID2P(LANG_COMPRESSOR_RATIO_LIMIT)),
    CHOICE_SETTING(F_SOUNDSETTING|F_NO_WRAP, compressor_settings.knee,
                   LANG_COMPRESSOR_KNEE, 1, "compressor knee",
                   "hard knee,soft knee", compressor_set, 2,
                   ID2P(LANG_COMPRESSOR_HARD_KNEE), ID2P(LANG_COMPRESSOR_SOFT_KNEE)),
    INT_SETTING_NOWRAP(F_TIME_SETTING | F_SOUNDSETTING,
                       compressor_settings.attack_time,
                       LANG_COMPRESSOR_ATTACK, 5,
                       "compressor attack time", UNIT_MS, 0, 30,
                       5, NULL, NULL, compressor_set),
    INT_SETTING_NOWRAP(F_TIME_SETTING | F_SOUNDSETTING,
                       compressor_settings.release_time,
                       LANG_COMPRESSOR_RELEASE, 500,
                       "compressor release time", UNIT_MS, 100, 1000,
                       100, NULL, NULL, compressor_set),

#ifdef AUDIOHW_HAVE_BASS_CUTOFF
    SOUND_SETTING(F_NO_WRAP, bass_cutoff, LANG_BASS_CUTOFF,
                  "bass cutoff", SOUND_BASS_CUTOFF),
#endif
#ifdef AUDIOHW_HAVE_TREBLE_CUTOFF
    SOUND_SETTING(F_NO_WRAP, treble_cutoff, LANG_TREBLE_CUTOFF,
                  "treble cutoff", SOUND_TREBLE_CUTOFF),
#endif
#ifdef HAVE_DIRCACHE
    /*enable dircache for all targets > 2MB of RAM by default*/
    OFFON_SETTING(F_BANFROMQS,dircache,LANG_DIRCACHE_ENABLE,true,"dircache",NULL),
    SYSTEM_STATUS(0, dircache_size, 0, "DSZ"),
#endif

#ifdef HAVE_TAGCACHE
#ifdef HAVE_TC_RAMCACHE
    CHOICE_SETTING(F_BANFROMQS, tagcache_ram, LANG_TAGCACHE_RAM,
                   0, "tagcache_ram", "off,on,quick",
                   NULL, 3,
                   ID2P(LANG_OFF), ID2P(LANG_ON), ID2P(LANG_QUICK_IGNORE_DIRACHE)),
#endif
    OFFON_SETTING(F_BANFROMQS, tagcache_autoupdate, LANG_TAGCACHE_AUTOUPDATE, false,
                  "tagcache_autoupdate", NULL),
#endif
    CHOICE_SETTING(F_TEMPVAR, default_codepage, LANG_DEFAULT_CODEPAGE, 14,
                   "default codepage",
                   /* The order must match with that in unicode.c */
                   "iso8859-1,iso8859-7,iso8859-8,cp1251,iso8859-11,cp1256,"
                   "iso8859-9,iso8859-2,cp1250,cp1252,sjis,gb2312,ksx1001,big5,utf-8",
                   NULL, 15,
                   ID2P(LANG_CODEPAGE_LATIN1),
                   ID2P(LANG_CODEPAGE_GREEK),
                   ID2P(LANG_CODEPAGE_HEBREW), ID2P(LANG_CODEPAGE_CYRILLIC),
                   ID2P(LANG_CODEPAGE_THAI), ID2P(LANG_CODEPAGE_ARABIC),
                   ID2P(LANG_CODEPAGE_TURKISH),
                   ID2P(LANG_CODEPAGE_LATIN_EXTENDED),
                   ID2P(LANG_CODEPAGE_CENTRAL_EUROPEAN),
                   ID2P(LANG_CODEPAGE_WESTERN_EUROPEAN),
                   ID2P(LANG_CODEPAGE_JAPANESE),
                   ID2P(LANG_CODEPAGE_SIMPLIFIED), ID2P(LANG_CODEPAGE_KOREAN),
                   ID2P(LANG_CODEPAGE_TRADITIONAL), ID2P(LANG_CODEPAGE_UTF8)),

    OFFON_SETTING(0, warnon_erase_dynplaylist, LANG_WARN_ERASEDYNPLAYLIST_MENU,
                  true, "warn when erasing dynamic playlist",NULL),
    OFFON_SETTING(0, keep_current_track_on_replace_playlist, LANG_KEEP_CURRENT_TRACK_ON_REPLACE,
                  true, "keep current track when replacing playlist",NULL),
    OFFON_SETTING(0, show_shuffled_adding_options, LANG_SHOW_SHUFFLED_ADDING_OPTIONS, true,
                      "show shuffled adding options", NULL),
    CHOICE_SETTING(0, show_queue_options, LANG_SHOW_QUEUE_OPTIONS, 0,
                      "show queue options", "off,on,in submenu",
                      NULL, 3,
                      ID2P(LANG_SET_BOOL_NO),
                      ID2P(LANG_SET_BOOL_YES),
                      ID2P(LANG_IN_SUBMENU)),

    CHOICE_SETTING(0, browser_default, LANG_DEFAULT_BROWSER, 0,
                      "default browser",
#ifdef HAVE_TAGCACHE
                      "files,database,playlists",
#else
                      "files,playlists",
#endif
                      NULL,
#ifdef HAVE_TAGCACHE
                      3
#else
                      2
#endif
                      ,ID2P(LANG_DIR_BROWSER),
#ifdef HAVE_TAGCACHE
                      ID2P(LANG_TAGCACHE),
#endif
                      ID2P(LANG_PLAYLISTS)),

#ifdef HAVE_BACKLIGHT
    CHOICE_SETTING(0, backlight_on_button_hold,
                   LANG_BACKLIGHT_ON_BUTTON_HOLD,
#ifdef HAS_BUTTON_HOLD
                   1,
#else
                   0,
#endif
                   "backlight on button hold", "normal,off,on",
                   backlight_set_on_button_hold, 3,
                   ID2P(LANG_NORMAL), ID2P(LANG_OFF), ID2P(LANG_ON)),

#ifdef HAVE_LCD_SLEEP_SETTING
    TABLE_SETTING_LIST(F_TIME_SETTING | F_ALLOW_ARBITRARY_VALS,
                    lcd_sleep_after_backlight_off, LANG_LCD_SLEEP_AFTER_BACKLIGHT_OFF,
                    5, "lcd sleep after backlight off",
                    off_on, UNIT_SEC, formatter_time_unit_0_is_always,
                    getlang_time_unit_0_is_always, lcd_set_sleep_after_backlight_off,
                    23, timeout_sec_common),
#endif /* HAVE_LCD_SLEEP_SETTING */
#endif /* HAVE_BACKLIGHT */

    OFFON_SETTING(0, hold_lr_for_scroll_in_list, -1, true,
                  "hold_lr_for_scroll_in_list",NULL),
    CHOICE_SETTING(0, show_path_in_browser, LANG_SHOW_PATH, SHOW_PATH_CURRENT,
                   "show path in browser", "off,current directory,full path",
                   NULL, 3, ID2P(LANG_OFF), ID2P(LANG_SHOW_PATH_CURRENT),
                   ID2P(LANG_DISPLAY_FULL_PATH)),

#ifdef HAVE_AGC
    {F_T_INT,&global_settings.rec_agc_preset_mic,LANG_RECORDING_AGC_PRESET,
        INT(1),"agc mic preset",UNUSED},
    {F_T_INT,&global_settings.rec_agc_preset_line,LANG_RECORDING_AGC_PRESET,
        INT(1),"agc line preset",UNUSED},
    {F_T_INT,&global_settings.rec_agc_maxgain_mic,-1,INT(104),
        "agc maximum mic gain",UNUSED},
    {F_T_INT,&global_settings.rec_agc_maxgain_line,-1,INT(96),
        "agc maximum line gain",UNUSED},
    {F_T_INT|F_HAS_CFGVALS,&global_settings.rec_agc_cliptime,LANG_RECORDING_AGC_CLIPTIME,
        INT(1),"agc cliptime", {.cfg_vals = "0.2 s,0.4 s,0.6 s,0.8 s,1 s"}},
#endif

#ifdef HAVE_REMOTE_LCD
#ifdef HAS_REMOTE_BUTTON_HOLD
    CHOICE_SETTING(0, remote_backlight_on_button_hold,
                   LANG_BACKLIGHT_ON_BUTTON_HOLD, 0,
                   "remote backlight on button hold",
                   "normal,off,on", remote_backlight_set_on_button_hold, 3,
                   ID2P(LANG_NORMAL), ID2P(LANG_OFF), ID2P(LANG_ON)),
#endif
#endif
#ifdef HAVE_HEADPHONE_DETECTION
    CHOICE_SETTING(0, unplug_mode, LANG_HEADPHONE_UNPLUG, 0,
                   "pause on headphone unplug", "off,pause,pause and resume",
                   NULL, 3, ID2P(LANG_OFF), ID2P(LANG_PAUSE),
                   ID2P(LANG_HEADPHONE_UNPLUG_RESUME)),
    OFFON_SETTING(0, unplug_autoresume,
                  LANG_HEADPHONE_UNPLUG_DISABLE_AUTORESUME, false,
                  "disable autoresume if phones not present",NULL),
#endif
    INT_SETTING(F_TIME_SETTING, pause_rewind, LANG_PAUSE_REWIND, 0,
                "rewind duration on pause", UNIT_SEC, 0, 15, 1,
                formatter_time_unit_0_is_off, getlang_time_unit_0_is_off, NULL),
#if CONFIG_TUNER
    CHOICE_SETTING(0, fm_region, LANG_FM_REGION, 0,
                   "fm_region", "eu,us,jp,kr,it,wo", set_radio_region, 6,
                   ID2P(LANG_FM_EUROPE), ID2P(LANG_FM_US),
                   ID2P(LANG_FM_JAPAN), ID2P(LANG_FM_KOREA),
                   ID2P(LANG_FM_ITALY), ID2P(LANG_FM_OTHER)),
#endif

#if CONFIG_TUNER
    TEXT_SETTING(0, fmr_file, "fmr", "-",
                     FMPRESET_PATH "/", ".fmr"),
    TEXT_SETTING(F_THEMESETTING|F_NEEDAPPLY,fms_file, "fms",
                     DEFAULT_FMS_NAME, SBS_DIR "/", ".fms"),
#ifdef HAVE_REMOTE_LCD
    TEXT_SETTING(F_THEMESETTING|F_NEEDAPPLY,rfms_file, "rfms",
                     DEFAULT_FMS_NAME, SBS_DIR "/", ".rfms"),
#endif
#endif /* CONFIG_TUNER */
    TEXT_SETTING(F_THEMESETTING|F_NEEDAPPLY, font_file, "font",
                     DEFAULT_FONTNAME, FONT_DIR "/", ".fnt"),
    INT_SETTING(0, glyphs_to_cache, LANG_GLYPHS, DEFAULT_GLYPHS,
                "glyphs", UNIT_INT, MIN_GLYPHS, MAX_GLYPHS, 10,
                NULL, NULL, NULL),
#ifdef HAVE_REMOTE_LCD
    TEXT_SETTING(F_THEMESETTING|F_NEEDAPPLY, remote_font_file, "remote font",
                     DEFAULT_REMOTE_FONTNAME, FONT_DIR "/", ".fnt"),
#endif
    TEXT_SETTING(F_THEMESETTING|F_NEEDAPPLY,wps_file, "wps",
                     DEFAULT_WPSNAME, WPS_DIR "/", ".wps"),
    TEXT_SETTING(F_THEMESETTING|F_NEEDAPPLY,sbs_file, "sbs",
                     DEFAULT_SBSNAME, SBS_DIR "/", ".sbs"),
#ifdef HAVE_REMOTE_LCD
    TEXT_SETTING(F_THEMESETTING|F_NEEDAPPLY,rwps_file,"rwps",
                     DEFAULT_WPSNAME, WPS_DIR "/", ".rwps"),
    TEXT_SETTING(F_THEMESETTING|F_NEEDAPPLY,rsbs_file, "rsbs",
                     DEFAULT_SBSNAME, SBS_DIR "/", ".rsbs"),
#endif
    TEXT_SETTING(0,lang_file,"lang","",LANG_DIR "/",".lng"),
#if LCD_DEPTH > 1
    TEXT_SETTING(F_THEMESETTING|F_NEEDAPPLY,backdrop_file,"backdrop",
                     DEFAULT_BACKDROP, NULL, NULL),
#endif
    TEXT_SETTING(0,kbd_file,"kbd","-",ROCKBOX_DIR "/",".kbd"),
#ifdef HAVE_USB_CHARGING_ENABLE
    CHOICE_SETTING(0, usb_charging, LANG_USB_CHARGING, TARGET_USB_CHARGING_DEFAULT, "usb charging",
                   "off,on,force", NULL, 3, ID2P(LANG_SET_BOOL_NO),
                   ID2P(LANG_SET_BOOL_YES), ID2P(LANG_FORCE)),
#endif
    OFFON_SETTING(F_BANFROMQS,cuesheet,LANG_CUESHEET_ENABLE,false,"cuesheet support",
                  NULL),
    TABLE_SETTING_LIST(F_TIME_SETTING | F_ALLOW_ARBITRARY_VALS, skip_length,
                  LANG_SKIP_LENGTH, 0, "skip length",
                  "outro,track",
                  UNIT_SEC, formatter_time_unit_0_is_skip_track,
                  getlang_time_unit_0_is_skip_track, NULL,
                  25, timeout_sec_common),
    CHOICE_SETTING(F_CB_ON_SELECT_ONLY, start_in_screen, LANG_START_SCREEN, 1,
                   "start in screen", "previous,root,files,"
#ifdef HAVE_TAGCACHE
#define START_DB_COUNT 1
                   "db,"
#else
#define START_DB_COUNT 0
#endif
                   "wps,menu,"
#ifdef HAVE_RECORDING
#define START_REC_COUNT 1
                   "recording,"
#else
#define START_REC_COUNT 0
#endif
#if CONFIG_TUNER
#define START_TUNER_COUNT 1
                   "radio,"
#else
#define START_TUNER_COUNT 0
#endif
                   "bookmarks,"
                   "plugin"
                   , start_in_callback,
    (7 + START_DB_COUNT + START_REC_COUNT + START_TUNER_COUNT),
                   ID2P(LANG_PREVIOUS_SCREEN), ID2P(LANG_MAIN_MENU),
                   ID2P(LANG_DIR_BROWSER),
#ifdef HAVE_TAGCACHE
                   ID2P(LANG_TAGCACHE),
#endif
                   ID2P(LANG_RESUME_PLAYBACK), ID2P(LANG_SETTINGS),
#ifdef HAVE_RECORDING
                   ID2P(LANG_RECORDING),
#endif
#if CONFIG_TUNER
                   ID2P(LANG_FM_RADIO),
#endif
                   ID2P(LANG_BOOKMARK_MENU_RECENT_BOOKMARKS),
                   ID2P(LANG_OPEN_PLUGIN)
                  ),
#if defined(HAVE_RTC_ALARM) && \
    (defined(HAVE_RECORDING) || CONFIG_TUNER)
    {F_T_INT|F_HAS_CFGVALS, &global_settings.alarm_wake_up_screen, LANG_ALARM_WAKEUP_SCREEN,
        INT(ALARM_START_WPS), "alarm wakeup screen", {.cfg_vals=ALARM_SETTING_TEXT}},
#endif /* HAVE_RTC_ALARM */

    /* Customizable icons */
    TEXT_SETTING(F_THEMESETTING|F_NEEDAPPLY, icon_file, "iconset", DEFAULT_ICONSET,
                     ICON_DIR "/", ".bmp"),
    TEXT_SETTING(F_THEMESETTING|F_NEEDAPPLY, viewers_icon_file, "viewers iconset",
                     DEFAULT_VIEWERS_ICONSET,
                     ICON_DIR "/", ".bmp"),
#ifdef HAVE_REMOTE_LCD
    TEXT_SETTING(F_THEMESETTING|F_NEEDAPPLY, remote_icon_file, "remote iconset", "-",
                     ICON_DIR "/", ".bmp"),
    TEXT_SETTING(F_THEMESETTING|F_NEEDAPPLY, remote_viewers_icon_file,
                     "remote viewers iconset", "-",
                     ICON_DIR "/", ".bmp"),
#endif /* HAVE_REMOTE_LCD */
#ifdef HAVE_LCD_COLOR
    TEXT_SETTING(F_THEMESETTING|F_NEEDAPPLY, colors_file, "filetype colours", "-",
                     THEME_DIR "/", ".colours"),
#endif
#ifdef HAVE_BUTTON_LIGHT
    TABLE_SETTING_LIST(F_TIME_SETTING | F_ALLOW_ARBITRARY_VALS,
                    buttonlight_timeout, LANG_BUTTONLIGHT_TIMEOUT,
                    DEFAULT_BACKLIGHT_TIMEOUT, "button light timeout",
                    off_on, UNIT_SEC, formatter_time_unit_0_is_always,
                    getlang_time_unit_0_is_always, buttonlight_set_timeout,
                    23, timeout_sec_common),
#endif
#ifdef HAVE_BUTTONLIGHT_BRIGHTNESS
    INT_SETTING(F_NO_WRAP, buttonlight_brightness, LANG_BUTTONLIGHT_BRIGHTNESS,
                DEFAULT_BRIGHTNESS_SETTING,
                "button light brightness",UNIT_INT, MIN_BRIGHTNESS_SETTING,
                MAX_BRIGHTNESS_SETTING, 1, NULL, NULL,
                buttonlight_set_brightness),
#endif
#ifndef HAVE_WHEEL_ACCELERATION
    INT_SETTING(F_TIME_SETTING, list_accel_start_delay, LANG_LISTACCEL_START_DELAY,
                2, "list_accel_start_delay", UNIT_SEC, 0, 10, 1,
                formatter_time_unit_0_is_off, getlang_time_unit_0_is_off, NULL),
    INT_SETTING(0, list_accel_wait, LANG_LISTACCEL_ACCEL_SPEED,
                3, "list_accel_wait", UNIT_SEC, 1, 10, 1,
                scanaccel_formatter, NULL, NULL),
#endif /* HAVE_WHEEL_ACCELERATION */
    /* keyclick */
#ifdef HAVE_HARDWARE_CLICK
    CHOICE_SETTING(0, keyclick, LANG_KEYCLICK_SOFTWARE, 0,
                   "keyclick", "off,weak,moderate,strong", NULL, 4,
                   ID2P(LANG_OFF), ID2P(LANG_WEAK), ID2P(LANG_MODERATE),
                   ID2P(LANG_STRONG)),
    OFFON_SETTING(0, keyclick_repeats, LANG_KEYCLICK_REPEATS, false,
                  "keyclick repeats", NULL),
    OFFON_SETTING(0, keyclick_hardware, LANG_KEYCLICK_HARDWARE, false,
        "hardware keyclick", NULL),
#else
    CHOICE_SETTING(0, keyclick, LANG_KEYCLICK, 0,
                   "keyclick", "off,weak,moderate,strong", NULL, 4,
                   ID2P(LANG_OFF), ID2P(LANG_WEAK), ID2P(LANG_MODERATE),
                   ID2P(LANG_STRONG)),
    OFFON_SETTING(0, keyclick_repeats, LANG_KEYCLICK_REPEATS, false,
                  "keyclick repeats", NULL),
#endif
    TEXT_SETTING(0, playlist_catalog_dir, "playlist catalog directory",
                     PLAYLIST_CATALOG_DEFAULT_DIR, NULL, NULL),
    INT_SETTING(F_TIME_SETTING, sleeptimer_duration, LANG_SLEEP_TIMER_DURATION,
                30, "sleeptimer duration", UNIT_MIN, 5, 300, 5,
                NULL, NULL, NULL),
    OFFON_SETTING(0, sleeptimer_on_startup, LANG_SLEEP_TIMER_ON_POWER_UP, false,
                  "sleeptimer on startup", NULL),
    OFFON_SETTING(0, keypress_restarts_sleeptimer, LANG_KEYPRESS_RESTARTS_SLEEP_TIMER, false,
                  "keypress restarts sleeptimer", set_keypress_restarts_sleep_timer),

    OFFON_SETTING(0, show_shutdown_message, LANG_SHOW_SHUTDOWN_MESSAGE, true,
                  "show shutdown message", NULL),

#ifdef HAVE_TOUCHPAD_SENSITIVITY_SETTING
/* If specific values are set for touchpad sensitivity setting we use those */
#if (defined(MAX_TOUCHPAD_SENSITIVITY_SETTING) \
  && defined(MIN_TOUCHPAD_SENSITIVITY_SETTING) \
  && defined(DEFAULT_TOUCHPAD_SENSITIVITY_SETTING))
    INT_SETTING(F_NO_WRAP, touchpad_sensitivity, LANG_TOUCHPAD_SENSITIVITY,
                DEFAULT_TOUCHPAD_SENSITIVITY_SETTING, "touchpad sensitivity",UNIT_INT,
                MIN_TOUCHPAD_SENSITIVITY_SETTING, MAX_TOUCHPAD_SENSITIVITY_SETTING, 1,
                NULL, NULL, touchpad_set_sensitivity),
#else /* we failback to boolean normal/high values */
    CHOICE_SETTING(0, touchpad_sensitivity, LANG_TOUCHPAD_SENSITIVITY, 0,
                   "touchpad sensitivity", "normal,high", touchpad_set_sensitivity, 2,
                   ID2P(LANG_NORMAL), ID2P(LANG_HIGH)),
#endif /* boolean or analogig values */
#endif /* HAVE_TOUCHPAD_SENSITIVITY_SETTING */

#ifdef HAVE_TOUCHPAD_DEADZONE
    INT_SETTING(F_NO_WRAP, touchpad_deadzone, LANG_DEADZONE,
                DEFAULT_TOUCHPAD_DEADZONE_SETTING, "touchpad deadzone", UNIT_INT,
                MIN_TOUCHPAD_DEADSPACE_SETTING, MAX_TOUCHPAD_DEADSPACE_SETTING, 1,
                NULL, NULL, touchpad_set_deadzone),
#endif

#ifdef HAVE_QUICKSCREEN
   CUSTOM_SETTING(0, qs_items[QUICKSCREEN_TOP], LANG_TOP_QS_ITEM,
                  NULL, "qs top",
                  qs_load_from_cfg, qs_write_to_cfg,
                  qs_is_changed, qs_set_default),
   CUSTOM_SETTING(0, qs_items[QUICKSCREEN_LEFT], LANG_LEFT_QS_ITEM,
                  &global_settings.playlist_shuffle, "qs left",
                  qs_load_from_cfg, qs_write_to_cfg,
                  qs_is_changed, qs_set_default),
   CUSTOM_SETTING(0, qs_items[QUICKSCREEN_RIGHT], LANG_RIGHT_QS_ITEM,
                  &global_settings.repeat_mode, "qs right",
                  qs_load_from_cfg, qs_write_to_cfg,
                  qs_is_changed, qs_set_default),
   CUSTOM_SETTING(0, qs_items[QUICKSCREEN_BOTTOM], LANG_BOTTOM_QS_ITEM,
                  NULL, "qs bottom",
                  qs_load_from_cfg, qs_write_to_cfg,
                  qs_is_changed, qs_set_default),
   OFFON_SETTING(0, shortcuts_replaces_qs, LANG_USE_SHORTCUTS_INSTEAD_OF_QS,
                  false, "shortcuts instead of quickscreen", NULL),
#endif
#ifdef HAVE_SPEAKER
    CHOICE_SETTING(0, speaker_mode, LANG_ENABLE_SPEAKER, 0, "speaker mode",
# ifdef HAVE_HEADPHONE_DETECTION
                   "on,off,auto", audio_enable_speaker, 3, ID2P(LANG_OFF), ID2P(LANG_ON), ID2P(LANG_AUTO)),
#else /* HAVE_HEADPHONE_DETECTION */
                   "on,off", audio_enable_speaker, 2, ID2P(LANG_OFF), ID2P(LANG_ON)),
#endif /* HAVE_HEADPHONE_DETECTION */
#endif /* HAVE_SPEAKER */
#ifdef HAVE_TOUCHSCREEN
    CHOICE_SETTING(0, touch_mode, LANG_TOUCHSCREEN_MODE, DEFAULT_TOUCHSCREEN_MODE,
                   "touchscreen mode", "point,grid", NULL, 2,
                   ID2P(LANG_TOUCHSCREEN_POINT), ID2P(LANG_TOUCHSCREEN_GRID)),
    CUSTOM_SETTING(0, ts_calibration_data, -1,
                    &default_calibration_parameters, "touchscreen calibration",
                    tsc_load_from_cfg, tsc_write_to_cfg,
                    tsc_is_changed, tsc_set_default),
#endif
    OFFON_SETTING(0, prevent_skip, LANG_PREVENT_SKIPPING, false, "prevent track skip", NULL),
    OFFON_SETTING(0, rewind_across_tracks, LANG_REWIND_ACROSS_TRACKS, false, "rewind across tracks", NULL),
#ifdef HAVE_PITCHCONTROL
    OFFON_SETTING(0, pitch_mode_semitone, LANG_SEMITONE, false,
                  "Semitone pitch change", NULL),
    OFFON_SETTING(0, pitch_mode_timestretch, LANG_TIMESTRETCH, false,
                  "Timestretch mode", NULL),
#endif

#ifdef USB_ENABLE_HID
    OFFON_SETTING(0, usb_hid, LANG_USB_HID, true, "usb hid", usb_set_hid),
    CHOICE_SETTING(0, usb_keypad_mode, LANG_USB_KEYPAD_MODE, 0,
            "usb keypad mode", "multimedia,presentation,browser"
#ifdef HAVE_USB_HID_MOUSE
            ",mouse"
#endif
            , NULL,
#ifdef HAVE_USB_HID_MOUSE
            4,
#else
            3,
#endif
            ID2P(LANG_MULTIMEDIA_MODE), ID2P(LANG_PRESENTATION_MODE),
            ID2P(LANG_BROWSER_MODE)
#ifdef HAVE_USB_HID_MOUSE
            , ID2P(LANG_MOUSE_MODE)
#endif
    ), /* CHOICE_SETTING( usb_keypad_mode ) */
#endif

#if defined(USB_ENABLE_STORAGE) && defined(HAVE_MULTIDRIVE)
    OFFON_SETTING(0, usb_skip_first_drive, LANG_USB_SKIP_FIRST_DRIVE, false, "usb skip first drive", usb_set_skip_first_drive),
#endif

    /* Customizable list */
    VIEWPORT_SETTING(ui_vp_config, "ui viewport"),
#ifdef HAVE_REMOTE_LCD
    VIEWPORT_SETTING(remote_ui_vp_config, "remote ui viewport"),
#endif

#ifdef HAVE_MORSE_INPUT
    OFFON_SETTING(0, morse_input, LANG_MORSE_INPUT, false, "morse input", NULL),
#endif

#ifdef HAVE_HOTKEY
    TABLE_SETTING(F_CB_ON_SELECT_ONLY, hotkey_wps,
        LANG_HOTKEY_WPS, HOTKEY_VIEW_PLAYLIST, "hotkey wps",
        "off,view playlist,show track info,pitchscreen,open with,delete,bookmark,plugin,bookmark list"
        ,UNIT_INT, hotkey_formatter, hotkey_getlang, hotkey_callback,9, HOTKEY_OFF,
        HOTKEY_VIEW_PLAYLIST, HOTKEY_SHOW_TRACK_INFO, HOTKEY_PITCHSCREEN,
        HOTKEY_OPEN_WITH, HOTKEY_DELETE, HOTKEY_BOOKMARK, HOTKEY_PLUGIN, HOTKEY_BOOKMARK_LIST),
    TABLE_SETTING(0, hotkey_tree,
        LANG_HOTKEY_FILE_BROWSER, HOTKEY_OFF, "hotkey tree",
#ifdef HAVE_TAGCACHE
        "off,properties,pictureflow,open with,delete,insert,insert shuffled",
#else
        "off,properties,open with,delete,insert,insert shuffled",
#endif
        UNIT_INT, hotkey_formatter, hotkey_getlang, NULL,
#ifdef HAVE_TAGCACHE
        7,
#else
        6,
#endif
        HOTKEY_OFF,HOTKEY_PROPERTIES,
#ifdef HAVE_TAGCACHE
        HOTKEY_PICTUREFLOW,
#endif
        HOTKEY_OPEN_WITH, HOTKEY_DELETE, HOTKEY_INSERT, HOTKEY_INSERT_SHUFFLED),
#endif /* HAVE_HOTKEY */

    INT_SETTING(F_TIME_SETTING, resume_rewind, LANG_RESUME_REWIND, 0,
                "resume rewind", UNIT_SEC, 0, 60, 5,
                formatter_time_unit_0_is_off, getlang_time_unit_0_is_off, NULL),
   CUSTOM_SETTING(0, root_menu_customized,
                  LANG_ROCKBOX_TITLE, /* lang string here is never actually used */
                  NULL, "root menu order",
                  root_menu_load_from_cfg, root_menu_write_to_cfg,
                  root_menu_is_changed, root_menu_set_default),

#if defined(DX50) || defined(DX90)
    CHOICE_SETTING(0,
                   governor,
                   LANG_IBASSO_FREQ_SCALING_GOVERNOR,
                   GOVERNOR_POWERSAVE,
                   "freq scaling governor",
                   "conservative,ondemand,interactive,powersave,performance",
                   ibasso_set_governor,
                   5,
                   "Conservative",
                   "Ondemand",
                   "Interactive",
                   "Powersave",
                   "Performance"),
#endif
#if defined(DX50) || defined(DX90) || (defined(HAVE_USB_POWER) && !defined(USB_NONE) && !defined(SIMULATOR))
    CHOICE_SETTING(0,
                   usb_mode,
                   LANG_USB_MODE,
                   USBMODE_DEFAULT,
                   "usb mode",
                   "mass storage,charge"
#if defined(DX50) || defined(DX90)
                   ",adb"
#endif
                   ,
#if defined(DX50) || defined(DX90)
                   ibasso_set_usb_mode,
                   3,
#else
                   usb_set_mode,
                   2,
#endif
                   ID2P(LANG_USB_MODE_MASS_STORAGE),
                   ID2P(LANG_USB_MODE_CHARGE)
#if defined(DX50) || defined(DX90)
                   ,ID2P(LANG_USB_MODE_ADB)
#endif
	    ),
#endif
#if defined(BUTTON_REC) || \
    (CONFIG_KEYPAD == GIGABEAT_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD) || \
    (CONFIG_KEYPAD == IRIVER_H10_PAD)
    OFFON_SETTING(0, clear_settings_on_hold, LANG_CLEAR_SETTINGS_ON_HOLD,
                  true, "clear settings on hold", NULL),
#endif
#if defined(HAVE_EROS_QN_CODEC)
    CHOICE_SETTING(0, hp_lo_select, LANG_HP_LO_SELECT, 0, "headphone lineout select",
    "auto,headphone,lineout", hp_lo_select_apply, 3,
    ID2P(LANG_AUTO), ID2P(LANG_HEADPHONE), ID2P(LANG_LINEOUT)),
#endif
    OFFON_SETTING(0, playback_log, LANG_LOGGING, false, "play log", NULL),
};

const int nb_settings = sizeof(settings)/sizeof(*settings);

const struct settings_list* get_settings_list(int*count)
{
    *count = nb_settings;
    return settings;
}
