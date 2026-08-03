// WS2812B etc animations
// For the animations themselves credit goes to https://github.com/Electriangle
#include "../new_common.h"
#include "../new_pins.h"
#include "../new_cfg.h"

#if ENABLE_DRIVER_PIXELANIM

// Commands register, execution API and cmd tokenizer
#include "../cmnds/cmd_public.h"
#include "../mqtt/new_mqtt.h"
#include "../logging/logging.h"
#include "drv_local.h"
#include "../hal/hal_pins.h"
#include <math.h>

/*
// Usage:
startDriver SM16703P
SM16703P_Init 16
startDriver PixelAnim

*/

// Credit: https://github.com/Electriangle/RainbowCycle_Main
byte *RainbowWheel_Wheel(byte WheelPosition) {
	static byte c[3];

	if (WheelPosition < 85) {
		c[0] = WheelPosition * 3;
		c[1] = 255 - WheelPosition * 3;
		c[2] = 0;
	}
	else if (WheelPosition < 170) {
		WheelPosition -= 85;
		c[0] = 255 - WheelPosition * 3;
		c[1] = 0;
		c[2] = WheelPosition * 3;
	}
	else {
		WheelPosition -= 170;
		c[0] = 0;
		c[1] = WheelPosition * 3;
		c[2] = 255 - WheelPosition * 3;
	}

	return c;
}
uint16_t j = 0;
uint16_t count = 0;
int direction = 1;
void fadeToBlackBy(uint8_t fadeBy)
{
	Strip_scaleAllPixels(255-fadeBy);
}
void ShootingStar_Run() {
	int tail_length = 32;
	if (direction == -1) {        // Reverse direction option for LEDs
		if (count < pixel_count) {
			Strip_setPixelWithBrig(pixel_count - (count % (pixel_count + 1)),
				led_baseColors[0], led_baseColors[1], led_baseColors[2], led_baseColors[3], led_baseColors[4]);    // Set LEDs with the color value
		}
		count++;
	}
	else {
		if (count < pixel_count) {     // Forward direction option for LEDs
			Strip_setPixelWithBrig(count % pixel_count,
				led_baseColors[0], led_baseColors[1], led_baseColors[2], led_baseColors[3], led_baseColors[4]);    // Set LEDs with the color value
		}
		count++;
	}
	if (count > pixel_count + 16) {
		count = 0;
	}
	fadeToBlackBy(tail_length);                 // Fade the tail LEDs to black
	Strip_Apply();
}
void RainbowCycle_Run() {
	byte *c;
	uint16_t i;

	for (i = 0; i < pixel_count; i++) {
		c = RainbowWheel_Wheel(((i * 256 / pixel_count) + j) & 255);
		Strip_setPixelWithBrig(pixel_count - 1 - i, *c, *(c + 1), *(c + 2), 0, 0);
	}
	Strip_Apply();
	j++;
	j %= 256;
}

void Fire_setPixelHeatColor(int Pixel, byte temperature) {
	// Rescale heat from 0-255 to 0-191
	byte t192 = round((temperature / 255.0) * 191);

	// Calculate ramp up from
	byte heatramp = t192 & 0x3F; // 0...63
	heatramp <<= 2; // scale up to 0...252

	// Figure out which third of the spectrum we're in:
	if (t192 > 0x80) {                    // hottest
		Strip_setPixelWithBrig(Pixel, 255, 255, heatramp, 0, 0); // red to yellow
	}
	else if (t192 > 0x40) {               // middle
		Strip_setPixelWithBrig(Pixel, 255, heatramp, 0, 0, 0); // red to yellow
	}
	else {                               // coolest
		Strip_setPixelWithBrig(Pixel, heatramp, 0, 0, 0, 0);
	}
}
// FlameHeight - Use larger value for shorter flames, default=50.
// Sparks - Use larger value for more ignitions and a more active fire (between 0 to 255), default=100.
// DelayDuration - Use larger value for slower flame speed, default=10.
int FlameHeight = 50;
int Sparks = 100;
static byte *pix_workBuffer = 0;
static int pix_workBufferSize = 0;

void Pix_EnsureAllocatedWork(int bytes) {
	if (bytes < pix_workBufferSize)
		return;
	pix_workBufferSize = bytes + 16;
	pix_workBuffer = (byte*)realloc(pix_workBuffer, pix_workBufferSize);
}
int RandomRange(int min, int max) {
	int r = rand() % (max - min);
	return min + r;
}
void Fire_Run() {
	int cooldown;

	// we need a buffer for that
	Pix_EnsureAllocatedWork(pixel_count);
	// in case that realloc failed...
	if (pix_workBuffer == 0) {
		return;
	}
	// alias it as 'heat'
	byte *heat = pix_workBuffer;

	// Cool down each cell a little
	for (int i = 0; i < pixel_count; i++) {
		cooldown = RandomRange(0, ((FlameHeight * 10) / pixel_count) + 2);

		if (cooldown > heat[i]) {
			heat[i] = 0;
		}
		else {
			heat[i] = heat[i] - cooldown;
		}
	}

	// Heat from each cell drifts up and diffuses slightly
	for (int k = (pixel_count - 1); k >= 2; k--) {
		heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
	}

	// Randomly ignite new Sparks near bottom of the flame
	if (rand()%255 < Sparks) {
		int y = rand()%7;
		heat[y] = heat[y] + RandomRange(160, 255);
	}

	// Convert heat to LED colors
	for (int j = 0; j < pixel_count; j++) {
		Fire_setPixelHeatColor(j, heat[j]);
	}
	Strip_Apply();

}
static int comet_pos = 0;
static int comet_dir_local = 1;
static int comet_tail_len = 16;

void Comet_Run() {
	int head = comet_pos;
	int tail = comet_tail_len;
	if (comet_dir_local > 0) {
		comet_pos++;
		if (comet_pos >= pixel_count) {
			comet_pos = pixel_count - 1;
			comet_dir_local = -1;
		}
	}
	else {
		comet_pos--;
		if (comet_pos < 0) {
			comet_pos = 0;
			comet_dir_local = 1;
		}
	}

	fadeToBlackBy(48);

	Strip_setPixelWithBrig(head,
		led_baseColors[0], led_baseColors[1], led_baseColors[2], led_baseColors[3], led_baseColors[4]);

	for (int t = 1; t <= tail; t++) {
		int idx = head - t * comet_dir_local;
		if (idx < 0 || idx >= pixel_count) continue;
		float scale = (float)(tail - t) / (float)tail;
		if (scale < 0) scale = 0;
		byte r = (byte)round(led_baseColors[0] * scale);
		byte g = (byte)round(led_baseColors[1] * scale);
		byte b = (byte)round(led_baseColors[2] * scale);
		Strip_setPixelWithBrig(idx, r, g, b, 0, 0);
	}

	Strip_Apply();
}
static int chase_pos = 0;

void TheaterChase_Run() {
	fadeToBlackBy(200);

	for (int i = 0; i < pixel_count; i++) {
		if ((i + chase_pos) % 3 == 0) {
			Strip_setPixelWithBrig(i,
				led_baseColors[0], led_baseColors[1], led_baseColors[2],
				led_baseColors[3], led_baseColors[4]);
		}
	}
	Strip_Apply();

	chase_pos++;
	if (chase_pos >= 3) chase_pos = 0;
}

static int chase_rainbow_pos = 0;

void TheaterChaseRainbow_Run() {
	for (int i = 0; i < pixel_count; i++) {
		if ((i + chase_rainbow_pos) % 3 == 0) {
			byte *c = RainbowWheel_Wheel((i + chase_rainbow_pos * 8) & 255);
			Strip_setPixelWithBrig(i, *c, *(c + 1), *(c + 2), 0, 0);
		}
		else {
			Strip_setPixelWithBrig(i, 0, 0, 0, 0, 0);
		}
	}
	Strip_Apply();

	chase_rainbow_pos++;
	if (chase_rainbow_pos >= 3) chase_rainbow_pos = 0;
}
// startDriver PixelAnim

int activeAnim = -1;
int g_speed = 0;
void PixelAnim_SetAnim(int j);

