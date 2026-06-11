// ============================================================
// NoiseGen1 — v1.5
// Generative noise synthesizer with audio-input mangler
//
// Platform : Electro-Smith Daisy Seed on Synthux Simple Fix
// Framework: DaisyDuino (Arduino)
// License  : MIT
//
// OVERVIEW
//   NoiseGen1 is a noise instrument with eleven patches, selected
//   with the toggle switch. Each patch runs its own DSP topology,
//   a genuinely different signal path rather than a parameter
//   variation, so the unit behaves like a collection of distinct
//   noise machines in one box.
//
// PATCHES (the on-board LED blinks N times = patch N)
//    1 CLASSICO   cascaded-FM noise core of v1.0, untouched, with
//                 the lightest possible output leveling
//    2 ABISSO     continuous subterranean rumble: deep sub drone
//                 fused with low-passed ground noise, no hits
//    3 CREPITIO   dry electric sparks: random crackle bursts with
//                 1-20 ms tails, silence in between
//    4 MITRAGLIA  noise wall fired in irregular bursts: random
//                 gate lengths with sudden ratchet strobes
//    5 RADIO      gritty shortwave: clipped carrier, crushed
//                 static, output decimation; the static takes
//                 over the transmission in random surges
//    6 PRESSA     heavy industrial grinder: beating square pair
//                 driven hard, gritty surface, slow heaving
//    7 LAMIERA    continuous metal scrape: slowly gliding
//                 inharmonic ring pair dragged by noise
//    8 SOLO-IN    extreme input destruction: a second mangling
//                 stage (drive, double fold, deep decimation) on
//                 top of the normal chain; silence with no input
//    9 MURO       harsh noise wall with life inside: crackling
//                 embers and sudden lurches of the low-pass
//   10 MAGMA      low FM boiling with resonant blop pings and a
//                 layer of frying noise on the surface
//   11 SCULTURA   cut-up collage of crushed digital garbage,
//                 clipped resonant screeches, metallic ring stabs
//                 and silence, hard-switched in random segments
//
// CONTROLS
//   POT 1 (A0)   DENSITY  — event rate, chaos amount, chop/cut speed
//   POT 2 (A1)   REGISTER — base frequency range of the active patch
//   POT 3 (A2)   INPUT    — input destruction amount and wet mix
//   SWITCH (D18)          — every flip advances to the next patch
//
// OUTPUT LEVELING
//   A slow RMS leveler keeps the perceived output volume constant
//   across patches and pot positions (gain range x0.35 to x2.5,
//   frozen below a silence threshold). Each patch sets its own
//   target so dynamic patches keep their character.
//
// AUDIO
//   IN : 3.5 mm mono — gated, boosted, mangled and recombined
//   OUT: 3.5 mm stereo
// ============================================================

#include "DaisyDuino.h"

// ── Patch table ──────────────────────────────────────────────
// The patch index IS the synthesis mode: the audio callback runs a
// different DSP path for each patch. The fields below only set the
// colors of that path: waveforms, transposition, glide, fold, tone.
struct NoisePatch {
    const char* name;
    uint8_t wave1, wave2, wave3;        // oscillator waveforms
    float fbScale, fmScale, cascScale, ringScale; // CLASSICO chaos scaling
    float foldBase, foldRange;          // wavefolder gain = base + DENSITY * range
    float freqMul1, freqMul2, freqMul3; // direct transposition of the 3 oscillators
    float freqSmooth;                   // glide on random retunes (1.0 = instant jump)
    float regBias;                      // REGISTER offset (negative = darker)
    float tempoScale;                   // random-stream speed (below 1 = faster)
    float resBias;                      // SVF resonance offset
    float staticMix;                    // constant crushed-static bed in the output
    float toneFreq;                     // final low-pass color
    float lvlTarget;                    // auto-level target (higher = louder, lighter touch)
    float coreMix;                      // generator level in the mix (0 = muted)
    float inPotLvl;                     // how much POT 3 brings the input in
    float inFloor;                      // fixed input level (SOLO-IN: 1.0)
    float lpL, bpL, hpL;                // CLASSICO filter mix, left channel
    float lpR, bpR, hpR;                // CLASSICO filter mix, right channel
};

#define W_SIN (uint8_t)Oscillator::WAVE_SIN
#define W_TRI (uint8_t)Oscillator::WAVE_TRI
#define W_SQR (uint8_t)Oscillator::WAVE_SQUARE
#define W_SAW (uint8_t)Oscillator::WAVE_SAW

