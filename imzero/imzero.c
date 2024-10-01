#include "imzero.h"
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <assert.h>

#include "libavutil/avstring.h"
#include "libavutil/channel_layout.h"
#include "libavutil/mathematics.h"
#include "libavutil/mem.h"
#include "libavutil/pixdesc.h"
#include "libavutil/dict.h"
#include "libavutil/fifo.h"
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libavutil/bprint.h"

#include <stdbool.h>

static FILE *fd;

#if 0
static void marshall_flush(void) {
    fputc('\n',fd);
    fflush(fd);
}
// Credits: adapted from ffprobe.c Copyright (c) 2007-2010 Stefano Sabatini
// FIXME follow flatbuffers spec (e.g. use \x escaping)
static void marshall_escape_str(FILE *fd, const char *str, size_t len) {
    static const char marshall_escape[] = {'"', '\\', '\b', '\f', '\n', '\r', '\t', 0};
    static const char marshall_subst[]  = {'"', '\\',  'b',  'f',  'n',  'r',  't', 0};

    for(size_t i=0;i<len;i++) {
        const char p = str[i];
        char *s = strchr(marshall_escape, p);
        if(s){
            fputc('\\',fd);
            fputc(marshall_subst[s - json_escape], fd);
        } else if ((unsigned char)p < 32) {
            fprintf(fd,"\\u00%02x", ((unsigned char)p) & 0xff);
        } else {
            fputc(str[i],fd);
        }
    }
}
#define FLOAT_DIRECTIVE "%.9g"
static void marshall_single_vec2(float x,float y) {
    fprintf(fd,"{\"x\":"FLOAT_DIRECTIVE",\"y\":"FLOAT_DIRECTIVE"}",x,y);
}
static void marshall_event_mouse_motion(float pos_x,float pos_y,uint32_t mouse_id,bool is_touch) {
    fprintf(fd,"{\"event_type\":\"EventMouseMotion\",\"event\":{\"pos\":");
    marshall_single_vec2(pos_x,pos_y);
    fprintf(fd,",\"mouse_id\":%"PRIx32",\"is_touch\":%s}}",mouse_id,is_touch ? "true" : "false");
}
static void marshall_event_mouse_button(float pos_x,float pos_y,uint32_t mouse_id,bool is_touch,uint8_t buttons,bool down) {
    fprintf(fd,"{\"event_type\":\"EventMouseButton\",\"event\":{\"pos\":");
    marshall_single_vec2(pos_x,pos_y);
    fprintf(fd,",\"mouse_id\":%"PRIx32",\"is_touch\":%s,\"buttons\":%"PRId8",\"type\":%d}}",mouse_id,is_touch ? "true" : "false",buttons,down ? 1 : 0);
}
static void marshall_event_mouse_wheel(float pos_x,float pos_y,uint32_t mouse_id,bool is_touch,uint8_t buttons) {
    fprintf(fd,"{\"event_type\":\"EventMouseWheel\",\"event\":{\"pos\":");
    marshall_single_vec2(pos_x,pos_y);
    fprintf(fd,",\"mouse_id\":%"PRIx32",\"is_touch\":%s,\"buttons\":0x%"PRIx8"}}",mouse_id,is_touch ? "true" : "false",buttons);
}
static void marshall_event_client_connect(const char *desc,size_t desc_len) {
    static const char *p = "{\"event_type\":\"EventClientConnect\",\"event\":{\"desc\":\"";
    fwrite(p,1,strlen(p),fd);
    marshall_escape_str(fd,desc,desc_len);
    fwrite("\"}}",1,3,fd);
}
static void marshall_event_client_disconnect(const char *desc, size_t desc_len) {
    static const char *p = "{\"event_type\":\"EventClientDisconnect\",\"event\":{\"desc\":\"";
    fwrite(p,1,strlen(p),fd);
    marshall_escape_str(fd,desc,desc_len);
    fwrite("\"}}",1,3,fd);
}
static void marshall_event_input_text(const char *text, size_t text_len) {
    static const char *p = "{\"event_type\":\"EventInputText\",\"event\":{\"text\":\"";
    fwrite(p,1,strlen(p),fd);
    marshall_escape_str(fd,text,text_len);
    fwrite("\"}}",1,3,fd);
}
static void marshall_event_keyboard(uint16_t modifiers,const char *codename,bool is_down,uint32_t native_sym,uint32_t scancode) {
    fprintf(fd,"{\"event_type\":\"EventKeyboard\",\"event\":{\"modifiers\":0x%"PRIx16",\"code\":%s,\"is_down\":%s,\"native_sym\":0x%"PRIx32",\"scancode\":0x%"PRIx32"}}",
               modifiers,codename,is_down ? "true" : "false",native_sym,scancode);
}
static void marshall_event_keep_alive(void) {
    fprintf(fd,"{\"event_type\":\"EventKeepAlive\"}}");
}
#else
static inline void send_uint8(uint8_t v) {
    fwrite(&v,sizeof(v),1,fd);
}
static inline void send_bool(bool v) {
    send_uint8((uint8_t)v);
}
static inline void send_uint16(uint16_t v) {
    fwrite(&v,sizeof(v),1,fd);
}
static inline void send_uint32(uint32_t v) {
    fwrite(&v,sizeof(v),1,fd);
}
static inline void send_uint64(uint64_t v) {
    fwrite(&v,sizeof(v),1,fd);
}
static inline void send_float(float v) {
    fwrite(&v,sizeof(v),1,fd);
}
static inline void send_string(const char *buf,size_t len) {
    send_uint32((uint32_t)len);
    if(len > 0) {
        fwrite(buf,1,len,fd);
    }
    send_uint8(0); // zero-terminate strings
}
const uint8_t eventType_MouseMotion = 0;
const uint8_t eventType_MouseButton = 1;
const uint8_t eventType_MouseWheel = 2;
const uint8_t eventType_ClientConnect = 3;
const uint8_t eventType_ClientDisconnect = 4;
const uint8_t eventType_InputText = 5;
const uint8_t eventType_InputKeyboard = 6;
const uint8_t eventType_KeepAlive = 7;

