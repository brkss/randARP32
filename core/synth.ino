#include <Arduino.h>
#include <driver/i2s.h>

// ===== MAX98357A I2S PINS =====
#define I2S_DOUT 22  // DIN
#define I2S_BCLK 26  // BCLK
#define I2S_LRC  25  // LRC/WS

// ===== ROTARY ENCODER (WH-040 / KY-040) =====
#define ENC_A  27   // CLK
#define ENC_B  14   // DT
#define ENC_SW 13   // SW (to GND) — remember + → 3V3

// ===== AUDIO SETTINGS =====
static const int   SR          = 44100;
static const int   AMP         = 20000;
static const float GLIDE_SEC   = 0.01f;
static const float ATTACK_SEC  = 0.003f;
static const float RELEASE_SEC = 0.012f;
static const float FADEIN_SEC  = 0.004f;

// ===== ENCODER / UI =====
volatile int32_t encDelta = 0;  // accumulated detents (from ISR)
float BPM = 120.0f;
const float BPM_MIN = 40.0f, BPM_MAX = 240.0f;
const float BPM_STEP = 3.0f;

// --- MODES: BPM ↔ LPF cutoff (Q fixed) ---
enum CtrlMode { CTRL_BPM = 0, CTRL_LPF_CUTOFF = 1 };
CtrlMode ctrlMode = CTRL_BPM;  // short-press cycles modes

// ===== Progressive 32-step pattern (A minor) =====
// NOTE: value 0 is interpreted as a REST.
const int8_t PROG_PATTERN[32] = {
  // Root + 5th + Octave jumps
   0, 12, 19, 24,   7, 19, 24, 31,
   
  // Suspended / 7th colors
   0,  7, 10, 19,  12, 15, 22, 24,
  
  // Climbing arpeggio
   0,  7, 12, 19,  24, 19, 12,  7,

  // Ending: resolving “answer” to the opening
  19, 24, 31, 24,  19, 12,  22, 24,
};
uint8_t prog_i = 0;
int ROOT_MIDI = 45;  // A2 (low) -> sweep makes it rumble to bright

// ===== Minimal Reverb / Delay =====
#define REVERB_ON 1
#if REVERB_ON
static const int   RVB_MS   = 500;
static const float RVB_MIX  = 0.4f;
static const float RVB_FB   = 0.4f;
static const float RVB_DAMP = 0.6f;
static const int   DLY_LEN  = (SR * RVB_MS) / 1000;
static int16_t dly[DLY_LEN];
static int     dlyIdx = 0;
static float   fbLP   = 0.0f;
inline float processReverb(float inDry) {
  float d = (float)dly[dlyIdx] / 32768.0f;
  fbLP += RVB_DAMP * (d - fbLP);
  float wr = inDry + RVB_FB * fbLP;
  if (wr > 1.0f) wr = 1.0f; else if (wr < -1.0f) wr = -1.0f;
  dly[dlyIdx] = (int16_t)(wr * 32767.0f);
  if (++dlyIdx >= DLY_LEN) dlyIdx = 0;
  return (1.0f - RVB_MIX) * inDry + RVB_MIX * d;
}
#endif

// ===== Resonant Low-Pass (RBJ biquad) =====
// Q is fixed (medium) and not encoder-controlled
float lpfCut = 2000.0f;   // Hz
float lpfQ   = 1.8f;     // resonance 

// Cutoff control: log-scaled (musical feel)
const float LPF_MIN = 60.0f;
const float LPF_MAX = 8000.0f;
const int   LPF_KNOB_STEPS = 80;  // ~3–4 turns on many KY-040s
int         lpfKnob = 0;          // virtual knob position [0..LPF_KNOB_STEPS]

// Biquad coefficients & state
float h_b0=0, h_b1=0, h_b2=0, h_a1=0, h_a2=0;
float h_x1=0, h_x2=0, h_yz1=0, h_yz2=0;

inline void lpf_recalc_coeffs(float fc, float Q) {
  fc = constrain(fc, LPF_MIN, LPF_MAX);
  float w0   = 2.0f * PI * fc / (float)SR;
  float cw0  = cosf(w0);
  float sw0  = sinf(w0);
  float alpha= sw0 / (2.0f * Q);

  float B0 = (1.0f - cw0) * 0.5f;
  float B1 =  1.0f - cw0;
  float B2 = (1.0f - cw0) * 0.5f;
  float A0 =  1.0f + alpha;
  float A1 = -2.0f * cw0;
  float A2 =  1.0f - alpha;

  h_b0 = B0 / A0;
  h_b1 = B1 / A0;
  h_b2 = B2 / A0;
  h_a1 = A1 / A0;
  h_a2 = A2 / A0;
}