#define NUM_PATCHES 11
const NoisePatch patches[NUM_PATCHES] = {
    // Patch 1 reproduces the v1.0 sound; its leveler target is high so the
    // correction barely touches it.
    //  name        w1     w2     w3     fb    fm    casc  ring  foldB foldR  mul1   mul2   mul3   smooth  regB   tempo  resB   stat   tone     lvl    core  inPot inFlr  lpL   bpL   hpL    lpR   bpR   hpR
    {"CLASSICO",  W_SIN, W_SIN, W_SIN, 1.00f,1.00f,1.00f,1.00f, 2.0f, 4.8f, 1.00f, 1.00f, 1.00f, 0.020f,  0.00f, 1.00f, 0.00f, 0.00f, 13500.f, 0.92f, 1.0f, 1.0f, 0.0f, 0.55f,0.45f,0.00f, 0.00f,0.40f,0.60f},
    {"ABISSO",    W_SIN, W_SIN, W_SIN, 1.00f,1.00f,1.00f,1.00f, 2.2f, 3.0f, 0.15f, 0.08f, 0.05f, 0.010f, -0.20f, 2.50f,-0.10f, 0.00f,  2500.f, 0.75f, 1.0f, 1.0f, 0.0f, 1.00f,0.00f,0.00f, 0.85f,0.15f,0.00f},
    {"CREPITIO",  W_SIN, W_SIN, W_TRI, 1.00f,1.00f,1.00f,1.00f, 1.4f, 1.6f, 1.00f, 1.20f, 1.50f, 0.050f,  0.10f, 0.70f, 0.00f, 0.00f, 13500.f, 0.70f, 1.0f, 1.0f, 0.0f, 0.35f,0.45f,0.20f, 0.10f,0.45f,0.45f},
    {"MITRAGLIA", W_TRI, W_TRI, W_SIN, 1.00f,1.20f,1.00f,1.00f, 2.4f, 4.0f, 1.00f, 1.00f, 1.00f, 0.020f,  0.00f, 0.50f, 0.00f, 0.00f, 13500.f, 0.72f, 1.0f, 1.0f, 0.0f, 0.20f,0.55f,0.25f, 0.00f,0.45f,0.55f},
    {"RADIO",     W_SIN, W_SIN, W_SQR, 1.00f,1.00f,1.00f,1.00f, 2.0f, 2.5f, 1.00f, 1.30f, 1.60f, 1.000f,  0.30f, 0.22f, 0.00f, 0.00f, 12000.f, 0.72f, 1.0f, 1.0f, 0.0f, 0.05f,0.45f,0.50f, 0.00f,0.35f,0.65f},
    {"PRESSA",    W_SQR, W_SQR, W_TRI, 1.00f,1.00f,1.00f,1.00f, 2.6f, 3.0f, 1.00f, 1.00f, 1.00f, 0.020f, -0.10f, 1.40f, 0.00f, 0.00f,  6000.f, 0.75f, 1.0f, 1.0f, 0.0f, 0.30f,0.50f,0.20f, 0.10f,0.45f,0.45f},
    {"LAMIERA",   W_TRI, W_SQR, W_SQR, 1.00f,1.00f,1.00f,1.00f, 1.8f, 2.0f, 1.00f, 1.00f, 1.00f, 0.004f,  0.00f, 1.20f, 0.05f, 0.00f,  8000.f, 0.72f, 1.0f, 1.0f, 0.0f, 0.30f,0.50f,0.20f, 0.10f,0.45f,0.45f},
    {"SOLO-IN",   W_SIN, W_SIN, W_SIN, 1.00f,1.00f,1.00f,1.00f, 3.0f, 5.5f, 1.00f, 1.00f, 1.00f, 0.020f,  0.00f, 1.00f, 0.00f, 0.00f, 11000.f, 0.72f, 0.0f, 0.0f, 1.0f, 0.55f,0.45f,0.00f, 0.00f,0.40f,0.60f},
    {"MURO",      W_SAW, W_SAW, W_TRI, 1.00f,1.00f,1.00f,1.00f, 2.4f, 2.6f, 0.50f, 0.35f, 0.20f, 0.015f, -0.25f, 1.60f,-0.05f, 0.40f,  3800.f, 0.75f, 1.0f, 1.0f, 0.0f, 0.80f,0.20f,0.00f, 0.65f,0.35f,0.00f},
    {"MAGMA",     W_SIN, W_SIN, W_TRI, 1.00f,1.00f,1.00f,1.00f, 3.0f, 4.0f, 0.70f, 0.50f, 0.35f, 0.030f, -0.10f, 0.90f, 0.05f, 0.05f,  4500.f, 0.72f, 1.0f, 1.0f, 0.0f, 0.60f,0.40f,0.00f, 0.40f,0.50f,0.10f},
    {"SCULTURA",  W_SIN, W_SQR, W_SQR, 1.00f,1.00f,1.00f,1.00f, 2.0f, 3.0f, 0.80f, 0.80f, 0.80f, 0.040f, -0.05f, 0.70f, 0.22f, 0.00f, 11000.f, 0.70f, 1.0f, 1.0f, 0.0f, 0.45f,0.45f,0.10f, 0.10f,0.45f,0.45f},
};

DaisyHardware hw;
float sample_rate;

// ── Oscillators, noise source, filter, LFOs ──────────────────
Oscillator osc1, osc2, osc3;
WhiteNoise noiseGen;
Svf        svfMain;
Oscillator lfo1, lfo2, lfo3, lfo4;

// ── Output conditioning: DC blocker + gentle low-pass ────────
DcBlock dcL, dcR;
Tone    toneL, toneR;

inline float wavefold(float x, float gain) { return sinf(x * gain * 1.5708f); }

static uint32_t lfsr = 0xDEADBEEFu;
inline uint32_t lfsrNext() { lfsr ^= lfsr << 13; lfsr ^= lfsr >> 17; lfsr ^= lfsr << 5; return lfsr; }
inline float randF() { return (lfsrNext() & 0x7FFFFFu) / (float)0x800000u; }

// Separate RNG for the audio callback (never share state with loop())
static uint32_t lfsrCb = 0xC0FFEE21u;
inline uint32_t lfsrCbNext() { lfsrCb ^= lfsrCb << 13; lfsrCb ^= lfsrCb >> 17; lfsrCb ^= lfsrCb << 5; return lfsrCb; }
inline float randCbF() { return (lfsrCbNext() & 0x7FFFFFu) / (float)0x800000u; }

float pot[3]       = {0.5f, 0.5f, 0.5f};
const float POT_SP = 0.004f;