static void marshall_flush(void) {
    fflush(fd);
}
static void marshall_single_vec2(float x,float y) {
    send_float(x);
    send_float(y);
}
static void marshall_event_mouse_motion(float pos_x,float pos_y,uint32_t mouse_id,bool is_touch) {
    send_uint32(sizeof(uint8_t)+2*sizeof(float)+sizeof(uint32_t)+sizeof(uint8_t));
    send_uint8(eventType_MouseMotion);
    marshall_single_vec2(pos_x,pos_y);
    send_uint32(mouse_id);
    send_bool(is_touch);
}
static void marshall_event_mouse_button(float pos_x,float pos_y,uint32_t mouse_id,bool is_touch,uint8_t buttons,bool down) {
    send_uint32(sizeof(uint8_t)+2*sizeof(float)+sizeof(uint32_t)+3*sizeof(uint8_t));
    send_uint8(eventType_MouseButton);
    marshall_single_vec2(pos_x,pos_y);
    send_uint32(mouse_id);
    send_bool(is_touch);
    send_uint8(buttons);
    send_bool(down);
}
static void marshall_event_mouse_wheel(float pos_x,float pos_y,uint32_t mouse_id,bool is_touch,uint8_t buttons) {
    send_uint32(sizeof(uint8_t)+2*sizeof(float)+sizeof(uint32_t)+2*sizeof(uint8_t));
    send_uint8(eventType_MouseWheel);
    marshall_single_vec2(pos_x,pos_y);
    send_uint32(mouse_id);
    send_bool(is_touch);
    send_uint8(buttons);
}
static uint32_t marshalled_length_string(size_t len) {
    return sizeof(uint32_t)+len+1;
}
static void marshall_event_client_connect(const char *desc,size_t desc_len) {
    send_uint32(sizeof(uint8_t)+marshalled_length_string(desc_len));
    send_uint8(eventType_ClientConnect);
    send_string(desc,desc_len);
}
static void marshall_event_client_disconnect(const char *desc, size_t desc_len) {
    send_uint32(sizeof(uint8_t)+marshalled_length_string(desc_len));
    send_uint8(eventType_ClientDisconnect);
    send_string(desc,desc_len);
}
static void marshall_event_input_text(const char *text, size_t text_len) {
    send_uint32(sizeof(uint8_t)+marshalled_length_string(text_len));
    send_uint8(eventType_InputText);
    send_string(text,text_len);
}
static void marshall_event_keyboard(uint16_t modifiers,const char *codename,bool is_down,uint32_t native_sym,uint32_t scancode) {
    send_uint32(sizeof(uint8_t)+sizeof(uint16_t)+marshalled_length_string(0)+sizeof(uint8_t)+2*sizeof(uint32_t));
    send_uint8(eventType_InputKeyboard);
    send_uint16(modifiers);
    send_string(NULL,0); // do not send codename as string
    send_bool(is_down);
    send_uint32(native_sym);
    send_uint32(scancode);
}
static void marshall_event_keep_alive(void) {
    send_uint32(sizeof(uint8_t));
    send_uint8(eventType_KeepAlive);
}
#endif
static uint8_t translate_mouse_buttons(uint8_t button) {
    #if 1
        static_assert(SDL_BUTTON_LEFT == 1);
        static_assert(SDL_BUTTON_MIDDLE == 2);
        static_assert(SDL_BUTTON_RIGHT == 3);
        static_assert(SDL_BUTTON_X1 == 4);
        static_assert(SDL_BUTTON_X2 == 5);
        return 1<<button;
    #else
        switch(button) {
        case SDL_BUTTON_LEFT:
            return 0b1;
        case SDL_BUTTON_MIDDLE:
            return 0b10;
        case SDL_BUTTON_RIGHT:
            return 0b100;
        case SDL_BUTTON_X1:
            return 0b1000;
        case SDL_BUTTON_X2:
            return 0b10000;
        }
    #endif
}
static void emit_sdl_mouse_motion(SDL_MouseMotionEvent *ev) {
    marshall_event_mouse_motion(ev->x,ev->y,ev->which,ev->which == SDL_TOUCH_MOUSEID);
    marshall_flush();
}
static void emit_sdl_mouse_wheel(SDL_MouseWheelEvent *ev) {
    marshall_event_mouse_wheel(ev->x,ev->y,ev->which,ev->which == SDL_TOUCH_MOUSEID,0);
    marshall_flush();
}
static void emit_sdl_mouse_button(SDL_MouseButtonEvent *ev,bool down) {
    marshall_event_mouse_button(ev->x,ev->y,ev->which,ev->which == SDL_TOUCH_MOUSEID,translate_mouse_buttons(ev->button),down);
    marshall_flush();
}
static void emit_sdl_input_text(SDL_TextInputEvent *ev) {
    marshall_event_input_text(ev->text,strlen(ev->text));
    marshall_flush();    
}

