/* CubicChaos — Cube (ESP32-S3)  [connection-first, nonblocking, MPU INT-driven]
 * Ported from ESP8266 to ESP32-S3 Arduino core:
 *  - ESP-NOW: esp_now.h (ESP32 API), esp_wifi_set_channel()
 *  - Random:  esp_random()
 *  - ISR:     IRAM_ATTR
 *
 * IMPORTANT:
 *  - Update the GPIO pin numbers below for your ESP32-S3 wiring.
 *  - ESP32-S3 does not have “D1/D2/D5…” labels like ESP8266 boards.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <MPU6050.h>
#include <FastLED.h>
#include <math.h>

/* -------- Pins (SET THESE FOR YOUR ESP32-S3) -------- */
#define SCL_PIN      1
#define SDA_PIN      2
#define MPU_INT_PIN  3
#define PIEZO_PIN    4
#define LED_PIN      5

/* -------- LEDs -------- */
#define FACE_LEDS 3
#define NUM_FACES 6
#define NUM_LEDS  (FACE_LEDS*NUM_FACES)
CRGB leds[NUM_LEDS];

/* -------- Protocol -------- */
#define PROTO_VER 1
// Colors
#define C_WHITE  0
#define C_BLUE   1
#define C_ORANGE 2
#define C_RED    3
#define C_GREEN  4
#define C_OFF    255
// Modes
#define M_STATIC        0
#define M_BREATH        1
#define M_SPARKLE       2
#define M_RAINBOW       3
#define M_SPARKLE_SHARP 4
// Sounds
#define S_NONE     0
#define S_START    1
#define S_TIMEGATE 2
#define S_APPROACH 3
#define S_RESET    4
#define S_ENDGAME  5

struct __attribute__((packed)) CubeStatus {
  uint8_t proto_ver, cube_id, face, cur_color, cur_mode;
  uint8_t center_follows, center_color, center_mode, flags; // bit0: stable(1)
};
struct __attribute__((packed)) CubeCmd {
  uint8_t proto_ver, cube_id, target_color, target_mode;
  uint8_t center_follows, center_color, center_mode, sound;
};

/* -------- Globals -------- */
MPU6050 mpu;
volatile bool imu_drdy=false;
static bool imu_ready=false;

uint8_t hubMac[6]={0};
const  uint8_t bcastMac[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
bool    hubKnown=false;
uint8_t hub_channel=1;
uint8_t cubeId=0;

// commanded by hub
uint8_t cur_color=C_WHITE, cur_mode=M_STATIC;
bool    center_follows=true;
uint8_t center_color=C_WHITE, center_mode=M_STATIC;

// orientation
uint8_t cur_face=255;
bool    stable20=false;

// link monitor
static uint32_t lastGoodMs=0, lastSendMs=0, lastFrameCheckMs=0, lastRePinMs=0;
static uint8_t  badFrames=3;
static const uint32_t FRAME_MS=300;
static inline bool isDisconnected(){ return badFrames>=3; }

/* -------- Debug -------- */
#define LOG(...) Serial.printf(__VA_ARGS__)
#define LOGN(x)  Serial.println(x)

/* -------- Face→LED mapping -------- */
// Faces: 0=+X,1=-X,2=+Y,3=-Y,4=+Z,5=-Z
static inline uint8_t faceBase(uint8_t f){ static const uint8_t base[6]={0,6,3,12,9,15}; return base[f]; }
static inline uint8_t faceCenterIdx(uint8_t f){ return faceBase(f)+1; }
static inline uint8_t faceOpp(uint8_t f){ static const uint8_t o[6]={1,0,3,2,5,4}; return o[f]; }
static inline void decodeIdx(uint8_t idx, uint8_t& f, uint8_t& off){
  for(uint8_t i=0;i<6;i++){
    uint8_t b=faceBase(i);
    if(idx>=b && idx<b+FACE_LEDS){ f=i; off=idx-b; return; }
  }
  f=0; off=0;
}

/* -------- Axis remap (edit if sensor board rotated) -------- */
static const int8_t R[3][3] = {
  {+1, 0, 0},
  { 0,+1, 0},
  { 0, 0,+1}
};

/* -------- Color helpers -------- */
static CRGB colorToCRGB(uint8_t c){
  switch(c){
    case C_WHITE:  return CRGB(100,100,100);
    case C_BLUE:   return CRGB(0,0,150);
    case C_ORANGE: return CRGB(150,35,0);
    case C_RED:    return CRGB(200,0,0);
    case C_GREEN:  return CRGB(0,200,0);
    default:       return CRGB::Black;
  }
}

/* -------- MPU INT ISR -------- */
void IRAM_ATTR onImuInt(){ imu_drdy = true; }

/* -------- IMU init + DRDY config -------- */
static void imuInit(){
  mpu.initialize();
  mpu.setSleepEnabled(false);
  mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);
  mpu.setFullScaleGyroRange(MPU6050_GYRO_FS_250);
  #ifdef MPU6050_DLPF_BW_10
    mpu.setDLPFMode(MPU6050_DLPF_BW_10);
  #else
    mpu.setDLPFMode(5); // ~10 Hz LPF
  #endif
  mpu.setInterruptLatch(false);
  mpu.setInterruptLatchClear(true);
  mpu.setIntDataReadyEnabled(true);
  delay(10);
  imu_ready = mpu.testConnection();
}

