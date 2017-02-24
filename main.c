//TIMER
//use timer to keep track of operation time
//the ending menu is complete, the operation time and running time are displayed
//the start menu and submenus return to the reset state after 10 seconds without a button press
//the sorting mode automatically leaves after 3 minutes.
//MENU USAGE
//A scrolls up, B scrolls down, C enters, D goes back
//when "sorting", simulate bottle counts with buttons 1, 2, 4, 5, and finish sorting with 7, 8
//
//VERSION NOTES
//in this version, implement adc interface to increasin the count of bottles
//right now, buttons 1 2 3 and 4 simulate increasing buttons through keypress interrupt
//minor update: limit bottle# display to 2 digits by using modulo 100

//there may have been a version control mistake involving versions 6 and 7, but right now version 6 works and versions 5 and 7 dont
//so, version 8 will be based on version 6, incoporating functions from v7 to identify what went wrong.
#include <xc.h>
#include <stdio.h>
#include <string.h>
#include "configBits.h"
#include "constants.h"
#include "lcd.h"
#include "I2C.h"
#include "macros.h"
#include "main.h"
// <editor-fold defaultstate="collapsed" desc=" MENUS">
char menu[4][10][18]={{"1. Sort        \0", "2. P Logs      \0", "3. Credits     \0"},//15 char each
                    {"event1         \0", "event2         \0", "event3         \0"},//16 char each
                    {"Microcontrol:  \0", "Clement Wan    \0", "Circuits:      \0", "Justin Qu      \0", "Electromech:   \0", "Peter Feng     \0"}

};

int menuIndex=1;
int menuSize=3;

int logIndex=1;
int events=3-1;

int creditIndex=1;
int creditSize=6-1;

int endIndex=1;
int endSize=4-1;    //</editor-fold>
// <editor-fold defaultstate="collapsed" desc=" GLOBAL VARS">

const char keys[] = "123A456B789C*0#D"; 
const char manualDateTime[7] = {  0x00, //Seconds 
                            0x00, //Minutes
                            0x00, //24 hour mode
                            0x00, //WEEKDAY
                            0x01, //DAY OF MONTH
                            0x00, //MONTH
                            0x17};//YEAR
char state='r';//states: reset, menu, logs, credits, sort, sense(?), end, X(error/emergency stop)
char nstate='r';//functions can change nstate, but only update_state can change state
unsigned char time[7];
int time_i[7];
int startTime[7];
int runTime;
int timeDiff;
int eskaNoCap=0;
int eskaWCap=0;
int yopNoCap=0;
int yopWCap=0;
int sorted_bottles;
char display0[18];
char display1[18];
char ndisplay0[18];
char ndisplay1[18];


char PROX1_PIN=0;//digital pins on port E
char PROX2_PIN=1;
char DIST1_TRIG=5;//RC5 triggers DIST1 to be echo'ed
char DIST1_PIN=0;//analog pins on port A
char IR1_PIN=1;
char IR2_PIN=2;
int PROX1[5];
int PROX2[5];
int DIST1[5];
int IR1[5];
int IR2[5];
int DIST_THRESHOLD_LOW=1024/4-1;
int DIST_THRESHOLD_HI=1024*3/4-1;
int IR_THRESHOLD_LOW=1024/4-1;
int IR_THRESHOLD_HI=1024*3/4-1;


int discretize;//=3000/delay;//3 seconds for discretize step at the start of the default substate
int discretize_counter;//=3000/delay;
int release=(4000);///delay;//1 second to open flaps, push for half a second, wait for 1.5 seconds. return to default state while closing for 1 second.
int release_counter=0;

int measure=5;
int measure_counter=0;

int latestSortedBottleTime[7];//if no bottle is successfully sorted in 15 seconds, terminate
int bottle_type=0;//0 when no bottle identified yet, 1-4 for each en, ec, yn, yc
//char substate='d';//default, release, commented for now as the "States" are determined by discretize measure, and release countdown values
    //</editor-fold>