/* ---- Cloudcutter Notify / Ambient (on-device) ----
 * Spec: notes/STANDARD_ANIMS.md — PascalCase: Beacon, BeaconX, Solid, SolidFlash
 *
 * Contract: SAVE main+halo → click 0..10 @ clickBright → effect → RESTORE
 *
 * Notify Beacon <click> <r> <g> <b> <speed> <reps> [bright] [ramp] [clickBright]
 * Notify BeaconX <click> <r> <g> <b> <speed> <reps> [bright] [ramp] [beams] [width] [tail] [clickBright]
 * Solid <click> <r> <g> <b> [bright] [ramp] [clickBright]
 * SolidFlash <click> <r> <g> <b> <on> <off> <reps> [bright] [rup] [rdn] [clickBright]
 *   on/off/rup/rdn in tenths of second (3 = 0.3s)
 *
 * Ambient Beacon|Solid|SolidFlash ... [click] [clickBright]  — forever until stop
 * BeaconX NOT in Ambient.
 * Legacy: Beacon <anim> <bright> <clickOld> <reps> [R G B]
 */
#ifndef BEACON_TAIL
#define BEACON_TAIL			12
#endif
#ifndef BEACON_SPEED_LED
#define BEACON_SPEED_LED	2.0f
#endif
#ifndef BEACON_CLICK_TICKS_DEF
#define BEACON_CLICK_TICKS_DEF	2
#endif
#ifndef NOTIFY_RAMP_TICKS
#define NOTIFY_RAMP_TICKS	20
#endif
/* tenths of sec → quickticks (25ms): 0.1s = 4 ticks */
#define TENTHS_TO_TICKS(t)	((t) <= 0 ? 0 : ((t) * 4))
#define BEACON_CH_WW		1
#define BEACON_CH_CW		2
#define BEACON_CH_R		10
#define BEACON_CH_G		11
#define BEACON_CH_B		12
#define BEACON_SET_FLAGS	(CHANNEL_SET_FLAG_FORCE | CHANNEL_SET_FLAG_SILENT | CHANNEL_SET_FLAG_SKIP_MQTT)

#define NOTIFY_PAT_BEACON		1
#define NOTIFY_PAT_BEACONX		2
#define NOTIFY_PAT_SOLID		3
#define NOTIFY_PAT_SOLIDFLASH	4

typedef struct beaconState_s {
	int active;
	int ambient;
	int pattern;
	int bright;
	int rampOn;
	int rampAge;
	int clickCount;		/* 0..10 */
	int clickBright;	/* main 0..100 during pulse */
	int clickTicks;
	int repeats;
	int revDone;
	float pos;
	float speed;
	byte colR, colG, colB;
	int saveWW, saveCW;
	int saveR, saveG, saveB;
	int clickPhase;
	int clickStep;
	float prevPos;
	/* Beacon / BeaconX */
	int beams;
	int beamWidth;
	int tailLen;
	/* SolidFlash envelope (ticks) */
	int flOn, flOff, flRup, flRdn;
	int flPhase;		/* 0 rup 1 on 2 rdn 3 off */
	int flAge;
	int flDone;
	float flLevel;		/* 0..1 paint scale for flash */
} beaconState_t;

static beaconState_t g_beacon;
static int g_beaconAnimIndex = -1;

static int Beacon_ModLED(int idx, int n) {
	if (n <= 0) return 0;
	idx %= n;
	if (idx < 0) idx += n;
	return idx;
}

static byte Beacon_Scale(byte c, int bright, float scale) {
	float v = (float)c * (float)bright * (1.0f / 255.0f) * scale;
	if (v < 0) v = 0;
	if (v > 255) v = 255;
	return (byte)(v + 0.5f);
}

/* 0..1 over first NOTIFY_RAMP_TICKS when rampOn; else 1. Clicks unaffected. */
static float Notify_RampScale(void) {
	if (!g_beacon.rampOn) {
		return 1.0f;
	}
	if (g_beacon.rampAge >= NOTIFY_RAMP_TICKS) {
		return 1.0f;
	}
	return (float)(g_beacon.rampAge + 1) / (float)NOTIFY_RAMP_TICKS;
}

static int Notify_EffectiveBright(void) {
	float b = (float)g_beacon.bright * Notify_RampScale();
	if (b < 0) b = 0;
	if (b > 255) b = 255;
	return (int)(b + 0.5f);
}

static void Beacon_AddPixel(int idx, int n, byte r, byte g, byte b, float scale, int bright) {
	byte er, eg, eb;
	if (scale <= 0.001f) return;
	idx = Beacon_ModLED(idx, n);
	er = Beacon_Scale(r, bright, scale);
	eg = Beacon_Scale(g, bright, scale);
	eb = Beacon_Scale(b, bright, scale);
	/* frame is cleared each tick; opposite sectors rarely share a pixel */
	Strip_setPixel(idx, er, eg, eb, 0, 0);
}

/* Sub-pixel head + decaying tail (one sector). */
static void Beacon_DrawSector(float headPos, int n, byte r, byte g, byte b, int bright) {
	int base = (int)floorf(headPos);
	float frac = headPos - (float)base;
	int t;

	/* head split across two LEDs */
	Beacon_AddPixel(base, n, r, g, b, 1.0f - frac, bright);
	Beacon_AddPixel(base + 1, n, r, g, b, frac, bright);

	for (t = 1; t <= BEACON_TAIL; t++) {
		float scale = (float)(BEACON_TAIL - t) / (float)BEACON_TAIL;
		if (scale < 0) scale = 0;
		/* tail behind head (CW motion = increasing index → tail at lower index) */
		Beacon_AddPixel(base - t, n, r, g, b, scale * (1.0f - frac), bright);
		Beacon_AddPixel(base - t + 1, n, r, g, b, scale * frac, bright);
	}
}

/* ---- Main / Halo solid ramp (FW, opt-in) ----
 * Main <ww> <cw> [ramp]   ww/cw 0-100 typical
 * Halo <r> <g> <b> [ramp] solid ring; needs SM16703P init
 * ramp 0=instant (default), 1=~0.5s. Clicks use WhitesHard — no ramp.
 */
typedef struct lightRamp_s {
	int active;
	int age;
	float curA, curB, curC;	/* main: ww,cw unused; halo: r,g,b */
	float curD;			/* main cw as curB; halo unused */
	int tgtA, tgtB, tgtC, tgtD;
	int isHalo;			/* 0=main 1=halo */
} lightRamp_t;

static lightRamp_t g_mainRamp;
static lightRamp_t g_haloRamp;

static void LightRamp_CancelMain(void) {
	g_mainRamp.active = 0;
}
static void LightRamp_CancelHalo(void) {
	g_haloRamp.active = 0;
}

static void Beacon_WhitesHard(int ww, int cw) {
	/* clicks / notify hold: never ramp */
	LightRamp_CancelMain();
	CHANNEL_Set(BEACON_CH_WW, ww, BEACON_SET_FLAGS);
	CHANNEL_Set(BEACON_CH_CW, cw, BEACON_SET_FLAGS);
}

static void Main_ApplyNow(int ww, int cw) {
	LightRamp_CancelMain();
	CHANNEL_Set(BEACON_CH_WW, ww, BEACON_SET_FLAGS);
	CHANNEL_Set(BEACON_CH_CW, cw, BEACON_SET_FLAGS);
}

static void Halo_ApplyNow(int r, int g, int b) {
	int i, n;
	LightRamp_CancelHalo();
	CHANNEL_Set(BEACON_CH_R, r, BEACON_SET_FLAGS);
	CHANNEL_Set(BEACON_CH_G, g, BEACON_SET_FLAGS);
	CHANNEL_Set(BEACON_CH_B, b, BEACON_SET_FLAGS);
	n = (int)pixel_count;
	if (n > 0) {
		for (i = 0; i < n; i++) {
			Strip_setPixel(i, r, g, b, 0, 0);
		}
		Strip_Apply();
	}
}

