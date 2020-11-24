/*
   MIT License

   Copyright (c) 2018 Sergey Matyukevich

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

/*
 * ChCore refers to
 * https://github.com/s-matyukevich/raspberry-pi-os/blob/master/docs/lesson01/rpi-os.md
 * for the min-uart init process.
 */
#include <common/machine.h>
#include <common/types.h>
#include <common/tools.h>
#include <common/uart.h>
#include <common/peripherals.h>

void uart_init(void)
{
	unsigned int ra;

	ra = get32(GPFSEL1);
	ra &= ~(7 << 12);
	ra |= 2 << 12;
	ra &= ~(7 << 15);
	ra |= 2 << 15;
	put32(GPFSEL1, ra);

	put32(GPPUD, 0);
	// delay(150);
	put32(GPPUDCLK0, (1 << 14) | (1 << 15));
	// delay(150);
	put32(GPPUDCLK0, 0);

	put32(AUX_ENABLES, 1);
	put32(AUX_MU_IER_REG, 0);
	put32(AUX_MU_CNTL_REG, 0);
	put32(AUX_MU_IER_REG, 0);
	put32(AUX_MU_LCR_REG, 3);
	put32(AUX_MU_MCR_REG, 0);
	put32(AUX_MU_BAUD_REG, 270);

	put32(AUX_MU_CNTL_REG, 3);

	/* Clear the screen */
	uart_send(12);
	uart_send(27);
	uart_send('[');
	uart_send('2');
	uart_send('J');
}

u32 uart_lsr(void)
{
	return get32(AUX_MU_LSR_REG);
}

u32 uart_recv(void)
{
	while (1) {
		if (uart_lsr() & 0x01)
			break;
	}

	return (get32(AUX_MU_IO_REG) & 0xFF);
}

u32 nb_uart_recv(void)
{
	if (uart_lsr() & 0x01)
		return (get32(AUX_MU_IO_REG) & 0xFF);
	else
		return NB_UART_NRET;
}

void uart_send(u32 c)
{
	while (1) {
		if (uart_lsr() & 0x20)
			break;
	}
	put32(AUX_MU_IO_REG, c);
}