/* -------- DRDY-gated, bounded-time average to unit vector -------- */
static bool readAccelUnit(float& x, float& y, float& z){
  if(!imu_ready) return false;

  const uint8_t  N      = 12;
  const uint32_t MAX_US = 6000;
  const uint32_t T0     = micros();

  long sx=0, sy=0, sz=0; uint8_t got=0;

  while(got < N && (micros()-T0) < MAX_US){
    if(!imu_drdy) continue;
    imu_drdy = false;

    int16_t ax,ay,az; mpu.getAcceleration(&ax,&ay,&az);
    (void)mpu.getIntStatus(); // clear INT

    int32_t rx = R[0][0]*ax + R[0][1]*ay + R[0][2]*az;
    int32_t ry = R[1][0]*ax + R[1][1]*ay + R[1][2]*az;
    int32_t rz = R[2][0]*ax + R[2][1]*ay + R[2][2]*az;

    sx+=rx; sy+=ry; sz+=rz; got++;
  }

  if(got < 4) return false;

  float fx = sx / (float)got, fy = sy / (float)got, fz = sz / (float)got;
  float mag = sqrtf(fx*fx + fy*fy + fz*fz);
  if(!(mag > 1e-3f)) return false;

  x = fx/mag; y = fy/mag; z = fz/mag;
  return true;
}

/* -------- Face resolve with 20° entry -------- */
static uint8_t detectFace20(uint8_t prevFace){
  float x,y,z;
  if(!readAccelUnit(x,y,z)) return prevFace;

  float dots[6]={ x,-x, y,-y, z,-z };
  int best=0; float bdot=dots[0];
  for(int i=1;i<6;i++){ if(dots[i]>bdot){ bdot=dots[i]; best=i; } }

  const float ENTER_COS = 0.9396926f; // cos 20°
  stable20 = (bdot >= ENTER_COS);

  if(prevFace>5) return best;
  if(best!=prevFace && bdot>=ENTER_COS) return best;
  return prevFace;
}

/* -------- Rendering -------- */
static void clearAll(){ fill_solid(leds, NUM_LEDS, CRGB::Black); }

static void renderStatic(uint8_t color, uint8_t f){
  CRGB col=colorToCRGB(color); uint8_t b=faceBase(f); leds[b]=leds[b+1]=leds[b+2]=col;
}
static void renderBreath(uint8_t color, uint8_t f, uint32_t){
  uint8_t lvl=beatsin8(30,40,255); CRGB c=colorToCRGB(color); c.nscale8_video(lvl);
  uint8_t b=faceBase(f); leds[b]=leds[b+1]=leds[b+2]=c;
}

/* Aggressive sparkle with black/off pops */
static void renderSparkle(uint8_t color, uint8_t f, uint32_t){
  uint8_t b=faceBase(f); CRGB col=colorToCRGB(color);
  for(uint8_t i=0;i<3;i++){
    uint8_t r = (uint8_t)(esp_random() & 7);
    if(r == 0){ leds[b+i] = CRGB::Black; }
    else if(r < 3){ CRGB c=col; c.nscale8_video(200); leds[b+i]=c; }
    else{ leds[b+i].fadeToBlackBy(220); }
  }
}

