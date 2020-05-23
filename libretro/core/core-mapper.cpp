#include "libretro.h"
#include "libretro-core.h"
#include "retroscreen.h"

//FIXME
unsigned char uae4all_keystate[320];
int lastmx=0, lastmy=0, newmousecounters=0;
int buttonstate[3]={0,0,0};

//CORE VAR
#ifdef _WIN32
char slash = '\\';
#else
char slash = '/';
#endif
extern const char *retro_save_directory;
extern const char *retro_system_directory;
extern const char *retro_content_directory;
char RETRO_DIR[512];

//TIME
#ifdef __CELLOS_LV2__
#include "sys/sys_time.h"
#include "sys/timer.h"
#define usleep  sys_timer_usleep
#else
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#endif

extern void Screen_SetFullUpdate(int scr);
extern void virtual_kdb(char *buffer,int vx,int vy);
extern int check_vkey2(int x,int y);

long frame=0;
unsigned long  Ktime=0 , LastFPSTime=0;

//VIDEO
#ifdef  RENDER16B
	uint16_t Retro_Screen[1280*1024];
#else
	unsigned int Retro_Screen[1280*1024];
#endif 

//SOUND
short signed int SNDBUF[1024*2];
int snd_sampler = 44100 / 50;

//PATH
char RPATH[512];

//EMU FLAGS
int NPAGE=-1, KCOL=1, BKGCOLOR=0;
int SHOWKEY=-1;

#if defined(ANDROID) || defined(__ANDROID__) || defined(VITA)
int MOUSE_EMULATED=1;
#else
int MOUSE_EMULATED=-1;
#endif

int SHIFTON=-1,PAS=4;
int SND=1; //SOUND ON/OFF
static int firstps=0;
int pauseg=0; //enter_gui
int touch=-1; // gui mouse btn
//JOY
int al[2][2];//left analog1
int ar[2][2];//right analog1
unsigned char MXjoy[2]; // joy

//MOUSE
extern int pushi;  // gui mouse btn
int gmx,gmy; //gui mouse
int mouse_wu=0,mouse_wd=0;
//KEYBOARD
char Key_Sate[512];
char Key_Sate2[512];
static char old_Key_Sate[512];

