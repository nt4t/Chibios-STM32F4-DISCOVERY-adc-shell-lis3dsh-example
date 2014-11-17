/*
Based on ChibiOS/testhal/STM32F0xx/ADC,

PA2(TX) and PA3(RX) are routed to USART2 38400-8-N-1.

PC0 and PC1 are analog inputs
Connect PC0 to 3.3V and PC1 to GND for analog measurements.

*/
#include "ch.h"
#include "hal.h"
#include "shell.h"
#include "chprintf.h"
#include "tm_stm32f4_lis302dl_lis3dsh.h"

#define ADC_GRP1_NUM_CHANNELS   1
#define ADC_GRP1_BUF_DEPTH      8

#define ADC_GRP2_NUM_CHANNELS   4
#define ADC_GRP2_BUF_DEPTH      16

#define SHELL_WA_SIZE   THD_WA_SIZE(1024)

static adcsample_t samples1[ADC_GRP1_NUM_CHANNELS * ADC_GRP1_BUF_DEPTH];
static adcsample_t samples2[ADC_GRP2_NUM_CHANNELS * ADC_GRP2_BUF_DEPTH];

/*
 * ADC streaming callback.
 */
float pc1 = 0;
float pc2 = 0;
float pc1_volt = 0;
float pc2_volt = 0;
float pc1_temp = 0;

size_t nx = 0, ny = 0;
static void adccallback(ADCDriver *adcp, adcsample_t *buffer, size_t n) {
    (void)adcp;
    if (samples2 == buffer) {
	nx += n;
    }
    else {
	ny += n;
    }

    pc1 = (float) samples2[0];
    pc2 = (float) samples2[1];
    pc1_volt = (pc1 / 4095) * 3.3;
    pc2_volt = (pc2 / 4095) * 3.3;
    pc1_temp = pc1_volt * 100;
}

static void adcerrorcallback(ADCDriver *adcp, adcerror_t err) {
    (void)adcp;
    (void)err;
}

/*
 * ADC conversion group.
 * Mode:        Continuous, 16 samples of 8 channels, SW triggered.
 * Channels:    IN11, IN12, IN11, IN12, IN11, IN12, Sensor, VRef.
 */
static const ADCConversionGroup adcgrpcfg2 = {
  TRUE,
  ADC_GRP2_NUM_CHANNELS,
  adccallback,
  adcerrorcallback,
  0,                        /* CR1 */
  ADC_CR2_SWSTART,          /* CR2 */
  ADC_SMPR1_SMP_AN12(ADC_SAMPLE_56) | ADC_SMPR1_SMP_AN11(ADC_SAMPLE_56) |
  ADC_SMPR1_SMP_SENSOR(ADC_SAMPLE_144) | ADC_SMPR1_SMP_VREF(ADC_SAMPLE_144),
  0,                        /* SMPR2 */
  ADC_SQR1_NUM_CH(ADC_GRP2_NUM_CHANNELS),
  ADC_SQR2_SQ8_N(ADC_CHANNEL_SENSOR) | ADC_SQR2_SQ7_N(ADC_CHANNEL_VREFINT),
  ADC_SQR3_SQ6_N(ADC_CHANNEL_IN12)   | ADC_SQR3_SQ5_N(ADC_CHANNEL_IN11) |
  ADC_SQR3_SQ4_N(ADC_CHANNEL_IN12)   | ADC_SQR3_SQ3_N(ADC_CHANNEL_IN11) |
  ADC_SQR3_SQ2_N(ADC_CHANNEL_IN12)   | ADC_SQR3_SQ1_N(ADC_CHANNEL_IN11)
};

static const PWMConfig pwmcfg = {
  100000,                                   /* 100kHz PWM clock frequency.  */
  128,                                      /* PWM period is 128 cycles.    */
  NULL,
  {
   {PWM_OUTPUT_ACTIVE_HIGH, NULL},
   {PWM_OUTPUT_ACTIVE_HIGH, NULL},
   {PWM_OUTPUT_ACTIVE_HIGH, NULL},
   {PWM_OUTPUT_ACTIVE_HIGH, NULL}
  },
  /* HW dependent part.*/
  0,
  0
};


/*
 * SPI1 configuration structure.
 * Speed 5.25MHz, CPHA=1, CPOL=1, 8bits frames, MSb transmitted first.
 * The slave select line is the pin GPIOE_CS_SPI on the port GPIOE.
 */
static const SPIConfig spi1cfg = {
  NULL,
  /* HW dependent part.*/
  GPIOE,
  GPIOE_CS_SPI,
  SPI_CR1_BR_0 | SPI_CR1_BR_1 | SPI_CR1_CPOL | SPI_CR1_CPHA
};

static uint8_t readByteSPI(uint8_t reg)
{
	char txbuf[2] = {0x80 | reg, 0xFF};
	char rxbuf[2];
	spiSelect(&SPID1);
	spiExchange(&SPID1, 2, txbuf, rxbuf);
	spiUnselect(&SPID1);
	return rxbuf[1];
}