// ── Control values shared between loop() and the audio thread ─
static volatile float vFreq1 = 40.0f,  vFreq2 = 700.0f, vFreq3 = 3000.0f;
static volatile float vFb1 = 0.32f, vFb2 = 0.38f, vFb3 = 0.42f;
static volatile float vFmIdx1 = 400.0f, vFmIdx2 = 700.0f, vFmIdx3 = 1200.0f;
static volatile float vCasc12 = 0.55f, vCasc23 = 0.60f;
static volatile float vSvfFreq = 800.0f, vSvfRes = 0.81f;
static volatile float vRingBase = 0.35f;
static volatile float vDensity = 0.5f, vRegister = 0.5f, vInMangle = 0.0f;
static volatile int   vPatch = 0;   // current patch, changed in loop()
// Input destruction parameters (precomputed in loop())
static volatile float vCrushSteps = 65536.0f;
static volatile int   vInDecim    = 1;

// ── Input path state ─────────────────────────────────────────
static float inHold   = 0.0f;     // decimator hold
static int   inDecCnt = 0;
static float inEnv    = 0.0f;     // envelope follower
static float gateGain = 0.0f;     // smoothed gate gain (no clicks)
static bool  gateOpen = false;

// Noise gate tuning
#define GATE_OPEN_LVL   0.015f    // opens above this input level
#define GATE_CLOSE_LVL  0.006f    // closes below this level (hysteresis)
#define GATE_ATTACK     0.0050f   // ~4 ms fade-in
#define GATE_RELEASE    0.00025f  // ~80 ms fade-out
#define IN_BOOST        2.5f      // input makeup gain after the gate

#define DEBUG_MODE false

