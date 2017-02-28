/* 
 * File:   macros.h
 * Author: Administrator
 *
 * Created on August 17, 2016, 2:42 PM
 */

#ifndef MACROS_H
#define	MACROS_H
/*
#define __delay_1s() for(char i=0;i<100;i++){__delay_ms(10);}
#define __lcd_newline() lcdInst(0b11000000);
#define __lcd_clear() lcdInst(0b10000000);
#define __lcd_home() lcdInst(0b11000000);*/
#define __bcd_to_num(num) (num & 0x0F) + ((num & 0xF0)>>4)*10

#define __delay_1s() for(char i=0;i<100;i++){__delay_ms(10);}
#define __lcd_newline() lcdInst(0b11000000);
#define __lcd_clear() lcdInst(0x01);__delay_ms(10);
#define __lcd_home() lcdInst(0b10000000);
#define __lcd_killcursor() lcdInst(0b10010000);
//#define __hex_to_bin() for(char i=0;i<32;i++) {bits[i]=hex&1;hex>>=1;}
#endif	/* MACROS_H */

