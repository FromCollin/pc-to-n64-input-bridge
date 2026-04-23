#include <Arduino.h>
#include "driver/rmt.h"

// ===== Pins & wiring =====
// DQ <-> GPIO16 (RMT RX)
// GPIO17 (RMT TX) -> Schottky diode (band on GPIO17) -> DQ
// 4.7k pull-up DQ -> 3.3V, GNDs common
static const int PIN_DQ = 16;
static const int PIN_TX = 17;

// ===== RMT config (1 tick = 1us) =====
static const uint8_t  CLK_DIV   = 80;               // 80MHz/80 = 1MHz => 1us ticks
static const auto     CH_TX     = RMT_CHANNEL_0;
static const auto     CH_RX     = RMT_CHANNEL_1;

// RX idle threshold: gap (in us) that marks end of a console packet.
// Small so we can answer quickly after stop bit (~3us total).
static const uint16_t RX_IDLE_US = 6;

// N64 controller-side timings (for TX)
// '1' = 1us LOW, 3us HIGH
// '0' = 3us LOW, 1us HIGH
// STOP = 2us LOW, 1us HIGH
static inline rmt_item32_t make_item(uint16_t low, uint16_t high){
  rmt_item32_t it;
  it.level0 = 0; it.duration0 = low;
  it.level1 = 1; it.duration1 = high;
  return it;
}
static const rmt_item32_t ITEM_BIT1 = make_item(1,3);
static const rmt_item32_t ITEM_BIT0 = make_item(3,1);
static const rmt_item32_t ITEM_STOP = make_item(2,1);

// ===== Python -> N64 state (unchanged packet format) =====
static uint8_t n64_btn0 = 0x00; // A..dR
static uint8_t n64_btn1 = 0x00; // L,R,cU,cD,cL,cR
static uint8_t n64_x    = 0x00; // signed
static uint8_t n64_y    = 0x00; // signed

static void mapPythonToN64(uint8_t b0, uint8_t b1, uint8_t x, uint8_t y){
  uint16_t in = (uint16_t)b0 | ((uint16_t)b1 << 8);

  uint8_t o0=0, o1=0;
  // byte0: [7..0] = A B Z S dU dD dL dR
  o0 |= ((in >> 0) & 1) << 7; // A
  o0 |= ((in >> 1) & 1) << 6; // B
  o0 |= ((in >> 2) & 1) << 5; // Z
  o0 |= ((in >> 3) & 1) << 4; // Start
  o0 |= ((in >> 4) & 1) << 3; // dU
  o0 |= ((in >> 5) & 1) << 2; // dD
  o0 |= ((in >> 6) & 1) << 1; // dL
  o0 |= ((in >> 7) & 1) << 0; // dR
  // byte1: [7..0] = 0 0 L R cU cD cL cR
  o1 |= ((in >>10) & 1) << 5; // L
  o1 |= ((in >>11) & 1) << 4; // R
  o1 |= ((in >>12) & 1) << 3; // cU
  o1 |= ((in >>13) & 1) << 2; // cD
  o1 |= ((in >>14) & 1) << 1; // cL
  o1 |= ((in >>15) & 1) << 0; // cR

  n64_btn0 = o0; n64_btn1 = o1; n64_x = x; n64_y = y;
}

// ===== TX helpers (RMT) =====
static int buildByte(uint8_t b, rmt_item32_t *out){
  int n=0; for (int i=7;i>=0;--i) out[n++] = ((b>>i)&1) ? ITEM_BIT1 : ITEM_BIT0;
  return n;
}

static void sendPacketRMT(const uint8_t *data, int len){
  // Stop RX so we don't record our own echo
  rmt_rx_stop(CH_RX);

  rmt_item32_t items[40]; int idx=0;
  for (int i=0;i<len;++i) idx += buildByte(data[i], items+idx);
  items[idx++] = ITEM_STOP;

  rmt_write_items(CH_TX, items, idx, true);
  rmt_wait_tx_done(CH_TX, pdMS_TO_TICKS(5));

  // Flush any pending RX items that might have been captured, then restart RX
  RingbufHandle_t rb=nullptr; size_t sz;
  rmt_get_ringbuf_handle(CH_RX, &rb);
  if (rb){
    while (true){
      rmt_item32_t* p = (rmt_item32_t*) xRingbufferReceive(rb, &sz, 0);
      if (!p) break;
      vRingbufferReturnItem(rb, (void*)p);
    }
  }
  rmt_rx_start(CH_RX, true);
}

