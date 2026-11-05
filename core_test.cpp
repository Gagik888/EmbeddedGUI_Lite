#include <EmbeddedGUI.h>
#include <stdlib.h>
uint32_t g_millis=0; OledSim g_oled; TwoWire Wire;

static void dump(const uint8_t ram[8][132], int w, int h, int x0=0,int y0=0,int cw=0,int ch=0){
  if(!cw)cw=w; if(!ch)ch=h;
  for(int y=y0;y<y0+ch;y++){ for(int x=x0;x<x0+cw;x++) putchar((ram[y>>3][x]>>(y&7))&1?'#':'.'); putchar('\n'); }
}
int fails=0;
#define CHECK(c,msg) do{ if(!(c)){ printf("FAIL: %s\n",msg); fails++; } }while(0)

int main(){
  EmbeddedGUI_OLED<128,64> gui;
  CHECK(sizeof(EmbeddedGUI_OLED<128,64>) < 1024+200, "sizeof");
  printf("sizeof(gui) on host(64-bit)=%zu\n", sizeof(gui));
  CHECK(gui.begin(), "begin");
  CHECK(g_oled.on, "display on");

  // --- 1. Модель-референс для fillRect/pixel/INVERT ---
  static uint8_t model[64][128]; memset(model,0,sizeof model);
  srand(1);
  gui.clear();
  for(int i=0;i<3000;i++){
    int x=rand()%160-16,y=rand()%96-16,w=rand()%40-3,h=rand()%30-3; int c=rand()%3;
    int op=rand()%3;
    if(op==0){ gui.fillRect(x,y,w,h,c);
      for(int j=y;j<y+h;j++)for(int k=x;k<x+w;k++) if(k>=0&&k<128&&j>=0&&j<64){ if(c==0)model[j][k]=0; else if(c==1)model[j][k]=1; else model[j][k]^=1; } }
    else if(op==1){ gui.pixel(x,y,c); if(x>=0&&x<128&&y>=0&&y<64){ if(c==0)model[y][x]=0; else if(c==1)model[y][x]=1; else model[y][x]^=1; } }
    else { gui.vLine(x,y,h,c); for(int j=y;j<y+h;j++) if(x>=0&&x<128&&j>=0&&j<64){ if(c==0)model[j][x]=0; else if(c==1)model[j][x]=1; else model[j][x]^=1; } }
  }
  int bad=0; for(int y=0;y<64;y++)for(int x=0;x<128;x++) if(gui.getPixel(x,y)!=model[y][x]) bad++;
  CHECK(bad==0,"fillRect/pixel/vLine vs reference model"); printf("model mismatches: %d\n",bad);

  // --- 2. Шрифт: все 95 символов ---
  gui.clear();
  char row[20]; int py=0;
  for(int base=32;base<127;base+=16){ int n=0; for(int c=base;c<base+16&&c<127;c++) row[n++]=c; row[n]=0; gui.text(0,py,row); py+=9; }
  gui.update();
  printf("--- font ---\n"); dump(g_oled.ram,128,54,0,0,100,54);

  // --- 3. Примитивы ---
  gui.clear();
  gui.circle(15,15,12); gui.fillCircle(45,15,12); gui.roundRect(65,3,30,24,6); gui.fillRoundRect(100,3,26,24,6);
  gui.line(0,35,60,60); gui.line(60,35,0,60); gui.rect(64,35,30,25); gui.fillRect(100,35,26,25);
  gui.text(102,40,F("INV"),1,EGUI_INVERT);
  gui.update();
  printf("--- primitives ---\n"); dump(g_oled.ram,128,64);

  // --- 4. Совпадение дисплея и буфера, ограничения I2C ---
  int diff=0; for(int p=0;p<8;p++)for(int x=0;x<128;x++) if(g_oled.ram[p][x]!=gui.buffer()[p*128+x]) diff++;
  CHECK(diff==0,"oled ram == canvas"); CHECK(g_oled.overflow==0,"no wire buffer overflow"); CHECK(g_oled.maxTx<=32,"tx<=32");
  printf("maxTx=%u overflow=%u txCount=%u dataBytes=%u\n",g_oled.maxTx,g_oled.overflow,g_oled.txCount,g_oled.dataBytes);

  // --- 5. Dirty pages: статичный кадр - шина молчит; одна страница - 128 байт ---
  uint32_t before=g_oled.dataBytes; gui.update(); CHECK(g_oled.dataBytes==before,"no traffic if clean");
  gui.pixel(5,20); gui.update(); CHECK(g_oled.dataBytes==before+128,"one page = 128 bytes");

  // --- 6. Числа ---
  char b[EGUI_NUM_BUF];
  CanvasBuffer::formatNumber(b,-2147483647L-1); printf("int min: %s\n",b);
  CanvasBuffer::formatNumber(b,0); CHECK(!strcmp(b,"0"),"0");
  CanvasBuffer::formatNumber(b,12345); CHECK(!strcmp(b,"12345"),"12345");
  CanvasBuffer::formatDecimal(b,3.14159f,2); CHECK(!strcmp(b,"3.14"),"3.14");
  CanvasBuffer::formatDecimal(b,-0.04f,1); CHECK(!strcmp(b,"0.0"),"-0.0 avoided");
  CanvasBuffer::formatDecimal(b,-12.05f,1); printf("dec: %s\n",b);
  CanvasBuffer::formatDecimal(b,0.05f,2); CHECK(!strcmp(b,"0.05"),"0.05");
  CanvasBuffer::formatDecimal(b,1e30f,4); printf("huge: %s\n",b);
  CanvasBuffer::formatDecimal(b,7.0f,0); CHECK(!strcmp(b,"7"),"dec0");

  // --- 7. Виджеты ---
  Label l(0,0,128,F("flash"),1); CHECK(!strcmp(l.text(),"flash"),"label F()");
  l.clearDirty(); l.setText("flash"); CHECK(!l.isDirty(),"no dirty on same text");
  l.setText("a very very long label text that overflows"); CHECK(strlen(l.text())==EGUI_LABEL_CAPACITY-1,"truncate");
  l.clearDirty(); l.setText("a very very long label text that overflows"); CHECK(!l.isDirty(),"no dirty on same truncated");
  l.setText("abcd"); l.setText("ab"); CHECK(!strcmp(l.text(),"ab"),"shrink");
  ProgressBar pb(0,0,100,12); pb.setValue(100);
  for(int i=0;i<40;i++){ g_millis+=33; pb.update(g_millis); }
  gui.clear(); gui.draw(pb); printf("progress after 1.3s: filled=%d\n", gui.getPixel(97,6));
  CHECK(gui.getPixel(97,6),"progress reached 100%");

  printf(fails?"\n%d FAILURES\n":"\nALL OK\n",fails);
  return fails;
}