static uint8_t writeByteSPI(uint8_t reg, uint8_t val)
{
	char txbuf[2] = {reg, val};
	char rxbuf[2];
	spiSelect(&SPID1);
	spiExchange(&SPID1, 2, txbuf, rxbuf);
	spiUnselect(&SPID1);
	return rxbuf[1];
}

/*
 * Red LEDs blinker thread, times are in milliseconds.
 */
static WORKING_AREA(waThread1, 128);
static msg_t Thread1(void *arg) {
  (void)arg;
  chRegSetThreadName("blinker");
  while (TRUE) {
//    palSetPad(GPIOD, GPIOD_LED5);
    chThdSleepMilliseconds(500);
//    palClearPad(GPIOD, GPIOD_LED5);
    chThdSleepMilliseconds(500);
  }
}

static void cmd_temp(BaseSequentialStream *chp, int argc, char *argv[]) {
    chprintf(chp, "LM35 Temp: %.2f \n\r", pc1_temp);  
}

static void cmd_volt(BaseSequentialStream *chp, int argc, char *argv[]) {
    chprintf(chp, "PC1 DCV: %.2f \n\r", pc1_volt);  
    chprintf(chp, "PC2 DCV: %.2f \n\r", pc2_volt);  
}

static void cmd_smp(BaseSequentialStream *chp, int argc, char *argv[]) {
    int first_arg = atoi(argv[0]);
    float smp = ((float) samples2[first_arg]  / 4095) * 3.3;
    chprintf(chp, "sample[%d]: %f \n\r", first_arg, smp);  
}

static void cmd_ledOn(BaseSequentialStream *chp, int argc, char *argv[]) {
    chprintf(chp, "PD3,4,6 Led on \n\r");  
    pwmEnableChannel(&PWMD4, 0, (pwmcnt_t)+4);
    pwmEnableChannel(&PWMD4, 1, (pwmcnt_t)+3);
    pwmEnableChannel(&PWMD4, 2, (pwmcnt_t)+2);
    pwmEnableChannel(&PWMD4, 3, (pwmcnt_t)+1);

}

static void cmd_ledOff(BaseSequentialStream *chp, int argc, char *argv[]) {
    chprintf(chp, "PD3,4,6 Led off \n\r");  
    pwmEnableChannel(&PWMD4, 0, (pwmcnt_t)0);
    pwmEnableChannel(&PWMD4, 1, (pwmcnt_t)0);
    pwmEnableChannel(&PWMD4, 2, (pwmcnt_t)0);
    pwmEnableChannel(&PWMD4, 3, (pwmcnt_t)0);

}

static void cmd_adcl(BaseSequentialStream *chp, int argc, char *argv[]) {
}

static void cmd_accel(BaseSequentialStream *chp, int argc, char *argv[]) {

    if (readByteSPI(LIS302DL_LIS3DSH_REG_WHO_I_AM) == LIS3DSH_ID) {
	chprintf((BaseSequentialStream *) &SD2, "LIS3DSH found \n\r");
    }
    else {
	chprintf((BaseSequentialStream *) &SD2, "LIS3DSH NOT found. Perhaps your revision of F4Discovery uses LIS302DL \n\r");
	chThdSleepMilliseconds(5500);
    }

    uint16_t temp;
    uint8_t tmpreg;

    /* Set data */
    temp = (uint16_t) (LIS3DSH_DATARATE_100 | LIS3DSH_XYZ_ENABLE);
    temp |= (uint16_t) (LIS3DSH_SERIALINTERFACE_4WIRE | LIS3DSH_SELFTEST_NORMAL);
    temp |= (uint16_t) (LIS3DSH_FULLSCALE_2);
    temp |= (uint16_t) (LIS3DSH_FILTER_BW_800 << 8);

    /* Configure MEMS: power mode(ODR) and axes enable */
    tmpreg = (uint8_t) (temp);

    /* Write value to MEMS CTRL_REG4 register */
    writeByteSPI(LIS3DSH_CTRL_REG4_ADDR, tmpreg);

    /* Configure MEMS: full scale and self test */
    tmpreg = (uint8_t) (temp >> 8);
    writeByteSPI(LIS3DSH_CTRL_REG5_ADDR, tmpreg);

//    chprintf((BaseSequentialStream *) &SD2, "read REG4 %x\n\r",  readByteSPI(LIS3DSH_CTRL_REG4_ADDR));

 
  while(TRUE) {
    int8_t buffer[6];
    
    buffer[0] = readByteSPI(LIS3DSH_OUT_X_L_ADDR);
    buffer[1] = readByteSPI(LIS3DSH_OUT_X_H_ADDR);

    chprintf((BaseSequentialStream *) &SD2, " %d ", (int16_t)(((buffer[1] << 8) + buffer[0]) * 0.02));
   
    buffer[2] = readByteSPI(LIS3DSH_OUT_Y_L_ADDR);
    buffer[3] = readByteSPI(LIS3DSH_OUT_Y_H_ADDR);

    chprintf((BaseSequentialStream *) &SD2, " %d ", (int16_t)(((buffer[3] << 8) + buffer[2]) * 0.02));

    buffer[4] = readByteSPI(LIS3DSH_OUT_Y_L_ADDR);
    buffer[5] = readByteSPI(LIS3DSH_OUT_Y_H_ADDR);

    chprintf((BaseSequentialStream *) &SD2, " %d ", (int16_t)(((buffer[5] << 8) + buffer[4]) * 0.02));

    chprintf((BaseSequentialStream *) &SD2, "temp %x \n\r", readByteSPI(0x0c));


    //TURN PWM LEDS
    int16_t x = (int16_t)(((buffer[1] << 8) + buffer[0]) * 0.001);

    if (x > 0){
	pwmEnableChannel(&PWMD4, 2, (pwmcnt_t) + x);
	pwmEnableChannel(&PWMD4, 0, (pwmcnt_t)0);
    }
    else {
	pwmEnableChannel(&PWMD4, 2, (pwmcnt_t)0);
	pwmEnableChannel(&PWMD4, 0, (pwmcnt_t) - x);
    }

    int16_t y = (int16_t)(((buffer[3] << 8) + buffer[2]) * 0.001);

    if (y > 0){
	pwmEnableChannel(&PWMD4, 1, (pwmcnt_t) + y);
	pwmEnableChannel(&PWMD4, 3, (pwmcnt_t)0);
    }
    else {
	pwmEnableChannel(&PWMD4, 1, (pwmcnt_t)0);
	pwmEnableChannel(&PWMD4, 3, (pwmcnt_t) - y);
    }

    chThdSleepMilliseconds(100);
  }
}