/* Aggressive sparkle with white pops */
static void renderSparkleSharp(uint8_t color, uint8_t f, uint32_t){
  uint8_t b=faceBase(f); CRGB col=colorToCRGB(color);
  for(uint8_t i=0;i<3;i++){
    uint8_t r = (uint8_t)(esp_random() & 7);
    if(r == 0){ leds[b+i] = CRGB::White; }
    else if(r < 3){ CRGB c=col; c.nscale8_video(200); leds[b+i]=c; }
    else{ leds[b+i].fadeToBlackBy(220); }
  }
}

/* Rainbow: exactly 2 LEDs lit; mirror on opposite face; all others off */
static float rp_pos=0, rp_target=0; static uint32_t rp_nextMs=0;
static inline float wraps(float x,float m){ while(x<0)x+=m; while(x>=m)x-=m; return x; }
static inline uint8_t hueForIndex(uint8_t idx, uint32_t ms){ return ((ms/12)&0xFF) + idx*6; }
static void renderRainbowTwo(uint32_t ms){
  if((int32_t)(ms-rp_nextMs)>=0){
    rp_target = (float)(esp_random()%NUM_LEDS);
    rp_nextMs = ms + 700 + (esp_random()%700);
  }

  float d=rp_target-rp_pos;
  if(d>NUM_LEDS/2.0f)  d-=NUM_LEDS;
  if(d<-NUM_LEDS/2.0f) d+=NUM_LEDS;
  if(d>0.40f)  d=0.40f;
  if(d<-0.40f) d=-0.40f;
  rp_pos = wraps(rp_pos+d,(float)NUM_LEDS);

  clearAll();

  // wrap-safe nearest integer index
  int i = (int)(rp_pos + 0.5f);
  if(i >= NUM_LEDS) i -= NUM_LEDS;   // critical fix
  if(i < 0) i += NUM_LEDS;

  uint8_t f,o;
  decodeIdx((uint8_t)i,f,o);

  leds[i] = CHSV(hueForIndex((uint8_t)i,ms),255,180);

  uint8_t opp = faceBase(faceOpp(f)) + o;
  leds[opp] = CHSV(hueForIndex(opp,ms),255,180);
}


/* Center LED */
static inline bool isFaceDoubleLit(uint8_t f){
  uint8_t b = faceBase(f);
  uint8_t on = (leds[b]   != CRGB::Black)
             + (leds[b+1] != CRGB::Black)
             + (leds[b+2] != CRGB::Black);
  return on == 2;
}
#define CENTER_FOLLOW_SCALE_40 102  // ~40%

static void applyCenter(uint8_t f, uint32_t ms){
  uint8_t ci = faceCenterIdx(f);

  if(center_follows){
    leds[ci] = leds[faceBase(f)];
    return;
  }

  CRGB col = colorToCRGB(center_color);
  CRGB out = col;
  switch(center_mode){
    case M_STATIC:  break;
    case M_BREATH:  { uint8_t lvl=beatsin8(30,40,255); out = col; out.nscale8_video(lvl); } break;
    case M_SPARKLE: {
      uint8_t r=(uint8_t)(esp_random()&7);
      if(r==0) out=CRGB::Black;
      else if(r<3){ out=col; out.nscale8_video(200); }
      else { out=leds[ci]; out.fadeToBlackBy(220); }
    } break;
    case M_SPARKLE_SHARP: out = ((esp_random()&1)==0)?CRGB::Black:col; break;
    case M_RAINBOW: out = CHSV(((ms/12)&0xFF)+ci*6,255,160); break;
  }

  if(isFaceDoubleLit(f) && cur_color != C_WHITE && center_color != C_WHITE){
    out.nscale8_video(CENTER_FOLLOW_SCALE_40);
  }

  leds[ci] = out;
}

/* -------- Tones (nonblocking) -------- */
static const uint16_t SEQ_START[][3]   ={{440,80,20},{660,80,20},{880,120,0}};
static const uint16_t SEQ_TIMEGATE[][3]={{1200,70,40},{1500,70,0}};
static const uint16_t SEQ_APPROACH[][3]={{800,70,20},{1000,70,20},{1200,70,0}};
static const uint16_t SEQ_RESET[][3]   ={{880,110,20},{660,140,0}};
static const uint16_t SEQ_ENDGAME[][3] ={{523,120,20},{659,120,20},{784,180,20},{1046,240,0}};

