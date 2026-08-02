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

/* ---- Cloudcutter Beacon (on-device, HA button only) ----
 * Spec: notes/ANIMATION_DESIGN.md
 * Beacon <anim> <bright> <click> <repeats>
 *  anim: mode (v1: 1 = dual-sector beacon)
 *  bright: 0-255 scale of base RGB
 *  click: 1=none 2=single 3=double/triple on main whites ch1/2
 *  repeats: full revolutions then self-stop + restore
 */
#ifndef BEACON_SECTOR_W
#define BEACON_SECTOR_W		12
#endif
#ifndef BEACON_TAIL
#define BEACON_TAIL			12
#endif
/* ~2 LED/tick equivalent with sub-pixel; tick = 25 ms */
#ifndef BEACON_SPEED_LED
#define BEACON_SPEED_LED	2.0f
#endif
#ifndef BEACON_CLICK_TICKS_DEF
#define BEACON_CLICK_TICKS_DEF	2
#endif
#define BEACON_CH_WW		1
#define BEACON_CH_CW		2
#define BEACON_CH_R		10
#define BEACON_CH_G		11
#define BEACON_CH_B		12
#define BEACON_SET_FLAGS	(CHANNEL_SET_FLAG_FORCE | CHANNEL_SET_FLAG_SILENT | CHANNEL_SET_FLAG_SKIP_MQTT)

