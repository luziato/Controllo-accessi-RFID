
#ifndef __UHF_H__
#define __UHF_H__


#include <windows.h>

#ifdef __cplusplus
extern "C"
{
#endif

/************************
 * FUNCTION: Connect reader
 * PARAMETERS:
	port: 100->USB, 0->COM1, 1->COM2...
	baud: Baud rate(9600-115200)
 * RETURN: >0 is device handle, otherwise connect failed
 ************************/
int _stdcall uhf_init(int port,long baud); 

/************************
 * FUNCTION: Disconnect reader
 * PARAMETERS:
	icdev: Handle of reader
 * RETURN: 0 if successful; otherwise nonzero
 ************************/
int _stdcall uhf_exit(int icdev)

/************************
 * FUNCTION: Read data from UHF tag
 * PARAMETERS:
	icdev: Handle of reader
	infoType: 1->EPC, 2->TID, 3->USER, 4->reserved
	address: Start address for reading
	rlen: Length of data to read
	pData: Data read
 * RETURN: 0 if successful, otherwise nonzero
 ************************/
int _stdcall uhf_read(int icdev, unsigned char infoType, unsigned int address, unsigned int rlen,
						   unsigned char* pData);

/************************
 * FUNCTION: Write UHF tag
 * PARAMETERS:
	icdev: Handle of reader
	infoType: 1->EPC, 2->TID, 3->USER, 4->reserved
	address: Start address for writing
	wlen: Length of data to write
	pData: Data for write
 * RETURN: 0 if successful, otherwise nonzero
 ************************/
int _stdcall uhf_write(int icdev, unsigned char infoType, unsigned int address, unsigned int wlen,
							unsigned char* pData);

/************************
 * FUNCTION: Control buzzer and led
 * PARAMETERS:
	icdev: Handle of reader
	action: beep:0x01, red led on:0x02, green led on:0x04, yellow led on:0x08
	time: Unit:10ms
 * RETURN: 0 if successful, otherwise nonzero
 ************************/
int _stdcall uhf_action(int icdev,unsigned char action, unsigned char time);


#ifdef __cplusplus
}
#endif

#endif