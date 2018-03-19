#include <SPI.h>
#include <MFRC522.h>

#ifndef ROTL
# define ROTL(x,n) (((uintmax_t)(x) << (n)) | ((uintmax_t)(x) >> ((sizeof(x) * 8) - (n))))
#endif

enum temp : uint8_t { //in steps of 10
	c190 = 19,
	c200 = 20,
	c210 = 21
};
enum flen : uint8_t { //in steps of 50
	m200 = 4,
	m300 = 6
};

//USER-VARS:
static const temp temperature = temp::c190; //190, 200, 210
static const flen spool_length = flen::m200; //200, 300

//SOURCE:
constexpr uint8_t RST_PIN = 9;
constexpr uint8_t SS_PIN = 10;
static const uint8_t led = 2;

#define AUTH0 8 //page from which password auth is required
#define PROT 1 //if write (0) or read and write (1) needs auth
#define AUTHLIM 0 //max number of auth attemps (0 = inf)

MFRC522 mfrc522(SS_PIN, RST_PIN);

const uint32_t PROGMEM c[] = {
	0x6D835AFC, 0x7D15CD97, 0x0942B409, 0x32F9C923, 0xA811FB02, 0x64F121E8,
	0xD1CC8B4E, 0xE8873E6F, 0x61399BBB, 0xF1B91926, 0xAC661520, 0xA21A31C9,
	0xD424808D, 0xFE118E07, 0xD18E728D, 0xABAC9E17, 0x18066433, 0x00E18E79,
	0x65A77305, 0x5AE9E297, 0x11FC628C, 0x7BB3431F, 0x942A8308, 0xB2F8FD20,
	0x5728B869, 0x30726D5A
};

struct str {
	const char* str PROGMEM;
};
const char STATPREF[] = "STATUS_";
const str STATSTRS[] PROGMEM = {
	{"OK"},
	{"ERROR"},
	{"COLLISION"},
	{"TIMEOUT"},
	{"NO_ROOM"},
	{"INTERNAL_ERROR"},
	{"INVALID"},
	{"CRC_WRONG"},
	{"MIFARE_NACK"}
};

struct page {
	const uint8_t pnum;
	const byte data[4];
};
const page PROGMEM pages[] = { //copied from soliforum page and ReelTool app source
	{ 0x04, {0x01, 0x03, 0xA0, 0x0C} },
	{ 0x05, {0x34, 0x03, 0x00, 0xFE} },
	{ 0x06, {0x00, 0x00, 0x00, 0x00} },
	{ 0x07, {0x00, 0x00, 0x00, 0x00} },
	{ 0x09, {0x00, 0x35, 0x34, 0x54} },
	{ 0x0C, {0xD2, 0x00, 0x2D, 0x00} },
	{ 0x0D, {0x54, 0x48, 0x47, 0x42} },
	{ 0x0E, {0x30, 0x33, 0x33, 0x38} },
	{ 0x0F, {0x00, 0x00, 0x00, 0x00} },
	{ 0x10, {0x00, 0x00, 0x00, 0x00} },
	{ 0x11, {0x34, 0x00, 0x00, 0x00} },
	{ 0x12, {0x00, 0x00, 0x00, 0x00} },
	{ 0x13, {0x00, 0x00, 0x00, 0x00} },
};

template <typename T> T pgm_getdata(const T * sce);

void setup() {
	Serial.begin(115200);
	while (!Serial);
	SPI.begin();
	mfrc522.PCD_Init();

	pinMode(led, OUTPUT);
}

void loop() {
	digitalWrite(led, HIGH);
	writeNew();
	digitalWrite(led, LOW);
	delay(3000);
}

