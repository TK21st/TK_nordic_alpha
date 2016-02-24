/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

/** @file
 * @defgroup uart_example_main main.c
 * @{
 * @ingroup uart_example
 * @brief UART Example Application main file.
 *
 * This file contains the source code for a sample application using UART.
 * 
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "app_uart.h"
#include "app_error.h"
#include "nrf_delay.h"
#include "nrf.h"
#include "bsp.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "heatshrink_encoder.h"
#include "heatshrink_decoder.h"
#include "heatshrink_config.h"

// HEATSHRINK_DYNAMIC_ALLOC is set to 0 for static allocation
static heatshrink_encoder hse;
static heatshrink_decoder hsd;

#define MAX_TEST_DATA_BYTES     (15U)                /**< max number of test bytes to be used for tx and rx. */
#define UART_TX_BUF_SIZE 256                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE 1                           /**< UART RX buffer size. */

void uart_error_handle(app_uart_evt_t * p_event)
{
    if (p_event->evt_type == APP_UART_COMMUNICATION_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_communication);
    }
    else if (p_event->evt_type == APP_UART_FIFO_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_code);
    }
}

/** @brief Function for setting the @ref ERROR_PIN high, and then enter an infinite loop.
 */
static void show_error(void)
{
    
    LEDS_ON(LEDS_MASK);
    while(true)
    {
        // Do nothing.
    }
}

static void fill_with_patient_data(uint8_t *buf, uint16_t size) {
    uint8_t patient_data[] = {238,239,239,239,240,240,240,240,240,240,240,240,240,239,238,238,238,238,238,238,237,237,236,235,234,233,232,232,231,231,230,230,230,231,231,232,233,233,234,234,234,235,235,235,236,237,238,238,239,240,241,242,243,243,244,244,244,244,244,244,244,243,243,244,244,244,244,244,244,244,244,244,245,245,245,246,246,246,247,247,247,248,248,248,248,248,248,247,247,247,248,248,249,249,249,249,249,249,249,248,248,248,248,248,248,248,248,248,249,249,249,249,248,248,247,247,248,248,248,248,248,248,249,249,249,249,249,248,248,247,246,247,247,248,249,249,250,251,251,251,250,249,249,248,247,246,246,245,244,244,244,244,244,244,245,246,246,246,247,246,246,246,245,245,246,246,246,246,246,246,246,246,247,248,248,248,248,248,248,249,249,250,251,250,250,249,248,246,246,246,245,246,246,245,244,243,243,241,240,239,237,236,235,234,234,233,233,234,234,234,234,234,234,234,234,234,234,234,234,234,234,233,233,233,233,234,234,234,235,235,235,235,235,236,236,236,236,236,236,236,236,236,237,237,238,238,238,237,237,236,235,235,235,235,235,236};
    for (int i=0; i<size; i++) {
        buf[i] = patient_data[i];
    }
}

int ASSERT(COND){
  if (!(COND)) return -1;
}

int ASSERT_EQ(COND1,COND2){
  if (COND1 != COND2) return -1;
}


