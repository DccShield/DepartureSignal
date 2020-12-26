#include <arduino.h>
#include "3ColorLightSignal.h"

ThirdColorLightSignal::ThirdColorLightSignal(char L1, char L2, char L3, char L4)
{
  LED1 = L1;
  LED2 = L2;
  LED3 = L3;
  LED4 = L4;
  
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
}

void ThirdColorLightSignal::Detection(void)
{
  detf = HIGH;
}

int ThirdColorLightSignal::nowState()
{
  return state; 
}

void ThirdColorLightSignal::ChangeState( int st)
{
  state = st; 
}


void ThirdColorLightSignal::statechk( void ) 
{
static int prestate = 0;
if(prestate != state ){
  Serial.print((int)LED1,DEC);
  Serial.print(",");
  Serial.println(state);
  prestate = state;
}
  switch(state){
      case ST_INIT:
              stops();
              state = ST_IDLE;
              break;

      case ST_IDLE:     //1
              break;

      case ST_STOP:     //2
              blank();
              state = ST_STOP2;
              break;

      case ST_STOP2:     //2
              stops();
              state = ST_IDLE;
              break;


      case ST_STOPDET: //3
              stops();
              cnt = 0;
              state = ST_STOPWAIT;      
              break;
              
      case ST_STOPWAIT: //4
              cnt++;
              if( cnt > STOPSTIM ) {
                cnt = 0;
//                caution();
//                state = ST_CAUTIONWAIT;
                state = ST_CAUTION;
              }
              break;
      
      case ST_CAUTION:     //5 停止注意同時
              caution_stops();
              state = ST_CAUTION2;
              break;
              
      case ST_CAUTION2:
              caution();
              cnt = 0;
              state = ST_CAUTIONWAIT;
              break;
      
      case ST_CAUTIONWAIT://6 注意進行同時OFF
              cnt++;
              if( cnt > CAUTIONTIM) {
                cnt = 0;
//                advance();
                  blank();
                state = ST_ADVANCE;
              }
              break;

      case ST_ADVANCE:     //7
              advance();
              state = ST_IDLE;
              break;

      default:
              break;
    }
}



void ThirdColorLightSignal::advance(void) // 進行
{
   digitalWrite(LED1, LOW);
   digitalWrite(LED2, LOW);
   digitalWrite(LED3, HIGH);
   digitalWrite(LED4, LOW);
}

void ThirdColorLightSignal::caution(void) // 注意
{
   digitalWrite(LED1, HIGH);
   digitalWrite(LED2, LOW);
   digitalWrite(LED3, HIGH);
   digitalWrite(LED4, HIGH);
}


void ThirdColorLightSignal::stops(void) // 停止
{
   digitalWrite(LED1, LOW);
   digitalWrite(LED2, HIGH);
   digitalWrite(LED3, HIGH);
   digitalWrite(LED4, HIGH);
}

void ThirdColorLightSignal::caution_stops(void) // 停止
{
   digitalWrite(LED1, LOW);
   digitalWrite(LED2, LOW);
   digitalWrite(LED3, HIGH);
   digitalWrite(LED4, HIGH);
}

void ThirdColorLightSignal::blank(void) // 消灯
{
   digitalWrite(LED1, LOW);
   digitalWrite(LED2, LOW);
   digitalWrite(LED3, LOW);
   digitalWrite(LED4, LOW);
}
