/*	Author: Nathan Lee
 *  Partner(s) Name: none
 *	Lab Section:
 *	Assignment: Lab #Custom Project  Exercise #Final
 *	Exercise Description: [optional - include for your own benefit]
 *
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 *
 *  Demo Link:  https://youtu.be/efcC8ClQNcM
 *  All files: https://drive.google.com/drive/folders/19VKn0H7n4cDavoql_fRwUQD7CTyKowdB?usp=sharing
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
//#include <util/delay.h>
#include "io.h"
//#include "timer.h"
#include "nokia5110.h"
#ifdef _SIMULATE_
#include "simAVRHeader.h"
#endif
/*
===========================================================================================================
Timer stuff start
===========================================================================================================
*/
volatile unsigned char TimerFlag = 0; 
unsigned long _avr_timer_M = 1;
unsigned long _avr_timer_cntcurr = 0;

void TimerOn(){
    TCCR1B = 0x0B;
    OCR1A = 125;
    TIMSK1 = 0x02;
    TCNT1 = 0;
    _avr_timer_cntcurr = _avr_timer_M;
    SREG |= 0x80;
}
void TimerOff(){
    TCCR1B = 0x00;
}
void TimerISR(){
    TimerFlag = 1;
}
ISR(TIMER1_COMPA_vect){
    _avr_timer_cntcurr--;
    if(_avr_timer_cntcurr == 0){
        TimerISR();
        _avr_timer_cntcurr = _avr_timer_M;
    }
}
void TimerSet(unsigned long M){
    _avr_timer_M = M;
    _avr_timer_cntcurr = _avr_timer_M;
}
/*
===========================================================================================================
Timer stuff end
===========================================================================================================
*/
/*
===========================================================================================================
PWM stuff for speaker start
===========================================================================================================
*/
void set_PWM(double frequency){
    static double current_frequency;
    if(frequency != current_frequency){
        if(!frequency){ TCCR3B &= 0x08;}
        else{TCCR3B |= 0x03;}
        if(frequency < 0.954){OCR3A = 0xFFFF;}
        else if (frequency >31250){OCR3A = 0x0000;}
        else{OCR3A = (short)(8000000/(128 * frequency)) -1;}
        TCNT3 =0;
        current_frequency = frequency;
    }
}

void PWM_on(){
    TCCR3A = (1 << COM3A0);
    TCCR3B = (1 << WGM32) | (1 << CS31) | (1 << CS30);
    set_PWM(0);
}

void PWM_off(){
    TCCR3A = 0x00;
    TCCR3B = 0x00;
}
/*
===========================================================================================================
PWM stuff for speaker end
===========================================================================================================
*/
/*
===========================================================================================================
task struct start
===========================================================================================================
*/
typedef struct task{
    signed char state;
    unsigned long int period;
    unsigned long int elapsedTime;
    int (*TickFct)(int);
} task;
/*
===========================================================================================================
task struct end
===========================================================================================================
*/
/*
===========================================================================================================
ADC stuff for joystick potentiometers start
===========================================================================================================
*/
void ADC_init(){
    ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
}

unsigned short getADC(unsigned char LRUD) {
    //change input
    // may need to mask 0x01
    // mux is 4-0
    LRUD = LRUD & 0x0F;
    if(LRUD == 0x00 || LRUD == 0x01){
        ADMUX = LRUD;
    }
    else{
        ADMUX = ADMUX;
    }
    
    // If admux bits are changed during a conversion, the change will not 
    // go in effect until conversion is complete (ADIF in ADCSRA is set).
    // so we need to wait until ADIF in ADCSRA is set
    while(!(ADCSRA & (1 << ADIF))){
        asm("nop");
    } 
    // conversion done
    // ADATE (autro trigger enable) 
    //handles conversion start after every conversion completion
    return ADC;
}

const unsigned char *address = 0x1A; //Set location to write to EEPROM
/*
===========================================================================================================
ADC stuff for joystick potentiometers end
===========================================================================================================
*/


/*
===========================================================================================================
Sound Logic start
===========================================================================================================
*/

unsigned char play = 0x01;