//  Key_Apostrophe,
//  Key_Equal,
//  Key_LeftBracket,
//  Key_Backslash,
//  Key_RightBracket,
//  Key_GraveAccent,
//  Key_ScrollLock,
//  Key_NumLock,
//  Key_PrintScreen,
static const char *translate_sdl_keysym_to_imzero(SDL_KeyCode key_code) {
    switch(key_code) {
    case SDLK_RETURN : return "Key_Enter";
    case SDLK_ESCAPE : return "Key_Escape";
    case SDLK_BACKSPACE : return "Key_Backspace";
    case SDLK_TAB : return "Key_Tab";
    case SDLK_SPACE : return "Key_Space";
    case SDLK_EXCLAIM : return "";
    case SDLK_QUOTEDBL : return "";
    case SDLK_HASH : return "";
    case SDLK_DOLLAR : return "";
    case SDLK_AMPERSAND : return "";
    case SDLK_QUOTE : return "";
    case SDLK_LEFTPAREN : return "";
    case SDLK_RIGHTPAREN : return "";
    case SDLK_ASTERISK : return "";
    case SDLK_PLUS : return "";
    case SDLK_COMMA : return "Key_Comma";
    case SDLK_MINUS : return "Key_Minus";
    case SDLK_PERIOD : return "Key_Period";
    case SDLK_SLASH : return "Key_Slash";
    case SDLK_0 : return "Key_0";
    case SDLK_1 : return "Key_1";
    case SDLK_2 : return "Key_2";
    case SDLK_3 : return "Key_3";
    case SDLK_4 : return "Key_4";
    case SDLK_5 : return "Key_5";
    case SDLK_6 : return "Key_6";
    case SDLK_7 : return "Key_7";
    case SDLK_8 : return "Key_8";
    case SDLK_9 : return "Key_9";
    case SDLK_SEMICOLON : return "Key_Semicolon";
    case SDLK_LESS : return "";
    case SDLK_EQUALS : return "";
    case SDLK_GREATER : return "";
    case SDLK_QUESTION : return "";
    case SDLK_AT : return "";
    case SDLK_LEFTBRACKET : return "";
    case SDLK_BACKSLASH : return "";
    case SDLK_RIGHTBRACKET : return "";
    case SDLK_CARET : return "";
    case SDLK_UNDERSCORE : return "";
    case SDLK_BACKQUOTE : return "";
    case SDLK_a : return "Key_A";
    case SDLK_b : return "Key_B";
    case SDLK_c : return "Key_C";
    case SDLK_d : return "Key_D";
    case SDLK_e : return "Key_E";
    case SDLK_f : return "Key_F";
    case SDLK_g : return "Key_G";
    case SDLK_h : return "Key_H";
    case SDLK_i : return "Key_I";
    case SDLK_j : return "Key_J";
    case SDLK_k : return "Key_K";
    case SDLK_l : return "Key_L";
    case SDLK_m : return "Key_M";
    case SDLK_n : return "Key_N";
    case SDLK_o : return "Key_O";
    case SDLK_p : return "Key_P";
    case SDLK_q : return "Key_Q";
    case SDLK_r : return "Key_R";
    case SDLK_s : return "Key_S";
    case SDLK_t : return "Key_T";
    case SDLK_u : return "Key_U";
    case SDLK_v : return "Key_V";
    case SDLK_w : return "Key_W";
    case SDLK_x : return "Key_X";
    case SDLK_y : return "Key_Y";
    case SDLK_z : return "Key_Z";
    case SDLK_CAPSLOCK : return "Key_CapsLock";
    case SDLK_F1 : return "Key_F1";
    case SDLK_F2 : return "Key_F2";
    case SDLK_F3 : return "Key_F3";
    case SDLK_F4 : return "Key_F4";
    case SDLK_F5 : return "Key_F5";
    case SDLK_F6 : return "Key_F6";
    case SDLK_F7 : return "Key_F7";
    case SDLK_F8 : return "Key_F8";
    case SDLK_F9 : return "Key_F9";
    case SDLK_F10 : return "Key_F10";
    case SDLK_F11 : return "Key_F11";
    case SDLK_F12 : return "Key_F12";
    case SDLK_PRINTSCREEN : return "";
    case SDLK_SCROLLLOCK : return "";
    case SDLK_PAUSE : return "Key_Pause";
    case SDLK_INSERT : return "Key_Insert";
    case SDLK_HOME : return "Key_Home";
    case SDLK_PAGEUP : return "Key_PageUp";
    case SDLK_DELETE : return "Key_Delete";
    case SDLK_END : return "Key_End";
    case SDLK_PAGEDOWN : return "Key_PageDown";
    case SDLK_RIGHT : return "Key_RightArrow";
    case SDLK_LEFT : return "Key_LeftArrow";
    case SDLK_DOWN : return "Key_DownArrow";
    case SDLK_UP : return "Key_UpArrow";
    case SDLK_NUMLOCKCLEAR : return "";
    case SDLK_KP_DIVIDE : return "Key_KeypadDivide";
    case SDLK_KP_MULTIPLY : return "Key_KeypadMultiply";
    case SDLK_KP_MINUS : return "Key_KeypadSubtract";
    case SDLK_KP_PLUS : return "Key_KeypadAdd";
    case SDLK_KP_ENTER : return "Key_KeypadEnter";
    case SDLK_KP_1 : return "Key_Keypad1";
    case SDLK_KP_2 : return "Key_Keypad2";
    case SDLK_KP_3 : return "Key_Keypad3";
    case SDLK_KP_4 : return "Key_Keypad4";
    case SDLK_KP_5 : return "Key_Keypad5";
    case SDLK_KP_6 : return "Key_Keypad6";
    case SDLK_KP_7 : return "Key_Keypad7";
    case SDLK_KP_8 : return "Key_Keypad8";
    case SDLK_KP_9 : return "Key_Keypad9";
    case SDLK_KP_0 : return "Key_Keypad0";
    case SDLK_KP_PERIOD : return "Key_KeypadDecimal";
    case SDLK_APPLICATION : return "";
    case SDLK_POWER : return "";
    case SDLK_KP_EQUALS : return "Key_KeypadEqual";
    case SDLK_F13 : return "Key_F13";
    case SDLK_F14 : return "Key_F14";
    case SDLK_F15 : return "Key_F15";
    case SDLK_F16 : return "Key_F16";
    case SDLK_F17 : return "Key_F17";
    case SDLK_F18 : return "Key_F18";
    case SDLK_F19 : return "Key_F19";
    case SDLK_F20 : return "Key_F20";
    case SDLK_F21 : return "Key_F21";
    case SDLK_F22 : return "Key_F22";
    case SDLK_F23 : return "Key_F23";
    case SDLK_F24 : return "Key_F24";
    case SDLK_EXECUTE : return "";
    case SDLK_HELP : return "";
    case SDLK_MENU : return "Key_Menu";
    case SDLK_SELECT : return "";
    case SDLK_STOP : return "";
    case SDLK_AGAIN : return "";
    case SDLK_UNDO : return "";
    case SDLK_CUT : return "";
    case SDLK_COPY : return "";
    case SDLK_PASTE : return "";
    case SDLK_FIND : return "";
    case SDLK_MUTE : return "";
    case SDLK_VOLUMEUP : return "";
    case SDLK_VOLUMEDOWN : return "";
    case SDLK_KP_COMMA : return "";
    case SDLK_KP_EQUALSAS400 : return "";
    case SDLK_ALTERASE : return "";
    case SDLK_SYSREQ : return "";
    case SDLK_CANCEL : return "";
    case SDLK_CLEAR : return "";
    case SDLK_PRIOR : return "";
    case SDLK_RETURN2 : return "";
    case SDLK_SEPARATOR : return "";
    case SDLK_OUT : return "";
    case SDLK_OPER : return "";
    case SDLK_CLEARAGAIN : return "";
    case SDLK_CRSEL : return "";
    case SDLK_EXSEL : return "";
    case SDLK_KP_00 : return "";
    case SDLK_KP_000 : return "";
    case SDLK_THOUSANDSSEPARATOR : return "";
    case SDLK_DECIMALSEPARATOR : return "";
    case SDLK_CURRENCYUNIT : return "";
    case SDLK_CURRENCYSUBUNIT : return "";
    case SDLK_KP_LEFTPAREN : return "";
    case SDLK_KP_RIGHTPAREN : return "";
    case SDLK_KP_LEFTBRACE : return "";
    case SDLK_KP_RIGHTBRACE : return "";
    case SDLK_KP_TAB : return "";
    case SDLK_KP_BACKSPACE : return "";
    case SDLK_KP_A : return "";
    case SDLK_KP_B : return "";
    case SDLK_KP_C : return "";
    case SDLK_KP_D : return "";
    case SDLK_KP_E : return "";
    case SDLK_KP_F : return "";
    case SDLK_KP_XOR : return "";
    case SDLK_KP_POWER : return "";
    case SDLK_KP_PERCENT : return "";
    case SDLK_KP_LESS : return "";
    case SDLK_KP_GREATER : return "";
    case SDLK_KP_AMPERSAND : return "";
    case SDLK_KP_DBLAMPERSAND : return "";
    case SDLK_KP_VERTICALBAR : return "";
    case SDLK_KP_DBLVERTICALBAR : return "";
    case SDLK_KP_COLON : return "";
    case SDLK_KP_HASH : return "";
    case SDLK_KP_SPACE : return "";
    case SDLK_KP_AT : return "";
    case SDLK_KP_EXCLAM : return "";
    case SDLK_KP_MEMSTORE : return "";
    case SDLK_KP_MEMRECALL : return "";
    case SDLK_KP_MEMCLEAR : return "";
    case SDLK_KP_MEMADD : return "";
    case SDLK_KP_MEMSUBTRACT : return "";
    case SDLK_KP_MEMMULTIPLY : return "";
    case SDLK_KP_MEMDIVIDE : return "";
    case SDLK_KP_PLUSMINUS : return "";
    case SDLK_KP_CLEAR : return "";
    case SDLK_KP_CLEARENTRY : return "";
    case SDLK_KP_BINARY : return "";
    case SDLK_KP_OCTAL : return "";
    case SDLK_KP_DECIMAL : return "";
    case SDLK_KP_HEXADECIMAL : return "";
    case SDLK_LCTRL : return "Key_LeftCtrl";
    case SDLK_LSHIFT : return "Key_LeftShift";
    case SDLK_LALT : return "Key_LeftAlt";
    case SDLK_LGUI : return "Key_LeftSuper";
    case SDLK_RCTRL : return "Key_RightCtrl";
    case SDLK_RSHIFT : return "Key_RightShift";
    case SDLK_RALT : return "Key_RightAlt";
    case SDLK_RGUI : return "Key_RightSuper";
    case SDLK_MODE : return "";
    case SDLK_AUDIONEXT : return "";
    case SDLK_AUDIOPREV : return "";
    case SDLK_AUDIOSTOP : return "";
    case SDLK_AUDIOPLAY : return "";
    case SDLK_AUDIOMUTE : return "";
    case SDLK_MEDIASELECT : return "";
    case SDLK_WWW : return "";
    case SDLK_MAIL : return "";
    case SDLK_CALCULATOR : return "";
    case SDLK_COMPUTER : return "";
    case SDLK_AC_SEARCH : return "";
    case SDLK_AC_HOME : return "";
    case SDLK_AC_BACK : return "Key_AppBack";
    case SDLK_AC_FORWARD : return "Key_AppForward";
    case SDLK_AC_STOP : return "";
    case SDLK_AC_REFRESH : return "";
    case SDLK_AC_BOOKMARKS : return "";
    case SDLK_BRIGHTNESSDOWN : return "";
    case SDLK_BRIGHTNESSUP : return "";
    case SDLK_DISPLAYSWITCH : return "";
    case SDLK_KBDILLUMTOGGLE : return "";
    case SDLK_KBDILLUMDOWN : return "";
    case SDLK_KBDILLUMUP : return "";
    case SDLK_EJECT : return "";
    case SDLK_SLEEP : return "";
    case SDLK_APP1 : return "";
    case SDLK_APP2 : return "";
    case SDLK_AUDIOREWIND : return "";
    case SDLK_AUDIOFASTFORWARD : return "";
    case SDLK_SOFTLEFT : return "";
    case SDLK_SOFTRIGHT : return "";
    case SDLK_CALL : return "";
    case SDLK_ENDCALL : return "";
    }
    return "";
}
static uint16_t translate_sdl_keymod_to_imzero(SDL_Keymod key_mod) {
    uint16_t p = 0x00;
    p |= ((key_mod & KMOD_LSHIFT) ? 1 : 0);
    p |= ((key_mod & KMOD_RSHIFT) ? 2 : 0);
    p |= ((key_mod & KMOD_LCTRL) ? 4 : 0);
    p |= ((key_mod & KMOD_RCTRL) ? 8 : 0);
    p |= ((key_mod & KMOD_LALT) ? 16 : 0);
    p |= ((key_mod & KMOD_RCTRL) ? 32 : 0);
    p |= ((key_mod & KMOD_LGUI) ? 64 : 0);
    p |= ((key_mod & KMOD_RGUI) ? 128 : 0);
    p |= ((key_mod & KMOD_NUM) ? 256 : 0);
    p |= ((key_mod & KMOD_CAPS) ? 512 : 0);
    p |= ((key_mod & KMOD_MODE) ? 1024 : 0);
    p |= ((key_mod & KMOD_SCROLL) ? 2048 : 0);
    return p;
}
static void emit_sdl_input_keyboard(SDL_KeyboardEvent *ev) {
    marshall_event_keyboard(
        translate_sdl_keymod_to_imzero(ev->keysym.mod),
        translate_sdl_keysym_to_imzero(ev->keysym.sym),
        ev->type == SDL_KEYDOWN,
        (uint32_t)ev->keysym.sym,
        (uint32_t)ev->keysym.scancode
        );
    marshall_flush();
}

