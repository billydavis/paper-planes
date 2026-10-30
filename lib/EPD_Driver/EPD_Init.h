#ifndef _EPD_INIT_H_
#define _EPD_INIT_H_

#include "spi.h"
// This 5.79in E-Paper panel is driven by two SSD1683 ICs.
// Each SSD1683 natively supports up to 400x300, but the panel itself is 792x272,
// so the master/slave chips are each driven at 396x272 and tiled together.
// That tiling leaves an 8-column seam where the two ICs meet, so an address
// offset is needed when writing image data across that seam.
// Because of this, EPD_W/EPD_H are defined as 800x272 even though the actual
// visible area is still 792x272.
// If you render a full-screen image directly via EPD_Display(), your source
// image needs to be formatted at 800x272.
// If you use EPD_ShowPicture() instead, format your source image at 792x272.
#define EPD_W	800
#define EPD_H	272

#define WHITE 0xFF
#define BLACK 0x00

#define ALLSCREEN_GRAGHBYTES  27200/2

#define Source_BYTES 400/8
#define Gate_BITS  	 272
#define ALLSCREEN_BYTES Source_BYTES*Gate_BITS

void EPD_READBUSY(void);
void EPD_HW_RESET(void);
void EPD_Update(void);
void EPD_PartUpdate(void);
void EPD_FastUpdate(void);
void EPD_DeepSleep(void);
void EPD_Init(void);
void EPD_FastMode1Init(void);
void EPD_SetRAMMP(void);
void EPD_SetRAMMA(void);
void EPD_SetRAMSP(void);
void EPD_SetRAMSA(void);
void EPD_Clear_R26A6H(void);
void EPD_Display_Clear(void);
void EPD_Display(const uint8_t *ImageBW);
void EPD_WhiteScreen_ALL_Fast(const unsigned char *datas);
#endif