static void Main_Begin(int ww, int cw, int ramp) {
	if (ww < 0) ww = 0;
	if (cw < 0) cw = 0;
	if (ww > 100) ww = 100;
	if (cw > 100) cw = 100;
	if (!ramp) {
		Main_ApplyNow(ww, cw);
		return;
	}
	g_mainRamp.curA = (float)CHANNEL_Get(BEACON_CH_WW);
	g_mainRamp.curB = (float)CHANNEL_Get(BEACON_CH_CW);
	g_mainRamp.tgtA = ww;
	g_mainRamp.tgtB = cw;
	g_mainRamp.age = 0;
	g_mainRamp.active = 1;
	g_mainRamp.isHalo = 0;
}

static int ClampByteInt(int v) {
	if (v < 0) return 0;
	if (v > 255) return 255;
	return v;
}

static void Halo_Begin(int r, int g, int b, int ramp) {
	r = ClampByteInt(r);
	g = ClampByteInt(g);
	b = ClampByteInt(b);
	if (!ramp) {
		Halo_ApplyNow(r, g, b);
		return;
	}
	/* start from current channel mirrors (or 0) */
	g_haloRamp.curA = (float)CHANNEL_Get(BEACON_CH_R);
	g_haloRamp.curB = (float)CHANNEL_Get(BEACON_CH_G);
	g_haloRamp.curC = (float)CHANNEL_Get(BEACON_CH_B);
	g_haloRamp.tgtA = r;
	g_haloRamp.tgtB = g;
	g_haloRamp.tgtC = b;
	g_haloRamp.age = 0;
	g_haloRamp.active = 1;
	g_haloRamp.isHalo = 1;
}

static void LightRamp_Tick(void) {
	float t;
	int v0, v1, v2, i, n;

	if (g_mainRamp.active) {
		g_mainRamp.age++;
		t = (float)g_mainRamp.age / (float)NOTIFY_RAMP_TICKS;
		if (t >= 1.0f) {
			t = 1.0f;
			g_mainRamp.active = 0;
		}
		v0 = (int)(g_mainRamp.curA + ((float)g_mainRamp.tgtA - g_mainRamp.curA) * t + 0.5f);
		v1 = (int)(g_mainRamp.curB + ((float)g_mainRamp.tgtB - g_mainRamp.curB) * t + 0.5f);
		CHANNEL_Set(BEACON_CH_WW, v0, BEACON_SET_FLAGS);
		CHANNEL_Set(BEACON_CH_CW, v1, BEACON_SET_FLAGS);
	}

	if (g_haloRamp.active) {
		/* do not fight active Notify paint */
		if (g_beacon.active) {
			LightRamp_CancelHalo();
		} else {
			g_haloRamp.age++;
			t = (float)g_haloRamp.age / (float)NOTIFY_RAMP_TICKS;
			if (t >= 1.0f) {
				t = 1.0f;
				g_haloRamp.active = 0;
			}
			v0 = (int)(g_haloRamp.curA + ((float)g_haloRamp.tgtA - g_haloRamp.curA) * t + 0.5f);
			v1 = (int)(g_haloRamp.curB + ((float)g_haloRamp.tgtB - g_haloRamp.curB) * t + 0.5f);
			v2 = (int)(g_haloRamp.curC + ((float)g_haloRamp.tgtC - g_haloRamp.curC) * t + 0.5f);
			CHANNEL_Set(BEACON_CH_R, v0, BEACON_SET_FLAGS);
			CHANNEL_Set(BEACON_CH_G, v1, BEACON_SET_FLAGS);
			CHANNEL_Set(BEACON_CH_B, v2, BEACON_SET_FLAGS);
			n = (int)pixel_count;
			if (n > 0) {
				for (i = 0; i < n; i++) {
					Strip_setPixel(i, v0, v1, v2, 0, 0);
				}
				Strip_Apply();
			}
		}
	}
}

commandResult_t PA_Cmd_Main(const void *context, const char *cmd, const char *args, int flags) {
	int ww, cw, ramp, narg;
	(void)context; (void)cmd; (void)flags;
	Tokenizer_TokenizeString(args, 0);
	narg = Tokenizer_GetArgsCount();
	if (narg < 2) {
		return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	}
	ww = Tokenizer_GetArgInteger(0);
	cw = Tokenizer_GetArgInteger(1);
	ramp = (narg >= 3) ? Tokenizer_GetArgInteger(2) : 0;
	Main_Begin(ww, cw, ramp ? 1 : 0);
	return CMD_RES_OK;
}

commandResult_t PA_Cmd_Halo(const void *context, const char *cmd, const char *args, int flags) {
	int r, g, b, ramp, narg;
	(void)context; (void)cmd; (void)flags;
	Tokenizer_TokenizeString(args, 0);
	narg = Tokenizer_GetArgsCount();
	if (narg < 3) {
		return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	}
	r = Tokenizer_GetArgInteger(0);
	g = Tokenizer_GetArgInteger(1);
	b = Tokenizer_GetArgInteger(2);
	ramp = (narg >= 4) ? Tokenizer_GetArgInteger(3) : 0;
	if (pixel_count == 0) {
		ADDLOG_ERROR(LOG_FEATURE_CMD, "Halo: pixel_count=0 (SM16703P Init first)");
		return CMD_RES_ERROR;
	}
	Halo_Begin(r, g, b, ramp ? 1 : 0);
	return CMD_RES_OK;
}

static int Beacon_ClickBusy(void) {
	return (g_beacon.clickCount > 0 && g_beacon.clickPhase > 0 && g_beacon.clickStep >= 0);
}

static void Beacon_HoldWhites(void) {
	if (!Beacon_ClickBusy()) {
		Beacon_WhitesHard(g_beacon.saveWW, g_beacon.saveCW);
	}
}

static void Beacon_Restore(void) {
	int n = (int)pixel_count;

	g_beacon.active = 0;
	g_beacon.ambient = 0;
	g_beacon.clickPhase = 0;
	g_beacon.clickStep = -1;
	g_beacon.flPhase = 0;
	g_beacon.flAge = 0;
	g_beacon.flDone = 0;
	g_beacon.flLevel = 0;
	g_lightMode = Light_RGB;

	/*
	 * Full black first, then saved solid — avoids half-ring / seam garbage
	 * when previous frame left partial SPI buffer state.
	 */
	if (n > 0) {
		Strip_setAllPixels(0, 0, 0, 0, 0);
		Strip_Apply();
		Strip_setAllPixels(g_beacon.saveR, g_beacon.saveG, g_beacon.saveB, 0, 0);
		Strip_Apply();
	}

	CHANNEL_Set(BEACON_CH_R, g_beacon.saveR, BEACON_SET_FLAGS);
	CHANNEL_Set(BEACON_CH_G, g_beacon.saveG, BEACON_SET_FLAGS);
	CHANNEL_Set(BEACON_CH_B, g_beacon.saveB, BEACON_SET_FLAGS);
	Beacon_WhitesHard(g_beacon.saveWW, g_beacon.saveCW);

	ADDLOG_INFO(LOG_FEATURE_CMD, "Notify: restore (clear+fill) WW=%i CW=%i RGB=%i,%i,%i",
		g_beacon.saveWW, g_beacon.saveCW, g_beacon.saveR, g_beacon.saveG, g_beacon.saveB);
}

static void Notify_Finish(void) {
	Beacon_Restore();
	activeAnim = -1;
	MQTT_PublishMain_StringString_DeDuped(DEDUP_CURRENT_ANIM, DEDUP_EXPIRE_TIME, "currentAnim", "None", 0);
}

static void Beacon_StartClickTrain(void) {
	if (g_beacon.clickCount <= 0) return;
	g_beacon.clickStep = 0;
	g_beacon.clickPhase = g_beacon.clickTicks;
	Beacon_WhitesHard(g_beacon.clickBright, g_beacon.clickBright);
}