double row0[6] = {440.00, 0, 440.00, 0, 440.00, 0}; // S , '\0'
double row1[9] = {440.00, 440.00, 0, 440.00, 440.00, 0, 440.00, 440.00, }; // O
double row2[6] = {440.00, 0, 440.00, 0, 440.00, 0}; // S
double row3[5] = {440.00, 0, 440.00, 440.00, 0}; // A
double row4[2] = {440.00, 0}; // E
double row5[3] = {440.00, 440.00, 0}; // T

double *sequence[6] = {row0, row1, row2, row3, row4, row5};
unsigned char sizes[6] = {6, 9, 6, 5, 2, 3};
enum SoundSM{SStart, player, Swait};
unsigned char i = 0;
unsigned char j = 0;
double* ptr; //= sequence[i];

int SoundSMTick(int state){
    unsigned char audio = (~PINA) & 0x08; 
    switch(state){
        case SStart:
            i = 0;
            j = 0;
            ptr = sequence[i];
            state = Swait;
            break;
        case Swait:
            if(audio != 0x00 && play == 0x01){
                ptr = sequence[i];
                j=0;
                state = player;
            }
            else{
                state = Swait;
            }
            break;
        case player:
            if(j < sizes[i]){
                state = player;
            }
            else{
                //i = (i ==5)? 0: i+1;
                state = Swait;
            }
            break;
        default: break;
    }
    switch(state){
        case SStart: break;
        case Swait: break;
        case player:
            set_PWM(*ptr);
            ptr++;
            j++;
            break;
        default: break;
    }
    return state;
}
// */
/*
===========================================================================================================
Sound Logic end
===========================================================================================================
*/
/*
===========================================================================================================
Joystick Logic start
===========================================================================================================
*/
enum JoystickSM { JStart, wait, update};
unsigned char alpha[5][6] = {    {'A', 'B', 'C', 'D', 'E', 'F'},
                                {'G', 'H', 'I', 'J', 'K', 'L'},
                                {'M', 'N', 'O', 'P', 'Q', 'R'},
                                {'S', 'T', 'U', 'V', 'W', 'X'},
                                {'Y', 'Z', 0x00, 0x00, 0x00, 0x00}};

unsigned short LR; // value read from left right joystick
unsigned short UD; // value read from up down joystick
unsigned char xdir; // x direction of highlight 0 = still, 1 = right, 2 = left
unsigned char ydir; // y direction of highlight 0 = still, 1 = up, 2 = down
unsigned char xcoord; // current x index on 2D array 
unsigned char ycoord; // current y index on 2D array