static const uint16_t (*seqPtr)[3]=nullptr;
static uint8_t seqLen=0, seqIdx=0;
static uint32_t stepEndMs=0;
static bool seqActive=false;

static void startSequencePtr(const uint16_t (*seq)[3], uint8_t len){
  seqPtr=seq; seqLen=len; seqIdx=0; seqActive=(len>0);
  if(seqActive){
    tone(PIEZO_PIN, seqPtr[0][0], seqPtr[0][1]);
    stepEndMs=millis()+seqPtr[0][1]+seqPtr[0][2];
  }else{
    noTone(PIEZO_PIN);
  }
}
static void updateTonePlayer(uint32_t ms){
  if(!seqActive) return;
  if((int32_t)(ms-stepEndMs)<0) return;
  if(++seqIdx>=seqLen){ seqActive=false; noTone(PIEZO_PIN); return; }
  tone(PIEZO_PIN, seqPtr[seqIdx][0], seqPtr[seqIdx][1]);
  stepEndMs=ms+seqPtr[seqIdx][1]+seqPtr[seqIdx][2];
}
static inline void playSound(uint8_t s){
  switch(s){
    case S_START:    startSequencePtr(SEQ_START,   sizeof(SEQ_START)/sizeof(*SEQ_START)); break;
    case S_TIMEGATE: startSequencePtr(SEQ_TIMEGATE,sizeof(SEQ_TIMEGATE)/sizeof(*SEQ_TIMEGATE)); break;
    case S_APPROACH: startSequencePtr(SEQ_APPROACH,sizeof(SEQ_APPROACH)/sizeof(*SEQ_APPROACH)); break;
    case S_RESET:    startSequencePtr(SEQ_RESET,   sizeof(SEQ_RESET)/sizeof(*SEQ_RESET)); break;
    case S_ENDGAME:  startSequencePtr(SEQ_ENDGAME, sizeof(SEQ_ENDGAME)/sizeof(*SEQ_ENDGAME)); break;
    default: break;
  }
}

/* -------- WiFi / channel pin -------- */
static inline bool setWifiChannel(uint8_t ch){
  if(ch < 1 || ch > 14) return false;
  return (esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE) == ESP_OK);
}

static bool pinToHubChannel(uint8_t& chOut){
  for(int t=0;t<8;t++){
    int n=WiFi.scanNetworks(false, true);
    for(int i=0;i<n;i++){
      if(WiFi.SSID(i)=="CubeHub"){
        chOut = (uint8_t)WiFi.channel(i);
        setWifiChannel(chOut);
        return true;
      }
    }
    delay(200);
  }
  chOut=1; setWifiChannel(1); return false;
}

/* -------- ESP-NOW helpers -------- */
static bool addPeerIfNeeded(const uint8_t* mac, uint8_t channel){
  if(esp_now_is_peer_exist(mac)) return true;

  esp_now_peer_info_t p{};
  memcpy(p.peer_addr, mac, 6);
  p.channel = channel;     // must match Wi-Fi channel
  p.encrypt = false;
  return (esp_now_add_peer(&p) == ESP_OK);
}

/* -------- ESP-NOW -------- */
static void sendStatus(bool broadcast){
  CubeStatus st{};
  st.proto_ver=PROTO_VER; st.cube_id=cubeId; st.face=(cur_face<=5)?cur_face:0;
  st.cur_color=cur_color; st.cur_mode=cur_mode;
  st.center_follows=center_follows; st.center_color=center_color; st.center_mode=center_mode;
  st.flags = (stable20?1:0);

  const uint8_t* dest = broadcast ? bcastMac : hubMac;
  esp_now_send(dest, (const uint8_t*)&st, sizeof(st));
}

// ESP32 recv callback signature
static void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len){
  if(len < (int)sizeof(CubeCmd)) return;

  CubeCmd cmd; memcpy(&cmd, data, sizeof(cmd));
  if(cmd.proto_ver!=PROTO_VER) return;

  const uint8_t* mac = info->src_addr;

  if(!hubKnown || memcmp(hubMac, mac, 6)!=0){
    memcpy(hubMac, mac, 6);
    addPeerIfNeeded(hubMac, hub_channel);
    hubKnown=true;
  }

  lastGoodMs = millis();
  badFrames = 0;

  cubeId         = cmd.cube_id ? cmd.cube_id : cubeId;
  cur_color      = cmd.target_color;
  cur_mode       = cmd.target_mode;
  center_follows = cmd.center_follows;
  center_color   = cmd.center_color;
  center_mode    = cmd.center_mode;

  playSound(cmd.sound);
}