static void Beacon_ClickTick(void) {
	int pulses = g_beacon.clickCount;

	if (pulses <= 0) return;
	if (g_beacon.clickPhase <= 0 && g_beacon.clickStep < 0) return;
	if (g_beacon.clickPhase <= 0) return;

	g_beacon.clickPhase--;
	if (g_beacon.clickPhase > 0) return;

	g_beacon.clickStep++;
	/* sequence: ON, OFF, ON, ... for `pulses` ONs → length pulses*2-1 phases */
	if (g_beacon.clickStep >= pulses * 2 - 1) {
		Beacon_WhitesHard(g_beacon.saveWW, g_beacon.saveCW);
		g_beacon.clickStep = -1;
		g_beacon.clickPhase = 0;
		return;
	}
	if ((g_beacon.clickStep & 1) == 0) {
		Beacon_WhitesHard(g_beacon.clickBright, g_beacon.clickBright);
	} else {
		Beacon_WhitesHard(g_beacon.saveWW, g_beacon.saveCW);
	}
	g_beacon.clickPhase = g_beacon.clickTicks;
}

static float Notify_SpeedFromArg(int speedArg) {
	float s;
	if (speedArg <= 0) {
		return BEACON_SPEED_LED;
	}
	s = (float)speedArg * 0.4f;
	if (s < 0.25f) s = 0.25f;
	if (s > 8.0f) s = 8.0f;
	return s;
}

static int Notify_ParsePattern(const char *s, int asInt) {
	if (s && s[0]) {
		if (!stricmp(s, "stop") || !stricmp(s, "off")) return 0;
		if (!stricmp(s, "beacon")) return NOTIFY_PAT_BEACON;
		if (!stricmp(s, "beaconx") || !stricmp(s, "beacon_x")) return NOTIFY_PAT_BEACONX;
		if (!stricmp(s, "solid")) return NOTIFY_PAT_SOLID;
		if (!stricmp(s, "solidflash") || !stricmp(s, "solid_flash")) return NOTIFY_PAT_SOLIDFLASH;
	}
	if (asInt == 0) return 0;
	if (asInt == NOTIFY_PAT_BEACON || asInt == NOTIFY_PAT_BEACONX ||
		asInt == NOTIFY_PAT_SOLID || asInt == NOTIFY_PAT_SOLIDFLASH) {
		return asInt;
	}
	return NOTIFY_PAT_BEACON;
}

static byte Notify_ClampByte(int v) {
	if (v < 0) return 0;
	if (v > 255) return 255;
	return (byte)v;
}

static int Notify_ClampClick(int c) {
	if (c < 0) return 0;
	if (c > 10) return 10;
	return c;
}

static int Notify_ClampMainBright(int b) {
	if (b < 0) return 0;
	if (b > 100) return 100;
	return b;
}

/* Draw N beams with configurable tail (width folded into soft head). */
static void Beacon_DrawBeams(float pos, int n, int beams, int tail, int bright,
	byte r, byte g, byte b) {
	int bi;
	if (beams < 1) beams = 1;
	if (beams > n) beams = n;
	if (tail < 0) tail = 0;
	for (bi = 0; bi < beams; bi++) {
		float head = pos + ((float)bi * (float)n / (float)beams);
		while (head >= (float)n) head -= (float)n;
		while (head < 0) head += (float)n;
		/* reuse sector drawer: uses BEACON_TAIL constant internally — use local loop */
		{
			int base = (int)floorf(head);
			float frac = head - (float)base;
			int t;
			int tMax = tail;
			Beacon_AddPixel(base, n, r, g, b, 1.0f - frac, bright);
			Beacon_AddPixel(base + 1, n, r, g, b, frac, bright);
			for (t = 1; t <= tMax; t++) {
				float scale = (float)(tMax - t) / (float)(tMax > 0 ? tMax : 1);
				if (scale < 0) scale = 0;
				Beacon_AddPixel(base - t, n, r, g, b, scale * (1.0f - frac), bright);
				Beacon_AddPixel(base - t + 1, n, r, g, b, scale * frac, bright);
			}
		}
	}
}

static void SolidFlash_TickEnvelope(void) {
	/* advance envelope; flLevel 0..1 */
	int needNext = 0;

	if (g_beacon.flPhase == 0) { /* rup */
		if (g_beacon.flRup <= 0) {
			g_beacon.flLevel = 1.0f;
			g_beacon.flPhase = 1;
			g_beacon.flAge = 0;
		} else {
			g_beacon.flAge++;
			g_beacon.flLevel = (float)g_beacon.flAge / (float)g_beacon.flRup;
			if (g_beacon.flLevel >= 1.0f) {
				g_beacon.flLevel = 1.0f;
				g_beacon.flPhase = 1;
				g_beacon.flAge = 0;
			}
		}
	} else if (g_beacon.flPhase == 1) { /* on plateau */
		g_beacon.flLevel = 1.0f;
		if (g_beacon.flOn <= 0) {
			g_beacon.flPhase = 2;
			g_beacon.flAge = 0;
		} else {
			g_beacon.flAge++;
			if (g_beacon.flAge >= g_beacon.flOn) {
				g_beacon.flPhase = 2;
				g_beacon.flAge = 0;
			}
		}
	} else if (g_beacon.flPhase == 2) { /* rdn */
		if (g_beacon.flRdn <= 0) {
			g_beacon.flLevel = 0.0f;
			g_beacon.flPhase = 3;
			g_beacon.flAge = 0;
		} else {
			g_beacon.flAge++;
			g_beacon.flLevel = 1.0f - (float)g_beacon.flAge / (float)g_beacon.flRdn;
			if (g_beacon.flLevel <= 0.0f) {
				g_beacon.flLevel = 0.0f;
				g_beacon.flPhase = 3;
				g_beacon.flAge = 0;
			}
		}
	} else { /* off */
		g_beacon.flLevel = 0.0f;
		if (g_beacon.flOff <= 0) {
			needNext = 1;
		} else {
			g_beacon.flAge++;
			if (g_beacon.flAge >= g_beacon.flOff) {
				needNext = 1;
			}
		}
	}
	if (needNext) {
		g_beacon.flDone++;
		g_beacon.flPhase = 0;
		g_beacon.flAge = 0;
		g_beacon.flLevel = 0.0f;
	}
}

typedef struct notifyArgs_s {
	int pattern;
	int clickCount;
	int clickBright;
	int r, g, b;
	int speedArg;
	int reps;
	int bright;
	int ambient;
	int ramp;
	int beams;
	int width;
	int tail;
	int onT, offT, rupT, rdnT; /* tenths for SolidFlash */
} notifyArgs_t;