int JoystickSMTick(int state){
    switch(state){
        case JStart:
            xcoord = 0;
            ycoord = 0;
            xdir = 0;
            ydir = 0;
            nokia_lcd_clear();
            nokia_lcd_set_cursor(0, 0);
            nokia_lcd_inverse_string(" A B C D E F ",1);   
            nokia_lcd_set_cursor(0, 10);
            nokia_lcd_inverse_string(" G H I J K L ",1);
            nokia_lcd_set_cursor(0, 20);
            nokia_lcd_inverse_string(" M N O P Q R ",1);
            nokia_lcd_set_cursor(0, 30);
            nokia_lcd_inverse_string(" S T U V W X ",1);
            nokia_lcd_set_cursor(0, 40);
            nokia_lcd_inverse_string(" Y Z ",1);
            nokia_lcd_set_cursor(0, 0);
            nokia_lcd_write_string(" A ",1);
            nokia_lcd_render();
            state = wait;
            break;
        case wait:
            if(play == 0x00){
                state = wait;
            }
            else if(xdir == 0 && ydir == 0){
                state = wait;
            }
            else{
                char restore[4] = {' ', alpha[ycoord][xcoord] , ' ', '\0'};
                nokia_lcd_set_cursor(12*xcoord, 10*ycoord);
                nokia_lcd_inverse_string(restore,1); 
                if(xdir == 0x00){
                    if(ydir == 0x00){
                        //nokia_lcd_write_string("center",1);
                        //no change in coords
                    }
                    else if (ydir == 0x01){
                        //nokia_lcd_write_string("up",1);
                        ycoord = (ycoord == 0)? 0 : (ycoord - 1);
                    }
                    else if (ydir == 0x02){
                        //nokia_lcd_write_string("down",1);
                        //score += 11;
                        if(!(ycoord == 4)&&!(ycoord == 3 && xcoord >= 2)){
                            ycoord = ycoord + 1;
                        }
                    }
                    else{
                        nokia_lcd_write_string("ydir error",1);
                    }
                }
                else if(xdir == 0x01){
                    if(ydir == 0x00){
                        //nokia_lcd_write_string("right",1);
                        if(ycoord == 4){
                            xcoord = (xcoord == 1)? 1 : (xcoord + 1);
                        }
                        else{
                            xcoord = (xcoord == 5)? 5 : (xcoord + 1);
                        }
                        
                    }
                    else{
                        nokia_lcd_write_string("ydir error",1);
                    }
                }
                else if(xdir == 0x02){
                    if(ydir == 0x00){
                        //nokia_lcd_write_string("left",1);
                        xcoord = (xcoord == 0)? 0 : (xcoord - 1);
                    }
                    else{
                        nokia_lcd_write_string("ydir error",1);
                    }
                }
                else{
                    nokia_lcd_write_string("xdir error",1);
                }
                restore[1] = alpha[ycoord][xcoord];
                nokia_lcd_set_cursor(12*xcoord, 10*ycoord);
                nokia_lcd_write_string(restore,1);
                nokia_lcd_render();
                state = update;
            }
            break;
        case update:
            if(play == 0x00){
                state = wait;
            }
            else if(xdir == 0 && ydir == 0){
                state = wait;
            }
            else{
                state = update;
            }
            break;
        default: state = wait; break;
    }
    switch(state){
        case JStart:
            break;
        case wait:
        case update:
            xdir = 0;
            ydir = 0;
            LR = getADC(0x01);;
            // max = 1111111111 = 1023
            // min = 0000001111 = 15
            // center = 1000101001 = 553
            if(LR >= 575){
                ydir = 0x02;
                PORTD = PORTD & ~(0x08);
                PORTD = PORTD | 0x04;
            }
            else if(LR < 525){
                ydir = 0x01;
                PORTD = PORTD & ~(0x04);
                PORTD = PORTD | 0x08;
            }
            else{
                ydir = 0x00;
                PORTD = PORTD & 0xF3;
            }

            UD = getADC(0x00);
            if(UD > 575){
                xdir = 0x01;
                PORTD = PORTD & ~(0x02);
                PORTD = PORTD | 0x01;
            }
            else if(UD < 525){
                xdir = 0x02;
                PORTD = PORTD & ~(0x01);
                PORTD = PORTD | 0x02;
            }
            else{
                xdir = 0x00;
                PORTD = PORTD & 0xFC;
            }

            if(ydir != 0x00 && xdir == 0x00)
                break;
            else if(ydir == 0x00 && xdir != 0x00)
                break;
            else{
                ydir = 0x00;
                xdir = 0x00;
            }
            break;
        default: break;
    }
    return state;
}
/*
===========================================================================================================
Joystick Logic end
===========================================================================================================
*/
/*
===========================================================================================================
Game Logic start
===========================================================================================================
*/

enum gameSM { GStart, playing, spress, rpress, TSreset, over};
unsigned char zeroScore;
unsigned char lives = 3;
unsigned char score = 0;

int gameSMTick(int state){
    unsigned char select = (~PINA) & 0x04;
    unsigned char reset = (~PINA) & 0x10; 
    unsigned char memReset = (~PINA) & 0x20;
    unsigned char answer[6] = {'S', 'O', 'S', 'A', 'E', 'T'};//SOSAET
    switch(state){
        case GStart:
            lives = 3;
            play = 0x01;
            zeroScore = 0x00;
            state = playing;
            break;
        case playing:
            if(reset != 0x00){
                state = rpress;
            }
            else if(select != 0x00){
                    state = spress;
            }
            else{
                state = playing;
            }
            break;
        case rpress:
            if(reset != 0x00){
                state = rpress;
            }
            else{
                lives = 3;
                score = 0;
                i=0;
                j=0;
                state = playing;
            }
            break;
        case spress:
            if(select != 0x00){
                state = spress;
            }
            else{
                if(alpha[ycoord][xcoord] == answer[i]){
                    score++;
                    i = (i == 5)? 0: i+1;
                    state = playing;
                }
                else{
                    lives = lives - 1;
                    if(lives <= 0){
                        lives = 0;
                        state = over;
                    }
                    else{
                        state = playing;
                    }
                }
            }
            break;
        case over:
            play = 0;
            if(memReset != 0x00){
                zeroScore = 0x01;
                state = TSreset;
            }
            else if(reset != 0x00){
                play = 1;
                state = rpress;
            }
            else{
                state = over;
            }
            break;
        case TSreset:
            if(memReset != 0x00){
                state = TSreset;
            }
            else{
                eeprom_write_byte(address, 0x00);
                state = over;
                zeroScore = 0x00;
            }
            break;
        default: break;
    }
    return state;
}
/*
===========================================================================================================
Game Logic end
===========================================================================================================
*/

