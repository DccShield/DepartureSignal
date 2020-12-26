#include "cds.h"
#include  <arduino.h>

Cds::Cds(char ch , char ct, int LTh, int HTh){
  port = ch;
  cntup = ct;
  LThreshold = LTh;
  HThreshold = HTh;
}

void Cds::Reset( void ){
  state = ST_INIT;
}

char Cds::statechk( char range ){
//  char temp[10];
//  sprintf(temp,"state:%d,cnt:%d:",state,cnt);
//  Serial.println(temp);
    
  switch(state){
    case ST_INIT:
              state = ST_MEAS;
              cnt = 0;
              break;
    case ST_MEAS:
              if(cnt > cntup ){
                state = ST_DETECTION;
                return HIGH;
              }
              Ain = analogRead( port );
// char temp[20];
// sprintf(temp,"port:%d Ain:%d",port,Ain);
// Serial.println(temp);
              if(range == LOW){
                if(Ain <=  LThreshold){
                  cnt++;
                } else
                  cnt = 0;
              }
              if(range == HIGH){
                if(Ain <=  HThreshold){
                  cnt++;
                } else
                  cnt = 0;
              }
              break;
    case ST_DETECTION:
              return HIGH;
              break;

    default:
              break;
  }
  return LOW;
}