void Notify_BeginEx(const notifyArgs_t *a) {
	int pattern, bright, ramp, clickCount, clickBright, reps;

	if (!a) return;
	pattern = a->pattern;

	if (pixel_count == 0 && pattern > 0) {
		ADDLOG_ERROR(LOG_FEATURE_CMD, "Notify: pixel_count=0 (SM16703P Init first)");
		return;
	}
	if (pattern <= 0) {
		if (g_beacon.active) {
			Notify_Finish();
		}
		return;
	}

	bright = a->bright;
	if (bright < 0) bright = 255;
	if (bright > 255) bright = 255;
	ramp = a->ramp ? 1 : 0;
	clickCount = Notify_ClampClick(a->clickCount);
	clickBright = Notify_ClampMainBright(a->clickBright > 0 ? a->clickBright : 100);

	reps = a->reps;
	if (a->ambient) {
		reps = 0x7fffffff;
	} else if (pattern == NOTIFY_PAT_SOLID) {
		/* hold until stop */
		reps = 0x7fffffff;
	} else if (reps < 1) {
		reps = 1;
	}

	/* SAVE first */
	g_beacon.saveWW = CHANNEL_Get(BEACON_CH_WW);
	g_beacon.saveCW = CHANNEL_Get(BEACON_CH_CW);
	g_beacon.saveR = CHANNEL_Get(BEACON_CH_R);
	g_beacon.saveG = CHANNEL_Get(BEACON_CH_G);
	g_beacon.saveB = CHANNEL_Get(BEACON_CH_B);

	g_beacon.colR = Notify_ClampByte(a->r);
	g_beacon.colG = Notify_ClampByte(a->g);
	g_beacon.colB = Notify_ClampByte(a->b);
	g_beacon.bright = bright;
	g_beacon.rampOn = ramp;
	g_beacon.rampAge = 0;
	g_beacon.clickCount = clickCount;
	g_beacon.clickBright = clickBright;
	g_beacon.clickTicks = BEACON_CLICK_TICKS_DEF;
	g_beacon.pattern = pattern;
	g_beacon.ambient = a->ambient ? 1 : 0;
	g_beacon.repeats = reps;
	g_beacon.revDone = 0;
	g_beacon.pos = 0.0f;
	g_beacon.prevPos = 0.0f;
	g_beacon.speed = (pattern == NOTIFY_PAT_SOLID || pattern == NOTIFY_PAT_SOLIDFLASH)
		? 0.0f : Notify_SpeedFromArg(a->speedArg);
	g_beacon.clickPhase = 0;
	g_beacon.clickStep = -1;
	g_beacon.active = 1;

	/* beams */
	if (pattern == NOTIFY_PAT_BEACON) {
		g_beacon.beams = 2;
		g_beacon.beamWidth = 1;
		g_beacon.tailLen = BEACON_TAIL;
	} else if (pattern == NOTIFY_PAT_BEACONX) {
		g_beacon.beams = a->beams > 0 ? a->beams : 2;
		g_beacon.beamWidth = a->width > 0 ? a->width : 1;
		g_beacon.tailLen = a->tail >= 0 ? a->tail : BEACON_TAIL;
		if (g_beacon.beams < 1) g_beacon.beams = 1;
	} else {
		g_beacon.beams = 0;
		g_beacon.tailLen = 0;
	}

	/* solid flash timing (tenths → ticks); ensure not all-zero cycle */
	g_beacon.flOn = TENTHS_TO_TICKS(a->onT > 0 ? a->onT : 3);
	g_beacon.flOff = TENTHS_TO_TICKS(a->offT >= 0 ? a->offT : 3);
	g_beacon.flRup = TENTHS_TO_TICKS(a->rupT);
	g_beacon.flRdn = TENTHS_TO_TICKS(a->rdnT);
	if (g_beacon.flOn <= 0 && g_beacon.flRup <= 0 && g_beacon.flRdn <= 0) {
		g_beacon.flOn = 4;
	}
	g_beacon.flPhase = 0;
	g_beacon.flAge = 0;
	g_beacon.flDone = 0;
	g_beacon.flLevel = 0.0f;

	g_speed = 0;
	if (g_beaconAnimIndex >= 0) {
		activeAnim = g_beaconAnimIndex;
		g_lightMode = Light_Anim;
		LED_SetEnableAll(true);
		MQTT_PublishMain_StringString_DeDuped(DEDUP_CURRENT_ANIM, DEDUP_EXPIRE_TIME,
			"currentAnim", a->ambient ? "Ambient" : "Notify", 0);
	} else {
		LED_SetEnableAll(true);
	}

	Beacon_WhitesHard(g_beacon.saveWW, g_beacon.saveCW);
	if (clickCount > 0) {
		Beacon_StartClickTrain();
	}

	ADDLOG_INFO(LOG_FEATURE_CMD,
		"Notify: pat=%i amb=%i click=%i@%i ramp=%i speed=%.2f reps=%i beams=%i tail=%i paint=%i,%i,%i save=%i/%i %i,%i,%i",
		pattern, g_beacon.ambient, clickCount, clickBright, ramp, g_beacon.speed, g_beacon.repeats,
		g_beacon.beams, g_beacon.tailLen,
		g_beacon.colR, g_beacon.colG, g_beacon.colB,
		g_beacon.saveWW, g_beacon.saveCW, g_beacon.saveR, g_beacon.saveG, g_beacon.saveB);
}

/* thin wrapper for stop / simple legacy */
void Notify_Begin(int pattern, int clickOn, int r, int g, int b,
	int speedArg, int reps, int bright, int ambient, int ramp) {
	notifyArgs_t a;
	memset(&a, 0, sizeof(a));
	a.pattern = pattern;
	a.clickCount = clickOn ? 2 : 0; /* legacy binary → 2 pulses */
	a.clickBright = 100;
	a.r = r; a.g = g; a.b = b;
	a.speedArg = speedArg;
	a.reps = reps;
	a.bright = bright;
	a.ambient = ambient;
	a.ramp = ramp;
	a.beams = 2;
	a.tail = BEACON_TAIL;
	a.onT = 3; a.offT = 3;
	Notify_BeginEx(&a);
}

void Beacon_Run(void) {
	int n, eb, paintB;
	float level;

	if (!g_beacon.active) {
		return;
	}
	n = (int)pixel_count;
	if (n <= 0) {
		Notify_Finish();
		return;
	}

	eb = Notify_EffectiveBright();
	level = 1.0f;

	if (g_beacon.pattern == NOTIFY_PAT_SOLIDFLASH) {
		SolidFlash_TickEnvelope();
		level = g_beacon.flLevel;
		if (!g_beacon.ambient && g_beacon.flDone >= g_beacon.repeats) {
			Notify_Finish();
			return;
		}
	}

	paintB = (int)((float)eb * level + 0.5f);
	if (paintB < 0) paintB = 0;
	if (paintB > 255) paintB = 255;

	Strip_setAllPixels(0, 0, 0, 0, 0);

	if (g_beacon.pattern == NOTIFY_PAT_BEACON || g_beacon.pattern == NOTIFY_PAT_BEACONX) {
		Beacon_DrawBeams(g_beacon.pos, n, g_beacon.beams, g_beacon.tailLen, paintB,
			g_beacon.colR, g_beacon.colG, g_beacon.colB);
	} else if (g_beacon.pattern == NOTIFY_PAT_SOLID || g_beacon.pattern == NOTIFY_PAT_SOLIDFLASH) {
		if (paintB > 0) {
			byte r = Beacon_Scale(g_beacon.colR, paintB, 1.0f);
			byte g = Beacon_Scale(g_beacon.colG, paintB, 1.0f);
			byte b = Beacon_Scale(g_beacon.colB, paintB, 1.0f);
			Strip_setAllPixels(r, g, b, 0, 0);
		}
	}
	Strip_Apply();

	Beacon_ClickTick();
	Beacon_HoldWhites();

	if (g_beacon.rampOn && g_beacon.rampAge < NOTIFY_RAMP_TICKS) {
		g_beacon.rampAge++;
	}

	/* motion only for beam patterns */
	if (g_beacon.pattern == NOTIFY_PAT_BEACON || g_beacon.pattern == NOTIFY_PAT_BEACONX) {
		g_beacon.prevPos = g_beacon.pos;
		g_beacon.pos += g_beacon.speed;
		if (g_beacon.pos >= (float)n) {
			g_beacon.pos -= (float)n;
			g_beacon.revDone++;
			if (!g_beacon.ambient && g_beacon.revDone >= g_beacon.repeats) {
				Notify_Finish();
				return;
			}
		}
	}
	/* SOLID holds until RingStop / new effect (repeats=max) */
}