/*
===========================================================================================================
LCD Logic + EEPROM start
===========================================================================================================
*/
enum displaySM { DStart, GameOver, GamePlay, Hreset};
unsigned char prevLives = 0;
unsigned char prevScore = 255;
unsigned char currTop = 0;
unsigned char mUpdate = 0;
unsigned char x;

int displaySMTick(int state){
    switch(state){
        case DStart:
            state = GamePlay;
            break;
        case GamePlay:
            if(play == 0x01){
                state = GamePlay;
            }
            else if(play == 0x00){
                currTop = eeprom_read_byte(address);
                if(score > 255){
                    score = 255;
                }
                else if (score < 0){
                    score = 0;
                }
                if(score > currTop){
                    eeprom_write_byte(address, score);
                    currTop = score;
                }
                LCD_ClearScreen();
                LCD_DisplayString(2, (const unsigned char *)("Top Score:     Score: "));
                LCD_Cursor(1);
                LCD_WriteData(3);         
                
                LCD_Cursor(14);
                LCD_WriteData((currTop%10)+ '0');
                currTop = currTop /10;
                LCD_Cursor(13);
                LCD_WriteData((currTop%10)+ '0');
                currTop = currTop /10;
                LCD_Cursor(12);
                LCD_WriteData((currTop%10)+ '0');

                LCD_Cursor(25);
                LCD_WriteData((score%10)+ '0');
                score = score /10;
                LCD_Cursor(24);
                LCD_WriteData((score%10)+ '0');
                score = score /10;
                LCD_Cursor(23);
                LCD_WriteData((score%10)+ '0');
                
                state = GameOver;
            }
            break;
        case GameOver:
            if(zeroScore == 0x01){
                LCD_ClearScreen();
                LCD_DisplayString(2, (const unsigned char *)("Top Score:     Score: "));
                LCD_Cursor(1);
                LCD_WriteData(3);         
                
                LCD_Cursor(12);
                LCD_WriteData((currTop%10)+ '0');
                currTop = currTop /10;
                LCD_Cursor(13);
                LCD_WriteData((currTop%10)+ '0');
                currTop = currTop /10;
                LCD_Cursor(14);
                LCD_WriteData((currTop%10)+ '0');

                LCD_Cursor(23);
                LCD_WriteData((score%10)+ '0');
                score = score /10;
                LCD_Cursor(24);
                LCD_WriteData((score%10)+ '0');
                score = score /10;
                LCD_Cursor(25);
                LCD_WriteData((score%10)+ '0');
                LCD_Cursor(26);
                state = GameOver;
            }
            else if(play == 0x01){
                state = GamePlay;
            }
            else if(play == 0x00){
                state = GameOver;
            }
            break;
        default: state = GamePlay; break;
    }
    switch(state){
        case DStart:
            break;
        case GamePlay:
            if(prevLives != lives){
                prevLives = lives;
                LCD_ClearScreen();
                for(x = 0; x < lives; x++){
                    LCD_Cursor(x+1);
                    LCD_WriteData(2);
                }
                mUpdate = 0x01;
            }
            if(prevScore != score || mUpdate != 0x00){
                prevScore = score;
                mUpdate = 0;
                switch(i){
                    case 0:
                    case 2:
                        LCD_Cursor(17);
                        LCD_WriteData(4);
                        LCD_Cursor(18);
                        LCD_WriteData(4);
                        LCD_Cursor(19);
                        LCD_WriteData(4);
                        break;
                    case 1:
                        LCD_Cursor(17);
                        LCD_WriteData(5);
                        LCD_Cursor(18);
                        LCD_WriteData(5);
                        LCD_Cursor(19);
                        LCD_WriteData(5);
                        break;
                    case 3:
                        LCD_Cursor(17);
                        LCD_WriteData(4);
                        LCD_Cursor(18);
                        LCD_WriteData(5);
                        LCD_Cursor(19);
                        LCD_WriteData(' ');
                        break;
                    case 4:
                        LCD_Cursor(17);
                        LCD_WriteData(4);
                        LCD_Cursor(18);
                        LCD_WriteData(' ');
                        LCD_Cursor(19);
                        LCD_WriteData(' ');
                        break;
                    case 5:
                        LCD_Cursor(17);
                        LCD_WriteData(5);
                        LCD_Cursor(18);
                        LCD_WriteData(' ');
                        LCD_Cursor(19);
                        LCD_WriteData(' ');
                        break;
                    default: break;
                }
            }
            break;
        case GameOver:
            break;
        default: break;
    }
    return state;
}
/*
===========================================================================================================
LCD Logic + EEPROM end
===========================================================================================================
*/

