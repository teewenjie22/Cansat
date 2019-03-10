#include <Arduino.h>
#include "wiring_private.h"
#include <SPI.h>
#include <SD.h>

//MSGLORA
Uart MSGSerial (&sercom3, 0, 1, SERCOM_RX_PAD_1, UART_TX_PAD_0); // Create the new UART instance assigning it to pin 0 and 1

#define MSGLORABOUDRATE 9600
#define MSGLORAPARAMETER "10,7,1,7"
#define MSGLORABANDWIDTH 868500000
#define MSGLORANETWORK 7
#define MSGLORAMYADDRESS 118
String MSGLORADESTADDR = "117";
#define LORABUFFERSIZE 10
String loraBuffer[LORABUFFERSIZE];
int loraReceive = 0;





//BT
Uart BTSerial (&sercom1, 8, 9, SERCOM_RX_PAD_1, UART_TX_PAD_0); // Create the new UART instance assigning it to pin 0 and 1
bool btStatus = false;
#define BTBUFFERSIZE 10
String btBuffer[BTBUFFERSIZE];
//Message btBuffer[BTBUFFERSIZE] = new Message;
int btReceive = 0;



//SD CARD
File myFile;
// for bluetooth log
char* btMsglog = "bt/btMsgLog.txt";
char* btMsgcounter = "bt/btMsgIdx.txt";
char* btActionList = "bt/btSendQ.txt";

// for lora log
String loraMsglog = "/lora117/lrMsgLog.txt";
String loraSndcounter = "/lora117/lrSndIdx.txt";
String loraRcvcounter = "/lora117/lrRcvIdx.txt";
String loraSendPktQueue = "/lora117/lrPktQ.txt";
String loraSendAckQueue = "/lora117/lrAckQ.txt";
String loraLastAckNumber = "/lora117/lrLstAck.txt";
String loradirectory = "/lora117/";

#define PIN_SD_CS SS1 // SD Card CS pin
#define SDRST 1

class btMessage
{
    String sourceAddr = "";
    String destAddr = "";
    String msg = "";
    String packet = "";

  public:
    btMessage(String packet)
    {
      this->packet = packet;
      sourceAddr = packet.substring(1, 4);
      destAddr = packet.substring(5, 8);
      msg = packet.substring(9, packet.length() - 1);
    }

    String getBtSourceAddr() {
      return sourceAddr;
    }
    String getBtDestAddr() {
      return destAddr;
    }
    String getBtMsg() {
      return msg;
    }
    String getBtMsgPkt() {
      return packet;
    }



};

class loraMessage
{
    // +RCV=50,5,{541,542,thisisthemessage},-99,40
    String sourceAddr = "";
    String msg = "";
    String msgId = "";

  public:
    loraMessage(String packet)
    {
      sourceAddr = packet.substring(packet.indexOf("=", 0) + 1, packet.indexOf(",", 0));
      msg = packet.substring(packet.indexOf("{", 0) + 1, packet.indexOf("}", 0));
      msgId = packet.substring(packet.indexOf("{", 0) + 1, packet.indexOf(";", 0));
    }

    String getLoraSourceAddr() {
      return sourceAddr;
    }
    String getLoraMsg() {
      return msg;
    }
    String getLoraRcvIndex() {
      return msg.substring(0, msg.indexOf(",", 0));
    }
    String getBtObj() {
      return "{" + msg.substring(msg.indexOf(",", 0) + 1) + "}";
    }
    int getMsgType() {
      if (msg.indexOf("ack") == -1 ) return 1;        //received a packret from lora and wanted to forward to bluetooth and send ack back to sender lora
      else if (msg.indexOf("ack") != -1 ) return 2;   //forward pck mode
    }
    String getMsgId() {
      return msgId;
    }

};


void setup() {

  delay(5000);
  Serial.begin(9600);

  /*----MSGlora initiate----*/
  pinPeripheral(0, PIO_SERCOM); //Assign RX function to pin 0
  pinPeripheral(1, PIO_SERCOM); //Assign TX function to pin 1
  MSGLORA_init();
  /*---- end MSGlora initiate----*/

  pinPeripheral(8, PIO_SERCOM); //Assign RX function to pin 0
  pinPeripheral(9, PIO_SERCOM); //Assign TX function to pin 1
  BTSerial.begin(9600);

  //Set baud rate
  BTSerial.begin(9600);
  BTSerial.print("AT");
  delay(100);
  Serial.print(BTSerial.readString());

  //used to check ble name
  BTSerial.print("AT+NAME?");
  Serial.println("SEND --> AT+NAME?");
  delay(100);
  Serial.println(BTSerial.readString());

  sdInit();

}

