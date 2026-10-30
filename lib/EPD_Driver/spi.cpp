#include "spi.h"

void EPD_GPIOInit(void)
{
    pinMode(SCK, OUTPUT);
    pinMode(MOSI, OUTPUT);
    pinMode(RES, OUTPUT);
    pinMode(DC, OUTPUT);
    pinMode(CS, OUTPUT);
    pinMode(BUSY, INPUT);
}

/**
 * @brief       Bit-bang (GPIO-emulated SPI) send one byte
 * @param       dat: the byte to send
 * @retval      none
 */
void EPD_WR_Bus(uint8_t dat)
{
    uint8_t i;
    EPD_CS_Clr();
    for (i = 0; i < 8; i++)
    {
        EPD_SCK_Clr();
        if (dat & 0x80)
        {
            EPD_MOSI_Set();
        }
        else
        {
            EPD_MOSI_Clr();
        }
        EPD_SCK_Set();
        dat <<= 1;
    }
    EPD_CS_Set();
}

/**
 * @brief       Write a register/command to the display
 * @param       reg: the command to write
 * @retval      none
 */
void EPD_WR_REG(uint8_t reg)
{
    EPD_DC_Clr();
    EPD_WR_Bus(reg);
    EPD_DC_Set();
}

/**
 * @brief       Write one byte of data to the display
 * @param       dat: the data to write
 * @retval      none
 */
void EPD_WR_DATA8(uint8_t dat)
{
    EPD_DC_Set();
    EPD_WR_Bus(dat);
    EPD_DC_Set();
}
