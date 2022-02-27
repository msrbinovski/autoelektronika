/* Standard includes. */
#include <stdio.h>
#include <conio.h>
#include <string.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "extint.h"

/* Hardware simulator utility functions */
#include "HW_access.h"

/* SERIAL SIMULATOR CHANNEL TO USE */
#define COM_CH0 (0)
#define COM_CH1 (1)

/* TASK PRIORITIES */
#define	prijem1	( tskIDLE_PRIORITY + (UBaseType_t)6 )
#define	led	( tskIDLE_PRIORITY + (UBaseType_t)5 )
#define	prijem0	( tskIDLE_PRIORITY + (UBaseType_t)4 )
#define	obrada_pod	( tskIDLE_PRIORITY + (UBaseType_t)3 )
#define	send ( tskIDLE_PRIORITY + (UBaseType_t)2 )
#define	display	( tskIDLE_PRIORITY + (UBaseType_t)1 )



/* TASKS: FORWARD DECLARATIONS */

static void SerialReceive0_Task(void* pvParameters);
static void SerialReceive1_Task(void* pvParameters);
static void Obrada_podataka(void* pvParameters);
static void Display_Task(void* pvParameters);
static void led_bar_tsk(void* pvParameters);
static void SerialSend_Task(void* pvParameters);

void main_demo(void);

/* TRASNMISSION DATA - CONSTANT IN THIS APPLICATION */
typedef float my_float;
typedef struct AD
{
	uint8_t ADmin;
	uint8_t ADmax;
	my_float vrednost;
	my_float trenutna_vrednost;
	my_float max_value;
	my_float min_value;
}struktura;

static struktura ADC;
//= { 5, 15, 5, 0, 5 };
static uint8_t stanje_auta = 0;


/* RECEPTION DATA BUFFER */
//#define R_BUF_SIZE (32)
//static uint8_t r_buffer[R_BUF_SIZE];
//static uint8_t r_point;
static uint8_t rezim = 0;
static uint8_t taster_max = 0, taster_min = 0;

/* 7-SEG NUMBER DATABASE - ALL HEX DIGITS */     //ne treba
static const uint8_t hexnum[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 };

/* GLOBAL OS-HANDLES */
//SemaphoreHandle_t LED_INT_BinarySemaphore;
//static SemaphoreHandle_t TXC_BinarySemaphore;    
static SemaphoreHandle_t RXC_BinarySemaphore0;
static SemaphoreHandle_t RXC_BinarySemaphore1;
static SemaphoreHandle_t LED_INT_BinarySemaphore;
static SemaphoreHandle_t Send_BinarySemaphore;
static SemaphoreHandle_t Display_BinarySemaphore;

static QueueHandle_t data_queue;
static TimerHandle_t tajmer_disp;

static uint32_t prvProcessRXCInterrupt(void)
{	// Ovo se desi kad stigne nesto sa serijske
	BaseType_t higher_priority_task_woken = pdFALSE;

	if (get_RXC_status(0) != 0) {// desio se interapt na kanalu 0

		if (xSemaphoreGiveFromISR(RXC_BinarySemaphore0, &higher_priority_task_woken) != pdTRUE) {    //predaj semafor za seriialreceive0
			printf("Greska prvProcessRXCInterrupt \n");
		}
	
		printf(" ");
	}
	if (get_RXC_status(1) != 0) {// desio se interapt na kanalu 1

		if (xSemaphoreGiveFromISR(RXC_BinarySemaphore1, &higher_priority_task_woken) != pdTRUE) {//predaj semafor za seriialreceive1
			printf("Greska prvProcessRXCInterrupt \n");
		}

		printf(" ");
	}
	portYIELD_FROM_ISR((uint32_t)higher_priority_task_woken);	//po zavrsetku taska, vrati se u task u kom si bio
}

