#include <SPI.h>
#include <MFRC522.h>

#ifndef ROTL
# define ROTL(x,n) (((uintmax_t)(x) << (n)) | ((uintmax_t)(x) >> ((sizeof(x) * 8) - (n))))
#endif

constexpr uint8_t RST_PIN = 9;
constexpr uint8_t SS_PIN = 10;
static const uint8_t led = 2;

bool bwrite = false;

MFRC522 mfrc522(SS_PIN, RST_PIN);

const uint32_t PROGMEM c[] = {
	0x6D835AFC, 0x7D15CD97, 0x0942B409, 0x32F9C923, 0xA811FB02, 0x64F121E8,
	0xD1CC8B4E, 0xE8873E6F, 0x61399BBB, 0xF1B91926, 0xAC661520, 0xA21A31C9,
	0xD424808D, 0xFE118E07, 0xD18E728D, 0xABAC9E17, 0x18066433, 0x00E18E79,
	0x65A77305, 0x5AE9E297, 0x11FC628C, 0x7BB3431F, 0x942A8308, 0xB2F8FD20,
	0x5728B869, 0x30726D5A
};

void setup() {
	Serial.begin(115200);
	while (!Serial);
	SPI.begin();
	mfrc522.PCD_Init();

	pinMode(led, OUTPUT);
  digitalWrite(led, LOW);
}

void loop() {
  readOld();
}

void readOld() {
	Serial.println(F("Hold the cartridge tag over the reader.."));
	while (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial());
	delay(100); //wait for tag to get into range

	//get key and pack
	uint8_t k[4],
		p[2];
	getkey(k, mfrc522.uid.uidByte);
	getpack(p, mfrc522.uid.uidByte);
	Serial.print(F("UID:")); printHex((uint8_t*)mfrc522.uid.uidByte, 7);
	Serial.print(F("KEY:")); printHex(k, 4);
	Serial.print(F("PACK:")); printHex(p, 2);

	uint8_t pb[2];
	bool status = !mfrc522.PCD_NTAG216_AUTH(k, pb); //STATUS OK
	printHex(pb, 2);
	if (status && !memcmp(p, pb, 2)) {
		Serial.println(F("Authenticated! Reading Data."));
		PICC_DumpNTAG21XToSerial();
    digitalWrite(led, HIGH);
    delay(200);
    digitalWrite(led, LOW);
	}
	else Serial.println(F("Read Failed."));
	Serial.println();
	delay(3000);
}

void printHex(uint8_t array[], unsigned int len) {
	char buffer[3];
	buffer[2] = NULL;
	for (int j = 0; j < len; j++) {
		sprintf(&buffer[0], "%02X", array[j]);
		Serial.print(buffer);
	} Serial.println();
}

// NFC-KEY derivation
void transform(uint8_t* ru)
{
	//Transform
	uint8_t i;
	uint8_t p = 0;
	uint32_t v1 = (((uint32_t)ru[3] << 24) | ((uint32_t)ru[2] << 16) | ((uint32_t)ru[1] << 8) | (uint32_t)ru[0]) + pgm_getdata<uint32_t>(c + p); p++;
	uint32_t v2 = (((uint32_t)ru[7] << 24) | ((uint32_t)ru[6] << 16) | ((uint32_t)ru[5] << 8) | (uint32_t)ru[4]) + pgm_getdata<uint32_t>(c + p); p++;

	for (i = 0; i < 12; i += 2)
	{
		uint32_t t1 = ROTL(v1 ^ v2, v2 & 0x1F) + pgm_getdata<uint32_t>(c + p); p++;
		uint32_t t2 = ROTL(v2 ^ t1, t1 & 0x1F) + pgm_getdata<uint32_t>(c + p); p++;
		v1 = ROTL(t1 ^ t2, t2 & 0x1F) + pgm_getdata<uint32_t>(c + p); p++;
		v2 = ROTL(t2 ^ v1, v1 & 0x1F) + pgm_getdata<uint32_t>(c + p); p++;
	}

	//Re-use ru
	ru[0] = v1 & 0xFF;
	ru[1] = (v1 >> 8) & 0xFF;
	ru[2] = (v1 >> 16) & 0xFF;
	ru[3] = (v1 >> 24) & 0xFF;
	ru[4] = v2 & 0xFF;
	ru[5] = (v2 >> 8) & 0xFF;
	ru[6] = (v2 >> 16) & 0xFF;
	ru[7] = (v2 >> 24) & 0xFF;
}