static ImZeroState* imzero_init(const char *user_interaction_path) {
    ImZeroState *state;
    state = av_mallocz(sizeof(ImZeroState));
    if(!state) {
        return NULL;
    }
    if(user_interaction_path != NULL && strlen(user_interaction_path)>0) {
        fd = fopen(user_interaction_path,"w");
        if(fd == NULL) {
            av_log(NULL, AV_LOG_WARNING, "Failed to open UserInteractionPath file \"%s\" for writing!\n",user_interaction_path);
        }
    } else if(user_interaction_path != NULL && strcmp(user_interaction_path,"-") == 0) {
        fd = stderr;
    } else {
        fd = NULL;
    }
    if(fd != NULL) {
        marshall_event_client_connect("ffplay FIXME",12);
        marshall_flush();
    }

    return state;
}
static void imzero_reset(ImZeroState *state) {
}
static void imzero_destroy(ImZeroState *state) {
    if(fd != NULL) {
        marshall_event_client_disconnect("ffplay FIXME",12);
        marshall_flush();
        fclose(fd);
        fd = NULL;
    }
}
void refresh_loop_wait_event_imzero(struct VideoState *is, SDL_Event *event) {
    double remaining_time = 0.0;
    SDL_PumpEvents();
    while (!SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)) {
        if (remaining_time > 0.0)
            av_usleep((int64_t)(remaining_time * 1000000.0));
        remaining_time = REFRESH_RATE;
        if (is->show_mode != SHOW_MODE_NONE && (!is->paused || is->force_refresh))
            video_refresh(is, &remaining_time);
        SDL_PumpEvents();
    }
}