// ══════════════════════════════════════════════════════════════
// AUDIO CALLBACK
// ══════════════════════════════════════════════════════════════
void AudioCallback(float **in, float **out, size_t size) {
    float fb1 = vFb1, fb2 = vFb2, fb3 = vFb3;
    float fmIdx1 = vFmIdx1, fmIdx2 = vFmIdx2, fmIdx3 = vFmIdx3;
    float casc12 = vCasc12, casc23 = vCasc23;
    float svfRes = vSvfRes;
    float ringBase = vRingBase;
    float density = vDensity;
    float inMangle = vInMangle;
    float crushSteps = vCrushSteps;
    int   inDecim = vInDecim < 1 ? 1 : vInDecim;
    const NoisePatch &P = patches[vPatch];
    int   mode = vPatch;            // each patch runs its own DSP path

    // CLASSICO chaos scaling and safety clamps
    fb1 *= P.fbScale; fb2 *= P.fbScale; fb3 *= P.fbScale;
    if (fb1 > 0.50f) fb1 = 0.50f; if (fb2 > 0.50f) fb2 = 0.50f; if (fb3 > 0.50f) fb3 = 0.50f;
    fmIdx1 *= P.fmScale; fmIdx2 *= P.fmScale; fmIdx3 *= P.fmScale;
    casc12 *= P.cascScale; casc23 *= P.cascScale;
    if (casc12 > 0.90f) casc12 = 0.90f; if (casc23 > 0.90f) casc23 = 0.90f;
    ringBase *= P.ringScale;
    svfRes += P.resBias;

    if (fmIdx1 > 2500.0f) fmIdx1 = 2500.0f;
    if (fmIdx2 > 3500.0f) fmIdx2 = 3500.0f;
    if (fmIdx3 > 5000.0f) fmIdx3 = 5000.0f;
    if (svfRes > 0.95f)   svfRes = 0.95f;
    if (svfRes < 0.30f)   svfRes = 0.30f;

    float foldG1 = P.foldBase + density * P.foldRange;
    float foldG2 = foldG1 * 0.40f;
    float foldG3 = foldG2 * 0.35f;

    static float svfCurr = 800.0f;
    svfCurr += 0.014f * (vSvfFreq - svfCurr);
    if (svfCurr < 20.0f) svfCurr = 20.0f; if (svfCurr > 12000.0f) svfCurr = 12000.0f;
    svfMain.SetRes(svfRes);

    static float lv1 = 0, lv2 = 0, lv3 = 0, lv4 = 0;
    static float prev1 = 0, prev2 = 0, prev3 = 0;
    // Smoothed base frequencies (random retunes glide per patch)
    static float f1Sm = 40.0f, f2Sm = 700.0f, f3Sm = 3000.0f;
    static float patchGate = 1.0f;  // segment gate (MITRAGLIA, SCULTURA)
    static float sparkEnv  = 0.0f;  // spark envelope (CREPITIO)
    static float sparkDec  = 0.995f;// spark decay of the current burst
    static int   gunCnt    = 0;     // samples left in the current burst (MITRAGLIA)
    static bool  gunOn     = true;  // current burst state (MITRAGLIA)
    static float burstEnv  = 0.0f;  // static-surge envelope (RADIO)
    static float radioHold = 0.0f;  // output decimator hold (RADIO)
    static int   radioCnt  = 0;
    static float emberEnv  = 0.0f;  // ember crackle envelope (MURO)
    static float muroCut   = 300.0f;// current low-pass position (MURO)
    static float soloHold  = 0.0f;  // extra decimator hold (SOLO-IN)
    static int   soloCnt   = 0;
    static int   cutCnt    = 0;     // samples left in the current segment (SCULTURA)
    static uint32_t segTex = 0;     // texture of the current segment (SCULTURA)
    static float cutLvl    = 1.0f;  // level of the current segment (SCULTURA)
    static float segF      = 800.0f;// metal-stab pitch of the segment (SCULTURA)
    static float garbHold  = 0.0f;  // digital-garbage sample hold (SCULTURA)
    static int   garbCnt   = 0;
    static int   garbN     = 60;    // garbage hold period = pitch (SCULTURA)
    static float lvlEnv    = 0.3f;  // auto-level amplitude follower
    static float lvlGain   = 1.0f;  // auto-level correction gain

    float tFreq = P.toneFreq;       // Tone::SetFreq requires an lvalue
    toneL.SetFreq(tFreq);
    toneR.SetFreq(tFreq);

    for (size_t s = 0; s < size; s++) {
        lv1 = lfo1.Process(); lv2 = lfo2.Process(); lv3 = lfo3.Process(); lv4 = lfo4.Process();

        // Per-patch transposition and glide of the random base frequencies
        f1Sm += P.freqSmooth * (vFreq1 * P.freqMul1 - f1Sm);
        f2Sm += P.freqSmooth * (vFreq2 * P.freqMul2 - f2Sm);
        f3Sm += P.freqSmooth * (vFreq3 * P.freqMul3 - f3Sm);

        // Wandering filter cutoff, shared by the modes that use it
        float svfLive = svfCurr * (1.0f + lv3 * 0.15f);
        if (svfLive < 20.0f) svfLive = 20.0f; if (svfLive > 12000.0f) svfLive = 12000.0f;

        float noise = noiseGen.Process();

        // ── INPUT: noise gate → boost → destruction ────────────
        float ainRaw = (in[0][s] + in[1][s]) * 0.5f;

        // envelope follower (fast attack, slow decay)
        float lvl = fabsf(ainRaw);
        inEnv += (lvl > inEnv ? 0.05f : 0.0005f) * (lvl - inEnv);
        // gate with hysteresis: opens on signal, closes on silence
        if (inEnv > GATE_OPEN_LVL)       gateOpen = true;
        else if (inEnv < GATE_CLOSE_LVL) gateOpen = false;
        gateGain += (gateOpen ? GATE_ATTACK : GATE_RELEASE)
                  * ((gateOpen ? 1.0f : 0.0f) - gateGain);

        float ain = ainRaw * gateGain * IN_BOOST;             // gated + boosted
        ain = tanhf(ain * (1.0f + inMangle * 12.0f));          // heavy overdrive
        ain = wavefold(ain, 1.0f + inMangle * 3.0f);           // wavefolding
        ain = floorf(ain * crushSteps) / crushSteps;           // bit reduction
        if (--inDecCnt <= 0) { inHold = ain; inDecCnt = inDecim; }
        ain = inHold;                                          // decimation
        float inFM = ain * inMangle;   // how much the input deforms the core

        // ── CORE: one DSP topology per patch ───────────────────
        float coreL = 0.0f, coreR = 0.0f, sig3 = 0.0f;

        switch (mode) {

        case 0: { // CLASSICO — the v1.0 cascaded-FM noise core, untouched
            float fmIdx1Live = fmIdx1 * (1.0f + lv1 * 0.30f);
            float casc12Live = casc12 * (1.0f + lv2 * 0.20f);
            float casc23Live = casc23 * (1.0f + lv2 * 0.18f);
            svfMain.SetFreq(svfLive);
            float ringLive = ringBase * (1.0f + lv4 * 0.40f);
            if (ringLive < 0.05f) ringLive = 0.05f; if (ringLive > 0.80f) ringLive = 0.80f;

            float fmSrc1 = noise * 0.70f + prev1 * fb1 + inFM * 0.8f;
            float freq1 = f1Sm + fmSrc1 * fmIdx1Live;
            if (freq1 < 6.0f) freq1 = 6.0f; if (freq1 > 5000.0f) freq1 = 5000.0f;
            osc1.SetFreq(freq1);
            float sig1 = osc1.Process(); prev1 = sig1;

            float fmSrc2 = sig1 * casc12Live + noise * 0.35f + prev2 * fb2 + inFM * 0.65f;
            float freq2 = f2Sm + fmSrc2 * fmIdx2;
            if (freq2 < 15.0f) freq2 = 15.0f; if (freq2 > 9000.0f) freq2 = 9000.0f;
            osc2.SetFreq(freq2);
            float sig2 = osc2.Process(); prev2 = sig2;

            float fmSrc3 = sig2 * casc23Live + noise * 0.25f + prev3 * fb3 + inFM * 0.55f;
            float freq3 = f3Sm + fmSrc3 * fmIdx3;
            if (freq3 < 30.0f) freq3 = 30.0f; if (freq3 > 13000.0f) freq3 = 13000.0f;
            osc3.SetFreq(freq3);
            sig3 = osc3.Process(); prev3 = sig3;

            float mixL = sig1 * 0.30f + sig2 * 0.35f + sig3 * 0.35f;
            float mixR = sig1 * 0.25f + sig2 * 0.30f + sig3 * 0.45f;

            svfMain.Process(mixL);
            coreL = svfMain.Low() * P.lpL + svfMain.Band() * P.bpL + svfMain.High() * P.hpL;
            svfMain.Process(mixR);
            coreR = svfMain.Low() * P.lpR + svfMain.Band() * P.bpR + svfMain.High() * P.hpR;

            // pulsing ring modulation against the noise source
            coreL = coreL * (1.0f - ringLive) + coreL * noise * ringLive;
            coreR = coreR * (1.0f - ringLive) + coreR * noise * ringLive;
            break;
        }

        case 1: { // ABISSO — continuous subterranean rumble, no hits
            float bf = f1Sm;
            if (bf < 12.0f) bf = 12.0f; if (bf > 45.0f) bf = 45.0f;
            osc1.SetFreq(bf);
            float sub = osc1.Process();
            osc2.SetFreq(bf * 1.50f);
            float sub2 = osc2.Process();
            // ground noise: low-passed rumble following the stepped stream
            // (stepped random targets, not an LFO sweep)
            svfMain.SetFreq(60.0f + svfLive * 0.015f);
            svfMain.Process(noise);
            float rumble = svfMain.Low();
            float breathe = 0.85f + lv1 * 0.15f;   // slow tidal swell
            coreL = (sub * 0.55f + sub2 * 0.20f + rumble * 1.5f) * breathe;
            coreR = (sub * 0.50f + sub2 * 0.22f + rumble * 1.4f) * breathe;
            break;
        }

        case 2: { // CREPITIO — dry electric sparks, no resonant ping
            // 2 to 68 sparks per second on DENSITY
            uint32_t thr = 3u + (uint32_t)(density * 90.0f);
            float imp = 0.0f;
            if ((lfsrCbNext() & 0xFFFFu) < thr) {
                imp = randCbF() * 2.0f - 1.0f;
                sparkEnv = 0.6f + randCbF() * 0.4f;
                sparkDec = 0.9930f + randCbF() * 0.0065f;   // 1-20 ms tails
            }
            sparkEnv *= sparkDec;
            float spark = tanhf(noise * sparkEnv * 4.0f);   // crunchy crackle
            coreL = spark * 1.10f + imp * 0.5f;
            coreR = spark * 1.05f + imp * 0.4f;
            break;
        }

        case 3: { // MITRAGLIA — irregular fire: random bursts and ratchets
            osc3.SetFreq(f3Sm);
            sig3 = osc3.Process();
            float wall = tanhf(noise * 5.0f) * 0.6f + sig3 * 0.5f;
            if (--gunCnt <= 0) {
                gunOn = !gunOn;
                uint32_t r = lfsrCbNext();
                float base = 0.012f + (1.0f - density) * 0.10f;  // DENSITY = speed
                if ((r & 7u) == 0u) gunCnt = (int)(48000.0f * 0.004f);  // ratchet
                else gunCnt = (int)(48000.0f * base * (0.5f + randCbF() * 2.0f));
                if (gunCnt < 90) gunCnt = 90;
            }
            patchGate += 0.05f * ((gunOn ? 1.0f : 0.0f) - patchGate);  // hard edge
            svfMain.SetFreq(svfLive);
            svfMain.Process(wall);
            float body = svfMain.Low() * 0.4f + svfMain.Band() * 0.6f;
            coreL = body * patchGate * 1.4f;
            coreR = (body * 0.8f + svfMain.High() * 0.3f) * patchGate * 1.4f;
            break;
        }

        case 4: { // RADIO — gritty shortwave: clipped carrier, crushed static
            osc3.SetFreq(f3Sm);                  // instant retunes (no glide)
            sig3 = osc3.Process();
            float carrier = tanhf(sig3 * 3.0f);  // hard-clipped square carrier
            osc2.SetFreq(600.0f + (lv3 * 0.5f + 0.5f) * 2600.0f);
            float whistle = osc2.Process();      // faint heterodyne behind it
            // random surges where the static takes over the transmission
            if ((lfsrCbNext() & 0x3FFFFu) < 5u) burstEnv = 0.6f + randCbF() * 0.4f;
            burstEnv *= 0.99995f;
            float st = tanhf(noise * 9.0f);      // hard, scratchy static
            float dry = carrier * 0.55f + st * (0.50f + burstEnv * 0.60f) + whistle * 0.12f;
            // output decimation: the whole signal grinds
            if (--radioCnt <= 0) { radioHold = dry; radioCnt = 6; }
            coreL = radioHold * 1.15f;
            coreR = radioHold * 1.05f + st * 0.08f;
            break;
        }

        case 5: { // PRESSA — heavy industrial grinder
            float bf = f1Sm;
            if (bf < 45.0f) bf = 45.0f; if (bf > 140.0f) bf = 140.0f;
            osc1.SetFreq(bf);
            osc2.SetFreq(bf * 1.011f);           // beating pair: slow grind
            float g1 = osc1.Process(), g2 = osc2.Process();
            float grind = tanhf((g1 + g2) * (2.5f + density * 3.0f));
            grind = grind * 0.70f + grind * noise * 0.55f;   // gritty surface
            float throb = 0.72f + lv1 * 0.28f;   // the machine heaves slowly
            coreL = grind * throb * 1.15f;
            coreR = grind * throb * 1.05f + grind * lv2 * 0.10f;
            break;
        }

        case 6: { // LAMIERA — continuous metal scrape, not blips
            float mf = f2Sm;                     // very slow glide (freqSmooth 0.004)
            if (mf < 150.0f) mf = 150.0f; if (mf > 1200.0f) mf = 1200.0f;
            osc2.SetFreq(mf);
            osc3.SetFreq(mf * 1.731f + 13.0f);   // inharmonic partner
            float a = osc2.Process();
            sig3 = osc3.Process();
            float ringm = a * sig3;
            float scrape = ringm * (0.6f + noise * 0.7f);  // noise drags the metal
            osc1.SetFreq(f1Sm);
            float body = osc1.Process();
            coreL = tanhf(scrape * 2.2f) * 1.05f + body * 0.20f;
            coreR = tanhf(scrape * 2.0f) * 0.95f - body * 0.18f;
            break;
        }

        case 7: { // SOLO-IN — extreme input destruction, second mangling stage
            osc3.SetFreq(f3Sm);
            sig3 = osc3.Process();   // still feeds the input ring modulator
            float xin = tanhf(ain * (2.0f + inMangle * 10.0f));
            xin = wavefold(xin, 2.0f + inMangle * 6.0f);
            xin = wavefold(xin, 1.6f);
            if (--soloCnt <= 0) { soloHold = xin; soloCnt = 1 + (int)(inMangle * 30.0f); }
            ain = soloHold;          // deep decimation on top of the normal chain
            break;
        }

        case 8: { // MURO — harsh noise wall with embers and lurches
            float bf = f1Sm;
            if (bf < 35.0f) bf = 35.0f; if (bf > 110.0f) bf = 110.0f;
            osc1.SetFreq(bf);
            osc2.SetFreq(bf * 1.494f);           // fused, beating pair
            float s1 = osc1.Process(), s2 = osc2.Process();
            float wall = tanhf(noise * 8.0f) * 0.50f + tanhf((s1 + s2) * 2.2f) * 0.55f;
            // embers: crackle transients buried inside the wall
            if ((lfsrCbNext() & 0xFFFFu) < (2u + (uint32_t)(density * 50.0f)))
                emberEnv = 1.0f;
            emberEnv *= 0.9960f;
            wall += noise * emberEnv * 0.8f;
            // the wall lurches: the low-pass jumps to a new spot now and then
            if ((lfsrCbNext() & 0xFFFFFu) < 8u)
                muroCut = 120.0f + randCbF() * 700.0f;
            svfMain.SetFreq(muroCut);
            svfMain.Process(wall);
            coreL = svfMain.Low() * 1.1f + wall * 0.45f;
            coreR = svfMain.Low() * 1.0f + wall * 0.42f;
            break;
        }

        case 9: { // MAGMA — low FM boiling, blop pings, frying surface noise
            float bf = f1Sm;
            if (bf < 35.0f) bf = 35.0f; if (bf > 130.0f) bf = 130.0f;
            osc2.SetFreq(bf * 2.7f);
            float m = osc2.Process();
            float bff = bf * (1.0f + m * (0.8f + lv2 * 0.5f));
            if (bff < 20.0f) bff = 20.0f;
            osc1.SetFreq(bff);
            float bub = osc1.Process();
            float siz = noise * (0.25f + density * 0.35f);  // frying grit
            // blop: a low resonant ping every now and then
            float imp = 0.0f;
            if ((lfsrCbNext() & 0xFFFFu) < (2u + (uint32_t)(density * 25.0f))) {
                imp = randCbF() * 2.0f - 1.0f;
                svfMain.SetFreq(60.0f + randCbF() * randCbF() * 240.0f);
            }
            svfMain.Process(bub * 0.5f + imp + siz * 0.7f);
            coreL = bub * 0.50f + svfMain.Low() * 1.2f + svfMain.Band() * 0.55f + siz * 0.30f;
            coreR = bub * 0.45f + svfMain.Low() * 1.1f + svfMain.Band() * 0.60f + siz * 0.28f;
            break;
        }

        case 10: { // SCULTURA — cut-up collage: garbage / screech / metal / silence
            if (--cutCnt <= 0) {
                // new segment: 60-860 ms, shorter when DENSITY is high
                float dur = 0.06f + randCbF() * (0.80f - density * 0.55f);
                cutCnt = (int)(dur * 48000.0f);
                segTex = lfsrCbNext() & 3u;
                cutLvl = 0.5f + randCbF() * 0.5f;
                garbN  = 20 + (int)(randCbF() * 180.0f);    // garbage pitch
                segF   = 200.0f + randCbF() * 2800.0f;      // metal-stab pitch
                if (segTex == 1u) svfMain.SetFreq(1500.0f + randCbF() * 6500.0f);
            }
            float tex = 0.0f;
            if (segTex == 0u) {                  // crushed digital garbage
                if (--garbCnt <= 0) { garbHold = randCbF() * 2.0f - 1.0f; garbCnt = garbN; }
                tex = tanhf(garbHold * 3.0f) * 0.7f;
            } else if (segTex == 1u) {           // clipped resonant screech
                svfMain.Process(noise * 0.5f);
                tex = tanhf(svfMain.Band() * 4.0f) * 0.8f;
            } else if (segTex == 2u) {           // metallic ring stab
                osc2.SetFreq(segF);
                osc3.SetFreq(segF * 1.41f);
                tex = tanhf(osc2.Process() * osc3.Process() * 3.0f) * 0.8f;
            }                                    // segTex 3: silence
            patchGate += 0.020f * ((segTex == 3u ? 0.0f : cutLvl) - patchGate);
            coreL = tex * patchGate * 1.30f;
            coreR = tex * patchGate * 1.25f;
            break;
        }
        }

        // ── INPUT RECOMBINATION: ring mod + wet mix ────────────
        float ainRing = ain * sig3;                  // mangled input x top osc
        // input level in the mix: POT 3 (scaled per patch) plus a fixed share.
        // SOLO-IN: coreMix 0 and inFloor 1, so only the destroyed input is heard
        float inMix = inMangle * P.inPotLvl + P.inFloor;
        if (inMix > 1.0f) inMix = 1.0f;
        float inSig = ain * 0.85f + ainRing * 0.95f; // direct + ring, well present
        float sigL = coreL * P.coreMix * (1.0f - inMix * 0.55f) + inSig * inMix;
        float sigR = coreR * P.coreMix * (1.0f - inMix * 0.55f) + inSig * inMix * 0.9f;

        // triple wavefolder (per-patch gain)
        sigL = wavefold(sigL, foldG1); sigL = wavefold(sigL, foldG2); sigL = wavefold(sigL, foldG3);
        sigR = wavefold(sigR, foldG1 * 0.93f); sigR = wavefold(sigR, foldG2 * 0.93f); sigR = wavefold(sigR, foldG3 * 0.93f);

        // constant crushed-static bed (MURO, MAGMA)
        if (P.staticMix > 0.0f) {
            float st = tanhf(noise * 6.0f);
            sigL += st * P.staticMix;
            sigR += st * P.staticMix * 0.9f;
        }

        // ── AUTO-LEVEL: constant output volume ─────────────────
        // Slow amplitude follower plus a bounded gain correction: no pot
        // or patch can raise the overall volume. Below the silence
        // threshold the gain freezes, so pauses are never pumped up.
        // Each patch sets its own target level.
        float mag = fabsf(sigL) + fabsf(sigR);
        lvlEnv += (mag > lvlEnv ? 0.0008f : 0.0002f) * (mag - lvlEnv);
        if (lvlEnv > 0.02f) {
            float want = P.lvlTarget / lvlEnv;
            if (want > 2.5f)  want = 2.5f;
            if (want < 0.35f) want = 0.35f;
            lvlGain += 0.00005f * (want - lvlGain);    // settles in ~0.5 s
        }
        sigL *= lvlGain;
        sigR *= lvlGain;

        // ── OUTPUT: soft limit → DC block → smoothing ──────────
        float oL = tanhf(sigL * 0.72f);
        float oR = tanhf(sigR * 0.72f);
        oL = dcL.Process(oL);
        oR = dcR.Process(oR);
        out[0][s] = toneL.Process(oL);
        out[1][s] = toneR.Process(oR);
    }
}