/* Notify <pattern> <click> <r> <g> <b> ... pattern-specific */
commandResult_t PA_Cmd_Notify(const void *context, const char *cmd, const char *args, int flags) {
	const char *patStr;
	int pat, narg;
	notifyArgs_t a;

	(void)context; (void)cmd; (void)flags;
	memset(&a, 0, sizeof(a));
	Tokenizer_TokenizeString(args, 0);
	narg = Tokenizer_GetArgsCount();
	if (narg < 1) {
		return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	}
	patStr = Tokenizer_GetArg(0);
	pat = Notify_ParsePattern(patStr, Tokenizer_GetArgInteger(0));
	if (pat <= 0) {
		Notify_BeginEx(&a); /* pattern 0 → stop */
		return CMD_RES_OK;
	}
	a.pattern = pat;
	a.ambient = 0;
	a.clickBright = 100;
	a.bright = 255;
	a.beams = 2;
	a.tail = BEACON_TAIL;
	a.onT = 3;
	a.offT = 3;

	if (pat == NOTIFY_PAT_SOLID) {
		/* Solid <click> <r> <g> <b> [bright] [ramp] [clickBright] — via Notify Solid ... */
		if (narg < 5) return CMD_RES_NOT_ENOUGH_ARGUMENTS;
		a.clickCount = Tokenizer_GetArgInteger(1);
		a.r = Tokenizer_GetArgInteger(2);
		a.g = Tokenizer_GetArgInteger(3);
		a.b = Tokenizer_GetArgInteger(4);
		a.bright = (narg >= 6) ? Tokenizer_GetArgInteger(5) : 255;
		a.ramp = (narg >= 7) ? Tokenizer_GetArgInteger(6) : 0;
		a.clickBright = (narg >= 8) ? Tokenizer_GetArgInteger(7) : 100;
	} else if (pat == NOTIFY_PAT_SOLIDFLASH) {
		/* SolidFlash click r g b on off reps [bright] [rup] [rdn] [clickBright] */
		if (narg < 8) return CMD_RES_NOT_ENOUGH_ARGUMENTS;
		a.clickCount = Tokenizer_GetArgInteger(1);
		a.r = Tokenizer_GetArgInteger(2);
		a.g = Tokenizer_GetArgInteger(3);
		a.b = Tokenizer_GetArgInteger(4);
		a.onT = Tokenizer_GetArgInteger(5);
		a.offT = Tokenizer_GetArgInteger(6);
		a.reps = Tokenizer_GetArgInteger(7);
		a.bright = (narg >= 9) ? Tokenizer_GetArgInteger(8) : 255;
		a.rupT = (narg >= 10) ? Tokenizer_GetArgInteger(9) : 0;
		a.rdnT = (narg >= 11) ? Tokenizer_GetArgInteger(10) : 0;
		a.clickBright = (narg >= 12) ? Tokenizer_GetArgInteger(11) : 100;
	} else if (pat == NOTIFY_PAT_BEACONX) {
		/* BeaconX click r g b speed reps [bright] [ramp] [beams] [width] [tail] [clickBright] */
		if (narg < 7) return CMD_RES_NOT_ENOUGH_ARGUMENTS;
		a.clickCount = Tokenizer_GetArgInteger(1);
		a.r = Tokenizer_GetArgInteger(2);
		a.g = Tokenizer_GetArgInteger(3);
		a.b = Tokenizer_GetArgInteger(4);
		a.speedArg = Tokenizer_GetArgInteger(5);
		a.reps = Tokenizer_GetArgInteger(6);
		a.bright = (narg >= 8) ? Tokenizer_GetArgInteger(7) : 255;
		a.ramp = (narg >= 9) ? Tokenizer_GetArgInteger(8) : 0;
		a.beams = (narg >= 10) ? Tokenizer_GetArgInteger(9) : 2;
		a.width = (narg >= 11) ? Tokenizer_GetArgInteger(10) : 1;
		a.tail = (narg >= 12) ? Tokenizer_GetArgInteger(11) : BEACON_TAIL;
		a.clickBright = (narg >= 13) ? Tokenizer_GetArgInteger(12) : 100;
	} else {
		/* Beacon click r g b speed reps [bright] [ramp] [clickBright] */
		if (narg < 7) return CMD_RES_NOT_ENOUGH_ARGUMENTS;
		a.clickCount = Tokenizer_GetArgInteger(1);
		a.r = Tokenizer_GetArgInteger(2);
		a.g = Tokenizer_GetArgInteger(3);
		a.b = Tokenizer_GetArgInteger(4);
		a.speedArg = Tokenizer_GetArgInteger(5);
		a.reps = Tokenizer_GetArgInteger(6);
		a.bright = (narg >= 8) ? Tokenizer_GetArgInteger(7) : 255;
		a.ramp = (narg >= 9) ? Tokenizer_GetArgInteger(8) : 0;
		a.clickBright = (narg >= 10) ? Tokenizer_GetArgInteger(9) : 100;
	}
	Notify_BeginEx(&a);
	return CMD_RES_OK;
}

commandResult_t PA_Cmd_Ambient(const void *context, const char *cmd, const char *args, int flags) {
	const char *patStr;
	int pat, narg;
	notifyArgs_t a;

	(void)context; (void)cmd; (void)flags;
	memset(&a, 0, sizeof(a));
	Tokenizer_TokenizeString(args, 0);
	narg = Tokenizer_GetArgsCount();
	if (narg < 1) {
		return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	}
	patStr = Tokenizer_GetArg(0);
	if (patStr && (!stricmp(patStr, "stop") || !stricmp(patStr, "off"))) {
		Notify_BeginEx(&a);
		return CMD_RES_OK;
	}
	pat = Notify_ParsePattern(patStr, Tokenizer_GetArgInteger(0));
	/* BeaconX not allowed in Ambient */
	if (pat == NOTIFY_PAT_BEACONX) {
		ADDLOG_ERROR(LOG_FEATURE_CMD, "Ambient: BeaconX not allowed (Notify only)");
		return CMD_RES_BAD_ARGUMENT;
	}
	if (pat != NOTIFY_PAT_BEACON && pat != NOTIFY_PAT_SOLID && pat != NOTIFY_PAT_SOLIDFLASH) {
		return CMD_RES_BAD_ARGUMENT;
	}
	a.pattern = pat;
	a.ambient = 1;
	a.clickBright = 100;
	a.bright = 255;
	a.beams = 2;
	a.tail = BEACON_TAIL;
	a.onT = 3;
	a.offT = 3;

	if (pat == NOTIFY_PAT_SOLID) {
		/* Ambient Solid r g b [bright] [ramp] [click] [clickBright] */
		if (narg < 4) return CMD_RES_NOT_ENOUGH_ARGUMENTS;
		a.r = Tokenizer_GetArgInteger(1);
		a.g = Tokenizer_GetArgInteger(2);
		a.b = Tokenizer_GetArgInteger(3);
		a.bright = (narg >= 5) ? Tokenizer_GetArgInteger(4) : 255;
		a.ramp = (narg >= 6) ? Tokenizer_GetArgInteger(5) : 0;
		a.clickCount = (narg >= 7) ? Tokenizer_GetArgInteger(6) : 0;
		a.clickBright = (narg >= 8) ? Tokenizer_GetArgInteger(7) : 100;
	} else if (pat == NOTIFY_PAT_SOLIDFLASH) {
		/* Ambient SolidFlash r g b on off [bright] [rup] [rdn] [click] [clickBright] */
		if (narg < 6) return CMD_RES_NOT_ENOUGH_ARGUMENTS;
		a.r = Tokenizer_GetArgInteger(1);
		a.g = Tokenizer_GetArgInteger(2);
		a.b = Tokenizer_GetArgInteger(3);
		a.onT = Tokenizer_GetArgInteger(4);
		a.offT = Tokenizer_GetArgInteger(5);
		a.reps = 0x7fffffff;
		a.bright = (narg >= 7) ? Tokenizer_GetArgInteger(6) : 255;
		a.rupT = (narg >= 8) ? Tokenizer_GetArgInteger(7) : 0;
		a.rdnT = (narg >= 9) ? Tokenizer_GetArgInteger(8) : 0;
		a.clickCount = (narg >= 10) ? Tokenizer_GetArgInteger(9) : 0;
		a.clickBright = (narg >= 11) ? Tokenizer_GetArgInteger(10) : 100;
	} else {
		/* Ambient Beacon r g b speed [bright] [ramp] [click] [clickBright] */
		if (narg < 5) return CMD_RES_NOT_ENOUGH_ARGUMENTS;
		a.r = Tokenizer_GetArgInteger(1);
		a.g = Tokenizer_GetArgInteger(2);
		a.b = Tokenizer_GetArgInteger(3);
		a.speedArg = Tokenizer_GetArgInteger(4);
		a.bright = (narg >= 6) ? Tokenizer_GetArgInteger(5) : 255;
		a.ramp = (narg >= 7) ? Tokenizer_GetArgInteger(6) : 0;
		a.clickCount = (narg >= 8) ? Tokenizer_GetArgInteger(7) : 0;
		a.clickBright = (narg >= 9) ? Tokenizer_GetArgInteger(8) : 100;
	}
	Notify_BeginEx(&a);
	return CMD_RES_OK;
}