inline float lpf_process(float x) {
  float y = h_b0*x + h_b1*h_x1 + h_b2*h_x2 - h_a1*h_yz1 - h_a2*h_yz2;
  h_x2 = h_x1; h_x1 = x;
  h_yz2 = h_yz1; h_yz1 = y;
  return y;
}

// --- helpers to convert between knob position and frequency (log mapping) ---
inline float knobToHz(int k) {
  if (k < 0) k = 0; if (k > LPF_KNOB_STEPS) k = LPF_KNOB_STEPS;
  float t = (float)k / (float)LPF_KNOB_STEPS; // 0..1
  return LPF_MIN * powf(LPF_MAX / LPF_MIN, t);
}
inline int hzToKnob(float hz) {
  hz = constrain(hz, LPF_MIN, LPF_MAX);
  float t = logf(hz / LPF_MIN) / logf(LPF_MAX / LPF_MIN);
  int k = (int)roundf(t * (float)LPF_KNOB_STEPS);
  if (k < 0) k = 0; if (k > LPF_KNOB_STEPS) k = LPF_KNOB_STEPS;
  return k;
}

// ===== Utils =====
static inline float midiToHz(int m) {
  return 440.0f * powf(2.0f, (m - 69) / 12.0f);
}

// ===== I2S Setup =====
#ifndef I2S_COMM_FORMAT_STAND_I2S
#define I2S_COMM_FORMAT_STAND_I2S I2S_COMM_FORMAT_I2S
#endif
void setupI2S() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SR,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len   = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pins = {
    .bck_io_num   = I2S_BCLK,
    .ws_io_num    = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num  = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_set_clk(I2S_NUM_0, SR, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
}

// ===== Encoder ISR =====
volatile uint32_t lastEdgeUs = 0;
void IRAM_ATTR encISR() {
  uint32_t now = micros();
  if (now - lastEdgeUs < 500) return;  // debounce
  lastEdgeUs = now;
  int b = digitalRead(ENC_B);
  encDelta += (b == LOW) ? +1 : -1;
}
void setupEncoder() {
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_A), encISR, RISING);
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(100);
  setupI2S();
  setupEncoder();

#if REVERB_ON
  for (int i = 0; i < DLY_LEN; ++i) dly[i] = 0;
  fbLP = 0.0f;
#endif

  // Initialize LPF (Q fixed), set knob from initial cutoff
  lpf_recalc_coeffs(lpfCut, lpfQ);
  h_x1 = h_x2 = h_yz1 = h_yz2 = 0.0f;
  lpfKnob = hzToKnob(lpfCut);

  Serial.println("Rotate = BPM. Short-press cycles: BPM ↔ LPF cutoff (Q fixed).");
}