// ══════════════════════════════════════════════════════════════
// SETUP
// ══════════════════════════════════════════════════════════════
void setup() {
    hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
    sample_rate = DAISY.get_samplerate();

    // seed the RNG from ADC noise
    uint32_t seed = 0;
    for (int i = 0; i < 32; i++) { seed = (seed << 1) | (analogRead(A0) & 1u); delayMicroseconds(50); }
    lfsr = (seed != 0) ? seed : 0xFEEDFACEu;

    osc1.Init(sample_rate); osc1.SetWaveform(Oscillator::WAVE_SIN); osc1.SetFreq(40.0f);   osc1.SetAmp(1.0f);
    osc2.Init(sample_rate); osc2.SetWaveform(Oscillator::WAVE_SIN); osc2.SetFreq(700.0f);  osc2.SetAmp(1.0f);
    osc3.Init(sample_rate); osc3.SetWaveform(Oscillator::WAVE_SIN); osc3.SetFreq(3000.0f); osc3.SetAmp(1.0f);
    noiseGen.Init(); noiseGen.SetAmp(1.0f);
    svfMain.Init(sample_rate); svfMain.SetFreq(800.0f); svfMain.SetRes(0.81f);

    lfo1.Init(sample_rate); lfo1.SetWaveform(Oscillator::WAVE_SIN); lfo1.SetFreq(0.05f); lfo1.SetAmp(1.0f);
    lfo2.Init(sample_rate); lfo2.SetWaveform(Oscillator::WAVE_TRI); lfo2.SetFreq(0.11f); lfo2.SetAmp(1.0f);
    lfo3.Init(sample_rate); lfo3.SetWaveform(Oscillator::WAVE_SIN); lfo3.SetFreq(0.08f); lfo3.SetAmp(1.0f);
    lfo4.Init(sample_rate); lfo4.SetWaveform(Oscillator::WAVE_SIN); lfo4.SetFreq(2.5f);  lfo4.SetAmp(1.0f);

    dcL.Init(sample_rate); dcR.Init(sample_rate);
    toneL.Init(sample_rate); toneR.Init(sample_rate);

    pinMode(D18, INPUT_PULLUP);
    pinMode(LED_BUILTIN, OUTPUT);   // patch indicator
    DAISY.begin(AudioCallback);
}