int main(void) {
    /* Insert DDR and PORT initializations */
    DDRA = 0x00; PORTA = 0xFF;
    DDRB = 0xFF; PORTB = 0x00;
    DDRC = 0xFF; PORTC = 0x00;
    DDRD = 0xFF; PORTD = 0x00;
    /* Insert your solution below */

    static task task1, task2, task3, task4;
    task *tasks[] = {&task1, &task2, &task3, &task4};
    //static task task1;
    //task *tasks[] = {&task1};

    const unsigned short numTasks = sizeof(tasks)/sizeof(task*);
    //const char start = -1;
    const char gcd = 10;

    unsigned char r;

    unsigned char heart[8] = { 0x00, 0x0A, 0x1F, 0x1F, 0x0E, 0x04, 0x00, 0x00 };
    LCD_WriteCommand(0x48 + 8);
    for(r = 0; r < 8; r++){
        LCD_WriteData(heart[r]);
    }
    LCD_WriteCommand(0x80);

    unsigned char crown[8] = { 0x00, 0x00, 0x04, 0x15, 0x1F, 0x1F, 0x1F, 0x00 };
    LCD_WriteCommand(0x48 + 8+ 8);
    for(r = 0; r < 8; r++){
        LCD_WriteData(crown[r]);
    }
    LCD_WriteCommand(0x80);

    unsigned char dot[8] = { 0x00, 0x04, 0x0E, 0x1F, 0x1F, 0x0E, 0x04, 0x00 };
    LCD_WriteCommand(0x48 + 8 +8+ 8);
    for(r = 0; r < 8; r++){
        LCD_WriteData(dot[r]);
    }
    LCD_WriteCommand(0x80);

    unsigned char dash[8] = { 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x00, 0x00, 0x00 };
    LCD_WriteCommand(0x48 + 8 + 8 + 8+ 8);
    for(r = 0; r < 8; r++){
        LCD_WriteData(dash[r]);
    }
    LCD_WriteCommand(0x80);

    TimerSet(gcd);
    TimerOn();

    LCD_init();

    ADC_init();

    nokia_lcd_init();
    nokia_lcd_clear();

    PWM_on();
    
    task1.state = SStart;
    task1.period = 100;
    task1.elapsedTime = task1.period;
    task1.TickFct = &SoundSMTick;
    
    task2.state = JStart;
    task2.period = 100;
    task2.elapsedTime = task2.period;
    task2.TickFct = &JoystickSMTick;

    task3.state = GStart;
    task3.period = 10;
    task3.elapsedTime = task3.period;
    task3.TickFct = &gameSMTick;

    task4.state = DStart;
    task4.period = 10;
    task4.elapsedTime = task4.period;
    task4.TickFct = &displaySMTick;


    unsigned short i;
    while (1) {

        for(i = 0; i < numTasks; i++)
        {
            if(tasks[i]->elapsedTime ==  tasks[i]->period)
            {
                tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
                tasks[i]->elapsedTime = 0;
            }
            tasks[i]->elapsedTime += gcd
            ;
        }

        while(!TimerFlag);
        TimerFlag = 0;
    }
    return 1;
}