commandResult_t PA_Cmd_RingStop(const void *context, const char *cmd, const char *args, int flags) {
	notifyArgs_t a;
	(void)context; (void)cmd; (void)args; (void)flags;
	memset(&a, 0, sizeof(a));
	Notify_BeginEx(&a);
	return CMD_RES_OK;
}

/* Solid <click> <r> <g> <b> [bright] [ramp] [clickBright] */
commandResult_t PA_Cmd_Solid(const void *context, const char *cmd, const char *args, int flags) {
	notifyArgs_t a;
	int narg;
	(void)context; (void)cmd; (void)flags;
	memset(&a, 0, sizeof(a));
	Tokenizer_TokenizeString(args, 0);
	narg = Tokenizer_GetArgsCount();
	if (narg < 4) return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	a.pattern = NOTIFY_PAT_SOLID;
	a.clickCount = Tokenizer_GetArgInteger(0);
	a.r = Tokenizer_GetArgInteger(1);
	a.g = Tokenizer_GetArgInteger(2);
	a.b = Tokenizer_GetArgInteger(3);
	a.bright = (narg >= 5) ? Tokenizer_GetArgInteger(4) : 255;
	a.ramp = (narg >= 6) ? Tokenizer_GetArgInteger(5) : 0;
	a.clickBright = (narg >= 7) ? Tokenizer_GetArgInteger(6) : 100;
	Notify_BeginEx(&a);
	return CMD_RES_OK;
}

/* SolidFlash <click> <r> <g> <b> <on> <off> <reps> [bright] [rup] [rdn] [clickBright] */
commandResult_t PA_Cmd_SolidFlash(const void *context, const char *cmd, const char *args, int flags) {
	notifyArgs_t a;
	int narg;
	(void)context; (void)cmd; (void)flags;
	memset(&a, 0, sizeof(a));
	Tokenizer_TokenizeString(args, 0);
	narg = Tokenizer_GetArgsCount();
	if (narg < 7) return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	a.pattern = NOTIFY_PAT_SOLIDFLASH;
	a.clickCount = Tokenizer_GetArgInteger(0);
	a.r = Tokenizer_GetArgInteger(1);
	a.g = Tokenizer_GetArgInteger(2);
	a.b = Tokenizer_GetArgInteger(3);
	a.onT = Tokenizer_GetArgInteger(4);
	a.offT = Tokenizer_GetArgInteger(5);
	a.reps = Tokenizer_GetArgInteger(6);
	a.bright = (narg >= 8) ? Tokenizer_GetArgInteger(7) : 255;
	a.rupT = (narg >= 9) ? Tokenizer_GetArgInteger(8) : 0;
	a.rdnT = (narg >= 10) ? Tokenizer_GetArgInteger(9) : 0;
	a.clickBright = (narg >= 11) ? Tokenizer_GetArgInteger(10) : 100;
	Notify_BeginEx(&a);
	return CMD_RES_OK;
}

/* BeaconX <click> <r> <g> <b> <speed> <reps> [bright] [ramp] [beams] [width] [tail] [clickBright] */
commandResult_t PA_Cmd_BeaconX(const void *context, const char *cmd, const char *args, int flags) {
	notifyArgs_t a;
	int narg;
	(void)context; (void)cmd; (void)flags;
	memset(&a, 0, sizeof(a));
	Tokenizer_TokenizeString(args, 0);
	narg = Tokenizer_GetArgsCount();
	if (narg < 6) return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	a.pattern = NOTIFY_PAT_BEACONX;
	a.clickCount = Tokenizer_GetArgInteger(0);
	a.r = Tokenizer_GetArgInteger(1);
	a.g = Tokenizer_GetArgInteger(2);
	a.b = Tokenizer_GetArgInteger(3);
	a.speedArg = Tokenizer_GetArgInteger(4);
	a.reps = Tokenizer_GetArgInteger(5);
	a.bright = (narg >= 7) ? Tokenizer_GetArgInteger(6) : 255;
	a.ramp = (narg >= 8) ? Tokenizer_GetArgInteger(7) : 0;
	a.beams = (narg >= 9) ? Tokenizer_GetArgInteger(8) : 2;
	a.width = (narg >= 10) ? Tokenizer_GetArgInteger(9) : 1;
	a.tail = (narg >= 11) ? Tokenizer_GetArgInteger(10) : BEACON_TAIL;
	a.clickBright = (narg >= 12) ? Tokenizer_GetArgInteger(11) : 100;
	Notify_BeginEx(&a);
	return CMD_RES_OK;
}

/* Legacy: Beacon <anim> <bright> <clickOld> <reps> [R G B]
 * OR new-style if first arg is not small anim index: use Notify Beacon path via 7+ args */
commandResult_t PA_Cmd_Beacon(const void *context, const char *cmd, const char *args, int flags) {
	notifyArgs_t a;
	int narg, clickOld;

	(void)context; (void)cmd; (void)flags;
	memset(&a, 0, sizeof(a));
	Tokenizer_TokenizeString(args, 0);
	narg = Tokenizer_GetArgsCount();
	if (narg < 4) {
		return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	}
	/* New style: Beacon <click> <r> <g> <b> <speed> <reps> ... (6+ args, r is color) */
	if (narg >= 6 && Tokenizer_GetArgInteger(0) <= 10 && Tokenizer_GetArgInteger(1) <= 255
		&& Tokenizer_GetArgInteger(2) <= 255) {
		/* ambiguous with legacy — if arg0 is 0..10 and arg1 is 0..255 treat as new if narg>=6 and bright-like */
		/* Prefer legacy if arg0 is small anim index 1 and arg2 is clickOld 1/2/3 style */
	}
	/* Legacy path: anim bright clickOld reps [R G B] */
	a.pattern = NOTIFY_PAT_BEACON;
	a.bright = Tokenizer_GetArgInteger(1);
	clickOld = Tokenizer_GetArgInteger(2);
	a.reps = Tokenizer_GetArgInteger(3);
	a.clickCount = (clickOld >= 2) ? (clickOld == 2 ? 1 : 3) : 0;
	if (clickOld == 2) a.clickCount = 1;
	else if (clickOld >= 3) a.clickCount = 3;
	else a.clickCount = 0;
	a.clickBright = 100;
	a.r = 255; a.g = 100; a.b = 0;
	if (narg >= 7) {
		a.r = Tokenizer_GetArgInteger(4);
		a.g = Tokenizer_GetArgInteger(5);
		a.b = Tokenizer_GetArgInteger(6);
	}
	a.speedArg = 0;
	a.beams = 2;
	a.tail = BEACON_TAIL;
	Notify_BeginEx(&a);
	return CMD_RES_OK;
}

ledAnim_t g_anims[] = {
	{ "Rainbow Cycle", RainbowCycle_Run },
	{ "Fire", Fire_Run },
	{ "Shooting Star", ShootingStar_Run },
	{ "Comet", Comet_Run },
	{ "Theater Chase", TheaterChase_Run },
	{ "Theater Chase Rainbow", TheaterChaseRainbow_Run },
	{ "Beacon", Beacon_Run }
};
int g_numAnims = sizeof(g_anims) / sizeof(g_anims[0]);

