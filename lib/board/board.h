/*
  Файл board.h
  Проект
  pcb: spn.55 
*/

#ifndef _BOARD_H_
#define _BOARD_H_

#ifdef __cplusplus
extern "C"
{
#endif

    void boardInit();
    void uart_mb_init();
    void uart_sp_init();

    void ledsOn();
    void ledsRed();
    void ledsGreen();
    void ledsBlue();
    void ledsOff();

    void ledRedToggle();
    void ledGreenToggle();
    void ledBlueToggle();

    void flagA();
    void flagB();

#ifdef __cplusplus
}
#endif

#endif // !_BOARD_H_
