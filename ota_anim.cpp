#include "ota_anim.h"
#include <Arduino.h>
#include <math.h>

static Adafruit_GC9A01A* g_tft = nullptr;
static int16_t SCR_W = 240, SCR_H = 240;
static GFXcanvas16 frame(240,240);

static OtaPhase g_phase = OtaPhase::WIFI_CONNECTING;
static uint32_t g_phaseStart = 0;
static float    g_progress01 = 0.f;
static uint32_t g_lastFrame = 0;

// colors
#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_GRAY    0x8410
#define C_YELL    0xFFE0
#define C_GREEN   0x07E0
#define C_RED     0xF800
#define C_CYAN    0x07FF
#define C_DIM     0x3186

struct EyeLayout { int cxL, cyL, cxR, cyR, w, h, r; } E;

static inline float easeInOut(float x){ x = constrain(x,0.f,1.f); return (x<0.5f)?(2*x*x):(1.f - powf(-2*x+2,2)/2.f); }
static inline float easeOut(float x){ x = constrain(x,0.f,1.f); return 1.f - (1.f-x)*(1.f-x); }

static void initEyes(){
  E.w=70; E.h=70; E.r=20;
  E.cxL=SCR_W/2-44; E.cyL=SCR_H/2;
  E.cxR=SCR_W/2+44; E.cyR=SCR_H/2;
}
static void drawEyeFilled(int cx,int cy,int w,int h,int rad,uint16_t c){ int x=cx-w/2,y=cy-h/2; frame.fillRoundRect(x,y,w,h,rad,c); }
static void drawRingArc(int cx,int cy,int r,int th,int startDeg,int sweepDeg,uint16_t col){
  if (sweepDeg<=0) return;
  int outer=r, inner=max(1,r-th);
  for (int a=0;a<=sweepDeg;a+=3){
    float ang=(startDeg+a-90)*DEG_TO_RAD;
    int x0=cx+(int)(cosf(ang)*inner), y0=cy+(int)(sinf(ang)*inner);
    int x1=cx+(int)(cosf(ang)*outer), y1=cy+(int)(sinf(ang)*outer);
    frame.drawLine(x0,y0,x1,y1,col);
  }
}
static void drawRingFull(int cx,int cy,int r,int th,uint16_t col){ drawRingArc(cx,cy,r,th,0,360,col); }
static void drawPie(int cx,int cy,int r,int sweepDeg,uint16_t col){
  sweepDeg=constrain(sweepDeg,0,360);
  for (int a=0;a<=sweepDeg;a+=3){
    float ang=(a-90)*DEG_TO_RAD;
    int x=cx+(int)(cosf(ang)*r), y=cy+(int)(sinf(ang)*r);
    frame.drawLine(cx,cy,x,y,col);
  }
}

// data rain
struct Drop{ int x; float y; float v; }; const int MAX_DROPS=26; static Drop drops[MAX_DROPS];
static void initDrops(){ for(int i=0;i<MAX_DROPS;i++){ drops[i].x=random(30,SCR_W-30); drops[i].y=random(-140,-10); drops[i].v=1.3f+(random(0,80)/100.f);} }
static void updateDrops(){
  for(int i=0;i<MAX_DROPS;i++){
    drops[i].y+=drops[i].v;
    if (drops[i].y>SCR_H-40){ drops[i].x=random(30,SCR_W-30); drops[i].y=random(-120,-10); drops[i].v=1.2f+(random(0,120)/100.f); }
    frame.fillRect(drops[i].x,(int)drops[i].y,1,2,C_WHITE);
  }
}

static float tNorm(){ uint32_t now=millis(); float t=(now-g_phaseStart)/1000.f; return fmodf(t,1.f); }

