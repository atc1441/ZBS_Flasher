
void UART_IRQ1(void) __interrupt (0);

#pragma callee_saves uartInit
void uartInit(void);

#pragma callee_saves uartTx
void uartTx(uint8_t val);