// ══════════════════════════════════════════════════════════════
// LOOP — pots, patch switch, three random streams
// ══════════════════════════════════════════════════════════════
void loop() {
    static uint32_t lastMs = 0;
    static bool swLast = false, swState = false, swInit = false;
    static uint32_t swLastMs = 0;
    static int  blinkLeft = 0;        // LED blinks remaining
    static uint32_t blinkNextMs = 0;
    static bool ledOn = false;
    static uint32_t freqNextMs = 400, filtNextMs = 600, chaosNextMs = 900;

    uint32_t now = millis();

    if (now - lastMs >= 5) {
        lastMs = now;
        float r0 = analogRead(A0) / 1023.0f;
        float r1 = analogRead(A1) / 1023.0f;
        float r2 = analogRead(A2) / 1023.0f;
        pot[0] += POT_SP * (r0 - pot[0]);
        pot[1] += POT_SP * (r1 - pot[1]);
        pot[2] += POT_SP * (r2 - pot[2]);

        vDensity  = pot[0];
        vRegister = pot[1];
        vInMangle = pot[2];

        // LFO rates follow DENSITY
        lfo1.SetFreq(0.03f + pot[0] * 0.09f);
        lfo2.SetFreq(0.07f + pot[0] * 0.17f);
        lfo3.SetFreq(0.05f + pot[0] * 0.12f);
        lfo4.SetFreq(0.8f + pot[0] * 13.2f);
        vSvfRes = 0.72f + pot[0] * 0.19f;

        // input destruction: bit depth + decimation follow INPUT
        float bits = 16.0f - pot[2] * 13.0f;      // 16 → 3 bit
        vCrushSteps = powf(2.0f, bits);
        vInDecim = 1 + (int)(pot[2] * 12.0f);     // 1 → 13
    }

    // PATCH SWITCH: every flip (either direction) advances one patch
    bool swRaw = (digitalRead(D18) == LOW);
    if (!swInit) {
        // first read after boot: remember the position, do not change patch
        swLast = swState = swRaw;
        swInit = true;
    }
    if (swRaw != swLast) { swLast = swRaw; swLastMs = now; }
    if ((now - swLastMs) > 30 && swRaw != swState) {   // 30 ms debounce
        swState = swRaw;
        int np = vPatch + 1;
        if (np >= NUM_PATCHES) np = 0;
        // waveform changes are cheap and safe outside the audio callback
        osc1.SetWaveform(patches[np].wave1);
        osc2.SetWaveform(patches[np].wave2);
        osc3.SetWaveform(patches[np].wave3);
        vPatch = np;
        blinkLeft = np + 1;   // the LED blinks N times = patch N
        blinkNextMs = now;
        ledOn = false;
    }

    // non-blocking LED: N blinks = current patch number
    if (blinkLeft > 0 && (int32_t)(now - blinkNextMs) >= 0) {
        ledOn = !ledOn;
        digitalWrite(LED_BUILTIN, ledOn ? HIGH : LOW);
        blinkNextMs = now + (ledOn ? 90 : 160);
        if (!ledOn) blinkLeft--;
    }

    // random-stream tempo follows DENSITY (plus the patch speed)
    float tempoMs = 2500.0f * powf(25.0f / 2500.0f, pot[0]);
    tempoMs *= patches[vPatch].tempoScale;
    if (tempoMs < 18.0f) tempoMs = 18.0f;

    // stream 1: base frequencies
    if ((int32_t)(now - freqNextMs) >= 0) {
        // the patch regBias shifts the register darker or brighter
        float reg = pot[1] + patches[vPatch].regBias;
        if (reg < 0.0f) reg = 0.0f; if (reg > 1.0f) reg = 1.0f;
        float f1Low = 8.0f + reg * 25.0f,   f1High = 60.0f + reg * 150.0f;
        vFreq1 = f1Low + randF() * (f1High - f1Low);
        float f2Low = 80.0f + reg * 200.0f, f2High = 600.0f + reg * 2800.0f;
        vFreq2 = f2Low + randF() * (f2High - f2Low);
        float f3Low = 300.0f + reg * 600.0f, f3High = 2000.0f + reg * 8000.0f;
        vFreq3 = f3Low + randF() * (f3High - f3Low);
        uint32_t iv;
        if ((lfsrNext() & 0xFF) < 72) { float f = 0.08f + randF() * 0.18f; iv = (uint32_t)(tempoMs * f); if (iv < 15) iv = 15; }
        else                          { float f = 0.65f + randF() * 1.10f; iv = (uint32_t)(tempoMs * f); if (iv < 22) iv = 22; }
        freqNextMs = now + iv;
    }

    // stream 2: filter target
    if ((int32_t)(now - filtNextMs) >= 0) {
        float reg = pot[1] + patches[vPatch].regBias;
        if (reg < 0.0f) reg = 0.0f; if (reg > 1.0f) reg = 1.0f;
        float fLow = 50.0f + reg * 300.0f, fHigh = 1000.0f + reg * 11000.0f;
        float newSvfFreq;
        if ((lfsrNext() & 0xF) < 3) newSvfFreq = (randF() < 0.5f) ? fLow * (0.7f + randF() * 0.5f) : fHigh * (0.8f + randF() * 0.4f);
        else                        newSvfFreq = fLow + randF() * (fHigh - fLow);
        if (newSvfFreq < 20.0f) newSvfFreq = 20.0f; if (newSvfFreq > 12000.0f) newSvfFreq = 12000.0f;
        vSvfFreq = newSvfFreq;
        float fi = tempoMs * (0.4f + randF() * 1.8f); if (fi < 28.0f) fi = 28.0f;
        filtNextMs = now + (uint32_t)fi;
    }

    // stream 3: chaos (feedback, FM depth, cascade, ring)
    if ((int32_t)(now - chaosNextMs) >= 0) {
        float dens = pot[0];
        float fr1 = randF(); fr1 *= fr1; float fr2 = randF(); fr2 *= fr2; float fr3 = randF(); fr3 *= fr3;
        vFb1 = 0.22f + fr1 * (0.18f + dens * 0.12f);
        vFb2 = 0.26f + fr2 * (0.16f + dens * 0.10f);
        vFb3 = 0.28f + fr3 * (0.14f + dens * 0.10f);
        float m1 = randF(); vFmIdx1 = 150.0f + m1 * m1 * (1200.0f + dens * 1300.0f);
        float m2 = randF(); vFmIdx2 = 300.0f + m2 * m2 * (1800.0f + dens * 1700.0f);
        float m3 = randF(); vFmIdx3 = 600.0f + m3 * m3 * (3000.0f + dens * 2000.0f);
        vCasc12 = 0.20f + randF() * (0.50f + dens * 0.20f);
        vCasc23 = 0.25f + randF() * (0.45f + dens * 0.20f);
        float rr = randF(); vRingBase = 0.15f + rr * rr * (0.40f + dens * 0.20f);
        float ci = tempoMs * (1.8f + randF() * 2.4f); if (ci < 55.0f) ci = 55.0f;
        chaosNextMs = now + (uint32_t)ci;
    }
}