// ===== Main Loop =====
void loop() {
  // --- Short-press to cycle mode ---
  static bool prevSwHigh = true;
  static uint32_t lastBtnMs = 0;
  bool swHigh = (digitalRead(ENC_SW) == HIGH);  // pull-up: HIGH when not pressed
  uint32_t ms = millis();
  if (prevSwHigh && !swHigh && (ms - lastBtnMs) > 250) {
    ctrlMode = (CtrlMode)((((int)ctrlMode) + 1) % 2);  // only 2 modes now
    Serial.print("Mode: ");
    if (ctrlMode == CTRL_BPM) Serial.println("BPM");
    else { Serial.print("LPF cutoff (Hz): "); Serial.println(lpfCut, 1); }
    lastBtnMs = ms;
  }
  prevSwHigh = swHigh;

  // --- Apply encoder delta to current mode (with acceleration) ---
  int32_t d = encDelta;
  if (d != 0) {
    encDelta = 0;
    int ad = abs(d);
    int accel = (ad >= 8) ? 8 : (ad >= 4) ? 4 : (ad >= 2) ? 2 : 1;

    if (ctrlMode == CTRL_BPM) {
      BPM += d * BPM_STEP * accel;
      BPM = constrain(BPM, BPM_MIN, BPM_MAX);
      Serial.print("BPM: "); Serial.println(BPM, 1);
    } else { // CTRL_LPF_CUTOFF with log mapping
      lpfKnob += d * accel;
      if (lpfKnob < 0) lpfKnob = 0;
      if (lpfKnob > LPF_KNOB_STEPS) lpfKnob = LPF_KNOB_STEPS;
      lpfCut = knobToHz(lpfKnob);
      lpf_recalc_coeffs(lpfCut, lpfQ);
      Serial.print("LPF cutoff: "); Serial.print(lpfCut, 1); Serial.println(" Hz");
    }
  }

  // --- Timing (16th notes) ---
  float secPerBeat   = 60.0f / BPM;
  float secPerStep   = secPerBeat / 4.0f;
  int   framesPerStep= (int)(secPerStep * SR);

  static float prevHz = 0.0f;

  // Read pattern step (0 = REST)
  int8_t stepVal = PROG_PATTERN[prog_i];
  prog_i = (prog_i + 1) % 32;

  const int N = 256;
  int samplesLeft = framesPerStep;
  int attackFrames  = max(1, (int)(ATTACK_SEC  * SR));
  int releaseFrames =        (int)(RELEASE_SEC * SR);
  if (releaseFrames > framesPerStep / 3) releaseFrames = framesPerStep / 3;
  int glideFrames   = max(1, (int)(GLIDE_SEC   * SR));
  int fadeInFrames  = min((int)(FADEIN_SEC * SR), framesPerStep / 4);

  static float phase = 0.0f;

  // === REST: oscillator muted, reverb tail keeps playing ===
  if (stepVal == 0) {
    while (samplesLeft > 0) {
      int block = min(N, samplesLeft);
      int16_t buf[2 * N];
      for (int i = 0; i < block; i++) {
        float y = 0.0f;
        y = lpf_process(y);
        #if REVERB_ON
          y = processReverb(y);
        #endif
        int16_t sample = (int16_t)(y * AMP);
        buf[2 * i + 0] = sample;
        buf[2 * i + 1] = sample;
      }
      size_t written;
      i2s_write(I2S_NUM_0, buf, block * sizeof(int16_t) * 2, &written, portMAX_DELAY);
      samplesLeft -= block;
    }
    prevHz = 0.0f;
    return;
  }

  // === NOTE: play oscillator ===
  int   midi     = ROOT_MIDI + stepVal;
  float targetHz = midiToHz(midi);

  float hz     = (prevHz == 0.0f) ? targetHz : prevHz;
  float hzStep = (targetHz - hz) / (float)glideFrames;

  while (samplesLeft > 0) {
    int block = min(N, samplesLeft);
    int16_t buf[2 * N];

    for (int i = 0; i < block; i++) {
      int elapsed = framesPerStep - samplesLeft + i;

      if (glideFrames > 0) { hz += hzStep; glideFrames--; }
      else { hz = targetHz; }

      float env = 1.0f;
      if (elapsed < attackFrames) {
        env = (float)elapsed / (float)attackFrames;
      } else if (elapsed > framesPerStep - releaseFrames) {
        int relPos = elapsed - (framesPerStep - releaseFrames);
        env = 1.0f - (float)relPos / (float)releaseFrames;
      }
      if (elapsed < fadeInFrames) {
        float w = (float)elapsed / (float)fadeInFrames;
        env *= sinf((PI * 0.5f) * w);
      }

      // --- Dual oscillator: Sine + Saw ---
      float dp   = 2.0f * PI * hz / SR;
      float sine = sinf(phase);
      float saw  = 1.0f - (fmodf(phase / (2.0f * PI), 1.0f) * 2.0f);
      phase += dp; if (phase >= 2.0f * PI) phase -= 2.0f * PI;

      float s = sine * 0.9f + saw * 0.1f;
      //float s   = tanhf(raw);   // smooths harsh peaks, keeps punch

      float y = s * env;

      // --- LPF then Reverb ---
      y = lpf_process(y);
      #if REVERB_ON
        y = processReverb(y);
      #endif

      int16_t sample = (int16_t)(y * AMP);
      buf[2 * i + 0] = sample;
      buf[2 * i + 1] = sample;
    }

    size_t written;
    i2s_write(I2S_NUM_0, buf, block * sizeof(int16_t) * 2, &written, portMAX_DELAY);
    samplesLeft -= block;
  }

  prevHz = targetHz;
}
