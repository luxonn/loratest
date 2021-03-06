// rf95_server.pde
// -*- mode: C++ -*-
// Example sketch showing how to create a simple messageing server
// with the RH_RF95 class. RH_RF95 class does not provide for addressing or
// reliability, so you should only use RH_RF95  if you do not need the higher
// level messaging abilities.
// It is designed to work with the other example rf95_client
// Tested with Anarduino MiniWirelessLoRa

#include <EEPROM.h>
#include <SPI.h>
#include <RH_RF95.h>

typedef struct {
	uint8_t type;
	uint8_t count;  // counter
	uint8_t initiator, replier;
	int rssi;       // signal strength
} pingpong_t;

// Singleton instance of the radio driver
static RH_RF95 rf95;
static pingpong_t ping;
static pingpong_t pong;

#define T_PING 1
#define T_PONG 2

void genId(const bool update) {
	const bool validMarker = EEPROM.read(1) == 123;

	if (validMarker && !update)
		ping.initiator = EEPROM.read(0);
	else {
		Serial.println(F("Update/set ID"));
		ping.initiator = rand();
		if (!validMarker)
			EEPROM.write(1, 123);
		EEPROM.write(0, ping.initiator);
		delay(5);
		EEPROM.read(1);
		EEPROM.read(0);
	}
}

void setup() {
	Serial.begin(115200);
	Serial.println(F("Init LoRa test"));

	if (!rf95.init())
		Serial.println(F("init failed"));
	// Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on
	rf95.setFrequency(869.525);
	rf95.setModemConfig(RH_RF95::Bw125Cr48Sf4096);

	int r;
	for(uint8_t i=A0; i<=A7; i++)
		r ^= analogRead(i);
	srandom(r);
	srand(~r);

	//    rf95.setTxPower(20);

	// init ping
	genId(false);
	ping.type = T_PING;
	ping.count = 0;
	ping.rssi = 0;
	pong.replier = -1;

	Serial.print(F("Hello World from "));
	Serial.println(ping.initiator);

	pong.initiator = -1;
	pong.replier = ping.initiator;
}

static char line[128];
static uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];

void loop()
{
	// send a ping
	ping.count++;

	sprintf(line, "Sending PING(%d,%d) ...", ping.count, ping.rssi);
	Serial.println(line);

	rf95.send((uint8_t *)&ping, sizeof(ping));
	rf95.waitPacketSent();

	// wait some random time in reception mode
	unsigned long wait = random(1000, 5000);
	sprintf(line, "Listening for %lu ms ...", wait);    
	Serial.println(line);

	unsigned long now = millis();
	while (millis() - now < wait) {
		uint8_t len = sizeof buf;

		if (rf95.available()) {
			memset(buf, 0x00, sizeof buf);
			rf95.recv(buf, &len);

			if (len == sizeof(pong)) {
				memcpy(&pong, buf, sizeof(pong));

				bool col = false;
				if (pong.type == T_PING) {
					col = pong.initiator == ping.initiator;
					// got ping
					sprintf(line, "Recv PING(%d,%d,%d)", pong.count, pong.rssi, pong.initiator);
					Serial.println(line);
					// send pong
					pong.type = T_PONG;
					pong.rssi = rf95.lastRssi();
					pong.replier = ping.initiator;
					sprintf(line, "Send PONG(%d,%d,%d)", pong.count, pong.rssi, pong.replier);
					Serial.println(line);
					rf95.send((uint8_t *)&pong, sizeof(pong));
					rf95.waitPacketSent();
				}
				else if (pong.type == T_PONG) {
					col = pong.replier == ping.initiator;
					// got pong, print stats
					sprintf(line, "Got PONG(%d,%d,%d)", pong.count, pong.rssi, pong.replier);
					Serial.println(line);
				}
				else {
					sprintf(line, "???(%d,%d,%d,%d,%d)", pong.count, pong.rssi, pong.initiator, pong.replier, pong.type);
					Serial.println(line);
				}

				if (col) {
					sprintf(line, "ID COL: %d,%d", pong.initiator, pong.replier);
					if (pong.initiator == ping.initiator) {
						Serial.println(F("ID collision, force new"));
						genId(true);
						Serial.print(F("New ID:"));
						Serial.println(ping.initiator);
					}
				}
			} else {
				Serial.println(F("Got spurious message: "));
				for(uint8_t i=0; i<len; i++) {
					Serial.print(buf[i], HEX);
					Serial.print(' ');
				}
				Serial.println(F(""));
				for(uint8_t i=0; i<len; i++)
					Serial.print((char)buf[i]);
				Serial.println(F(""));
			}
		}
	}
}
