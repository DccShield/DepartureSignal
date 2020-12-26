// 
// 出発信号機アクセサリデコーダ
// http://dcc.client.jp
// http://ayabu.blog.shinobi.jp

#include <arduino.h>
#include <avr/eeprom.h>   //required by notifyCVRead() function if enabled below
#include "DccCV.h"
#include "Cds.h"
#include "3ColorLightSignal.h"
#include "NmraDcc.h"


#define DEBUG      //リリースのときはコメントアウトすること
//#define DEBUG_M    //リリースのときはコメントアウトすること 速度・ファンクションパケット表示

//各種設定、宣言

void Dccinit(void);

//使用クラスの宣言
NmraDcc   Dcc;
DCC_MSG  Packet;

ThirdColorLightSignal StrSignal(9,10,11,12);
ThirdColorLightSignal DivSignal(5,6,7,8);
Cds CdsSensorStr(A0, 2, 200,200);
Cds CdsSensorDiv(A1, 2, 200,200);
 
struct CVPair {
  uint16_t  CV;
  uint8_t Value;
};

CVPair FactoryDefaultCVs [] =
{
  {CV_ACCESSORY_DECODER_ADDRESS_LSB, 1},
  {CV_ACCESSORY_DECODER_ADDRESS_MSB, 0},
  {CV_29_CONFIG, 0b10000000},             // CV29 Software sw CV29＝128 アクセサリデコーダ
//  {CV_MULTIFUNCTION_PRIMARY_ADDRESS, DECODER_ADDRESS}, // CV01
//  {CV_ACCESSORY_DECODER_ADDRESS_MSB, 0},               // CV09 The LSB is set CV 1 in the libraries .h file, which is the regular address location, so by setting the MSB to 0 we tell the library to use the same address as the primary address. 0 DECODER_ADDRESS
//  {CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB, 0},          // CV17 XX in the XXYY address
//  {CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB, 0},          // CV18 YY in the XXYY address
//  {CV_29_CONFIG, 128 },                                // CV29 Make sure this is 0 or else it will be random based on what is in the eeprom which could caue headaches
};

void(* resetFunc) (void) = 0;  //declare reset function at address 0

//uint8_t FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs) / sizeof(CVPair);
uint8_t FactoryDefaultCVIndex = 0;

void notifyDccReset(uint8_t hardReset );

//------------------------------------------------------------------
// Arduino固有の関数 setup() :初期設定
//------------------------------------------------------------------
void setup()
{
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("Hello,DepartureSignal");
#endif
  
  Dccinit();
 
  //Reset task
  gPreviousL5 = millis();
}

//---------------------------------------------------------------------
// Arduino main loop
//---------------------------------------------------------------------
void loop() {
  // You MUST call the NmraDcc.process() method frequently from the Arduino loop() function for correct library operation
  Dcc.process();

  if( FactoryDefaultCVIndex && Dcc.isSetCVReady())    // この処理何？ Dcc.isSetCVReady() はEEPROMが使用可能か確認,1で書き込み可能
  {
    FactoryDefaultCVIndex--; // Decrement first as initially it is the size of the array 
    Dcc.setCV( FactoryDefaultCVs[FactoryDefaultCVIndex].CV, FactoryDefaultCVs[FactoryDefaultCVIndex].Value);
  }

  if((millis() - gPreviousL5) >= 100){
    gPreviousL5 = millis();
    CloseSignalState();
    StrSignal.statechk();
    DivSignal.statechk();
  }
}

void CloseSignalState( void ){
  enum{
      STS_INIT = 0,
      STS_IDLE,
      STS_STR,
      STS_STR2,
      STS_STRIDLE,
      STS_DIV,
      STS_DIV2,
      STS_DIVIDLE,
  };
  static int state = STS_INIT;

  switch(state){
    case STS_INIT:
                  StrSignal.ChangeState(ST_ADVANCE);
                  DivSignal.ChangeState(ST_STOP);
                  state = STS_STRIDLE;                  // 初回はSTRIDLE
                  break;
    case STS_IDLE:
                  break;
    case STS_STR:
                  DivSignal.ChangeState(ST_STOP);       // 分岐停止
                  state = STS_STR2;
                  break;
    case STS_STR2:  // STR.DIVが同時に切り替わらない様に100ms遅らせる
                  StrSignal.ChangeState(ST_CAUTION);    // 直進注意
                  state = STS_STRIDLE;
                  break;
    case STS_STRIDLE:
                  if(CdsSensorStr.statechk(LOW) == 1){  // STR側 在線検出
                    StrSignal.ChangeState(ST_STOP);     // 停止信号
                    CdsSensorStr.Reset();
                    state = STS_STRIDLE;
                  }
                  if( gCTevent == 1 ){
                    gCTevent = 0;
                    if(gDir == 0){
                      state = STS_DIV; 
                    } else if(gDir == 1){
                      state = STS_STR;
                    }
                  }
                  break;
    case STS_DIV:
                  StrSignal.ChangeState(ST_STOP);       // 直進停止
                  state = STS_DIV2;
                  break;
    case STS_DIV2:  // STR.DIVが同時に切り替わらない様に100ms遅らせる
                  DivSignal.ChangeState(ST_CAUTION);    // 分岐注意
                  state = STS_DIVIDLE;
                  break;
    case STS_DIVIDLE:
                  if(CdsSensorDiv.statechk(LOW) == 1){  // DIVR側 在線検出
//                  DivSignal.ChangeState(ST_STOPDET);  // 停止信号
                    DivSignal.ChangeState(ST_STOP);     // 停止信号
                    CdsSensorDiv.Reset();
                    state = STS_DIVIDLE;
                  }
                  if( gCTevent == 1 ){
                    gCTevent = 0;
                    if(gDir == 0){
                      state = STS_DIV; 
                    } else if(gDir == 1){
                      state = STS_STR;
                    }
                  }
                  break;
    default:
                  break;
  }
  
}


