#include <SPI.h>
#include <MFRC522.h>

constexpr uint8_t RST_PIN = 9;
constexpr uint8_t SS_PIN = 10;

MFRC522 mfrc522(SS_PIN, RST_PIN);

bool bwrite = false;


void setup() {
	Serial.begin(9600);
	while (!Serial);
	SPI.begin();
	mfrc522.PCD_Init();
	mfrc522.PCD_DumpVersionToSerial();
	Serial.println("Hold the cartridge tag over the reader..");
}

void loop() {
	while (!mfrc522.PICC_IsNewCardPresent());
	if (bwrite) {
		writeNew();
	}
	else {
		readOld();
	}
}

void readOld() {

}

void writeNew() {

}

void readData() {

}

void writeData() {

}
