## DaVinci Tag Cloner

### Requirements:

* Arduino
* MFRC522 Module
* LED
* NTAG21X Paper Tag

### Usage:

To write a new tag, open the *WritePaperTag.ino* sketch and set the temperature and spool length in the **USER-VARS** section to one of the given values. If you want to read from an old tag, you can open the *ReadCartridgeTag.ino* sketch to dump its contents.
 
### Sources:

* [MFRC522 lib](https://github.com/miguelbalboa/rfid)
* [NFCKey Algo](https://github.com/jackfagner/NfcKey)
* [ReelTool source](http://www.soliforum.com/topic/17124/android-app-for-resettingwriting-blank-ntags/)