//------------------------------------------------------------------
// アクセサリアドレス取得
//------------------------------------------------------------------
uint16_t getMyAddr_Acc(void)
{
  uint16_t  Addr ;
  uint8_t   CV29Value ;

  CV29Value = Dcc.getCV( CV_29_CONFIG ) ;

  if( CV29Value & 0b10000000 ) { // Accessory Decoder? 
    Addr = ( Dcc.getCV( CV_ACCESSORY_DECODER_ADDRESS_MSB ) << 8 ) | Dcc.getCV( CV_ACCESSORY_DECODER_ADDRESS_LSB ) ;
  }
  else   // Multi-Function Decoder?(アドレス4桁？)
  {
    if( CV29Value & 0b00100000 )  // Two Byte Address?
      Addr = ( Dcc.getCV( CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB ) << 8 ) | Dcc.getCV( CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB ) ;

    else
      Addr = Dcc.getCV( 1 ) ;
  }

  return Addr ;
}

//------------------------------------------------------------------
// アクセサリ命令のon/offの処理
// This function is called whenever a normal DCC Turnout Packet is received
//------------------------------------------------------------------
void notifyDccAccState( uint16_t Addr, uint16_t BoardAddr, uint8_t OutputAddr, uint8_t State)
{
  uint16_t aAccAdress = Addr;
  uint8_t aDir = (OutputAddr & 0b001);
  
    //DEBUG
     Serial.print("notifyDccAccState: ") ;
     Serial.print(Addr,DEC) ;
     Serial.print(",BAdr:");
     Serial.print(BoardAddr,DEC) ;
     Serial.print(",OAdr:");
     Serial.print(OutputAddr,DEC) ;
     Serial.print(", Target Adr:");
     Serial.print(aAccAdress,DEC) ;
     Serial.print(",");
     Serial.print(State, HEX) ;
     Serial.print("==");
     Serial.print(gAccessoryAddress,DEC) ;
     Serial.println(",");

 
  //アドレスチェック(CVから得た11bit分の信号を比較)
//  if( gAccessoryAddress == aAccAdress){
//      ServoCH0.change(aDir);
//  }
//  if( gAccessoryAddress2 == aAccAdress){
//      ServoCH1.change(aDir);    
//  }
//  gCTevent = 1;
//  gAccAdr = aAccAdress;
//  gDir = (OutputAddr & 0b001);
  if( gAccessoryAddress != aAccAdress)
  {
    return;
  }
  gDir = (OutputAddr & 0b001);
  gCTevent = 1;
}


//---------------------------------------------------------------------
//DCC速度信号の受信によるイベント
//extern void notifyDccSpeed( uint16_t Addr, uint8_t Speed, uint8_t ForwardDir, uint8_t MaxSpeed )
//---------------------------------------------------------------------
extern void notifyDccSpeed( uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps )
{
}


//------------------------------------------------------------------
// CVをデフォルトにリセット(Initialize cv value)
// Serial.println("CVs being reset to factory defaults");
//------------------------------------------------------------------
void resetCVToDefault()
{
  for (int j = 0; j < FactoryDefaultCVIndex; j++ ) {
    Dcc.setCV( FactoryDefaultCVs[j].CV, FactoryDefaultCVs[j].Value);
  }
};

//------------------------------------------------------------------
// CV8 によるリセットコマンド受信処理
//------------------------------------------------------------------
void notifyCVResetFactoryDefault()
{

  // Make FactoryDefaultCVIndex non-zero and equal to num CV's to be reset 
  // to flag to the loop() function that a reset to Factory Defaults needs to be done
  FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs)/sizeof(CVPair);
  
#if 0  
  //When anything is writen to CV8 reset to defaults.

  resetCVToDefault();
  Serial.println("Resetting...");
  delay(1000);  //typical CV programming sends the same command multiple times - specially since we dont ACK. so ignore them by delaying
  resetFunc();