static int compress_and_expand_and_check(uint8_t *input, uint32_t input_size) {
    // clear encoder&decoder state machines
    heatshrink_encoder_reset(&hse);
    heatshrink_decoder_reset(&hsd);
    // allocate memory for before and after
    size_t comp_sz = input_size + (input_size/2) + 4;
    size_t decomp_sz = input_size + (input_size/2) + 4;
    uint8_t *comp = malloc(comp_sz);
    uint8_t *decomp = malloc(decomp_sz);
    if (comp == NULL) printf("malloc fail\r\n");
    if (decomp == NULL) printf("malloc fail\r\n");
    memset(comp, 0, comp_sz);
    memset(decomp, 0, decomp_sz);

    size_t count = 0;

   
    uint32_t sunk = 0;
    uint32_t polled = 0;
    while (sunk < input_size) {
        ASSERT(heatshrink_encoder_sink(&hse, &input[sunk], input_size - sunk, &count) >= 0);
        sunk += count;
        if (sunk == input_size) {
            ASSERT_EQ(HSER_FINISH_MORE, heatshrink_encoder_finish(&hse));
        }

        HSE_poll_res pres;
        do {                    /* "turn the crank" */
            pres = heatshrink_encoder_poll(&hse, &comp[polled], comp_sz - polled, &count);
            ASSERT(pres >= 0);
            polled += count;
        } while (pres == HSER_POLL_MORE);
        ASSERT_EQ(HSER_POLL_EMPTY, pres);
        if (polled >= comp_sz) {printf("compression should never expand that muchr\r\n"); show_error(); return-1;}
        if (sunk == input_size) {
            ASSERT_EQ(HSER_FINISH_DONE, heatshrink_encoder_finish(&hse));
        }
    }

    float compression_ratio  = (float)(input_size-polled)/(float)(input_size);

    printf("in: %u compressed: %u ratio: %.2f \r\n", input_size, polled, compression_ratio);
    uint32_t compressed_size = polled;
    sunk = 0;
    polled = 0;
    
    while (sunk < compressed_size) {
        ASSERT(heatshrink_decoder_sink(&hsd, &comp[sunk], compressed_size - sunk, &count) >= 0);
        sunk += count;
        if (sunk == compressed_size) {
            ASSERT_EQ(HSDR_FINISH_MORE, heatshrink_decoder_finish(&hsd));
        }

        HSD_poll_res pres;
        do {
            pres = heatshrink_decoder_poll(&hsd, &decomp[polled],
                decomp_sz - polled, &count);
            ASSERT(pres >= 0);
            polled += count;
        } while (pres == HSDR_POLL_MORE);
        ASSERT_EQ(HSDR_POLL_EMPTY, pres);
        if (sunk == compressed_size) {
            HSD_finish_res fres = heatshrink_decoder_finish(&hsd);
            ASSERT_EQ(HSDR_FINISH_DONE, fres);
        }

        if (polled > input_size) {
            printf("Decompressed data is larger than original input\r\n");
            show_error();
            return -1;
        }
    }
    printf("decompressed: %u\r\n", polled);
    if (polled != input_size) {
        printf("Decompressed length does not match original input length\r\n");
   //     show_error();
   //     return -1;
    }

    for (size_t i=0; i<input_size; i++) {
        if (input[i] != decomp[i]) {
            printf("*** mismatch at %zd \r\n", i);
        }
        ASSERT_EQ(input[i], decomp[i]);
    }
    free(comp);
    free(decomp);
}

int pseudorandom_data_should_match(uint32_t size) {
    uint8_t input[size];
    fill_with_patient_data(input,size);
    return compress_and_expand_and_check(input, size);
}




/**
 * @brief Function for main application entry.
 */
int main(void)
{
    LEDS_CONFIGURE(LEDS_MASK);
    LEDS_OFF(LEDS_MASK);
    uint32_t err_code;
    const app_uart_comm_params_t comm_params =
      {
          RX_PIN_NUMBER,
          TX_PIN_NUMBER,
          RTS_PIN_NUMBER,
          CTS_PIN_NUMBER,
          APP_UART_FLOW_CONTROL_ENABLED,
          false,
          UART_BAUDRATE_BAUDRATE_Baud38400
      };

    APP_UART_FIFO_INIT(&comm_params,
                         UART_RX_BUF_SIZE,
                         UART_TX_BUF_SIZE,
                         uart_error_handle,
                         APP_IRQ_PRIORITY_LOW,
                         err_code);

    APP_ERROR_CHECK(err_code);

    printf("\n\rStart: \n\r");

    printf("INPUT_BUFFER_SIZE: %u\r\n", HEATSHRINK_STATIC_INPUT_BUFFER_SIZE);
    printf("WINDOW_BITS: %u\r\n", HEATSHRINK_STATIC_WINDOW_BITS);
    printf("LOOKAHEAD_BITS: %u\r\n", HEATSHRINK_STATIC_LOOKAHEAD_BITS);

    printf("sizeof(heatshrink_encoder): %zd\r\n", sizeof(heatshrink_encoder));
    printf("sizeof(heatshrink_decoder): %zd\r\n", sizeof(heatshrink_decoder));

    for (uint32_t size=4; size < 256; size <<= 1) {
      pseudorandom_data_should_match(size);
    }
		printf("done\r\n");
    while(1);
		
    while (true)
    {
        uint8_t cr;
        while(app_uart_get(&cr) != NRF_SUCCESS);
        while(app_uart_put(cr) != NRF_SUCCESS);

        if (cr == 'q' || cr == 'Q')
        {
            printf(" \n\rExit!\n\r");

            while (true)
            {
                // Do nothing.
            }
        }
    }
}


/** @} */