// ESP32 send callback signature
static void onDataSent(const uint8_t* mac, esp_now_send_status_t status){
  if(status == ESP_NOW_SEND_SUCCESS){
    lastGoodMs = millis();   // treat successful TX as “link alive”
    if(hubKnown && memcmp(mac, hubMac, 6)==0){
      // optional: keep as-is, but lastGoodMs already updated
    }
  }
}


/* -------- Setup -------- */
void setup(){
  Serial.begin(115200); delay(20); LOGN("\n[CUBE] Boot (ESP32-S3)");

  pinMode(PIEZO_PIN,OUTPUT); noTone(PIEZO_PIN);

  FastLED.addLeds<NEOPIXEL,LED_PIN>(leds,NUM_LEDS);
  FastLED.setCorrection(UncorrectedColor);
  FastLED.setDither(0);
  FastLED.clear(); FastLED.show();
  FastLED.setBrightness(80);   // 25% of 255


  Wire.begin(SDA_PIN, SCL_PIN);
  imuInit();
  pinMode(MPU_INT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(MPU_INT_PIN), onImuInt, RISING);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(true, true);
  delay(20);

  pinToHubChannel(hub_channel);

  if(esp_now_init()!=ESP_OK){
    delay(300);
    ESP.restart();
  }

  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(onDataSent);

  // Broadcast peer (safe on ESP32; required on many builds)
  addPeerIfNeeded(bcastMac, hub_channel);

  uint32_t t=millis();
  lastGoodMs=t; lastSendMs=t; lastFrameCheckMs=t; lastRePinMs=t;

  rp_pos=(float)(esp_random()%NUM_LEDS);
  rp_target=rp_pos;
  rp_nextMs=t+600;
}

/* -------- Loop -------- */
void loop(){
  uint32_t ms=millis();

  // Orientation @25 Hz
  static uint32_t lastIMU=0;
  if(ms-lastIMU>=40){
    lastIMU=ms;
    cur_face = detectFace20(cur_face);
  }

  // Link window @300 ms + occasional re-pin
  if(ms-lastFrameCheckMs>=FRAME_MS){
    lastFrameCheckMs=ms;
    if(ms - lastGoodMs > FRAME_MS){ if(badFrames<255) badFrames++; } else { badFrames=0; }
    if(isDisconnected() && ms-lastRePinMs>5000){
      pinToHubChannel(hub_channel);
      // keep peers aligned to channel
      addPeerIfNeeded(bcastMac, hub_channel);
      if(hubKnown) addPeerIfNeeded(hubMac, hub_channel);
      lastRePinMs=ms;
    }
  }

  // Render (connection priority). All faces render.
  if(isDisconnected()){
    clearAll();
    const CRGB dimWhite(128,128,128);
    for(uint8_t f=0; f<NUM_FACES; ++f){
      uint8_t b=faceBase(f);
      leds[b]=leds[b+1]=leds[b+2]=dimWhite;
      leds[faceCenterIdx(f)]=CRGB::Red;
    }
  }else{
    if(cur_mode==M_RAINBOW){
      renderRainbowTwo(ms);
    }else{
      clearAll();
      for(uint8_t f=0; f<NUM_FACES; ++f){
        switch(cur_mode){
          case M_STATIC:        renderStatic(cur_color, f); break;
          case M_BREATH:        renderBreath(cur_color, f, ms); break;
          case M_SPARKLE:       renderSparkle(cur_color, f, ms); break;
          case M_SPARKLE_SHARP: renderSparkleSharp(cur_color, f, ms); break;
          default:              renderStatic(cur_color, f); break;
        }
        applyCenter(f, ms);
      }
    }
  }
  FastLED.show();

  // Status @10 Hz (broadcast while disconnected)
  if(ms-lastSendMs>=100){
    lastSendMs=ms;
    bool bcast = !hubKnown || isDisconnected();
    sendStatus(bcast);
  }

  updateTonePlayer(ms);
}