void loop() {
  // put your main code here, to run repeatedly:

  btRead(); // put received msg from BT into btBuffer
  loraRead();// put receive msg from Lora into loraBuffer
  
  //  // BT loging and action
      for (int i = 0; i < btReceive; i++){
        btMessage btMsgToLog(btBuffer[i]);
  //        Serial.println(btMsgToLog.getBtSourceAddr());
  //        Serial.println(btMsgToLog.getBtDestAddr());
  //        Serial.println(btMsgToLog.getBtMsg());
        bool btLogStat = btMsgLog(btMsgToLog.getBtSourceAddr(), btMsgToLog.getBtDestAddr(), btMsgToLog.getBtMsg()); // log the message to sd card
  
        if (btLogStat) Serial.println("BT LOG Complete");
        else Serial.println("BT LOG fail");
  
        btAction(btMsgToLog, getLineIndex("LORASND"));
        
      }

  // LORA loging and action
      for (int i = 0; i < loraReceive; i++){
        loraMessage loraMsgToLog(loraBuffer[i]);
        Serial.println(loraBuffer[i]);
  //        Serial.println(loraMsgToLog.getLoraSourceAddr());
  //        Serial.println(loraMsgToLog.getLoraMsg());
        bool loraLogStat = loraMsgLog(loraMsgToLog.getLoraSourceAddr(), loraMsgToLog.getLoraMsg()); // log the message to sd card
  
        if (loraLogStat) Serial.println("LORA LOG " + loraMsgToLog.getMsgId() +" is Complete");
        else Serial.println("LORA LOG fail");
  
        loraAction(loraMsgToLog, getLineIndex("LORARCV"), getLineIndex("LORALAN"));
  
  
      }


  // Bt send
        btSendAll();


  //lora send
  loraSendAll();

  Serial.println("----------end of cycle--------");
  int i = 0;
  while(i<5){
    delay(1000);
    i++;
    Serial.println("Unplug now");
  }
}

