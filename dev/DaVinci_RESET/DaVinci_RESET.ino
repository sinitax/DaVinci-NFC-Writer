#include <SPI.h>
#include <MFRC522.h>

#ifndef ROTL
# define ROTL(x,n) (((uintmax_t)(x) << (n)) | ((uintmax_t)(x) >> ((sizeof(x) * 8) - (n))))
#endif

constexpr uint8_t RST_PIN = 9;
constexpr uint8_t SS_PIN = 10;
static const uint8_t led = 2;

bool bwrite = false;

#define NTAG213_PWD 0x2B
#define NTAG213_PCK 0x2C

MFRC522 mfrc522(SS_PIN, RST_PIN);

const uint32_t PROGMEM c[] = {
	0x6D835AFC, 0x7D15CD97, 0x0942B409, 0x32F9C923, 0xA811FB02, 0x64F121E8,
	0xD1CC8B4E, 0xE8873E6F, 0x61399BBB, 0xF1B91926, 0xAC661520, 0xA21A31C9,
	0xD424808D, 0xFE118E07, 0xD18E728D, 0xABAC9E17, 0x18066433, 0x00E18E79,
	0x65A77305, 0x5AE9E297, 0x11FC628C, 0x7BB3431F, 0x942A8308, 0xB2F8FD20,
	0x5728B869, 0x30726D5A
};

//MFRC522::StatusCode PCD_NTAG213_AUTH(byte *pwd, byte *pck);
//MFRC522::StatusCode PCD_NTAG213_SETPWD(byte *pwd, byte *pck);
template <typename T> T pgm_getdata(const T * sce);

void setup() {
	Serial.begin(115200);
	while (!Serial);
	SPI.begin();
	mfrc522.PCD_Init();

	//pinMode(2, OUTPUT);
	readOld();
}

void loop() {
	//digitalWrite(led, bwrite);
	//bwrite ? writeNew() : readOld();
}

void readOld() {
	Serial.println(F("Hold the cartridge tag over the reader.."));
	while (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial());
	uint8_t* k = getkey(mfrc522.uid.uidByte);
	uint8_t* p = getpack(mfrc522.uid.uidByte);
	Serial.print(F("UID:")); printHex((uint8_t*)mfrc522.uid.uidByte, 7);
	Serial.print(F("KEY:")); printHex(k, 4);
	Serial.print(F("PACK:")); printHex(p, 2);

	uint8_t pb[2];
	bool status = !mfrc522.PCD_NTAG216_AUTH(k, pb); //STATUS OK
	printHex(pb, 2);
	if (status && memcmp(p, pb, 2) == 0) {
		Serial.println(F("Authenticated! Reading Data."));
		mfrc522.PICC_DumpMifareUltralightToSerial();
		bwrite = true;
	}
	else Serial.println(F("Read Failed."));
	delay(4000);
}

void writeNew() {
	Serial.println(F("Hold paper tag over reader.."));
	while (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial());

	bwrite = false;
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

uint8_t* getkey(uint8_t* uid)
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
	delay(1);

	return k;
}

uint8_t* getpack(uint8_t* uid)
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
	uint8_t p[] = { (uint8_t)(_p & 0xFF), (uint8_t)(_p >> 8) };
	delay(1);

	return p;
}

/*
* Authenticate with a NTAG213
*
* Implemented by Sinitax.
*/
MFRC522::StatusCode MFRC522::PCD_NTAG213_AUTH(byte* pwd, byte* pack) //4 byte pass, 2 byte pack
{
	MFRC522::StatusCode result;
	byte cmdBuffer[7]; //since 1 byte cmd, 4 bytes pwd, 2 bytes crc

	cmdBuffer[0] = 0x1B; //auth command
	memcpy(&cmdBuffer[1], pwd, 4); //pwd

	result = PCD_CalculateCRC(cmdBuffer, 5, &cmdBuffer[5]); //append crc to cmdBuffer

	if (result != STATUS_OK) {
		Serial.println(F("crcfail"));
		return result;
	}

	// Transceive the data, store the reply in cmdBuffer[]
	byte rxlength = 7;
	result = PCD_TransceiveData(cmdBuffer, 7, cmdBuffer, &rxlength);

	memcpy(pack, cmdBuffer, 2); 

	if (result != STATUS_OK) {
		return result;
	}
	return STATUS_OK;
}

/*
* Set key of NTAG213
*
* Implemented by Sinitax.
*/
MFRC522::StatusCode MFRC522::PCD_NTAG213_SETPWD(byte* pwd, byte* pack) //4 byte key, 2 pyte pack
{
	MFRC522::StatusCode result;
	byte cmdBuffer[18]; // We need room for 16 bytes data and 2 bytes CRC_A.
	cmdBuffer[0] = 0x1B; //auth command

	memcpy(cmdBuffer + 1, pwd, 4);

	result = PCD_CalculateCRC(cmdBuffer, 5, &cmdBuffer[5]); //append crc to cmdBuffer if successful

	if (result != STATUS_OK) {
		return result;
	}

	// Transceive the data, store the reply in cmdBuffer
	byte validBits = 0;
	byte rxlength = 4;
	result = PCD_TransceiveData(cmdBuffer, 7, cmdBuffer, &rxlength, &validBits);

	memcpy(pack, cmdBuffer, 2);

	
	if (result != STATUS_OK) {
		return result;
	}
	return STATUS_OK;
}

template <typename T> T pgm_getdata(const T * sce)
{
	static T temp;
	memcpy_P(&temp, sce, sizeof(T));
	return temp;
}