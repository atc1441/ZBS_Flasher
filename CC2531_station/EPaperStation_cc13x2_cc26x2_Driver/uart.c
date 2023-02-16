#include "uart.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ti/drivers/UART2.h>
#include "ti_drivers_config.h"

#define MAX_LENGTH1             512
char         input[MAX_LENGTH1];
size_t bytesToRead = MAX_LENGTH1;
volatile size_t bytesReadCount;

UART2_Handle uart;

/*
int puts(const char *out_buffer1){
    size_t bytesWritten = 0;
    char out_buffer[500];
    int len_out = sprintf(out_buffer, "%s\r\n",out_buffer1);
    UART2_write(uart, (char *)&out_buffer, len_out, &bytesWritten);
    return len_out-2;
}

int putchar(int out_char){
    size_t bytesWritten = 0;
    char out_buffer[500];
    int len_out = sprintf(out_buffer, "%c\r\n",out_char);
    UART2_write(uart, (char *)&out_buffer, len_out, &bytesWritten);
    return out_char;
}*/


void ReceiveonUARTcallback(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status)
{
    if (status == UART2_STATUS_SUCCESS)
    {
        bytesReadCount = count;
    }else{
        UART2_read(uart, &input, bytesToRead, NULL);
    }

}


void initUart()
{
    UART2_Params uartParams;
    UART2_Params_init(&uartParams);
    uartParams.baudRate = 115200;
    uartParams.readMode = UART2_Mode_CALLBACK;
    uartParams.readCallback = ReceiveonUARTcallback;
    uartParams.readReturnMode = UART2_ReadReturnMode_PARTIAL;
    while(uart == NULL)
    {
        uart = UART2_open(CONFIG_UART2_0, &uartParams);
    }
    UART2_read(uart, &input, bytesToRead, NULL);
}

void uartEnableInt()
{
    UART2_read(uart, &input, bytesToRead, NULL);// Start UART interrupt again
}

void uartWrite(uint8_t *buffer, size_t len)
{
    size_t bytesWritten = 0;
    UART2_write(uart, buffer, len, &bytesWritten);
}