#endif
};

//------------------------------------------------------------------
// CV Ackの処理
// そこそこ電流を流さないといけない
//------------------------------------------------------------------
void notifyCVAck(void)
{
//サーボモータを動かすとギミックを壊しかねないのでコメントアウト
//Serial.println("notifyCVAck");
#if 0 
  digitalWrite(O1,HIGH);
  digitalWrite(O2,HIGH);
  digitalWrite(O3,HIGH);
  digitalWrite(O4,HIGH);
  delay( 6 );
  digitalWrite(O4,LOW);
  digitalWrite(O3,LOW);
  digitalWrite(O2,LOW);
  digitalWrite(O1,LOW);
#endif
//MOTOR_Ack();
}

void MOTOR_Ack(void)
{
//  analogWrite(O4, 128);
//  delay( 6 );  
//  analogWrite(O4, 0);
}

//------------------------------------------------------------------
// DCC初期化処理）
//------------------------------------------------------------------
void Dccinit(void)
{

  //DCCの応答用負荷ピン
#if defined(DCCACKPIN)
  //Setup ACK Pin
  pinMode(DccAckPin, OUTPUT);
  digitalWrite(DccAckPin, 0);
#endif

#if !defined(DECODER_DONT_DEFAULT_CV_ON_POWERUP)
  if ( Dcc.getCV(CV_MULTIFUNCTION_PRIMARY_ADDRESS) == 0xFF ) {   //if eeprom has 0xFF then assume it needs to be programmed
    notifyCVResetFactoryDefault();
  } else {
    Serial.println("CV Not Defaulting");
  }
#else
  Serial.println("CV Defaulting Always On Powerup");
  notifyCVResetFactoryDefault();
#endif

  // Setup which External Interrupt, the Pin it's associated with that we're using, disable pullup.
  Dcc.pin(0, 2, 0);   // ArduinoNANO D2(PD2)pinをDCC信号入力端子に設定

  // Call the main DCC Init function to enable the DCC Receiver
  // void NmraDcc::init( uint8_t ManufacturerId, uint8_t VersionId, uint8_t Flags, uint8_t OpsModeAddressBaseCV )
  // FLAGS_MY_ADDRESS_ONLY        0x01  // Only process DCC Packets with My Address
  // FLAGS_AUTO_FACTORY_DEFAULT   0x02  // Call notifyCVResetFactoryDefault() if CV 7 & 8 == 255
  // FLAGS_OUTPUT_ADDRESS_MODE    0x40  // CV 29/541 bit 6
  // FLAGS_DCC_ACCESSORY_DECODER  0x80  // CV 29/541 bit 7
//  Dcc.init( MAN_ID_NUMBER, 100,   FLAGS_MY_ADDRESS_ONLY , 0 );  // Function decoder only
  Dcc.init( MAN_ID_NUMBER, MAN_VER_NUMBER, FLAGS_OUTPUT_ADDRESS_MODE | FLAGS_DCC_ACCESSORY_DECODER, 0 );  // アクセサリデコーダ
//  Dcc.init( MAN_ID_NUMBER, MAN_VER_NUMBER, FLAGS_DCC_ACCESSORY_DECODER, 0 );  // アクセサリデコーダ
  
  //アクセサリアドレス(下位2bitを考慮)の先修得
  gAccessoryAddress = getMyAddr_Acc();
  
  //Init CVs
//  gCV1_SAddr = Dcc.getCV( CV_MULTIFUNCTION_PRIMARY_ADDRESS );   // Function decoder only
//  gCVx_LAddr = (Dcc.getCV( CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB ) << 8) + Dcc.getCV( CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB ); // Function decoder only
}


//------------------------------------------------------------------
// CV値が変化した時の処理（特に何もしない）
//------------------------------------------------------------------
extern void     notifyCVChange( uint16_t CV, uint8_t Value) {
   //CVが変更されたときのメッセージ
  #ifdef DEBUG
    Serial.print("CV "); 
    Serial.print(CV); 
    Serial.print(" Changed to "); 
    Serial.println(Value, DEC);
  #endif
};


//------------------------------------------------------------------
// This function is called whenever a DCC Signal Aspect Packet is receivedz
//------------------------------------------------------------------
void notifyDccSigState( uint16_t Addr, uint8_t OutputIndex, uint8_t State)
{
  // Serial.print("notifyDccSigState: ") ;
  // Serial.print(Addr,DEC) ;
  // Serial.print(',');
  // Serial.print(OutputIndex,DEC) ;
  // Serial.print(',');
  // Serial.println(State, HEX) ;
}



//------------------------------------------------------------------
// Resrt処理（特に何もしない）
//------------------------------------------------------------------
void notifyDccReset(uint8_t hardReset )
{
}