inline void event_loop_handle_event_imzero(struct VideoState *cur_stream,SDL_Event *event, double *incr, double *pos, double *frac) {
    if(cursor_hidden) { // FIXME
        SDL_ShowCursor(SDL_ENABLE);
        cursor_hidden = 0;
    }
    // If we don't yet have a window, skip all key events, because read_thread might still be initializing...
    if (!cur_stream->width) {
        return;
    }

    switch (event->type) {
    case SDL_MOUSEMOTION:
        emit_sdl_mouse_motion(&event->motion);
        break;
    case SDL_KEYUP: // fallthrough
    case SDL_KEYDOWN:
        emit_sdl_input_keyboard(&event->key);
        break;
    case SDL_MOUSEBUTTONUP:
        emit_sdl_mouse_button(&event->button,false);
        break;
    case SDL_MOUSEBUTTONDOWN:
        emit_sdl_mouse_button(&event->button,true);
        break;
    case SDL_MOUSEWHEEL:
        emit_sdl_mouse_wheel(&event->wheel);
        break;
    case SDL_TEXTINPUTEVENT_TEXT_SIZE:
        emit_sdl_input_text(&event->text);
        break;
    case SDL_WINDOWEVENT:
        switch (event->window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                screen_width  = cur_stream->width  = event->window.data1;
                screen_height = cur_stream->height = event->window.data2;
                if (cur_stream->vis_texture) {
                    SDL_DestroyTexture(cur_stream->vis_texture);
                    cur_stream->vis_texture = NULL;
                }
                if (vk_renderer)
                    vk_renderer_resize(vk_renderer, screen_width, screen_height);
            case SDL_WINDOWEVENT_EXPOSED:
                cur_stream->force_refresh = 1;
        }
        break;
    case SDL_QUIT:
    case FF_QUIT_EVENT:
        do_exit(cur_stream);
        break;
    default:
        break;
    }
}