static int mbt[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

//STATS GUI
int BOXDEC= 32+2;
int STAT_BASEY;

static retro_input_state_t input_state_cb;
static retro_input_poll_t input_poll_cb;

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void goto_main_thd(){
   co_switch(mainThread);
}
void goto_emu_thd(){
   co_switch(emuThread);
}


long GetTicks(void)
{ // in MSec
#ifndef _ANDROID_

#ifdef __CELLOS_LV2__

   //#warning "GetTick PS3\n"

   unsigned long	ticks_micro;
   uint64_t secs;
   uint64_t nsecs;

   sys_time_get_current_time(&secs, &nsecs);
   ticks_micro =  secs * 1000000UL + (nsecs / 1000);

   return ticks_micro;///1000;
#else
   struct timeval tv;
   gettimeofday (&tv, NULL);
   return (tv.tv_sec*1000000 + tv.tv_usec);///1000;
#endif
#else

   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   return (now.tv_sec*1000000 + now.tv_nsec/1000);///1000;
#endif

} 

int slowdown=0;
int second_joystick_enable = 0;
//NO SURE FIND BETTER WAY TO COME BACK IN MAIN THREAD IN HATARI GUI
void gui_poll_events(void)
{
   Ktime = GetTicks();

   if(Ktime - LastFPSTime >= 1000/50)
   {
	  slowdown=0;
      frame++; 
      LastFPSTime = Ktime;
		
      co_switch(mainThread);

   }
}

int STATUTON=-1;
#define RETRO_DEVICE_AMIGA_KEYBOARD RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_KEYBOARD, 0)
#define RETRO_DEVICE_AMIGA_JOYSTICK RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)

void enter_gui(void)
{
	pauseg=0;
}

void pause_select(void)
{
   if(pauseg==1 && firstps==0)
   {
      firstps=1;
      enter_gui();
      firstps=0;
   }
}

void texture_uninit(void)
{

}

void texture_init(void)
{
   memset(Retro_Screen, 0, sizeof(Retro_Screen));
   memset(old_Key_Sate ,0, sizeof(old_Key_Sate));

   memset(uae4all_keystate ,0, sizeof(uae4all_keystate));

   gmx=(retrow/2)-1;
   gmy=(retroh/2)-1;
}

int pushi=0; //mouse button

extern unsigned amiga_devices[ 2 ];

extern void vkbd_key(int key,int pressed);

//#include "target.h"
#include "keyboard.h"
#include "keybuf.h"
#include "libretro-keymap.h"
extern int keycode2amiga(int prKeySym);


void retro_key_down(int key)
{
  int iAmigaKeyCode = keyboard_translation[key];
  //LOGI("kbdkey(%d)=%d pressed\n",key,iAmigaKeyCode);
  unsigned char kkey = iAmigaKeyCode | 0x80;

  if (iAmigaKeyCode >= 0)
  	if (!uae4all_keystate[iAmigaKeyCode])
  	{
  		uae4all_keystate[iAmigaKeyCode] = 1;
  		record_key(iAmigaKeyCode << 1);
  	}
  				
}

void retro_key_up(int key)
{
  int iAmigaKeyCode = keyboard_translation[key];
  //LOGI("kbdkey(%d)=%d released\n",key,iAmigaKeyCode);
  unsigned char kkey = iAmigaKeyCode | 0x00;
  if (iAmigaKeyCode >= 0)
  {
	uae4all_keystate[iAmigaKeyCode] = 0;
  	record_key((iAmigaKeyCode << 1) | 1);
  }

}


void vkbd_key(int key,int pressed){

	//LOGI("key(%x)=%x shift:%d (%d)\n",key,pressed,SHIFTON,((key>>4)*8)+(key&7));

	int key2=key;

	if(pressed){

		if(SHIFTON==1){
			uae4all_keystate[AK_LSH] = 1;
			record_key((AK_LSH << 1));
		}

		uae4all_keystate[key2] = 1;
		record_key(key2 << 1);
	}
	else {

		if(SHIFTON==1){
			uae4all_keystate[AK_LSH] = 0;
			record_key((AK_LSH << 1) | 1);
		}	
	
		uae4all_keystate[key2] = 0;
		record_key((key2 << 1) | 1);		
	}

}

void retro_virtualkb(void)
{
   int i;
   //   RETRO        B    Y    SLT  STA  UP   DWN  LEFT RGT  A    X    L    R    L2   R2   L3   R3
   //   INDEX        0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15
   static int oldi=-1;
   static int vkx=0,vky=0;

   if(oldi!=-1)
   {
      vkbd_key(oldi,0);
      oldi=-1;
   }

   if(SHOWKEY==1)
   {
      static int vkflag[5]={0,0,0,0,0};		

      if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) && vkflag[0]==0 )
         vkflag[0]=1;
      else if (vkflag[0]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) )
      {
         vkflag[0]=0;
         vky -= 1; 
      }

      if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) && vkflag[1]==0 )
         vkflag[1]=1;
      else if (vkflag[1]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) )
      {
         vkflag[1]=0;
         vky += 1; 
      }

      if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) && vkflag[2]==0 )
         vkflag[2]=1;
      else if (vkflag[2]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) )
      {
         vkflag[2]=0;
         vkx -= 1;
      }

      if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) && vkflag[3]==0 )
         vkflag[3]=1;
      else if (vkflag[3]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) )
      {
         vkflag[3]=0;
         vkx += 1;
      }

      if(vkx<0)vkx=NPLGN-1;
      if(vkx>NPLGN-1)vkx=0;
      if(vky<0)vky=NLIGN-1;
      if(vky>NLIGN-1)vky=0;

      virtual_kdb(( char *)Retro_Screen,vkx,vky);
 
      i=RETRO_DEVICE_ID_JOYPAD_A;
      if(input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i)  && vkflag[4]==0) 	
         vkflag[4]=1;
      else if( !input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i)  && vkflag[4]==1)
      {
         vkflag[4]=0;
         i=check_vkey2(vkx,vky);

         if(i==-1){
            oldi=-1;
	 }
         if(i==-2)
         {
	    // SWAP PAGE
            NPAGE=-NPAGE;oldi=-1;
         }
         else if(i==-3)
         {
            //KDB bgcolor
            KCOL=-KCOL;
            oldi=-1;
         }
         else if(i==-4)
         {
            //VKBD show/hide 			
            oldi=-1;
            Screen_SetFullUpdate(0);
            SHOWKEY=-SHOWKEY;
         }
         else if(i==-5)
         {
           oldi=-1;
         }
         else if(i==-6)
         {
	   //Exit
	   retro_deinit();
	   oldi=-1;
           exit(0);
         }
         else if(i==-7)
         {
            oldi=-1;
         }
         else if(i==-8)
         {
            oldi=-1;
         }
         else
         {

            if(i==AK_LSH) //SHIFT
            {
               SHIFTON=-SHIFTON;
               oldi=-1;
            }
            else if(i==0x27) //CTRL
            {     
               oldi=-1;
            }
	    else if(i==-12) 
            {
               oldi=-1;
            }
	    else if(i==-13) //GUI
            {     
	       pauseg=1;
               oldi=-1;
            }
	    else if(i==-14) //JOY PORT TOGGLE
            {    
               SHOWKEY=-SHOWKEY;
               oldi=-1;
            }
            else
            {
               oldi=i;
               vkbd_key(oldi,1);            
            }

         }
      }

    }
}