void MSGLORA_init() {
  MSGSerial.begin(9600);
  delay(20);

  MSGSerial.print("AT\r\n");
  Serial.print("MSGLORA -->AT\r\n");
  while (!MSGSerial.available()) {
    Serial.println("wait1");
  }
  //  if (MSGSerial.readString().startsWith("+OK")) {
  //    while (MSGSerial.available()) {
  //      char clearbuffer = MSGSerial.read();
  //    }
  //    return;
  //  }

   MSGSerial.print("AT+IPR=" + String(MSGLORABOUDRATE) + "\r\n");
   Serial.print("MSGLORA -->AT+IPR=" + String(MSGLORABOUDRATE) + "\r\n");
   Serial.println(MSGSerial.readString());
   delay(20);

   MSGSerial.print("AT+PARAMETER=" + String(MSGLORAPARAMETER) + "\r\n");
   Serial.print("MSGLORA -->AT+PARAMETER=" + String(MSGLORAPARAMETER) + "\r\n");
   Serial.println(MSGSerial.readString());
   delay(20);

   MSGSerial.print("AT+BAND=" + String(MSGLORABANDWIDTH) + "\r\n");    //Bandwidth set to 868.5MHz
   Serial.print("MSGLORA -->AT+BAND=" + String(MSGLORABANDWIDTH) + "\r\n");
   Serial.println(MSGSerial.readString());
   delay(20);

   MSGSerial.print("AT+ADDRESS=" + String(MSGLORAMYADDRESS) + "\r\n");   //needs to be unique
   Serial.print("MSGLORA -->AT+ADDRESS=" + String(MSGLORAMYADDRESS) + "\r\n");
   Serial.println(MSGSerial.readString());
   delay(20);

   MSGSerial.print("AT+NETWORKID=" + String(MSGLORANETWORK) + "\r\n");   //needs to be same for receiver and transmitter
   Serial.print("MSGLORA -->AT+NETWORKID=" + String(MSGLORANETWORK) + "\r\n");
   Serial.println(MSGSerial.readString());
   delay(20);

   while (MSGSerial.available()) {
     char clearbuffer = MSGSerial.read();
   }

}
void SERCOM3_Handler()
{
  MSGSerial.IrqHandler();
}
void SERCOM1_Handler()
{
  BTSerial.IrqHandler();
}
// SD
void sdInit() {while (!SD.begin(PIN_SD_CS)) {
    Serial.println("SD initialization failed!");
  }

  //reset all table
  if (SDRST == 1) {
    SD.remove(btMsglog);
    SD.remove(btMsgcounter);
    SD.remove(btActionList);
    SD.remove(loraMsglog);
    SD.remove(loraRcvcounter);
    SD.remove(loraSndcounter);
    SD.remove(loraLastAckNumber);
    SD.remove(loraSendPktQueue);
    SD.remove(loraSendAckQueue);
    Serial.println("format sd done");

  }
  
  // create bt log if not exist
  if (SD.exists(btMsglog)) {
    //    Serial.print(msglog);
    //    Serial.println(" exists.");
  } 
  else {
    //    Serial.println("Creating  ");
    //    Serial.print(msglog);
    
    myFile = SD.open(btMsglog, FILE_WRITE); // creating file
    if (myFile) Serial.println("Creating " + (String)btMsglog); 
    myFile.close();
  }

  // create lora log if not exist
  if (SD.exists(loraMsglog)) {
    //    Serial.print(msglog);
    //    Serial.println(" exists.");
  } else {
    //    Serial.println("Creating  ");
    //    Serial.print(msglog);
    Serial.println("Creating " + (String)loraMsglog);
    myFile = SD.open(loraMsglog, FILE_WRITE); // creating file
    if (myFile) Serial.println("Creating " + (String)loraMsglog);
    myFile.close();
  }
  
  //print start indicator
  myFile = SD.open(btMsglog, FILE_WRITE);
  if (myFile) {
    Serial.println((String)btMsglog + " --> writing 1st line");
    myFile.println("[************New record starts here*************]");
    myFile.print("[");
    myFile.print("source");
    myFile.print("\t\t");
    myFile.print("dest");
    myFile.print("\t\t");
    myFile.print("msg");
    myFile.println("]");
  }
  else {
    Serial.println((String)btMsglog + " -- > write fail");
  }
  myFile.close();

  myFile = SD.open(loraMsglog, FILE_WRITE);
  if (myFile) {
    Serial.println((String)loraMsglog + " --> writing 1st line");
    myFile.println("[************New record starts here*************]");
    myFile.print("[");
    myFile.print("source");
    myFile.print("\t\t");
    myFile.print("msg");
    myFile.println("]");
  }
  else {
   Serial.println((String)loraMsglog + " -- > write fail");
  }
  myFile.close();

  // create bt counter if not exist
  if (SD.exists(btMsgcounter)) {
    //    Serial.print(msgcountercounter);
    //    Serial.println(" exists.");
  } else {
    //    Serial.println("Creating  ");
    //    Serial.print(msgcounter);
    Serial.println("Creating " + (String)btMsgcounter);
    myFile = SD.open(btMsgcounter, FILE_WRITE); // creating file
    myFile.println("0");
    myFile.close();
  }

  // create lora log if not exist
  if (SD.exists(loraSndcounter)) {
    //    Serial.print(loraSndcounter);
    //    Serial.println(" exists.");
  } else {
    //    Serial.println("Creating  ");
    //    Serial.print(loraSndcounter);
    Serial.println("Creating " + (String)loraSndcounter);
    myFile = SD.open(loraSndcounter, FILE_WRITE); // creating file
    myFile.println("0");
    myFile.close();
  }

  // create lora log if not exist
  if (SD.exists(loraRcvcounter)) {
    //    Serial.print(loraRcvcounter);
    //    Serial.println(" exists.");
  } else {
    //    Serial.println("Creating  ");
    //    Serial.print(loraRcvcounter);
    Serial.println("Creating " + (String)loraRcvcounter);
    myFile = SD.open(loraRcvcounter, FILE_WRITE); // creating file
    myFile.println("-1");
    myFile.close();
  }
  
  if (SD.exists(loraLastAckNumber)) {
    //    Serial.print(loraLastAckNumber);
    //    Serial.println(" exists.");
  } else {
    //    Serial.println("Creating  ");
    //    Serial.print(loraLastAckNumber);
    Serial.println("Creating " + (String)loraLastAckNumber);
    myFile = SD.open(loraLastAckNumber, FILE_WRITE); // creating file
    myFile.println("-1");
    myFile.close();
  }

    if (SD.exists(btActionList)) {
    //    Serial.print(btActionList);
    //    Serial.println(" exists.");
  } else {
    //    Serial.println("Creating  ");
    //    Serial.print(btActionList);
    Serial.println("Creating " + (String)btActionList);
    myFile = SD.open(btActionList, FILE_WRITE); // creating file
    myFile.close();
  }

  if (SD.exists(loraSendPktQueue)) {
    //    Serial.print(loraSendPktQueue);
    //    Serial.println(" exists.");
  } else {
    //    Serial.println("Creating  ");
    //    Serial.print(loraSendPktQueue);
    Serial.println("Creating " + (String)loraSendPktQueue);
    myFile = SD.open(loraSendPktQueue, FILE_WRITE); // creating file
    myFile.close();
  }

  if (SD.exists(loraSendAckQueue)) {
    //    Serial.print(loraSendAckQueue);
    //    Serial.println(" exists.");
  } else {
    //    Serial.println("Creating  ");
    //    Serial.print(loraSendAckQueue);
    Serial.println("Creating " + (String)loraSendAckQueue);
    myFile = SD.open(loraSendAckQueue, FILE_WRITE); // creating file
    myFile.close();
  }
  
  Serial.println("SD INIT DONE");
}