typedef struct beaconState_s {
	int active;
	int bright;		/* 0-255 */
	int clickMode;		/* 1 none, 2 single, 3 double */
	int clickTicks;		/* duration of each on/off phase */
	int repeats;
	int revDone;
	float pos;		/* sub-pixel head position */
	float speed;		/* LED per tick */
	byte colR, colG, colB;
	int saveWW, saveCW;
	int saveR, saveG, saveB;
	int clickPhase;		/* 0 idle, >0 remaining ticks in current sub-phase */
	int clickStep;		/* which part of multi-click sequence */
	int seamLatched;	/* one click train per seam crossing per rev */
	float prevPos;
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

static void Beacon_WhitesHard(int ww, int cw) {
	CHANNEL_Set(BEACON_CH_WW, ww, BEACON_SET_FLAGS);
	CHANNEL_Set(BEACON_CH_CW, cw, BEACON_SET_FLAGS);
}

static void Beacon_Restore(void) {
	int n = (int)pixel_count;
	int i;

	g_beacon.active = 0;
	/* leave Light_Anim so stock tick stops calling us */
	g_lightMode = Light_RGB;

	if (n > 0) {
		for (i = 0; i < n; i++) {
			Strip_setPixel(i, g_beacon.saveR, g_beacon.saveG, g_beacon.saveB, 0, 0);
		}
		Strip_Apply();
	}

	CHANNEL_Set(BEACON_CH_R, g_beacon.saveR, BEACON_SET_FLAGS);
	CHANNEL_Set(BEACON_CH_G, g_beacon.saveG, BEACON_SET_FLAGS);
	CHANNEL_Set(BEACON_CH_B, g_beacon.saveB, BEACON_SET_FLAGS);
	Beacon_WhitesHard(g_beacon.saveWW, g_beacon.saveCW);

	ADDLOG_INFO(LOG_FEATURE_CMD, "Beacon: restore WW=%i CW=%i RGB=%i,%i,%i",
		g_beacon.saveWW, g_beacon.saveCW, g_beacon.saveR, g_beacon.saveG, g_beacon.saveB);
}

static void Beacon_StartClickTrain(void) {
	if (g_beacon.clickMode <= 1) return;
	g_beacon.clickStep = 0;
	g_beacon.clickPhase = g_beacon.clickTicks;
	/* step 0 = first ON */
	Beacon_WhitesHard(100, 100);
}

static void Beacon_ClickTick(void) {
	int pulses;

	if (g_beacon.clickMode <= 1) return;
	if (g_beacon.clickPhase <= 0 && g_beacon.clickStep < 0) return;
	if (g_beacon.clickPhase <= 0) return;

	g_beacon.clickPhase--;
	if (g_beacon.clickPhase > 0) return;

	/* end of current sub-phase */
	pulses = (g_beacon.clickMode == 2) ? 1 : 3; /* 2=single, 3=triple-ish */
	/* sequence: ON, OFF, ON, OFF, ... for `pulses` ONs */
	g_beacon.clickStep++;
	if (g_beacon.clickStep >= pulses * 2 - 1) {
		/* done — restore whites */
		Beacon_WhitesHard(g_beacon.saveWW, g_beacon.saveCW);
		g_beacon.clickStep = -1;
		g_beacon.clickPhase = 0;
		return;
	}
	if ((g_beacon.clickStep & 1) == 0) {
		/* even: ON */
		Beacon_WhitesHard(100, 100);
	} else {
		Beacon_WhitesHard(g_beacon.saveWW, g_beacon.saveCW);
	}
	g_beacon.clickPhase = g_beacon.clickTicks;
}

void Beacon_Run(void) {
	int n;
	float half;
	float crossed;

	if (!g_beacon.active) {
		return;
	}
	n = (int)pixel_count;
	if (n <= 0) {
		Beacon_Restore();
		activeAnim = -1;
		return;
	}

	half = (float)n * 0.5f;

	/* clear frame then draw two opposite sectors */
	Strip_setAllPixels(0, 0, 0, 0, 0);
	Beacon_DrawSector(g_beacon.pos, n, g_beacon.colR, g_beacon.colG, g_beacon.colB, g_beacon.bright);
	Beacon_DrawSector(g_beacon.pos + half, n, g_beacon.colR, g_beacon.colG, g_beacon.colB, g_beacon.bright);
	Strip_Apply();

	/* seam click: head crossing index 0 (mod n) */
	if (g_beacon.clickMode > 1) {
		float p0 = g_beacon.prevPos;
		float p1 = g_beacon.pos;
		/* unwrap */
		while (p1 < p0) p1 += (float)n;
		/* crossed integer multiple of n (seam) */
		crossed = floorf(p0 / (float)n) != floorf(p1 / (float)n);
		if (crossed && !g_beacon.seamLatched) {
			Beacon_StartClickTrain();
			g_beacon.seamLatched = 1;
		}
		/* clear latch once well past seam */
		if (fmodf(g_beacon.pos, (float)n) > 2.0f) {
			g_beacon.seamLatched = 0;
		}
	}
	Beacon_ClickTick();

	g_beacon.prevPos = g_beacon.pos;
	g_beacon.pos += g_beacon.speed;
	if (g_beacon.pos >= (float)n) {
		g_beacon.pos -= (float)n;
		g_beacon.revDone++;
		if (g_beacon.revDone >= g_beacon.repeats) {
			Beacon_Restore();
			activeAnim = -1;
			MQTT_PublishMain_StringString_DeDuped(DEDUP_CURRENT_ANIM, DEDUP_EXPIRE_TIME, "currentAnim", "None", 0);
			return;
		}
	}
}

void Beacon_Begin(int anim, int bright, int clickMode, int repeats) {
	(void)anim; /* v1: single beacon mode */

	if (pixel_count == 0) {
		ADDLOG_ERROR(LOG_FEATURE_CMD, "Beacon: pixel_count=0 (start SM16703P + Init first)");
		return;
	}
	if (bright < 0) bright = 0;
	if (bright > 255) bright = 255;
	if (clickMode < 1) clickMode = 1;
	if (clickMode > 3) clickMode = 3;
	if (repeats < 1) repeats = 1;

	g_beacon.saveWW = CHANNEL_Get(BEACON_CH_WW);
	g_beacon.saveCW = CHANNEL_Get(BEACON_CH_CW);
	g_beacon.saveR = CHANNEL_Get(BEACON_CH_R);
	g_beacon.saveG = CHANNEL_Get(BEACON_CH_G);
	g_beacon.saveB = CHANNEL_Get(BEACON_CH_B);
	/* if halo channels empty, fall back to led_baseColors */
	if (g_beacon.saveR == 0 && g_beacon.saveG == 0 && g_beacon.saveB == 0) {
		g_beacon.colR = (byte)led_baseColors[0];
		g_beacon.colG = (byte)led_baseColors[1];
		g_beacon.colB = (byte)led_baseColors[2];
	} else {
		g_beacon.colR = (byte)g_beacon.saveR;
		g_beacon.colG = (byte)g_beacon.saveG;
		g_beacon.colB = (byte)g_beacon.saveB;
	}

	g_beacon.bright = bright;
	g_beacon.clickMode = clickMode;
	g_beacon.clickTicks = BEACON_CLICK_TICKS_DEF;
	g_beacon.repeats = repeats;
	g_beacon.revDone = 0;
	g_beacon.pos = 0.0f;
	g_beacon.prevPos = 0.0f;
	g_beacon.speed = BEACON_SPEED_LED;
	g_beacon.clickPhase = 0;
	g_beacon.clickStep = -1;
	g_beacon.seamLatched = 0;
	g_beacon.active = 1;

	/* own cadence: every quick-tick */
	g_speed = 0;
	if (g_beaconAnimIndex >= 0) {
		PixelAnim_SetAnim(g_beaconAnimIndex);
	}
	LED_SetEnableAll(true);

	ADDLOG_INFO(LOG_FEATURE_CMD,
		"Beacon: start bright=%i click=%i ticks=%i reps=%i n=%i RGB=%i,%i,%i savedWW/CW=%i/%i",
		bright, clickMode, g_beacon.clickTicks, repeats, (int)pixel_count,
		g_beacon.colR, g_beacon.colG, g_beacon.colB, g_beacon.saveWW, g_beacon.saveCW);
}

commandResult_t PA_Cmd_Beacon(const void *context, const char *cmd, const char *args, int flags) {
	int anim, bright, clickMode, repeats;

	Tokenizer_TokenizeString(args, 0);
	if (Tokenizer_GetArgsCount() < 4) {
		return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	}
	anim = Tokenizer_GetArgInteger(0);
	bright = Tokenizer_GetArgInteger(1);
	clickMode = Tokenizer_GetArgInteger(2);
	repeats = Tokenizer_GetArgInteger(3);
	Beacon_Begin(anim, bright, clickMode, repeats);
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
	//cmddetail:{"name":"Beacon","args":"[anim][bright][click][repeats]",
	//cmddetail:"descr":"On-device dual-sector ring beacon with optional white click; self-stop and restore.",
	//cmddetail:"fn":"PA_Cmd_Beacon","file":"driver/drv_pixelAnim.c","requires":"",
	//cmddetail:"examples":"Beacon 1 64 2 3"}
	CMD_RegisterCommand("Beacon", PA_Cmd_Beacon, NULL);
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