void writeNew() {
	Serial.println(F("Hold new tag over reader.."));
	while (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial());
	delay(100); //wait for tag to get into range

	MFRC522::StatusCode status;

	//calculate key and pack
	uint8_t k[4],
		p[2];
	getpack(p, mfrc522.uid.uidByte);
	getkey(k, mfrc522.uid.uidByte);

	//write static pages
	uint8_t pg[4];
	for (int i = 0; i < sizeof(pages) / sizeof(page); i++) {
		memcpy_P(pg, pages[i].data, 4);
		if (SECURE_WRITEPAGE(pgm_getdata<uint8_t>(&pages[i].pnum), pg)) return;
	}

	//write temperature
	pg[0] = 0x5A; pg[1] = 0x50; pg[2] = 0x00; pg[3] = 0x00;
	switch (temperature * 10) {
		case 190:
			pg[2] = 0x40;
			break;
		case 200:
			pg[2] = 0x45; 
			break;
		case 210:
			pg[2] = 0x50;
			break;
		default:
			return;
	}
	if (SECURE_WRITEPAGE(0x08, pg)) return;

	//write spool length
	switch (spool_length * 50) {
		case 200:
			//write len
			pg[0] = 0x40; pg[1] = 0x0D; pg[2] = 0x03; pg[3] = 0x00;
			if (SECURE_WRITEPAGE(0x14, pg)) return;
			//checksum
			pg[0] = 0x08; pg[1] = 0x1F; pg[2] = 0x31; pg[3] = 0x54;
			if (SECURE_WRITEPAGE(0x15, pg)) return;
			pg[0] = 0x50; pg[1] = 0xB1; pg[2] = 0xE0; pg[3] = 0xCE;
			if (SECURE_WRITEPAGE(0x16, pg)) return;
			pg[0] = 0x52; pg[1] = 0xE7; pg[2] = 0x4F; pg[3] = 0x76;
			if (SECURE_WRITEPAGE(0x17, pg)) return;
			break;
		case 300:
			//write len
			pg[0] = 0xE0; pg[1] = 0x93; pg[2] = 0x04; pg[3] = 0x00;
			if (SECURE_WRITEPAGE(0x14, pg)) return;
			//checksum
			pg[0] = 0xA8; pg[1] = 0x81; pg[2] = 0x36; pg[3] = 0x54;
			if (SECURE_WRITEPAGE(0x15, pg)) return;
			pg[0] = 0xF0; pg[1] = 0x3F; pg[2] = 0xEE; pg[3] = 0xCE;
			if (SECURE_WRITEPAGE(0x16, pg)) return;
			pg[0] = 0xF2; pg[1] = 0x6E; pg[2] = 0x4D; pg[3] = 0x76;
			if (SECURE_WRITEPAGE(0x17, pg)) return;
			break;
		default:
			return;
	}

	//lock tag with key and pack
	PCD_NTAG21X_SETPWD(k, p);

	byte cmdBuf[18];
	//set write-protection
	Serial.println(F("Enabling Write-Protection.."));
	uint8_t buflen = 18;
	status = mfrc522.MIFARE_Read(0x2A, cmdBuf, &buflen);
	printStat(status);
	if (status) return;
	pg[0] = ((cmdBuf[0] & 0x78) | ((PROT & 1U) * 0x80) | (AUTHLIM & 0x07)) ; pg[1] = 0x00; pg[2] = 0x00; pg[3] = 0x00;
	if(SECURE_WRITEPAGE(0x2A, pg)) return;

	//protect storage at page 8
	Serial.println(F("Protecting Storage.."));
	buflen = 18;
	status = mfrc522.MIFARE_Read(0x29, cmdBuf, &buflen);
	printStat(status);
	if (status) return;
	pg[0] = cmdBuf[0]; pg[1] = 0; pg[2] = cmdBuf[1]; pg[3] = (AUTH0 & 0xFF);
	if(SECURE_WRITEPAGE(0x29, pg)) return;

	//blink 3 times to show successful write
	for (int i = 0; i < 3; i++) {
		digitalWrite(led, LOW);
		delay(200);
		digitalWrite(led, HIGH);
		delay(200);
	}
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

/*
* Set key of NTAG213
*
* Implemented by Sinitax.
*/
void PCD_NTAG21X_SETPWD(byte* pwd, byte* pack) //4 byte key, 2 pyte pack
{
	byte packp = 0x2C,
		pwdp = 0x2B;

	//write pack
	byte page[4];
	memcpy(page, pack, 2);
	page[2] = 0; //set rfuid 0
	page[3] = 0;
	Serial.println(F("Writing Pack:"));
	if (SECURE_WRITEPAGE(packp, page)) return;

	//write password
	Serial.println(F("Writing Pass:"));
	if (SECURE_WRITEPAGE(pwdp, pwd)) return;
}

bool SECURE_WRITEPAGE(byte pn, byte* p) {
	while (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()); //wait for card if bad connection
	Serial.print("Write "); Serial.print("0x"); Serial.print(pn, HEX); Serial.print(F(" : "));
	MFRC522::StatusCode ret = mfrc522.MIFARE_Ultralight_Write(pn, p, 4);
	delay(1);
	printStat(ret);
	if (ret) blink();
	return ret;
}

void blink() {
	//blink once for error
	digitalWrite(led, LOW);
	delay(200);
	digitalWrite(led, HIGH);
	delay(200);
}

void printStat(uint8_t id) {
	Serial.print(STATPREF);
	Serial.println(pgm_getdata<str>(&STATSTRS[id]).str);
}

void PICC_DumpNTAG21XToSerial() {
	MFRC522::StatusCode status;
	byte byteCount;
	byte buffer[18];
	byte i;

	Serial.println(F("Page  0  1  2  3"));
	// Try the mpages of the original Ultralight. Ultralight C has more pages.
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
} // End PICC_DumpMifareUltralightToSerial()

template <typename T> T pgm_getdata(const T * sce)
{
	static T temp;
	memcpy_P(&temp, sce, sizeof(T));
	return temp;
}