void Screen_SetFullUpdate(int scr)
{
   if(scr==0 ||scr>1)memset(Retro_Screen, 0, sizeof(Retro_Screen));
}

void Process_key()
{
	int i;

	for(i=0;i<320;i++)
        	Key_Sate[i]=input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0,i) ? 0x80: 0;
   
	if(memcmp( Key_Sate,old_Key_Sate , sizeof(Key_Sate) ) )
	 	for(i=0;i<320;i++)
			if(Key_Sate[i] && Key_Sate[i]!=old_Key_Sate[i]  )
        	{	
				if(i==RETROK_F12){					
					continue;
				}
				//printf("press: %d \n",i);
				retro_key_down(i);
	
        	}	
        	else if ( !Key_Sate[i] && Key_Sate[i]!=old_Key_Sate[i]  )
        	{
				if(i==RETROK_F12){
					continue;
				}

				//printf("release: %d \n",i);
				retro_key_up(i);
	
        	}	

	memcpy(old_Key_Sate,Key_Sate , sizeof(Key_Sate) );

}

/*
In joy mode
L3  GUI LOAD
R3  GUI SNAP 
L2  STATUS ON/OFF
R2  AUTOLOAD TAPE
L   CAT
R   RESET
SEL MOUSE/JOY IN GUI
STR ENTER/RETURN
A   FIRE1/VKBD KEY
B   RUN
X   FIRE2
Y   VKBD ON/OFF
In Keayboard mode
F8 LOAD DSK/TAPE
F9 MEM SNAPSHOT LOAD/SAVE
F10 MAIN GUI
F12 PLAY TAPE
*/
extern int lastmx, lastmy, newmousecounters;
extern int buttonstate[3];