static void cmd_threads(BaseChannel *chp, int argc, char *argv[]) {
  static const char *states[] = {
    "READY",
    "CURRENT",
    "SUSPENDED",
    "WTSEM",
    "WTMTX",
    "WTCOND",
    "SLEEPING",
    "WTEXIT",
    "WTOREVT",
    "WTANDEVT",
    "SNDMSG",
    "WTMSG",
    "FINAL"
  };
  Thread *tp;
  char buf[60];

  chprintf(chp, "           addr      stack      prio  refs  state  time \n\r\n\r");  
 
  tp = chRegFirstThread();
  do {
    sprintf(buf, "%8p %8p %4i %4i %9s %i",
            tp, tp->p_ctx, tp->p_prio, tp->p_refs - 1,
            states[tp->p_state], tp->p_time);
    
    chprintf(chp, "threads: %s \n\r", buf);  
    
    tp = chRegNextThread(tp);
  } while (tp != NULL);
}

static const ShellCommand shCmds[] = {
    {"temp",   cmd_temp},
    {"volt",   cmd_volt},
    {"ledon",  cmd_ledOn},
    {"ledoff", cmd_ledOff},
    {"threads", cmd_threads},
    {"smp",   cmd_smp},
    {"accel",   cmd_accel},
    {"adcl",   cmd_adcl},
    {NULL, NULL}
};

static const ShellConfig shCfg = {
    (BaseSequentialStream *)&SD2,
    shCmds
};



/*
 * Application entry point.
 */
int main(void) {

    Thread *sh = NULL;

    halInit();
    chSysInit();

    sdStart(&SD2, NULL);

    palSetPadMode(GPIOA, 2, PAL_MODE_ALTERNATE(7));      /* USART1 TX.       */
    palSetPadMode(GPIOA, 3, PAL_MODE_ALTERNATE(7));      /* USART1 RX.       */

    /*
    * Setting up analog inputs used by the demo.
    */
    palSetGroupMode(GPIOC, PAL_PORT_BIT(1) | PAL_PORT_BIT(2),
                  0, PAL_MODE_INPUT_ANALOG);

    spiStart(&SPID1, &spi1cfg);

    /*
    * Creates the blinker thread.
    */
    chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

    /*
    * Linear conversion.
    */
//    adcConvert(&ADCD1, &adcgrpcfg1, samples1, ADC_GRP1_BUF_DEPTH);
//    chThdSleepMilliseconds(1000);

    pwmStart(&PWMD4, &pwmcfg);
    palSetPadMode(GPIOD, GPIOD_LED4, PAL_MODE_ALTERNATE(2));      /* Green.   */
    palSetPadMode(GPIOD, GPIOD_LED3, PAL_MODE_ALTERNATE(2));      /* Orange.  */
    palSetPadMode(GPIOD, GPIOD_LED5, PAL_MODE_ALTERNATE(2));      /* Red.     */
    palSetPadMode(GPIOD, GPIOD_LED6, PAL_MODE_ALTERNATE(2));      /* Blue.    */

    adcStart(&ADCD1, NULL);
    adcSTM32EnableTSVREFE();
    adcStartConversion(&ADCD1, &adcgrpcfg2, samples2, ADC_GRP2_BUF_DEPTH);

    shellInit();

    for (;;) {
    if (!sh)
      sh = shellCreate(&shCfg, SHELL_WA_SIZE, NORMALPRIO);
    else if (chThdTerminated(sh)) {
      chThdRelease(sh);
      sh = NULL;
    }
    chThdSleepMilliseconds(1000);
    }
}