void PixelAnim_SetAnim(int j)
{
	activeAnim = j;
	if(j >= 0)
	{
		g_lightMode = Light_Anim;
		if(CFG_HasFlag(OBK_FLAG_LED_AUTOENABLE_ON_ANY_ACTION))
		{
			LED_SetEnableAll(true);
		}
		apply_smart_light();
		MQTT_PublishMain_StringString_DeDuped(DEDUP_CURRENT_ANIM, DEDUP_EXPIRE_TIME, "currentAnim", g_anims[j].name, 0);
	}
	else
	{
		if (g_beacon.active) {
			Beacon_Restore();
		}
		MQTT_PublishMain_StringString_DeDuped(DEDUP_CURRENT_ANIM, DEDUP_EXPIRE_TIME, "currentAnim", "None", 0);
	}
}
commandResult_t PA_Cmd_Anim(const void *context, const char *cmd, const char *args, int flags) {

	Tokenizer_TokenizeString(args, 0);

	if (Tokenizer_GetArgsCount() == 0) {
		return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	}

	PixelAnim_SetAnim(Tokenizer_GetArgInteger(0));

	return CMD_RES_OK;
}
commandResult_t PA_Cmd_AnimSpeed(const void *context, const char *cmd, const char *args, int flags) {

	Tokenizer_TokenizeString(args, 0);

	if (Tokenizer_GetArgsCount() == 0) {
		return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	}

	g_speed = Tokenizer_GetArgInteger(0);

	return CMD_RES_OK;
}
void PixelAnim_Init() {
	/* index of "Beacon" entry — last in g_anims */
	g_beaconAnimIndex = g_numAnims - 1;

	//cmddetail:{"name":"Anim","args":"[AnimationIndex]",
	//cmddetail:"descr":"Starts given WS2812 animation by index.",
	//cmddetail:"fn":"PA_Cmd_Anim","file":"driver/drv_pixelAnim.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("Anim", PA_Cmd_Anim, NULL);
	//cmddetail:{"name":"AnimSpeed","args":"[Interval]",
	//cmddetail:"descr":"Sets WS2812 animation speed",
	//cmddetail:"fn":"PA_Cmd_AnimSpeed","file":"driver/drv_pixelAnim.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("AnimSpeed", PA_Cmd_AnimSpeed, NULL);
	//cmddetail:{"name":"Notify","args":"[pattern][click][r][g][b][speed][reps][bright?][ramp?]",
	//cmddetail:"descr":"Finite ring notify; ramp 0|1 default 0 (off). click 0|1. Clicks no ramp.",
	//cmddetail:"fn":"PA_Cmd_Notify","file":"driver/drv_pixelAnim.c","requires":"",
	//cmddetail:"examples":"Notify beacon 1 255 100 0 0 3 255 1"}
	CMD_RegisterCommand("Notify", PA_Cmd_Notify, NULL);
	//cmddetail:{"name":"Ambient","args":"[pattern][r][g][b][speed][bright?][ramp?]|stop",
	//cmddetail:"descr":"Background ring loop until Ambient stop / RingStop. ramp default 0.",
	//cmddetail:"fn":"PA_Cmd_Ambient","file":"driver/drv_pixelAnim.c","requires":"",
	//cmddetail:"examples":"Ambient beacon 255 100 0 0"}
	CMD_RegisterCommand("Ambient", PA_Cmd_Ambient, NULL);
	//cmddetail:{"name":"RingStop","args":"",
	//cmddetail:"descr":"Stop Notify/Ambient and restore saved main+halo.",
	//cmddetail:"fn":"PA_Cmd_RingStop","file":"driver/drv_pixelAnim.c","requires":"",
	//cmddetail:"examples":"RingStop"}
	CMD_RegisterCommand("RingStop", PA_Cmd_RingStop, NULL);
	//cmddetail:{"name":"Beacon","args":"legacy anim bright click reps [RGB]",
	//cmddetail:"descr":"Legacy 4-arg Beacon → plain Beacon pattern.",
	//cmddetail:"fn":"PA_Cmd_Beacon","file":"driver/drv_pixelAnim.c","requires":"",
	//cmddetail:"examples":"Beacon 1 200 2 3 255 100 0"}
	CMD_RegisterCommand("Beacon", PA_Cmd_Beacon, NULL);
	//cmddetail:{"name":"BeaconX","args":"[click][r][g][b][speed][reps]...",
	//cmddetail:"descr":"Extended beacon (beams/width/tail). Notify only, not Ambient.",
	//cmddetail:"fn":"PA_Cmd_BeaconX","file":"driver/drv_pixelAnim.c","requires":"",
	//cmddetail:"examples":"BeaconX 0 255 100 0 0 3 255 0 4 1 8"}
	CMD_RegisterCommand("BeaconX", PA_Cmd_BeaconX, NULL);
	//cmddetail:{"name":"Solid","args":"[click][r][g][b][bright?][ramp?][clickBright?]",
	//cmddetail:"descr":"Solid ring hold with save/restore + optional main clicks.",
	//cmddetail:"fn":"PA_Cmd_Solid","file":"driver/drv_pixelAnim.c","requires":"",
	//cmddetail:"examples":"Solid 0 0 255 0"}
	CMD_RegisterCommand("Solid", PA_Cmd_Solid, NULL);
	//cmddetail:{"name":"SolidFlash","args":"[click][r][g][b][on][off][reps]...",
	//cmddetail:"descr":"Solid flash/pulse. on/off/rup/rdn in tenths of a second.",
	//cmddetail:"fn":"PA_Cmd_SolidFlash","file":"driver/drv_pixelAnim.c","requires":"",
	//cmddetail:"examples":"SolidFlash 0 0 255 0 3 3 3"}
	CMD_RegisterCommand("SolidFlash", PA_Cmd_SolidFlash, NULL);
	//cmddetail:{"name":"Main","args":"[ww][cw][ramp?]",
	//cmddetail:"descr":"Set main whites ch1/2. ramp 0 instant (default), 1 ~0.5s.",
	//cmddetail:"fn":"PA_Cmd_Main","file":"driver/drv_pixelAnim.c","requires":"",
	//cmddetail:"examples":"Main 100 100 1"}
	CMD_RegisterCommand("Main", PA_Cmd_Main, NULL);
	//cmddetail:{"name":"Halo","args":"[r][g][b][ramp?]",
	//cmddetail:"descr":"Solid ring RGB without notify contract. ramp 0|1.",
	//cmddetail:"fn":"PA_Cmd_Halo","file":"driver/drv_pixelAnim.c","requires":"",
	//cmddetail:"examples":"Halo 255 0 0 1"}
	CMD_RegisterCommand("Halo", PA_Cmd_Halo, NULL);
}

void PixelAnim_CreatePanel(http_request_t *request) {
	const char* activeStr = "";
	char tmpA[16];
	int i;

	if (http_getArg(request->url, "an", tmpA, sizeof(tmpA))) {
		j = atoi(tmpA);
		hprintf255(request, "<h3>Ran %i!</h3>", (j));
		PixelAnim_SetAnim(j);
	}
	if (http_getArg(request->url, "spd", tmpA, sizeof(tmpA))) {
		j = atoi(tmpA);
		hprintf255(request, "<h3>Speed %i!</h3>", (j));
		g_speed = j;
	}

	poststr(request, "<tr><td>");
	hprintf255(request, "<h5>Speed</h5>");
	hprintf255(request, "<form action=\"index\">");
	hprintf255(request, "<input type=\"range\" min=\"0\" max=\"10\" name=\"spd\" id=\"spd\" value=\"%i\" onchange=\"this.form.submit()\">",
		g_speed);
	hprintf255(request, "<input  type=\"submit\" class='disp-none'></form>");
	poststr(request, "</td></tr>");

	if (g_lightMode == Light_Anim) {
		activeStr = "[ACTIVE]";
	}
	poststr(request, "<tr><td>");
	hprintf255(request, "<h5>LED Animation %s</h5>", activeStr);

	for (i = 0; i < g_numAnims; i++) {
		const char* c;
		if (i == activeAnim && g_lightMode == Light_Anim) {
			c = "bgrn";
		}
		else {
			c = "bred";
		}
		poststr(request, "<form action=\"index\">");
		hprintf255(request, "<input type=\"hidden\" name=\"an\" value=\"%i\">", i);
		hprintf255(request, "<input class=\"%s\" type=\"submit\" value=\"%s\"/></form>",
			c, g_anims[i].name);
	}
	poststr(request, "</td></tr>");
}
int g_ticks = 0;
void PixelAnim_SetAnimQuickTick() {
	/* Main/Halo ramps run even when not in Light_Anim (solid on/off UX) */
	LightRamp_Tick();

	if (g_lightEnableAll == 0) {
		// disabled
		return;
	}
	if (g_lightMode != Light_Anim) {
		if(activeAnim != -1) PixelAnim_SetAnim(-1);
		// disabled
		return;
	}
	if (activeAnim != -1) {
		g_ticks++;
		if (g_ticks >= g_speed) {
			g_anims[activeAnim].runFunc();
			g_ticks = 0;
		}
	}
}



//ENABLE_DRIVER_PIXELANIM
#endif