static uint32_t OnLED_ChangeInterrupt(void) {    //poziva se kad se desi interapt sa led bara

	BaseType_t higher_priority_task_woken = pdFALSE;
	printf("Usao u onledchange\n");

	if (xSemaphoreGiveFromISR(LED_INT_BinarySemaphore, &higher_priority_task_woken) != pdTRUE) { //predaja semafora
		printf("Greska\n");
	}

	portYIELD_FROM_ISR((uint32_t)higher_priority_task_woken);
}

static void TimerCallBack(TimerHandle_t timer)
{

	if (send_serial_character((uint8_t)COM_CH0, (uint8_t)'T') != 0) { //SLANJE TRIGGER SIGNALA NA KANALU 0 SVAKIH 100ms
		printf("Greska timer \n");
	}
	//proveri da li treba static
	static uint32_t brojac1 = 0;
	brojac1++;
	if (brojac1 == (uint32_t)20) {	//svake 2 sekunde
		brojac1 = (uint32_t)0;
		if (xSemaphoreGive(Send_BinarySemaphore, 0) != pdTRUE) {
			printf("Greska\n");
		}
	}
	if (xSemaphoreGive(Display_BinarySemaphore, 0) != pdTRUE) { //predaja semafora displeju na svakih 100ms
		printf("greska\n");
	}

}

static void SerialReceive0_Task(void* pvParameters)
{	//sve sto dobijamo sa kanala0
	static uint8_t cc;
	static char tmp_str[200];
	//static uint8_t k;
	static uint8_t z = 0;


	for (;;)
	{
		if (xSemaphoreTake(RXC_BinarySemaphore0, portMAX_DELAY) != pdTRUE) {
			printf("Greska\n");
		}
		if (get_serial_character(COM_CH0, &cc) != 0) {
			printf("Greska\n");
		}

		if (cc != (uint8_t)43) {	//sve dok nije naisao na +, smesti karakter po karakter u tmp_str
			tmp_str[z] = (char)cc;
			z++;
		}
		else {
			tmp_str[z] = '\0';	//na mestu + prepise terminator
			z = 0;

			if (xQueueSend(data_queue, &tmp_str, 0) != pdTRUE) {
				printf("Greskared, slanje\n");
			}

		}
	}

}

static void SerialReceive1_Task(void* pvParameters)
{	//sve sto dobijamo sa kanala1
	static uint8_t cc1 = 0;
	static char tmp_string[100], string_queue[100];
	static uint8_t i = 0;

	for (;;)
	{
		if (xSemaphoreTake(RXC_BinarySemaphore1, portMAX_DELAY) != pdTRUE) {
			printf("Greska\n");
		}

		if (get_serial_character(COM_CH1, &cc1) != 0) {
			printf("Greska\n");
		}
		printf("STIGLO: %c\n", cc1);
		if (cc1 != (uint8_t)43) {	//43 ascii +
			tmp_string[i] = (char)cc1;
			i++;
		}
		else {
			tmp_string[i] = '\0'; //na mestu + postavi terminator
			i = 0;
			printf("String sa serijske %s \n", tmp_string);
			strcpy(string_queue, tmp_string);	//kopira string u red
			if (xQueueSend(data_queue, &string_queue, 0) != pdTRUE) {	//informacije preko reda saljemo u task za obradu
				printf("Greska_get\n");
			}
			printf(" \n");
		}

	}

}

