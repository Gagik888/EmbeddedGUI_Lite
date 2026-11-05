#include <Arduino.h>
#include <Wire.h>
uint32_t g_millis=0; OledSim g_oled; TwoWire Wire;
#include EXAMPLE
static void dump(){ for(int y=0;y<64;y+=2){ for(int x=0;x<128;x++){ int a=(g_oled.ram[y>>3][x]>>(y&7))&1, b=(g_oled.ram[(y+1)>>3][x]>>((y+1)&7))&1; putchar(a&&b?'#':a?'^':b?'v':' ');} putchar('\n'); } }
int main(int argc,char**argv){
  int frames = argc>1?atoi(argv[1]):60;
  setup();
  for(int i=0;i<frames;i++){ g_millis+=40; loop(); EXTRA_PER_FRAME }
  dump();
  printf("overflow=%u maxTx=%u dataBytes=%u\n",g_oled.overflow,g_oled.maxTx,g_oled.dataBytes);
  return g_oled.overflow!=0;
}