static inline void sendSTATUS(){ const uint8_t s[3]={0x05,0x00,0x00}; sendPacketRMT(s,3); }
static inline void sendPOLL(){   const uint8_t r[4]={n64_btn0,n64_btn1,n64_x,n64_y}; sendPacketRMT(r,4); }

// ===== RX decode (RMT on GPIO16) =====
// We decide bit by LOW duration: ~1us => '1', ~3us => '0'
// Tolerance: <2us => '1', else '0'. We only use the first 8 bits (command byte).
static bool decodeCmdFromItems(const rmt_item32_t* it, int count, uint8_t &out){
  if (count < 8) return false;
  uint8_t cmd=0; int bits=0;

  for (int i=0; i<count && bits<8; ++i){
    // Expect level0=LOW then level1=HIGH
    if (it[i].level0 != 0 || it[i].level1 != 1) continue;
    uint16_t low = it[i].duration0;
    // classify bit
    uint8_t bit = (low < 2) ? 1 : 0;   // threshold at 2us
    cmd = (uint8_t)((cmd << 1) | bit);
    bits++;
  }
  if (bits == 8){ out = cmd; return true; }
  return false;
}

// ===== Serial (USB) pump: read 4B packets from Python =====
static void pumpSerial(){
  while (Serial.available() >= 4){
    uint8_t b0 = Serial.read();
    uint8_t b1 = Serial.read();
    uint8_t x  = Serial.read();
    uint8_t y  = Serial.read();
    mapPythonToN64(b0,b1,x,y);
  }
}

void setup(){
  Serial.begin(115200);
  delay(50);
  Serial.println("\nESP32 N64 (RMT TX+RX): decode 0x00/0x01 and respond. Python drives state.");

  // --- TX setup ---
  rmt_config_t tx = {};
  tx.rmt_mode = RMT_MODE_TX;
  tx.channel = CH_TX;
  tx.gpio_num = (gpio_num_t)PIN_TX;
  tx.mem_block_num = 1;
  tx.tx_config.loop_en = false;
  tx.tx_config.carrier_en = false;
  tx.tx_config.idle_output_en = true;
  tx.tx_config.idle_level = RMT_IDLE_LEVEL_HIGH;
  tx.clk_div = CLK_DIV;
  rmt_config(&tx);
  rmt_driver_install(CH_TX, 0, 0);

  // --- RX setup ---
  pinMode(PIN_DQ, INPUT);
  rmt_config_t rx = {};
  rx.rmt_mode = RMT_MODE_RX;
  rx.channel = CH_RX;
  rx.gpio_num = (gpio_num_t)PIN_DQ;
  rx.clk_div = CLK_DIV;
  rx.mem_block_num = 1;
  rx.rx_config.filter_en = true;
  rx.rx_config.filter_ticks_thresh = 1;     // ignore <1us glitches
  rx.rx_config.idle_threshold = RX_IDLE_US; // end-of-frame gap
  rmt_config(&rx);
  rmt_driver_install(CH_RX, 1024, 0);

  RingbufHandle_t rb=nullptr;
  rmt_get_ringbuf_handle(CH_RX, &rb);
  rmt_rx_start(CH_RX, true);
}

void loop(){
  pumpSerial();

  // heartbeat
  static uint32_t lastBeat=0;
  if (millis()-lastBeat >= 1000){ lastBeat=millis(); Serial.println("[alive]"); }

  // Pull one RX frame (if any)
  RingbufHandle_t rb=nullptr; size_t rx_size=0;
  rmt_get_ringbuf_handle(CH_RX, &rb);
  if (!rb) return;

  rmt_item32_t* items = (rmt_item32_t*) xRingbufferReceive(rb, &rx_size, 0);
  if (!items) return;

  int count = rx_size / sizeof(rmt_item32_t);
  uint8_t cmd=0; bool ok = decodeCmdFromItems(items, count, cmd);

  // Return buffer ASAP
  vRingbufferReturnItem(rb, (void*)items);

  if (!ok) return;

  // Respond based on command
  if (cmd == 0x00){
    sendSTATUS();
  } else if (cmd == 0x01){
    sendPOLL();
  } else {
    // Unhandled cmds (02/03/FF etc.) -> ignore
  }
}