static void SerialSend_Task(void* pvParameters)
{
	static uint16_t tmp_broj, tmp_broj_decimale;
	static uint8_t i = 0, k = 0, j = 0, cifra1, cifra2;
	static uint8_t cifra_tmp = 0;
	static char tmp_str1[10];
	static char string_kontinualno[12], string_kontrolisano[13];

	string_kontinualno[0] = 'k';
	string_kontinualno[1] = 'o';
	string_kontinualno[2] = 'n';
	string_kontinualno[3] = 't';
	string_kontinualno[4] = 'i';
	string_kontinualno[5] = 'n';
	string_kontinualno[6] = 'u';
	string_kontinualno[7] = 'a';
	string_kontinualno[8] = 'l';
	string_kontinualno[9] = 'n';
	string_kontinualno[10] = 'o';
	string_kontinualno[11] = '\0';


	string_kontrolisano[0] = 'k';
	string_kontrolisano[1] = 'o';
	string_kontrolisano[2] = 'n';
	string_kontrolisano[3] = 't';
	string_kontrolisano[4] = 'r';
	string_kontrolisano[5] = 'o';
	string_kontrolisano[6] = 'l';
	string_kontrolisano[7] = 'i';
	string_kontrolisano[8] = 's';
	string_kontrolisano[9] = 'a';
	string_kontrolisano[10] = 'n';
	string_kontrolisano[11] = 'o';
	string_kontrolisano[12] = '\0';

	for (;;)
	{
		if (xSemaphoreTake(Send_BinarySemaphore, portMAX_DELAY) != pdTRUE) {
			printf("Greska\n");
		}
		else {
			printf("\n");
			printf("___________________________");
			printf("\n");
		}
		switch (rezim)
		{
		case 0: printf("kontinualnan rezim\n");	//ako je rezim 0 ---> kontinualno
			for (i = 0; i <= (uint8_t)11; i++)
			{ //KONTINUALNO
				if (send_serial_character((uint8_t)COM_CH1, (uint8_t)string_kontinualno[i]) != 0) {
					printf("Greska\n");
				}
				vTaskDelay(pdMS_TO_TICKS(100));

			}
			break;
		case 1: 	printf("kontrolisan rezim\n");	//ako je rezim 1 ---> kontrolisano
			for (i = 0; i <= (uint8_t)12; i++)
			{ //KONTROLISANO
				if (send_serial_character((uint8_t)COM_CH1, (uint8_t)string_kontrolisano[i]) != 0) {
					printf("Greska\n");
				}
				vTaskDelay(pdMS_TO_TICKS(100));

			}
			break;
		default: printf("Usao u default\n");
			break;

		}

		k = (uint8_t)0;
		tmp_broj = (uint16_t)ADC.vrednost;
		tmp_broj_decimale = (uint16_t)ADC.vrednost * (uint16_t)100 - (uint16_t)100 * (uint16_t)tmp_broj;

		if (tmp_broj == (uint8_t)0) {
			if (send_serial_character((uint8_t)COM_CH1, 48) != 0) {
				printf("Greska\n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
			if (send_serial_character((uint8_t)COM_CH1, (uint8_t)13) != 0) {	//13 --> CR
				printf("Greska\n");
			}
		}
		else {
			while (tmp_broj != (uint16_t)0) {
				cifra_tmp = (uint8_t)tmp_broj % (uint8_t)10;	//sve dok je tmp_broj razlicit od 0 iscupa se poslednja cifra i smesti se tmp
				tmp_broj = tmp_broj / (uint16_t)10;				//u neki privremeni string i to od pozadi
				tmp_str1[k] = cifra_tmp + (char)48;
				k++;
			}
			j = 1;

			if (k != (uint8_t)0) {
				while (k != (uint8_t)0) {	//sve dok je k razlicito od 0, ispisuje u obrnutom redosledu

					if (send_serial_character((uint8_t)COM_CH1, (uint8_t)tmp_str1[k - j]) != 0) {
						printf("Greska\n");
					}
					k--;

					vTaskDelay(pdMS_TO_TICKS(100));
				}
			}

			printf("tmp_broj_decimale %d \n", tmp_broj_decimale);
			if (send_serial_character((uint8_t)COM_CH1, (uint8_t)46) != 0) {	//46 ---> posalje tacku
				printf("Greska\n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));

			cifra1 = (uint8_t)tmp_broj_decimale / (uint8_t)10; //prva cifra decimale
			cifra2 = (uint8_t)tmp_broj_decimale % (uint8_t)10;	//druga cifra decimale

			if (send_serial_character((uint8_t)COM_CH1, (uint8_t)48 + cifra1) != 0) {	//posalje prvu cifru decimale
				printf("Greska\n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));

			if (send_serial_character((uint8_t)COM_CH1, (uint8_t)48 + cifra2) != 0) {	//posalje drugu cifru decimale
				printf("Greska\n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));

			if (send_serial_character((uint8_t)COM_CH1, (uint8_t)13) != 0) {	//CR	
				printf("Greska\n");
			}
			i = 0;
		}
	}
}






static void Obrada_podataka(void* pvParameters)   //obrada podataka
{

	static char niz[7];
	static int vrednost1 = 0, brojac = 0;
	static double volti = 0.0;
	static double suma1;

	for (;;) {

		//Iz reda smestamo u niz
		if (xQueueReceive(data_queue, &niz, portMAX_DELAY) != pdTRUE) {
			printf("Greska\n");
		}

		my_float pomocna_suma = (my_float)0;

		//ako je niz admin
		if (niz[0] == 'a' && niz[1] == 'd' && niz[2] == 'm' && niz[3] == 'i' && niz[4] == 'n')
		{
			//ocitavamo vrednost  koja ide nakon admin i smestamo u struktutu
			ADC.ADmin = ((uint8_t)niz[5] - (uint8_t)48) * (uint8_t)10 + ((uint8_t)niz[6] - (uint8_t)48);
			printf("ADmin = %d\n", ADC.ADmin);
		}
		//ako je niz admax
		else if (niz[0] == 'a' && niz[1] == 'd' && niz[2] == 'm' && niz[3] == 'a' && niz[4] == 'x')
		{
			//ocitavamo vrednost  koja ide nakon admax i smestamo u struktutu
			ADC.ADmax = ((uint8_t)niz[5] - (uint8_t)48) * (uint8_t)10 + ((uint8_t)niz[6] - (uint8_t)48);

			printf("ADmax = %d\n", ADC.ADmax);
		}
		//ako je niz kontinu
		else if (niz[0] == 'k' && niz[1] == 'o' && niz[2] == 'n' && niz[3] == 't' && niz[4] == 'i' && niz[5] == 'n' && niz[6] == 'u')
		{

			printf("K O N T I N U A L N O\n");

			if (stanje_auta == (uint8_t)1) {
				if (set_LED_BAR((uint8_t)1, 0x07) != 0) { //upali diode
					printf("Greska\n");
				}

			}
			rezim = 0;
		}

		else if (niz[0] == 'k' && niz[1] == 'o' && niz[2] == 'n' && niz[3] == 't' && niz[4] == 'r' && niz[5] == 'o' && niz[6] == 'l')
		{
			printf("K O N T R O L I S A N O\n");
			rezim = 1;
		}

		else if (niz[0] == (char)48 || niz[0] == (char)49) {	//pristigla vrednost sa serialreceive0 0 ili 1

			//npr 0000, 1023

			int brojacc = 0, cifra, suma = 0;
			while (brojacc < 4) {
				if (niz[brojacc] >= (char)48 && niz[brojacc] <= (char)57) { //u pitanju je broj od 0 do 9
					cifra = niz[brojacc] - 48; //kastujemo u intiger
					suma = suma * 10 + cifra;
					suma1 = ((double)ADC.ADmax - (double)ADC.ADmin) * (double)suma;
				}
				brojacc++;
			}

			volti = (double)suma1 / (double)1023 + (double)ADC.ADmin;
			if (volti < (double)ADC.ADmin) {
				volti = (double)ADC.ADmin;
			}
			ADC.trenutna_vrednost = (my_float)volti;

			if (ADC.trenutna_vrednost > ADC.max_value) {
				ADC.max_value = ADC.trenutna_vrednost;
			}
			if (ADC.trenutna_vrednost < ADC.min_value) {
				ADC.min_value = ADC.trenutna_vrednost;
			}
			if (rezim == (uint8_t)1) {

				printf("Vrednost: %lf\n", volti);

				if (volti < 12.5) {
					//STRUJNO
					if (stanje_auta == (uint8_t)1) {
						if (set_LED_BAR((uint8_t)1, 0X0B) != 0) {
							printf("Greska\n");
						}
					}
					else {
						if (set_LED_BAR((uint8_t)1, 0X0A) != 0) {
							printf("Greska\n");
						}
					}
				}
				else if (volti > (double)13.5 && volti < (double)14) {
					if (stanje_auta == (uint8_t)1) {
						if (set_LED_BAR((uint8_t)1, 0X07) != 0) {
							printf("Greska\n");
						}
					}
					else {
						if (set_LED_BAR((uint8_t)1, 0X06) != 0) {
							printf("Greska\n");
						}
					}
				}
				else if (volti >= (double)14) {
					if (stanje_auta == (uint8_t)1) {
						if (set_LED_BAR((uint8_t)1, 0X01) != 0) {
							printf("Greska\n");
						}
					}
					else {
						if (set_LED_BAR((uint8_t)1, 0X00) != 0) {
							printf("Greska\n");
						}
					}
				}
				else {
					printf(" ");
				}
			}
			else {
				printf(" ");
			}
			if (brojac < 20) {
				vrednost1 = vrednost1 + suma;
				brojac++;
			}
			else {
				//smestamo usrednjenu vrednost u strukturu
				pomocna_suma = (my_float)vrednost1 / (my_float)20;
				ADC.vrednost = ((my_float)ADC.ADmax - (my_float)ADC.ADmin) * pomocna_suma / (my_float)1023 + (my_float)ADC.ADmin;
				brojac = 0;
				vrednost1 = 0;
				printf("Usrednjena vrednsot: %.2f\n", ADC.vrednost);

			}
		}
		else if (niz[0] == 'L') {	//ono sto stize sa led_bara L100 L111...
			printf("LEDOVKE\n");
			stanje_auta = (uint8_t)niz[1] - (uint8_t)48;  //nezavisno da li je 0 ili 1
			if (set_LED_BAR((uint8_t)1, stanje_auta) != 0) {
				printf("Greska\n");
			}
			taster_min = (uint8_t)niz[2] - (uint8_t)48;
			taster_max = (uint8_t)niz[3] - (uint8_t)48;
		}
		else {
			printf("\n");
		}

	}

}

static void Display_Task(void* pvParameters)
{
	
	static uint8_t i = 0, j = 0, k = 0, cifra_tmp, cifra_tmp1, z, l;
	static uint16_t tmp_broj = 0, tmp_broj1 = 0;
	static uint16_t tmp_broj2 = 0, cifra_tmp2 = 0;

	for (;;)
	{

		if (xSemaphoreTake(Display_BinarySemaphore, portMAX_DELAY) != pdTRUE) {
			printf("Greska\n");
		}

		tmp_broj = (uint8_t)ADC.trenutna_vrednost;
		i = 0;
		if (tmp_broj < ADC.ADmin) {
			tmp_broj = (uint8_t)ADC.ADmin;
		}
		printf("TRENUTNA VREDNOST %d\n", tmp_broj);

		for (z = (uint8_t)6; z <= (uint8_t)8; z++) {
			if (select_7seg_digit((uint8_t)z) != 0) {
				printf("Greska\n");
			}
			if (set_7seg_digit(0x00) != 0) {
				printf("Greska\n");
			}
		}
		if (tmp_broj == (uint8_t)0) {
			if (select_7seg_digit((uint8_t)8) != 0) {
				printf("Greska\n");
			}
			if (set_7seg_digit(hexnum[0]) != 0) {
				printf("Greska\n");
			}
		}
		else {
			while (tmp_broj != (uint8_t)0) {

				cifra_tmp = (uint8_t)tmp_broj % (uint8_t)10;

				if (select_7seg_digit((uint8_t)8 - i) != 0) {
					printf("Greska\n");
				}
				if (set_7seg_digit(hexnum[cifra_tmp]) != 0) {
					printf("Greska\n");
				}
				tmp_broj = tmp_broj / (uint8_t)10;
				i++;
			}
		}

		if (taster_max == (uint8_t)1) {        //kada je pritisnut taster MAX na led baru

			taster_max = 0;
			tmp_broj1 = (uint8_t)ADC.max_value;
			j = 0;
			if (tmp_broj1 == (uint8_t)0) {
				if (select_7seg_digit((uint8_t)5) != 0) {
					printf("Greska\n");
				}
				if (set_7seg_digit(hexnum[0]) != 0) {
					printf("Greska\n");
				}
			}
			else {
				while (tmp_broj1 != (uint8_t)0) {
					cifra_tmp1 = (uint8_t)tmp_broj1 % (uint8_t)10;
					if (select_7seg_digit((uint8_t)5 - j) != 0) {
						printf("Greska\n");
					}
					if (set_7seg_digit(hexnum[cifra_tmp1]) != 0) {
						printf("Greska\n");
					}
					tmp_broj1 = tmp_broj1 / (uint16_t)10;
					j++;

				}
			}
		}


		if (taster_min == (uint8_t)1) {        //kada je pritisnut taster MIN na led baru

			taster_min = 0;
			tmp_broj2 = (uint8_t)ADC.min_value;
			for (l = (uint8_t)0; l <= (uint8_t)2; l++) {
				if (select_7seg_digit((uint8_t)l) != 0) {
					printf("Greska\n");
				}
				if (set_7seg_digit(0x00) != 0) {
					printf("Greska\n");
				}
			}
			k = 0;
			if (tmp_broj2 == (uint8_t)0) {
				if (select_7seg_digit((uint8_t)2) != 0) {
					printf("Greska\n");
				}
				if (set_7seg_digit(hexnum[0]) != 0) {
					printf("Greska\n");
				}
			}
			else {
				while (tmp_broj2 != (uint8_t)0) {
					cifra_tmp2 = tmp_broj2 % (uint16_t)10;
					if (select_7seg_digit((uint8_t)2 - k) != 0) {
						printf("Greska\n");
					}
					if (set_7seg_digit(hexnum[cifra_tmp2]) != 0) {
						printf("Greska\n");
					}
					tmp_broj2 = tmp_broj2 / (uint16_t)10;
					k++;
				}
			}
		}



	}
}


static void led_bar_tsk(void* pvParameters) {

	uint8_t led_tmp, cifra_tmp, led_tmp1, i;

	static char tmp_string[20];

	for (;;)
	{
		if (xSemaphoreTake(LED_INT_BinarySemaphore, portMAX_DELAY) != pdTRUE) {
			printf("Greska semafor\n");
		}

		if (get_LED_BAR((uint8_t)0, &led_tmp) != 0) {
			printf("Greska\n");
		}
		//sve sto primi sa tastera 
		led_tmp1 = led_tmp;
		tmp_string[0] = 'L';
		for (i = (uint8_t)1; i <= (uint8_t)3; i++) {
			cifra_tmp = led_tmp1 % (uint8_t)2;
			led_tmp1 = led_tmp1 / (uint8_t)2;
			tmp_string[i] = cifra_tmp + (char)48;
		}
		tmp_string[4] = '\0';
		printf("***********************************");
		printf("LED - %s \n", tmp_string);
		if (xQueueSend(data_queue, &tmp_string, 0) != pdTRUE) {
			printf("Greska\n");
		}
	}

}


void main_demo(void)
{

	uint8_t i;
	if (init_7seg_comm() != 0) {
		printf("Neuspesno\n");
	}
	if (init_LED_comm() != 0) {
		printf("Neuspesno\n");
	}
	if (init_serial_uplink(COM_CH0) != 0) {
		printf("Neuspesno\n");
	}
	if (init_serial_downlink(COM_CH0) != 0) {
		printf("Neuspesno\n");
	}
	if (init_serial_uplink(COM_CH1) != 0) {
		printf("Neuspesno\n");
	}
	if (init_serial_downlink(COM_CH1) != 0) {
		printf("Neuspesno\n");
	}


	/* LED BAR INTERRUPT HANDLER */
	vPortSetInterruptHandler(portINTERRUPT_SRL_OIC, OnLED_ChangeInterrupt);

	vPortSetInterruptHandler(portINTERRUPT_SRL_RXC, prvProcessRXCInterrupt);


	// Semaphores
	RXC_BinarySemaphore0 = xSemaphoreCreateBinary();
	if (RXC_BinarySemaphore0 == NULL) {
		printf("Greska\n");
	}

	RXC_BinarySemaphore1 = xSemaphoreCreateBinary();
	if (RXC_BinarySemaphore1 == NULL) {
		printf("Greska\n");
	}
	/*
		TXC_BinarySemaphore = xSemaphoreCreateBinary();
		if (TXC_BinarySemaphore == NULL) {
			printf("Greska\n");
		}
	*/
	LED_INT_BinarySemaphore = xSemaphoreCreateBinary();
	if (LED_INT_BinarySemaphore == NULL) {
		printf("Greska\n");
	}

	Display_BinarySemaphore = xSemaphoreCreateBinary();
	if (Display_BinarySemaphore == NULL) {
		printf("Greska\n");
	}

	Send_BinarySemaphore = xSemaphoreCreateBinary();
	if (Send_BinarySemaphore == NULL) {
		printf("Greska\n");
	}

	// Queues
	data_queue = xQueueCreate(1, 7u * sizeof(char));

	BaseType_t status;
	tajmer_disp = xTimerCreate(
		"timer",
		pdMS_TO_TICKS(100),
		pdTRUE,
		NULL,
		TimerCallBack
	);

	status = xTaskCreate(SerialReceive0_Task, "receive_task", configMINIMAL_STACK_SIZE, NULL, prijem0, NULL);  //task koji prima podatke sa kanala0
	if (status != pdPASS) {
		for (;;) {}
	}
	//printf(" ");

	status = xTaskCreate(SerialReceive1_Task, "receive_task", configMINIMAL_STACK_SIZE, NULL, prijem1, NULL);  //task koji prima podatke sa kanala1
	if (status != pdPASS) {
		for (;;) {}
	}
	//printf(" ");

	status = xTaskCreate(SerialSend_Task, "send_task", configMINIMAL_STACK_SIZE, NULL, send, NULL); //task za slanje podataka
	if (status != pdPASS) {
		for (;;) {}
	}
	//printf(" ");

	status = xTaskCreate(Obrada_podataka, "Obrada", configMINIMAL_STACK_SIZE, NULL, obrada_pod, NULL);  //task za obradu podataka
	if (status != pdPASS) {
		for (;;) {}
	}
	//printf(" ");

	status = xTaskCreate(led_bar_tsk, "led_task", configMINIMAL_STACK_SIZE, NULL, led, NULL);  //task za led
	if (status != pdPASS) {
		for (;;) {}
	}
	//printf(" ");

	status = xTaskCreate(Display_Task, "display", configMINIMAL_STACK_SIZE, NULL, (UBaseType_t)display, NULL); //task za displej
	if (status != pdPASS) {
		for (;;) {}
	}
	//printf(" ");

	if (xTimerStart(tajmer_disp, 0) != pdPASS) {
		printf("Greska prilikom kreiranja\n");
	}

	ADC.min_value = (double)ADC.ADmax;
	for (i = (uint8_t)0; i <= (uint8_t)8; i++) {
		if (select_7seg_digit((uint8_t)i) != 0) {
			printf("Greska\n");
		}
		if (set_7seg_digit(0x00) != 0) {
			printf("Greska\n");
		}
	}
	vTaskStartScheduler();
	for (;;) {}
}