void getkey(uint8_t* key, uint8_t* uid) //pass pointer to array
{
	int i;
	//Rotate
	uint8_t r = (uid[1] + uid[3] + uid[5]) & 7; //Rotation offset
	uint8_t ru[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; //Rotated UID
	for (i = 0; i < 7; i++)
		ru[(i + r) & 7] = uid[i];

	//Transform
	transform(ru);

	//Calc key
	r = (ru[0] + ru[2] + ru[4] + ru[6]) & 3; //Offset
	uint8_t k[4] = { ru[r + 3], ru[r + 2], ru[r + 1], ru[r] };
	memcpy(key, k, 4);
}

void getpack(uint8_t* pack, const uint8_t* uid) //pass pointer to array
{
	int i;
	//Rotate
	uint8_t r = (uid[2] + uid[5]) & 7; //Rotation offset
	uint8_t ru[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; //Rotated UID
	for (i = 0; i < 7; i++)
		ru[(i + r) & 7] = uid[i];

	//Transform
	transform(ru);

	//Calc pack
	uint16_t _p = 0;
	for (i = 0; i < 8; i++)
		_p += ru[i] * 13;

	_p = (_p ^ 0x5555) & 0xFFFF;
	uint8_t p[2] = { (uint8_t)(_p & 0xFF), (uint8_t)(_p >> 8) };
	memcpy(pack, p, 2);
}

MFRC522::StatusCode PCD_NTAG21X_SETPWD(byte* pwd, byte* pack, uint8_t tagtype) //4 byte key, 2 pyte pack
{
	MFRC522::StatusCode result;
	byte packp = 0x2C,
		pwdp = 0x2B;

	//write pack
	byte page[4];
	memcpy(page, pack, 2);
	page[2] = 0; //set rfuid 0
	page[3] = 0;
	result = mfrc522.MIFARE_Ultralight_Write(packp, page, 4);
	if (result != MFRC522::STATUS_OK) {
		Serial.println(F("WRITE_PACK_FAIL"));
		return result;
	}

	//write password
	result = mfrc522.MIFARE_Ultralight_Write(pwdp, pwd, 4);
	if (result != MFRC522::STATUS_OK) {
		Serial.println(F("WRITE_PASS_FAIL"));
		return result;
	}

	return  MFRC522::STATUS_OK;
}

void PICC_DumpNTAG21XToSerial() {
	MFRC522::StatusCode status;
	byte byteCount;
	byte buffer[18];
	byte i;

	Serial.println(F("Page  0  1  2  3"));
	for (byte page = 0; page <= 0x2C; page += 4) { // Read returns data for 4 pages at a time. Tag has 0x2c pages
												// Read pages
		byteCount = sizeof(buffer);
		status = mfrc522.MIFARE_Read(page, buffer, &byteCount);
		if (status != MFRC522::STATUS_OK) {
			Serial.print(F("MIFARE_Read() failed: "));
			Serial.println(mfrc522.GetStatusCodeName(status));
			break;
		}
		// Dump data
		for (byte offset = 0; offset < 4; offset++) {
			i = page + offset;
			if (i < 10)
				Serial.print(F("  ")); // Pad with spaces
			else
				Serial.print(F(" ")); // Pad with spaces
			Serial.print(i,HEX);
			Serial.print(F("  "));
			for (byte index = 0; index < 4; index++) {
				i = 4 * offset + index;
				if (buffer[i] < 0x10)
					Serial.print(F(" 0"));
				else
					Serial.print(F(" "));
				Serial.print(buffer[i], HEX);
			}
			Serial.println();
		}
	}
}

template <typename T> T pgm_getdata(const T * sce)
{
	static T temp;
	memcpy_P(&temp, sce, sizeof(T));
	return temp;
}