/*------------------------------------------------------------------------------------------*/
// use to check for ack
//if ack not rvc retransmit
void btRead() {
  String recPakt = "";
  int startIndex = 0, endIndex = 0;
  String msg = "";
  btReceive = 0;

  if (BTSerial.available()) {
    delay(1000);
    recPakt = BTSerial.readString();
  //        Serial.print(recPakt);

    if (recPakt.indexOf("OK+CONN") != -1) {
      btStatus = true;
      Serial.println("BT CONNECTION ESTABLISHED");
    }

    if (recPakt.indexOf("OK+LOST") != -1) {
      btStatus = false;
      Serial.println("BT CONNECTION LOSS");
    }

  //    Serial.println(recPakt.length());
    while (endIndex < recPakt.length() - 1 && endIndex > -1 && btStatus && btReceive < BTBUFFERSIZE) {
      startIndex = recPakt.indexOf("{", endIndex);
      endIndex = recPakt.indexOf("}", startIndex + 1);
      if (startIndex < endIndex){
              msg = recPakt.substring(startIndex, endIndex + 1);
        //      Serial.print("<");
        btBuffer[btReceive] = msg;
              Serial.print("BT RCV -->");
              Serial.println(msg);
        //      Serial.println(">");
  //            Serial.println(startIndex);
  //            Serial.println(endIndex);
        btReceive++;
      }

    }

  }
}

int getLineIndex(String type) {
  String filename = "";
  if (type == "BT") filename = btMsgcounter;
  else if (type == "LORARCV") filename = loraRcvcounter;
  else if (type == "LORASND") filename = loraSndcounter;
  else if (type == "LORALAN") filename = loraLastAckNumber;



  myFile = SD.open(filename, FILE_READ);
  String counter;
  if (myFile) {
    while (myFile.available()) {
      char ltr = myFile.read();
      counter += ltr;
    }
  }
  else {
    Serial.println("read fail");
    return -1;
  }
  //  Serial.println(counter);
  myFile.close();
  return counter.toInt();
}

bool btMsgLog(String source, String dest, String msg) {
  myFile = SD.open(btMsglog, FILE_WRITE);
  if (myFile) {
    myFile.print("[");
    myFile.print(source);
    myFile.print("\t\t");
    myFile.print(dest);
    myFile.print("\t\t");
    myFile.print(msg);
    myFile.println("]");
  }
  else {
    Serial.println("read fail");
    return false;
  }
  myFile.close();

  return true;

}

void loraRead() {
  String recPakt = "";
  int startIndex = 0, endIndex = 0;
  String msg = "";
  loraReceive = 0;

  if (MSGSerial.available()) {
    delay(1000);
    recPakt = MSGSerial.readString();
  //        Serial.print(recPakt);


    while (endIndex < recPakt.length() - 1 && endIndex > -1 && startIndex > -1 && loraReceive < LORABUFFERSIZE) {
      startIndex = recPakt.indexOf("+RCV", endIndex);
      endIndex = recPakt.indexOf("\r\n", startIndex);
      if (startIndex < endIndex){
              msg = recPakt.substring(startIndex, endIndex + 1);
      //      Serial.print("<");
      loraBuffer[loraReceive] = msg;
  //            Serial.println(msg);
      //      Serial.println(">");
      //      Serial.println(startIndex);
      //      Serial.println(endIndex);
      loraReceive++;
      }

    }
  //        Serial.println(loraReceive);

  }
}

bool loraMsgLog(String source, String msg) {
  myFile = SD.open(loraMsglog, FILE_WRITE);
  if (myFile) {
    myFile.print("[");
    myFile.print(source);
    myFile.print("\t\t");
    myFile.print(msg);
    myFile.println("]");
  }
  else {
    Serial.println("read fail");
    return false;
  }
  myFile.close();
  return true;

}

