#include "RGBmatrixPanel.h"
#include <avr/pgmspace.h>
#include <ffft.h>
#include <stdint.h>

// FFT Stuff:
#define  IR_AUDIO  3 // ADC channel to capture
//volatile long zero = 0;
volatile byte position = 0;
int16_t      capture[FFT_N];   /* Wave capture buffer */
complex_t    bfly_buff[FFT_N]; /* FFT buffer */
uint16_t     spectrum[FFT_N/2]; /* Spectrum output buffer */
// Only enough RAM for 64 sample buffer (32 outputs)
// In library file ffft.h, FFT_N must be defined as 64.
// If set to 128 this WILL crash and burn due to lack of RAM.

// RGB matrix stuff:
#define A   A0
#define B   A1
#define C   A2
#define CLK 8  // MUST be on PORTB!
#define LAT A3
#define OE  9
RGBmatrixPanel matrix(A, B, C, CLK, LAT, OE, false);

byte peak[32], count = 0;

void setup() {
  memset(peak, 0, sizeof(peak));
  Serial.begin(115200);
  adcInit();
  matrix.begin();
//  adcCalb();
}

void loop() {
  char     c;
  byte     x, y, y2, r, g, b, lo;
  int      hue;
  uint16_t n;

  if(position != FFT_N) return;

  fft_input(capture, bfly_buff);
  position = 0;  // Restart A/D
  fft_execute(bfly_buff);
  fft_output(bfly_buff, spectrum);

  for(x=0, hue=0; x<32; x++, hue += 40) { // For each column...

    c = spectrum[x]; // 32 samples if FFT_N = 64
    // ADD COLUMN SCALING HERE
    if(c > 15) c = 15;
    if(c > peak[x]) peak[x] = c;
    if(peak[x] <= 0) { // Empty column
      matrix.drawLine(x, 0, x, 15, 0);
      continue;
    }

    y = 16 - peak[x]; // y = 0 to 15
    if(y > 0) matrix.drawLine(x, 0, x, y-1, 0); // Fill black above peak
    // WILL CRASH HERE if out of RAM!
    matrix.drawPixel(x, y, 0xfff); // Draw white dot at peak

    // Draw fading streak from below peak to just above current level
    for(y++, y2 = 16 - c, n=150; y < y2; y++) {
      matrix.drawPixel(x, y, matrix.ColorHSV(hue, 255, n, true));
      if(n >= 50) n -= 50;
    }

    // Draw bar for current level, with slight glow at top
    y = y2;
    n = 100;
    // If current level is peak, don't overwrite white dot
    if(c == peak[x]) {
      y++;
      n = 150;
    }
    while(y < 16) {
      matrix.drawPixel(x, y, matrix.ColorHSV(hue, n, 255, true));
      n += 50;
      if(n > 255) n = 255;
      y++;
    }
  }

  // Every alternate frame, make the peak pixels drop by 1:
  if(++count == 1) {
    count = 0;
    for(x=0; x<32; x++) {
      if(peak[x] > 0) peak[x]--;
    }
  }
}

byte foo = 0;

// Free-running ADC fills capture buffer
ISR(ADC_vect)
{
  // Capture every third sample to get 192:1 prescaler on A/D
  // Equiv to 6400 Hz sampling...seems awfully slow, but makes for
  // more interesting display.
  if(++foo < 3) return;
  foo = 0;

  if(position >= FFT_N) return;
  
//  capture[position] = ADC + zero;
  capture[position] = ADC - 512; // Hardcoded zero calibration
  // Filter out low-level noise
  if((capture[position] >= -2) && (capture[position] <= 2))
    capture[position] = 0;

  position++;
}

void adcInit(){
  // Free running ADC mode, f = ( 16MHz / prescaler ) / 13 cycles per conversion 
  ADMUX  = IR_AUDIO; // Analog channel selection.  Right-adjusted.  Using AREF pin tued to 3.3V regulator output
  ADCSRA = _BV(ADEN)  | // ADC enable
           _BV(ADSC)  | // ADC start
           _BV(ADATE) | // Auto trigger
           _BV(ADIE)  | // Interrupt enable
           _BV(ADPS2) | _BV(ADPS1); // prescaler 64, then /3 in interrupt for ~ 6KHz sample rate
//           _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); // prescaler 128 : 9615 Hz - 150 Hz per 64 divisions, better for most music
  sei(); // Enable interrupts
}

// ADC calibration requires silence...can't guarantee that.
// For now, this is disabled and zero is hardcoded to
// typical median value (512).  Seems OK.
#if(0)
void adcCalb(){
  Serial.println("Start to calc zero");
  long midl = 0;
  // get 2 meashurment at 2 sec
  // on ADC input must be NO SIGNAL!!!
  for (byte i = 0; i < 2; i++)
  {
    position = 0;
    delay(100);
    midl += capture[0];
    delay(900);
  }
  zero = -midl/2;
  Serial.print("Done. Middle=");
  Serial.println(midl);
  Serial.print(". Zero=");
  Serial.println(zero);
}
#endif

