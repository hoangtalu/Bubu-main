// fortune_teller.cpp
#include "fortune_teller.h"
#include <U8g2_for_Adafruit_GFX.h>

namespace FortuneTeller {

// ======= Config =======
static constexpr int CANVAS_W = 120;
static constexpr int CANVAS_H = 120;
static constexpr float DESIRED_SCALE = 1.5f;
static constexpr float MAX_SCALE     = 2.0f;
static constexpr uint32_t DEFAULT_DURATION = 7999; // ms

// ======= State =======
static Adafruit_GC9A01A* tft = nullptr;
static GFXcanvas16 canvas(CANVAS_W, CANVAS_H);
static U8G2_FOR_ADAFRUIT_GFX u8g2;
static bool active = false;
static uint32_t startedAt = 0;
static uint32_t playFor   = DEFAULT_DURATION;
static uint32_t lastSwitch = 0;   // tracks last time we changed the line

static const char* fortunes[] = {
"Hôm nay có vẻ ổn.\nNhưng đừng chủ quan,\nvũ trụ ghét người tự tin.",
"Bạn sẽ gặp may mắn… \nnếu ra khỏi giường trước 10h.",
"Tránh xa\nngười tên Huy hôm nay.\nChỉ là cảm giác thôi.",
"Cà phê hôm nay\nngon hơn hôm qua.\nNhưng bạn vẫn trễ deadline.",
"Đừng bắt đầu mối quan hệ mới.\nBánh tráng trộn không chữa được trái tim vỡ.",
"Hôm nay,\nđừng quên uống nước.\nDa bạn đang khóc.",
"Ai đó đang nghĩ về bạn.\nChắc là đang chửi.",
"Tiền sẽ đến. \nNhưng rồi sẽ đi. \nRất nhanh.",
"Đừng chơi đá gà cảm xúc.\n Bạn sẽ thua.",
"Có thể bạn đúng. \nNhưng to tiếng sẽ làm bạn sai.",
"Thử im lặng hôm nay. \nSự bí ẩn là vũ khí.",
"Ai đó sẽ làm bạn cười. \nCó thể là chính bạn, \nkhi soi gương.",
"Cẩn thận lời nói. \nMồm đi trước não là đặc sản rồi.",
"Bụng đói là bụng nóng. \nNạp năng lượng \ntrước khi ai đó bị ăn tươi.",
"Tình yêu đến \nkhi bạn không kỳ vọng. \nHoặc khi bạn thơm.",
"Đi đường vòng \ncó thể lâu hơn,\n nhưng đôi khi ít kẹt xe hơn.",
"Người bạn ghét \nđang hạnh phúc. \nHọc cách buông bỏ, hoặc unfollow.",
"Trả lời tin nhắn đi. \nNgười ta đang chờ. \nCó thể là nhà mạng.",
"Bạn cần ngủ. \nMắt bạn trông như\n bánh tráng nhúng nước.",
"Lì xì tâm linh hôm nay: \nnhận ít nhưng đòi nhiều.",
"Dừng lại, thở sâu, r\nồi tiếp tục giả vờ bạn ổn.",
"Lạc quan lên. \nHôm nay bạn chỉ bị xui nhẹ.",
"Ăn phở hôm nay, \nkhông ai có thể ngăn bạn được.",
"Đừng tin vào vận may. \nTin vào bản thân… hoặc vào \nGoogle Maps.",
"Đừng xem lại tin nhắn cũ. \nTự hại mình để làm gì?",
"Cười nhiều hơn hôm nay. \nNgười ta sẽ nghĩ bạn biết bí mật gì đó.",
"Bỏ qua lỗi lầm cũ. \nKhông phải của người khác. Của bạn.",
"Có cơ hội mới đang tới. \nNhớ mở cửa.",
"Bạn chưa hết thời. \nMới chỉ… hơi mốc thôi.",
"Tình yêu là giả, \nhóa đơn là thật.",
"Bật chế độ bay. \nTrốn đời một tí cũng được.",
"Không phải ai nói thương bạn \ncũng mua trà sữa cho bạn.",
"Một người lạ sẽ giúp bạn. \nCó thể là shipper.",
"Hôm nay là ngày hoàn hảo \nđể tha thứ… hoặc tắt điện thoại.",
"Ngủ muộn khiến bạn mộng mị. \nMộng mị khiến bạn… trễ học.",
"Bạn đang ổn. \nBubu xác nhận.",
"Trà sữa không\n giải quyết được mọi thứ. \nNhưng là khởi đầu tốt.",
"Dù ai nói ngả nói nghiêng, \nbạn vẫn phải đi làm.",
"Cẩn thận với đồ ăn cay. \nBụng bạn đang yếu lòng.",
"Đừng thử may mắn hôm nay. \nNó đang bận với người khác.",
"Một cú lướt TikTok\n có thể thay đổi tâm trạng bạn. \nHoặc hủy hoại nó.",
"Hôm nay là ngày tốt \nđể xóa mấy app độc hại.",
"Thử mặc đồ khác màu. \nBiết đâu đổi luôn vận.",
"Ngày đẹp để im lặng \ntrong group chat.",
"Có người nói dối bạn.\n Có thể là chính bạn.",
"Không ai nhớ lỗi lầm của bạn \ntrừ trí nhớ của bạn. \nTha cho mình đi.",
"Mọi chuyện rồi sẽ ổn. \nNếu không ổn thì chưa hết chuyện.",
"Ai đó đang stalk bạn. \nMà bạn lại đang post nhảm.",
"Cười ít lại, \nđừng lộ bài.",
"Hôm nay, bạn chính là… \nnhân vật phụ đáng yêu.",
"Nhắm mắt lại. \nNghĩ về điều tốt đẹp. \nKhông có? Tạo ra đi.",
"Nắng lên rồi. \nNhưng đừng để ảo tưởng lên theo.",
"Có người thương bạn, \nnhưng còn ngại. \nCó thể là Bubu.",
"Hôm nay tốt cho tóc. \nNhưng không cho tình duyên.",
"Đừng xin vía nữa. \nBạn cần xin deadline.",
"Đang ổn định? \nĐó là lúc bão tới.",
"Có ai đó đang nhớ bạn… \nđể đòi nợ.",
"Tránh xa drama. \nBạn không có điều kiện tinh thần.",
"Bạn là ngọn lửa. \nNhưng đừng đốt luôn deadline.",
"Tập thể dục 5 phút. \nĐủ để Bubu bớt lo.",
"Tin vào nhân quả. \nBún đậu nay, dạ dày mai.",
"Hôm nay là ngày đẹp để… \ncắn môi và giả vờ đang suy nghĩ.",
"Bạn không thất bại. \nBạn chỉ đang thử bản beta.",
"Ai đó nói xấu bạn. \nNhưng bằng ngữ pháp sai.",
"Đừng edit ảnh quá đà. \nNgười ta ngoài đời sẽ bất ngờ.",
"Đừng đánh giá ngày \nqua bài post. \nĐó là highlight, không phải sự thật.",
"Tình yêu như ổ điện. \nĐừng thò tay khi không biết\n dây nào nóng.",
"Dự đoán: \nhôm nay bạn sẽ quên điều quan trọng. \nKiểm tra ví đi.",
"Bạn chưa bị ghét,\n chỉ là người ta mệt bạn.",
"Đừng đi ăn một mình tối nay. \nCảm xúc sẽ ăn bạn lại.",
"Hôm nay đẹp trời. \nNhưng đừng quên mang áo mưa.",
"Nhạc buồn nên dừng lại. \nTrừ khi bạn muốn hóa mây.",
"Có thể bạn không sai. \nNhưng bạn hơi lạ.",
"Mơ lớn. \nNhưng nợ nhỏ thôi.",
"Hãy sống như lá me bay – \nnhẹ nhàng, khó đoán, và không mắc nợ.",
"Hôm nay là ngày tốt \nđể bắt đầu lại. \nNhưng không phải với người yêu cũ.",
"Bạn đang ở đúng chỗ. \nChỉ sai thời điểm thôi.",
"Hôm nay, \nmọi thứ trông có vẻ chán. \nCó thể là do bạn.",
"Bubu thấy bạn ổn. \nNhưng còn hơi… cần ngủ.",
"Thử nói thật lòng hôm nay. \nNhẹ lòng, hoặc mất bạn.",
"Đừng chia sẻ quá nhiều.\n Có thể bạn đang nói với group có người “bỏ vô sọt rác.”",
"Hôm nay bạn có cơ hội. \nNhớ mở mắt.",
"Bạn không một mình. \nWi-Fi cũng đang khóc.",
"Thử đứng trước gương \nvà cười. \nNếu gương nứt, chạy đi.",
"Đừng gửi tin nhắn \nlúc 2 giờ sáng. Đừng.",
"Bạn cần một chuyến đi. \nKhông cần xa, chỉ cần không ngồi im.",
"Ai đó sẽ khiến bạn cười. \nCó thể là Bubu run khi thấy ổ cắm.",
"Tâm bạn động. \nNhưng ví bạn đừng động theo.",
"Tình duyên sẽ ổn… \nsau vài lần sụp đổ nữa.",
"Đừng tìm ai đó chữa lành. \nHãy tự vá mình trước.",
"Bắt đầu lại từ… \nviệc dọn phòng.",
"Hôm nay, bớt nói. \nNhiều người đang mệt bạn rồi.",
"Có thể bạn chưa biết: \nhôm nay trời đẹp. \nMắt bạn đang mờ.",
"Đừng uống \nly cà phê thứ 4. \nNhịp tim bạn đang mệt.",
"Ai đó thích bạn.\n Nhưng còn sống ảo nên chưa dám nói.",
"Gửi một lời khen đi.\n Không ai cản bạn đâu.",
"Đừng tin vào chỉ tay. \nTin vào đôi tay bạn.",
"Hôm nay, \nbạn sẽ khiến ai đó nhớ mãi. \nCẩn thận làm gì.",
"Tắt màn hình đi. \nMắt bạn đang chửi bạn.",
"Bubu thấy bạn. \nVà Bubu nghĩ… bạn đang \nlàm tốt hơn bạn nghĩ.",

};
static const int fortuneCount = sizeof(fortunes)/sizeof(fortunes[0]);

static int currentIndex = 0;

// ---- layout helper ----
struct WrappedLayout {
  String lines[48];
  int lineCount = 0;
  int maxLineWidth = 0;
  int totalHeight = 0;
};

static WrappedLayout wrapText(const char* text, int areaW, int lineHeight) {
  WrappedLayout L;
  String input(text);
  input.replace("\\n", "\n");

  int start = 0;
  while (start < input.length()) {
    int nl = input.indexOf('\n', start);
    String para = (nl == -1) ? input.substring(start) : input.substring(start, nl);
    start = (nl == -1) ? input.length() : nl + 1;

    String line, word; int pos = 0;
    while (pos < para.length()) {
      int sp = para.indexOf(' ', pos); if (sp == -1) sp = para.length();
      word = para.substring(pos, sp); pos = sp + 1;

      String test = line + word + " ";
      if (u8g2.getUTF8Width(test.c_str()) > areaW && line.length() > 0) {
        if (L.lineCount < 48) L.lines[L.lineCount++] = line;
        int w = u8g2.getUTF8Width(line.c_str());
        if (w > L.maxLineWidth) L.maxLineWidth = w;
        line = word + " ";
      } else {
        line = test;
      }
    }
    if (line.length() > 0 && L.lineCount < 48) {
      L.lines[L.lineCount++] = line;
      int w = u8g2.getUTF8Width(line.c_str());
      if (w > L.maxLineWidth) L.maxLineWidth = w;
    }
  }
  return L;
}

// ---- blitter (nearest, centered) ----
static void blitCanvasScaled(float scale) {
  if (scale < 1.0f) scale = 1.0f;
  if (scale > MAX_SCALE) scale = MAX_SCALE;

  int dstW = int(CANVAS_W * scale + 0.5f);
  int dstH = int(CANVAS_H * scale + 0.5f);
  int dstX0 = (tft->width()  - dstW)/2;
  int dstY0 = (tft->height() - dstH)/2;

  uint16_t* buf = canvas.getBuffer();

  for (int dy=0; dy<dstH; ++dy) {
    int sy = int(dy / scale); if (sy >= CANVAS_H) sy = CANVAS_H - 1;
    uint16_t* srcRow = buf + sy * CANVAS_W;
    int ty = dstY0 + dy;
    for (int dx=0; dx<dstW; ++dx) {
      int sx = int(dx / scale); if (sx >= CANVAS_W) sx = CANVAS_W - 1;
      tft->drawPixel(dstX0 + dx, ty, srcRow[sx]);
    }
  }
}

static void drawFortuneAutoFit(const char* text) {
  const int marginX = 4;
  const int marginY = 2;
  const int areaW   = CANVAS_W - marginX*2;
  const int lineH   = 15; // Unifont ~16px; 15 packs nicely in 120px

  WrappedLayout L = wrapText(text, areaW, lineH);
  L.totalHeight = L.lineCount * lineH;

  // compute safe scale for 240x240 target (slight guard)
  const float safeW = 240.0f - 2.0f;
  const float safeH = 240.0f - 2.0f;
  float sW = (L.maxLineWidth > 0) ? (safeW / float(L.maxLineWidth + 2*marginX)) : MAX_SCALE;
  float sH = (L.totalHeight  > 0) ? (safeH / float(L.totalHeight  + 2*marginY)) : MAX_SCALE;

  float scale = DESIRED_SCALE;
  if (scale > sW) scale = sW;
  if (scale > sH) scale = sH;
  if (scale < 1.0f) scale = 1.0f;
  if (scale > MAX_SCALE) scale = MAX_SCALE;

  canvas.fillScreen(GC9A01A_BLACK);

  int startY = (CANVAS_H - L.totalHeight)/2 + lineH/2;
  if (startY < marginY + lineH/2) startY = marginY + lineH/2;

  for (int i=0; i<L.lineCount; ++i) {
    int16_t w = u8g2.getUTF8Width(L.lines[i].c_str());
    int16_t startX = (CANVAS_W - w)/2; if (startX < marginX) startX = marginX;
    u8g2.setCursor(startX, startY + i*lineH);
    u8g2.print(L.lines[i]);
  }

  tft->fillScreen(GC9A01A_BLACK);
  blitCanvasScaled(scale);
}

// ======= Public API =======
void setup(Adafruit_GC9A01A* tftRef) {
  tft = tftRef;
  // Bind U8g2 to our offscreen canvas & set VN glyph font
  u8g2.begin(canvas);
  u8g2.setFont(u8g2_font_unifont_t_vietnamese2);
  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(GC9A01A_WHITE);
  // Seed once (engine likely already seeds; harmless here)
  randomSeed(esp_random());
}

void begin(uint32_t durationMs) {
  if (!tft) return;
  playFor    = durationMs ? durationMs : DEFAULT_DURATION;
  startedAt  = millis();
  lastSwitch = startedAt;                 // ← ensures no immediate rotate in loop()
  active     = true;

  currentIndex = random(fortuneCount);
  drawFortuneAutoFit(fortunes[currentIndex]);
}

bool loop() {
  if (!active) return false;
  uint32_t now = millis();
  // Update the text every 5s inside the emotion window
  if (now - lastSwitch > 5000) {
    lastSwitch = now;
    currentIndex = random(fortuneCount);
    drawFortuneAutoFit(fortunes[currentIndex]);
  }
  if (now - startedAt >= playFor) {
    end();
    return false;
  }
  return true;
}

void end() {
  active = false;
}

void reseed() {
  currentIndex = random(fortuneCount);
}

} // namespace FortuneTeller