void btAction(btMessage &msg, int sndCounter) {
  //receive message from bt and forward the message to main lora(117)

  String pkt = "{" + (String)sndCounter + ";" + msg.getBtSourceAddr() + ";" + msg.getBtDestAddr() + ";" + msg.getBtMsg() + "}";
  myFile = SD.open(loraSendPktQueue, FILE_WRITE);
  if (myFile) {
    Serial.println("Adding 1 line into " + (String)loraSendPktQueue);
    myFile.print("AT+SEND=117,");
    myFile.print(pkt.length());
    myFile.print(",");
    myFile.print(pkt);
    myFile.print("\r\n");
  }
  else
  {
    Serial.println("error on writing action list");
  }
  myFile.close();

  SD.remove(loraSndcounter);
  myFile = SD.open(loraSndcounter, FILE_WRITE); // creating file
  sndCounter += 1;
  myFile.println(sndCounter);
  myFile.close();


}

void loraAction(loraMessage &msg, int rcvCounter, int lastAckNum) {

  switch (msg.getMsgType()) {
    case 1:
      {
        if ( msg.getLoraRcvIndex().toInt() == (rcvCounter + 1) || rcvCounter == -1){
          rcvCounter = msg.getLoraRcvIndex().toInt();
          // send acks
          String pkt = "{" + (String)rcvCounter + ";ack}";
          myFile = SD.open(loraSendAckQueue, FILE_WRITE);
          if (myFile) {
            myFile.print("AT+SEND=" + msg.getLoraSourceAddr() + ",");
            myFile.print(pkt.length());
            myFile.print(",");
            myFile.println(pkt);
          }
          else
          {
            Serial.println("error on writing action list");
          }
          myFile.close();

          btMessage btObj(msg.getBtObj());
          myFile = SD.open(btActionList, FILE_WRITE);
          if (myFile) {
            myFile.println(btObj.getBtMsg());
          }
          else
          {
            Serial.println("error on writing action list");
          }

          SD.remove(loraRcvcounter);
          myFile = SD.open(loraRcvcounter, FILE_WRITE); // creating file
          myFile.println(rcvCounter);
          myFile.close();
         
        }
        else {
          
          String pkt = "{" + (String)rcvCounter + ";ack}";
          myFile = SD.open(loraSendAckQueue, FILE_WRITE);
          if (myFile) {
            myFile.print("AT+SEND=" + msg.getLoraSourceAddr() + ",");
            myFile.print(pkt.length());
            myFile.print(",");
            myFile.println(pkt);
         }
       }



        

        break;
      }
    // when lora receive ack pkt
    case 2: {
        
        if (msg.getLoraRcvIndex().toInt() == (lastAckNum + 1) || lastAckNum == -1 ) {
          Serial.println("ack mode");
          SD.remove(loraLastAckNumber);
          myFile = SD.open(loraLastAckNumber, FILE_WRITE); // creating file
          lastAckNum  = msg.getLoraRcvIndex().toInt();
          myFile.println(lastAckNum);
          myFile.close();

          SD.remove("temp1.txt");

          File temp1;
          temp1 = SD.open("temp1.txt", FILE_WRITE);
          myFile = SD.open(loraSendPktQueue, FILE_READ);
          if (!temp1) Serial.println("Error");

          while (myFile.available()) {
            if (myFile.read() == '\n') break;
            
          }
          while (myFile.available())
          temp1.write(myFile.read());

          myFile.close();
          temp1.close();
          SD.remove(loraSendPktQueue);

          temp1 = SD.open("temp1.txt", FILE_READ);
          myFile = SD.open(loraSendPktQueue, FILE_WRITE);

          while (temp1.available()) {
            myFile.write(temp1.read()); 
          }

          myFile.close();
          temp1.close();
          SD.remove("temp1.txt");
        }
        break;
      }
    default:
      break;
  }


}
void btSendAll() {
  myFile = SD.open(btActionList, FILE_READ);
  while (myFile.available()) {
    BTSerial.write(myFile.read());

  }
  myFile.close();
}

void loraSendAll() {
  myFile = SD.open(loraSendPktQueue, FILE_READ);
  String msgToSend = "";
  while (myFile.available()) {
      if (myFile.peek() == '\n'){
        MSGSerial.write(myFile.read());
        Serial.println("Send 1 Line");
        delay(1000);
      }
      else MSGSerial.write(myFile.read());
  }
  myFile.close();
 
}
void loraSendAckAll() {
  myFile = SD.open(loraSendAckQueue, FILE_READ);
  String msgToSend = "";
  while (myFile.available()) {
      if (myFile.peek() == '\n'){
        MSGSerial.write(myFile.read());
        Serial.println("Send 1 Line");
        delay(1000);
      }
      else MSGSerial.write(myFile.read());
  }
  myFile.close();
  SD.remove(loraSendAckQueue);
}