static void drawWifiConnecting(float t){
  float sway=sinf(t*2.3f*PI)*6.f;
  drawEyeFilled(E.cxL+(int)sway,E.cyL,E.w,E.h,E.r,C_WHITE);
  drawEyeFilled(E.cxR-(int)sway,E.cyR,E.w,E.h,E.r,C_WHITE);
  int sweep=(int)fmodf(t*360.f,360.f);
  drawRingFull(SCR_W/2,SCR_H/2,96,4,C_DIM);
  drawRingArc (SCR_W/2,SCR_H/2,96,4,0,sweep,C_CYAN);
}
static void drawChecking(float t){
  float k=sinf(t*PI);
  int h=E.h-(int)(k*18);
  drawEyeFilled(E.cxL,E.cyL,E.w,h,E.r,C_WHITE);
  drawEyeFilled(E.cxR,E.cyR,E.w,h,E.r,C_WHITE);
  int sweep=(int)(t*360.f);
  drawRingFull(SCR_W/2,SCR_H/2,96,4,C_DIM);
  drawRingArc (SCR_W/2,SCR_H/2,96,4,0,sweep,C_YELL);
}
static void drawDownloading(float t){
  float p=(g_progress01>0)?g_progress01:easeInOut(min(1.f,t));
  float bounce=(sin(floor(p*10.f)*0.9f)+1.f)*0.5f*2.f;
  drawEyeFilled(E.cxL,E.cyL+(int)bounce,E.w,E.h,E.r,C_WHITE);
  drawEyeFilled(E.cxR,E.cyR+(int)bounce,E.w,E.h,E.r,C_WHITE);
  updateDrops();
  int sweep=(int)(p*360.f);
  drawPie(SCR_W/2,SCR_H/2,100,sweep,C_YELL);
  drawRingFull(SCR_W/2,SCR_H/2,106,2,C_GRAY);
}
static void drawVerifying(float t){
  drawEyeFilled(E.cxR,E.cyR,E.w,E.h,E.r,C_WHITE);
  int mw=(int)(E.w*0.70f), mh=(int)(E.h*0.70f);
  drawEyeFilled(E.cxL,E.cyL,mw,mh,E.r,C_WHITE);
  frame.drawRoundRect(E.cxL-mw/2,E.cyL-mh/2,mw,mh,E.r,C_CYAN);
  float k=easeInOut(fmodf(t,1.f));
  int x0=E.cxR-E.w/2+6, x1=E.cxR+E.w/2-6;
  int sx=x0+(int)((x1-x0)*k);
  frame.drawLine(sx,E.cyR-E.h/2+6,sx,E.cyR+E.h/2-6,C_CYAN);
  drawRingFull(SCR_W/2,SCR_H/2,96,2,C_DIM);
}
static void drawFlashing(float t){
  float p=(g_progress01>0)?g_progress01:easeInOut(min(1.f,t));
  int fillH=(int)(E.h*p);
  frame.fillRoundRect(E.cxL-E.w/2,E.cyL-E.h/2,E.w,E.h,E.r,C_GRAY);
  frame.fillRoundRect(E.cxL-E.w/2,E.cyL+E.h/2-fillH,E.w,fillH,E.r,C_WHITE);
  frame.fillRoundRect(E.cxR-E.w/2,E.cyR-E.h/2,E.w,E.h,E.r,C_GRAY);
  frame.fillRoundRect(E.cxR-E.w/2,E.cyR+E.h/2-fillH,E.w,fillH,E.r,C_WHITE);
  int th=3+(int)(2.f*(0.5f+0.5f*sinf(t*2.0f*PI)));
  drawRingFull(SCR_W/2,SCR_H/2,96,th,C_YELL);
  drawRingFull(SCR_W/2,SCR_H/2,106,2,C_GRAY);
}
static void drawOK(float){
  int w=E.w,h=10,yOff=8;
  frame.fillRoundRect(E.cxL-w/2,E.cyL+yOff,w,h,6,C_WHITE);
  frame.fillRoundRect(E.cxR-w/2,E.cyR+yOff,w,h,6,C_WHITE);
  drawRingFull(SCR_W/2,SCR_H/2,96,5,C_GREEN);
  frame.drawPixel(SCR_W/2-28,SCR_H/2-36,C_WHITE);
  frame.drawPixel(SCR_W/2+28,SCR_H/2-36,C_WHITE);
}
static void drawError(float t){
  float tilt=sinf(t*2.0f*PI)*6.f;
  drawEyeFilled(E.cxL-(int)tilt,E.cyL,E.w,E.h,E.r,C_WHITE);
  drawEyeFilled(E.cxR+(int)tilt,E.cyR,E.w,E.h,E.r,C_WHITE);
  int th=4+(int)(2.f*(0.5f+0.5f*sinf(t*6.0f*PI)));
  drawRingFull(SCR_W/2,SCR_H/2,96,th,C_RED);
  drawRingFull(SCR_W/2,SCR_H/2,106,2,C_GRAY);
}
static void drawReboot(float t){
  float s=1.f-easeOut(min(1.f,t));
  int w=max(4,(int)(E.w*s)), h=max(4,(int)(E.h*s));
  int cx=SCR_W/2, cy=SCR_H/2;
  drawEyeFilled(cx-(int)(30*s),cy,w,h,max(2,(int)(E.r*s)),C_WHITE);
  drawEyeFilled(cx+(int)(30*s),cy,w,h,max(2,(int)(E.r*s)),C_WHITE);
  int sweep=(int)(t*720.f);
  drawRingArc(SCR_W/2,SCR_H/2,96,4,0,sweep%360,C_CYAN);
}

void OTAAnim_begin(Adafruit_GC9A01A* tft, int16_t w, int16_t h){
  g_tft=tft; SCR_W=w; SCR_H=h;
  initEyes();
  OTAAnim_start(OtaPhase::WIFI_CONNECTING);
}

void OTAAnim_start(OtaPhase p){
  g_phase=p; g_phaseStart=millis(); g_progress01=0.f;
  if (p==OtaPhase::DOWNLOADING) initDrops();
}
void OTAAnim_setProgress(float p01){ g_progress01 = constrain(p01,0.f,1.f); }

void OTAAnim_drawFrame(){
  // ~33 fps
  uint32_t now=millis();
  static uint32_t last=0;
  if (now-last<30) return;
  last=now;

  frame.fillScreen(C_BLACK);
  float t=tNorm();

  switch(g_phase){
    case OtaPhase::WIFI_CONNECTING: drawWifiConnecting(t); break;
    case OtaPhase::CHECKING:        drawChecking(t); break;
    case OtaPhase::DOWNLOADING:     drawDownloading(t); break;
    case OtaPhase::VERIFYING:       drawVerifying(t); break;
    case OtaPhase::FLASHING:        drawFlashing(t); break;
    case OtaPhase::OK:              drawOK(t); break;
    case OtaPhase::ERROR_:          drawError(t); break;
    case OtaPhase::REBOOT_:         drawReboot(t); break;
  }
  if (g_tft) g_tft->drawRGBBitmap(0,0,frame.getBuffer(),SCR_W,SCR_H);
}