int Retro_PollEvent()
{
   //    RETRO          B    Y    SLT  STA  UP   DWN  LEFT RGT  A    X    L    R    L2   R2   L3   R3
   //    INDEX          0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15

   static char vbt[16]={0x10,0x00,0x00,0x00,0x01,0x02,0x04,0x08,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

   int i;
   MXjoy[0]=0;
   MXjoy[1]=0;
   input_poll_cb();

   int mouse_l;
   int mouse_r;
   int16_t rmouse_x,rmouse_y;
   rmouse_x=rmouse_y=0;

   if(SHOWKEY==-1 && pauseg==0)Process_key();

 
      if(pauseg==0)
      { // if emulation running

      // Joy mode for first/main joystick.
      for(i=RETRO_DEVICE_ID_JOYPAD_B;i<=RETRO_DEVICE_ID_JOYPAD_A;i++)
      {
         if( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i))
	 {
            MXjoy[0] |= vbt[i]; // Joy press
	 }
      }

      // second joystick.
      for(i=RETRO_DEVICE_ID_JOYPAD_B;i<=RETRO_DEVICE_ID_JOYPAD_A;i++)
      {
         if( input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, i))
	 {
            MXjoy[1] |= vbt[i]; // Joy press
	 }
         if ((0 != MXjoy[1]) && (0 == second_joystick_enable))
         {
            LOGI("Switch to joystick mode for Port 0.\n");
            MXjoy[1] = 0;
            second_joystick_enable = 1;
         }
      }

}// if pauseg=0
else{
   // if in gui

}


   i=RETRO_DEVICE_ID_JOYPAD_SELECT;     //show vkbd toggle
   if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && mbt[i]==0 )
   {
      mbt[i]=1;
   }
   else if ( mbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) )
   {
      mbt[i]=0;
      SHOWKEY=-SHOWKEY;
   }

   i=RETRO_DEVICE_ID_JOYPAD_START;     //mouse/joy toggle
   if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && mbt[i]==0 )
      mbt[i]=1;
   else if ( mbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) ){
      mbt[i]=0;
      MOUSE_EMULATED=-MOUSE_EMULATED;
      if (MOUSE_EMULATED==1)
      {
         LOGI("Switch to mouse mode for Port 0.\n");
         second_joystick_enable = 0;   // disable 2nd joystick if mouse activated...
      }

   }

   if(MOUSE_EMULATED==1){

	  if(slowdown>0 && pauseg!=0 )return 1;

      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))rmouse_x += PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))rmouse_x -= PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))rmouse_y += PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))rmouse_y -= PAS;
      mouse_l=input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
      mouse_r=input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);

      slowdown=1;
   }
   else {

      mouse_wu = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP);
      mouse_wd = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN);
//if(mouse_wu || mouse_wd)printf("-----------------MOUSE UP:%d DOWN:%d\n",mouse_wu, mouse_wd);
      rmouse_x = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
      rmouse_y = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
      mouse_l    = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
      mouse_r    = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);

   }

   // Emulate mouse as PUAE default configuration...
   int analog_deadzone=0;
   unsigned int opt_analogmouse_deadzone = 20; // hardcoded deadzone...
   analog_deadzone = (opt_analogmouse_deadzone * 32768 / 100);
   int analog_right[2]={0};
   analog_right[0] = (input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X));
   analog_right[1] = (input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y));

   if (abs(analog_right[0]) > analog_deadzone)
      rmouse_x += analog_right[0] * 10 *  0.7 / (32768 );

   if (abs(analog_right[1]) > analog_deadzone)
      rmouse_y += analog_right[1] * 10 *  0.7 / (32768 );

   // L2 & R2 as mouse button...
   mouse_l    |= input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2);
   mouse_r    |= input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2);

   if ((mouse_l || mouse_r) && (second_joystick_enable))
   {
      mouse_l = mouse_r = 0;
      LOGI("Switch to mouse mode for Port 0.\n");
      second_joystick_enable = 0;   // disble 2nd joystick if mouse activated...
   }

   static int mmbL=0,mmbR=0;

   if(mmbL==0 && mouse_l){

      mmbL=1;		
      pushi=1;
      touch=1;

   }
   else if(mmbL==1 && !mouse_l) {

      mmbL=0;
      pushi=0;
      touch=-1;
   }

   if(mmbR==0 && mouse_r){
      mmbR=1;	
      pushi=1;
      touch=1;	
   }
   else if(mmbR==1 && !mouse_r) {
      mmbR=0;
      pushi=0;
      touch=-1;
   }

   gmx+=rmouse_x;
   gmy+=rmouse_y;
   if(gmx<0)gmx=0;
   if(gmx>retrow-1)gmx=retrow-1;
   if(gmy<0)gmy=0;
   if(gmy>retroh-1)gmy=retroh-1;

   lastmx +=rmouse_x;
   lastmy +=rmouse_y;
   newmousecounters=1;
   if(pauseg==0) buttonstate[0] = mmbL;
   if(pauseg==0) buttonstate[2] = mmbR;

return 1;

}