void main(void) {
    //discretize=3000/delay;//3 seconds for discretize step at the start of the default substate
    //discretize_counter=3000/delay;
    //release=(4000)/delay;//1 second to open flaps, push for half a second, wait for 1.5 seconds. return to default state while closing for 1 second.

    // <editor-fold defaultstate="collapsed" desc=" STARTUP SEQUENCE ">
    
    TRISC = 0x00;
    TRISD = 0x00;   //All output mode
    TRISB = 0xFF;   //All input mode    
    LATB = 0x00; 
    LATC = 0x00;
    ADCON0 = 0x00;  //Disable ADC
    ADCON1 = 0xFF;  //Set PORTB to be digital instead of analog default  
    initLCD();
    INT1IE = 1;
    nRBPU = 0;
    I2C_Master_Init(10000); //Initialize I2C Master with 100KHz clock
    //</editor-fold>
    di(); // Disable all interrupts
    set_time();//RTC BATTERY IS BROKEN
    while(1){
        di();
        update_RTC();
        read_sensors();
        update_state();
        update_display();//display is the internal array
        update_lcd();//lcd is the actual screen
        ei();
        __delay_ms(CYCLE_DELAY);
        di();
        /*
        sprintf(ndisplay0, "ding");
        update_lcd();
        ei();
        __delay_ms(50);
        di();*/
        
    }
    
    return;
}
void update_lcd(void){
    int flag=0;
    int i,j;
    
    if (strcmp(display0, ndisplay0)!=0){
        strcpy(display0,ndisplay0);
        flag=1;
    }
    if (strcmp(display1, ndisplay1)!=0){
        strcpy(display1,ndisplay1);
        flag=1;
    }
    if (flag){
        __lcd_clear();
        __lcd_home();
        printf(display0);
        __lcd_newline();
        printf(display1);
        __lcd_killcursor();
    }
}
void update_RTC(void){

    //Reset RTC memory pointer 
    I2C_Master_Start(); //Start condition
    I2C_Master_Write(0b11010000); //7 bit RTC address + Write
    I2C_Master_Write(0x00); //Set memory pointer to seconds
    I2C_Master_Stop(); //Stop condition

    //Read Current Time
    I2C_Master_Start();
    I2C_Master_Write(0b11010001); //7 bit RTC address + Read
    for(unsigned char i=0;i<0x06;i++){
        time[i] = I2C_Master_Read(1);
        time_i[i]=__bcd_to_num(time[i]);
    }
    timeDiff=time_i[1]*60-startTime[1]*60+time_i[0]-startTime[0];
    time[6] = I2C_Master_Read(0);       //Final Read without ack
    I2C_Master_Stop();
}
void update_display(void){
    switch(state)
    {
        case 'r': 
            sprintf(ndisplay0,"%02x/%02x/%02x PUSH TO", time[6],time[5],time[4]);
            sprintf(ndisplay1,"%02x:%02x:%02x   START", time[2],time[1],time[0]);
            break;
        case 'm':
        case 'l':
        case 'c':
        case 'e':
            display_menu();
            break;
        case 's':
            sprintf(ndisplay0,"%d:%02d en:%02d ec:%02d ",(timeDiff/60),timeDiff%60, eskaNoCap,eskaWCap);
            sprintf(ndisplay1,"yn:%02d yc:%02d", yopNoCap, yopWCap);
            break;
        default: 
            sprintf(ndisplay0, "ERROR");
            sprintf(ndisplay1, "EMERGENCY STOP");
            break;

    }
}
void update_state(void){
    if ((nstate=='s'&&state!='s')||(nstate=='m'&&state!='m')){
        int i;
        for (i=0;i<7;i++)
            startTime[i]=time_i[i];
    }
    sorted_bottles=eskaNoCap+eskaWCap+yopNoCap+yopWCap;
    sorted_bottles%=100;
    if (state=='s'&&timeDiff>=180){
        nstate='e';
    }
    if (nstate=='e'&&state!='e'){
        runTime=timeDiff;
        sprintf(menu[3][0],"bottles: %02d    \0", sorted_bottles);
        sprintf(menu[3][1],"run time: %01d:%02d \0",(runTime/60),timeDiff%60);
        sprintf(menu[3][2],"en:%02d ec:%02d    ", eskaNoCap,eskaWCap);
        sprintf(menu[3][3],"yn:%02d yc:%02d    ", yopNoCap, yopWCap);
    }
    if ((state=='m'||state=='l'||state=='c')&&timeDiff>=10){
        nstate='r';
    }
    state=nstate;//functions elsewhere only write to nstate
    /*
    sprintf(ndisplay0, "%c  %c  %d", state, nstate, timeDiff);
        update_lcd();
        ei();
        __delay_ms(50);
        di();*/
    //reset variables eg timers
    if (state!='m'&&state!='l'&&state!='c')
    {
        menuIndex=1;
        logIndex=1;
        creditIndex=1;
    }
    if (state!='e')
        endIndex=1;
    if (state=='r'){
        eskaNoCap=0;
        eskaWCap=0;
        yopNoCap=0;
        yopWCap=0;
    }
}
void display_menu(void){
    int menuSelection;
    int* menuIndexPtr;
    int* menuSizePtr;
    int selector=0;//only the main menu so far has a selector. all other scroll, but do not have a selector
    switch(state)
    {
        case 'l':
            menuSelection=1;
            menuIndexPtr=&logIndex;
            menuSizePtr=&events;
            break;
        case 'c':
            menuSelection=2;
            menuIndexPtr=&creditIndex;
            menuSizePtr=&creditSize;
            break;
        case 'm':
        default:
            menuSelection=0;
            menuIndexPtr=&menuIndex;
            menuSizePtr=&menuSize;
            selector=1;
            break;
        case 'e':
            menuSelection=3;
            menuIndexPtr=&endIndex;
            menuSizePtr=&endSize;
    }
    if (selector){
        if (*menuIndexPtr<*menuSizePtr){
            sprintf(ndisplay0,"%s%c", menu[menuSelection][*menuIndexPtr-1], '<');
            sprintf(ndisplay1,menu[menuSelection][*menuIndexPtr]);
        }
        else{
            sprintf(ndisplay0,menu[menuSelection][*menuIndexPtr-2]);
            sprintf(ndisplay1,"%s%c", menu[menuSelection][*menuIndexPtr-1], '<');
        }
    }
    else{
        sprintf(ndisplay0,"%s%c", menu[menuSelection][*menuIndexPtr-1], '^');
        sprintf(ndisplay1,"%s%c",menu[menuSelection][*menuIndexPtr],'^');
        
    }
        
}
void interrupt keypressed(void) {
    
    if(INT1IF){//keypressed
        int i;
        unsigned char keypress = (PORTB & 0xF0) >> 4;
        switch(state)
        {            case 'm':
                for (i=0;i<7;i++)
                    startTime[i]=time_i[i];
                if (keys[keypress]=='B'&&menuIndex<menuSize)
                    menuIndex++;
                else if (keys[keypress]=='A'&&menuIndex>1)
                    menuIndex--;
                else if (keys[keypress]=='C'){
                    switch(menuIndex){
                        case 1: nstate='s';
                                break;
                        case 2: nstate='l';
                                break;
                        case 3: nstate='c';
                                break;
                        default:
                            __lcd_home();
                            printf("invalid");
                            __lcd_newline();
                            printf("menu selection");
                            __lcd_killcursor();
                            break;
                    }
                }
                break;
            case 'r':
                nstate='m';
                break;

            case 'l':
                for (i=0;i<7;i++)
                    startTime[i]=time_i[i];
                
                if (keys[keypress]=='B'&&logIndex<events){
                //__lcd_home();    
                //printf("%c,%d,%d",keys[keypress],logIndex, events);
                //__delay_1s();
                    logIndex++;
                }
                else if (keys[keypress]=='A'&&logIndex>1)
                    logIndex--;
                else if(keys[keypress]=='D')
                    nstate='m';
                break;
            case 'c':
                for (i=0;i<7;i++)
                    startTime[i]=time_i[i];
                
                if (keys[keypress]=='B'&&creditIndex<creditSize)
                    creditIndex++;
                else if (keys[keypress]=='A'&&creditIndex>1)
                    creditIndex--;
                else if(keys[keypress]=='D')
                    nstate='m';
                break;
            case 's':
                if (keys[keypress]=='1'){
                    eskaNoCap++;
                    eskaNoCap%=100;
                }
                else if (keys[keypress]=='2'){
                    eskaWCap++;
                    eskaWCap%=100;
                }
                else if (keys[keypress]=='4'){
                    yopNoCap++;
                    yopNoCap%=100;
                }
                else if (keys[keypress]=='5'){
                    yopWCap++;
                    yopWCap%=100;
                }
                else if (keys[keypress]=='7'||keys[keypress]=='8')
                    nstate='e';
                break;
            case 'e':
                if (keys[keypress]=='B'&&endIndex<endSize)
                    endIndex++;
                else if (keys[keypress]=='A'&&endIndex>1)
                    endIndex--;
                else if (keys[keypress]=='C')
                    nstate='r';
                break;
            default:
                printf("no page found");
                break;
        }
        INT1IF = 0;     //Clear flag bit
        /*
        __lcd_newline();
        unsigned char keypress = (PORTB & 0xF0) >> 4;
        putch(keys[keypress]);
        __lcd_home();
        INT1IF = 0;     //Clear flag bit
         */
    }
}
void set_time(void){
    I2C_Master_Start(); //Start condition
    I2C_Master_Write(0b11010000); //7 bit RTC address + Write
    I2C_Master_Write(0x00); //Set memory pointer to seconds
    for(char i=0; i<7; i++){
        I2C_Master_Write(manualDateTime[i]);
    }    
    I2C_Master_Stop(); //Stop condition
    
    
}
void read_sensors(void){
    if (state!='s'){
        return;
    }
    // <editor-fold defaultstate="collapsed" desc="RIGHT SHIFT ARRAYS">
    for(char i=0;i<5-1;i++){
        PROX1[i+1]=PROX1[i];
        PROX2[i+1]=PROX2[i];
        DIST1[i+1]=DIST1[i];
        IR1[i+1]=IR1[i];
        IR2[i+1]=IR2[i];
    }
    //</editor-fold>
    
    // <editor-fold defaultstate="collapsed" desc="READ SENSORS">
    //analog reads PORT A
    readADC(DIST1_PIN);
    DIST1[0]=16*16*ADRESH+ADRESL;
    readADC(IR1_PIN);
    IR1[0]=16*16*ADRESH+ADRESL;
    readADC(IR2_PIN);
    IR2[0]=16*16*ADRESH+ADRESL;
    //digital reads PORT E
    PROX1[0]=(PORTE>>PROX1_PIN)&1;
    PROX2[0]=(PORTE>>PROX2_PIN)&1;
    //</editor-fold>
    
    /*
     * reminder of sorting subsequence:
     * constant readings stored in arrays (past 5 should do of each distance1, proximity1, proximity2, IR1, IR2)
     * 
     * REFER TO "SENSING BOX CODING DIAGRAM"
     * the new sensor architecture is as follows: the DIST1 sensor identifies the orientation.
     *  the value is 0-4cm when the lid faces PROX2, and 4-8 cm when facing PROX1. 
     * 8+ assumes that the bottle is not yet in position
     * whichever direction the lid is facing, PROXi is 0 when there is a lid, 1 when theres no lid
     * If either ir1 or ir2 is high, ie light passes through, then the bottle is eska. otherwise yop.
     * 
    */
}
void sort (void){
    // <editor-fold defaultstate="collapsed" desc="DISCRETIZE">
    if (discretize_counter>2000/CYCLE_DELAY)//clockwise phase
        ;//spin motor clockwise
    else if (discretize_counter>1000/CYCLE_DELAY)//pause phase
        ;//leave motor still
    else if (discretize_counter>0)//ccw phase
        ;//spin motor ccw
    //no significance to discretize_counter=0;
    //</editor-fold>
    // <editor-fold defaultstate="collapsed" desc="MEASURE">
    if ((PROX1[0]+PROX2[0])&&measure_counter==0&&release_counter<=1000/CYCLE_DELAY){
        measure_counter=measure;
        int i;
        for (i=0;i<7;i++)
            latestSortedBottleTime[i]=time_i[i];
    }
    if (measure_counter==1){
         bottle_type=_measure();
        if (bottle_type!=0){
            release_counter=release;
            if (bottle_type==1)
                eskaNoCap++;
            if (bottle_type==2)
                eskaWCap++;
            if (bottle_type==3)
                yopNoCap++;
            if (bottle_type==4)
                yopWCap++;
        }
        else
            measure_counter+=1;
    }
    //</editor-fold>
    // <editor-fold defaultstate="collapsed" desc="RELEASE">
    //if (measure_counter==0&&release_counter==0)
    //    release_counter=release;
    if (release_counter==1000/CYCLE_DELAY)
        discretize_counter=discretize;    
    else if (release_counter>3500/CYCLE_DELAY)
        ;//open flaps
    else if (release_counter>3000/CYCLE_DELAY)
        ;//open gate
    else if (release_counter>2500/CYCLE_DELAY)
        ;//push
    else if (release_counter>2000/CYCLE_DELAY)
        ;//retract solenoid
    else if (release_counter>1000/CYCLE_DELAY)
        ;//wait
    else if (release_counter>0){
        ;//close flaps, close gate
    }
    //</editor-fold>
    // <editor-fold defaultstate="collapsed" desc="COUNTDOWN">
    if (discretize_counter>0)
        discretize_counter--;
    if (measure_counter>0)
        measure_counter--;
    if (release_counter>0)
        release_counter--;
    //</editor-fold>
    // <editor-fold defaultstate="collapsed" desc="TERMINATE">
    if (time_i[1]*60-latestSortedBottleTime[1]*60+time_i[0]-latestSortedBottleTime[0]>=15)
        nstate='e';
    
    //</editor-fold>
    /*
     * if no bottle is successfully sorted in 15 seconds, terminate
     * 
     * default sorting state
     *  from default, when either proximity sensor gives a 0, countdown for 5 cycles for measurement
     * 
     * when the countdown hits 1, decide the type of bottle
     * 
     * if identified, go to release bottle substate (empty until motor implementation)
     * 
     *  open gate
     *  open flap
     *  push
     *  wait
     *  return to "default"
     *  close gate 
     *  close flap
     *  
     * after release bottle substate, return immediately to default state, while performing 
     * descritize sequence: (sequence performs based on a flag number. 0 when not in the state, 
     * positive when in this state, continually increasing in value until reaching a "final" number. 
     * for example. switch flag to 1 to begin. sequence continually increases to 180, at which point ireturns to 0
     * if the flag number is 0-60, move clocwise, and if 120-179, move aclockwise
     * rotate clockwise
     * wait
     * rotate aclockwise
     */
}
int _measure(void){
    /* 
     according to the sensing diagram:
     * if dist is above the high threshold, the bottle is not there. 
     * if dist is above the low threshold, only listen to proximity 2 sensor
     * if dist is below the low threshold, only listen to proximity 1 sensor
     * if the proximity sensor is high, theres no cap
     * if the proximity sensor is low, there is a cap
     * if either proximity sensor reads high, the bottle is eska
     * return 0 for no bottle, 1 for en, 2 for ew, 3 for yn, 4 for yc
     * if at any point the 5 readings "disagree" on a threshold, return 0
     */
    int measurement=0;
    // <editor-fold defaultstate="collapsed" desc="DO THE SENSORS AGREE">
    // <editor-fold defaultstate="collapsed" desc="PROXIMITY">
    for (char i=0;i<5;i++)
        measurement+=PROX1[i];
    if (measurement%5!=0)
        return 0;
    for (char i=0;i<5;i++)
        measurement+=PROX2[i];
    if (measurement%5!=0)
        return 0;
    //</editor-fold>
    // <editor-fold defaultstate="collapsed" desc="DISTANCE">
    for (char i=0;i<5;i++)
        measurement+=(DIST1[i]<DIST_THRESHOLD_LOW);
    if (measurement%5!=0)
        return 0;
    for (char i=0;i<5;i++)
        measurement+=(DIST1[i]<DIST_THRESHOLD_HI);
    if (measurement%5!=0)
        return 0;
    //</editor-fold>
    // <editor-fold defaultstate="collapsed" desc="IR">
    for (char i=0;i<5;i++)
        measurement+=(IR1[i]<IR_THRESHOLD_LOW);
    if (measurement%5!=0)
        return 0;

    for (char i=0;i<5;i++)
        measurement+=(IR1[i]<IR_THRESHOLD_HI);
    if (measurement%5!=0)
        return 0;
    
    for (char i=0;i<5;i++)
        measurement+=(IR2[i]<IR_THRESHOLD_LOW);
    if (measurement%5!=0)
        return 0;
    
    for (char i=0;i<5;i++)
        measurement+=(IR1[i]<IR_THRESHOLD_HI);
    if (measurement%5!=0)
        return 0;
    //</editor-fold>
    //</editor-fold>
    measurement=0;
    // <editor-fold defaultstate="collapsed" desc="DISTANCE+PROXIMITY">
    if (DIST1[0]>DIST_THRESHOLD_HI)
        return 0;
    if (DIST1[0]>DIST_THRESHOLD_LOW){
        measurement+=PROX2[0];
    }
    else
        measurement+=PROX1[0];
    //</editor-fold>
    // <editor-fold defaultstate="collapsed" desc="IR">
    if (IR1[0]>IR_THRESHOLD_HI||IR2[0]>IR_THRESHOLD_HI);//eska if at least one sensor reads high
    else if (IR1[0]<IR_THRESHOLD_LOW&&IR2[0]<IR_THRESHOLD_LOW)
        measurement+=2;//yop if both sensors read low
    else
        return 0;//keep measuring if the ir sensors don't agree
    //</editor-fold>
    return measurement;
}
void readADC(char channel){
    // Select A2D channel to read
    ADCON0 = ((channel <<2));
    ADON = 1;
    ADCON0bits.GO = 1;
   while(ADCON0bits.GO_NOT_DONE){__delay_ms(5);}
    
    
}