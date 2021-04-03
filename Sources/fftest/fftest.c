
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h> 
#include "FHT.h"		/* Defs for using Fixed-point FHT module */
#include "light_ws2812.h"
#include "colorflow.h"

/*------------------------------------------------*/
/* Global variables                               */
#define CHANNELS		8 // количество каналов
#define LEDPERCANEL		18 //  количество светодиодов на один канал
#define NUMLEDMAX		CHANNELS * LEDPERCANEL // общее количество светодиодов

#define HSV_HUE_MAX		255
#define HSV_SAT_MAX		255
#define HSV_VAL_MAX		255

#define ABS(x) ((x) < 0 ? -(x) : (x)) // модуль числа

uint16_t capture[CHANNELS]; // массив для разноса значений FHT по каналам
uint8_t counter[CHANNELS]; // массив для отсчета паузы перед затузанием по каналам

volatile uint8_t peak[CHANNELS]; // массив для значений текущей яркости по каналам

volatile uint8_t no_audio;       // глобальный флаг отсутствия аудио
volatile uint8_t pause;			// глобальный флаг включения бегущих огней

volatile static uint8_t soundSpectr = 1; // Режимспектрапоумолчанию
volatile static uint8_t colorMusic = 0; //ФлагрежимаЦветомузыки
volatile static uint8_t vuMetr = 0; // ФлагрежимаВольтметр
volatile uint16_t devider; // делитель

volatile static uint16_t fade = 0; // счетчик для затухания
volatile static uint16_t fadespeed = 320; // скорость затухания огней по умолчанию = fadeinitial * faderate

volatile static uint8_t cmumode = 1; 

volatile uint16_t min_value ; // минимальное пороговое значение каналов
volatile uint16_t max_value ; // максимальное пороговое значение каналов
volatile static unsigned char changeValue = 1;
static unsigned char currentledp = 0; // переменная для эффектов - текущий светодиод для обработки

const CF_ColorPallete_TypeDef CF_ColorPallete_Rainbow = {8, CF_COLORPALLETE_RAINBOW}; //палитра "радуга"
volatile static uint16_t rainbow; // переменная текущего цвета Hue радуги
volatile static uint8_t currentled = 0; // переменная для цму - текущий светодиод для обработки
volatile static uint8_t rainbowspeed; // счетчик для регулировки скорости смены Hue цветов радуги
volatile static uint8_t rainbowChannel; // счетчик для регулировки скорости смены Hue цветов радуги
const volatile static uint8_t MaxBright = 63; // константа максимального адреса в таблице яркости
volatile static uint8_t volfade = 0; // переменная для скорости затухания в режиме vu-meter

volatile uint8_t fourChannels = 0; //4 канала для 2 режима

// массивы для хранения значений светодиодов в режиме паузы для организации затухания

uint8_t Hue[NUMLEDMAX]; // массив для затухания эффуктов - цвет
uint8_t Color[50]; // массив для предрасчета еффектов

struct
{
	uint8_t position; 
	uint8_t vector; 
	uint8_t R; 
	uint8_t G; 
	uint8_t B;
} lines[6]; 

volatile static uint8_t vector = 1; // переменная эффектов - направление

// структуры для преобразования цветового пространства HSV -> RGB
CF_HSV_TypeDef HSV;
CF_RGB_TypeDef RGB;

// структура-массив для посылки данных ws2812b
struct cRGB led[NUMLEDMAX];


volatile static uint8_t tempeffone = 0; // переменная эффектов - для временного значения
// массив яркости для ЦМУ
uint8_t EffectCalc[NUMLEDMAX]; // массив для предрасчета еффектов


const uint8_t BrightnessTable2[64] = {
0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 2 , 2 , 3 , 4 , 5 , 6,
7 , 8 , 9 , 11 , 12 , 14 , 16 , 18 , 20 , 22 , 24 , 27 , 30 , 32 , 36 , 39,
42 , 46 , 49 , 53 , 58 , 62 , 66 , 71 , 76 , 81 , 87 , 92 , 98 , 104 , 110 , 117,
123 , 130 , 138 , 145 , 153 , 161 , 169 , 177 , 186 , 195 , 204 , 214 , 223 , 234 , 244 , 255
};
const uint8_t BrightnessTable[64] = {
	0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 2 , 2 , 3 , 4 , 5 , 6 , 7 , 9,
	10 , 12 , 13 , 15 , 17 , 19 , 21 , 24 , 26 , 29 , 32 , 35 , 38 , 41 , 44 , 48,
	52 , 55 , 59 , 64 , 68 , 72 , 77 , 82 , 87 , 92 , 98 , 103 , 109 , 115 , 121 , 127,
	134 , 141 , 147 , 154 , 162 , 169 , 177 , 185 , 193 , 201 , 209 , 218 , 227 , 236 , 245 , 255,
};
const uint8_t BrightnessTable3[64] = {
	0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 2 , 2 , 3 , 3,
	4 , 5 , 6 , 7 , 9 , 10 , 11 , 13 , 15 , 17 , 19 , 21 , 23 , 26 , 28 , 31,
	34 , 38 , 41 , 45 , 49 , 53 , 57 , 62 , 67 , 72 , 77 , 82 , 88 , 94 , 100 , 107,
	114 , 121 , 129 , 136 , 144 , 153 , 161 , 170 , 180 , 189 , 199 , 210 , 220 , 231 , 243 , 255
};

void hsvtorgb (void) { // HSV => RGB	
	CF_HSV2RGB(&HSV, &RGB, &CF_ColorPallete_Rainbow);	
}
//

void shift (void) {
		tempeffone = EffectCalc[0];
		for (uint8_t y = 0; y < (NUMLEDMAX-1); y++)	{
			EffectCalc[y] = EffectCalc[y + 1];
		}
		EffectCalc[(NUMLEDMAX-1)] = tempeffone;
}
/*------------------------------------------------*/
/* Timer 1 Output COMPARE A Interrupt             */

ISR( TIMER1_COMPA_vect ) {
	
	//==================================================+
	// Переменные счетчиков								|
	//==================================================+
	static unsigned char ScanPeriodButton=4; // защита от "дребезга" для кнопок, такт для других счетчиков

	static unsigned char almostsec = 0; //"почтисекунда" - условная временная единица, один такт отсчета ScanPeriodButton
	static unsigned char second = 0; // счетчик "секунд" 
	//==================================================+		
	// Переменные индикации, кнопок						|
	//==================================================+
	static unsigned char bOldButton; // флаги для распознования одиночного нажатия кнопок
	static unsigned char bButton; // флаги для распознования одиночного нажатия кнопок
	//==================================================+
	// Переменные бегущих огней							|
	//==================================================+	
	static unsigned char event = 0; // флаг срабатывания/действия
	static unsigned char effectinit = 0; // флаг расчета эффектов
	
	static unsigned char vmetrcount = 0; // переменная для регулировки скорости спадания пика
	static unsigned char changeMode = 1; // флагсменырежима
	
	OCR1A += (uint16_t) UINT16_MAX; // таймер прерывания
	
	//смена Hue для радуги
	if (rainbowspeed) rainbowspeed--;
	if (rainbowspeed == 0) {
		rainbow++;
		if (rainbow > HSV_HUE_MAX) rainbow = 0;
		rainbowspeed = rainbowChannel;	
	}
		
	//==================================================+
	// Обработка кнопок "Fade Speed" и "Runlight" 		|
	//==================================================+	
	
	if (ScanPeriodButton) ScanPeriodButton--;	// отсчет периодичности опроса кнопки
	else { // по истечению периода опроса проверяем состояния кнопок
		
		ScanPeriodButton=4;	// перезагружаем счетчик периода опроса 
		//
		//==================================================+
		// Определение нажатий						 		|
		//==================================================+
		//if (((PINB & (1<<PB0))) && ((PINC & (1<<PC0))) && ((PINB & (1<<PB1))))  bButton = 0; // ничего не нажато
		
		if ((!(PINB & (1<<PB0)))) { // определяем нажатие
			bOldButton = 1;
			bButton = 1;
		} else bButton = 0;
			

		//==================================================+
		// Обработка одиночных нажатий				 		|
		//==================================================+
		if ((bOldButton != 0) && (bButton == 0)) { // защита от ложного срабатывания при удержании кнопки в нажатом состоянии + срабатывание на отпускание
			
				second = 0; // обнуляем счетчик для паузы/эффектов
				almostsec = 0;
				
				changeMode = 1; // флаг смены режима для индикации
				changeValue = 1; // флаг смены режима для установки значений
				cmumode++;
				if (cmumode > 3) cmumode = 1;
				bOldButton = 0;		
						
		}
		
		
		
		if ((no_audio == 1) && (pause == 0)) { // если звука нет и нам надо определить переходить ли в "паузу"
			almostsec++; // счетчик "почтисекунд" =)
					
				if (almostsec == 50) { // условно 40 почтисекунд = 1 секунде
					second++; // считаем секунды
					almostsec = 0;
			}
			
			if (second == 6) { // если 3 условных секунд нету сигнала переходим в паузу
									
				// переход в паузу и установка начальных значений для эффектов
				pause = 1;
				second = 0;
				almostsec = 0;
				effectinit = 1;

			}
		} else {
			second = 0;
			changeValue = 1;
		}
		
		//==================================================+
		// Обработка эффектов								|
		//==================================================+
		
		if (effectinit == 1) {
			
				// чистка массивов
				for(uint8_t y = 0; y < NUMLEDMAX; y++) {
					EffectCalc[y] = (((sin((2*M_PI*y)/((NUMLEDMAX/2))))+1)*20)+20;
					led[y].r = 0;
					led[y].g = 0;
					led[y].b = 0;
				}
				rainbowChannel = 3;
				currentledp = 0;
				almostsec = 0;
				event = 0;
				
			effectinit = 0;		
		}

			if (pause == 1)	{		
			
				for(uint8_t y = 0; y < NUMLEDMAX/2; y++) {
					HSV.H = (y*2)+rainbow;
					if (HSV.H > HSV_HUE_MAX) HSV.H -= HSV_HUE_MAX;
					HSV.S = HSV_SAT_MAX;
					HSV.V = BrightnessTable[EffectCalc[y]];
					hsvtorgb();
					led[y].r=RGB.R;led[y].g=RGB.G;led[y].b=RGB.B;
					led[NUMLEDMAX-y-1].r=RGB.R;led[NUMLEDMAX-y-1].g=RGB.G;led[NUMLEDMAX-y-1].b=RGB.B;
				}
				shift();
			}
	}
	
	if (changeMode)
	{
		switch (cmumode)
		{
			case 1:
				soundSpectr = 1;
				colorMusic = 0;
				vuMetr = 0;
				PORTB |= (1<<PB3);		// вкл
				PORTB &= ~(1<<PB4);		// выкл
				PORTB &= ~(1<<PB5);		// выкл
			break;
			case 2:
				soundSpectr = 0;
				colorMusic = 1;
				vuMetr = 0;
				PORTB &= ~(1<<PB3);		// выкл
				PORTB |= (1<<PB4);		// вкл
				PORTB &= ~(1<<PB5);		// выкл
			break;
			case 3:
				soundSpectr = 0;
				colorMusic = 0;
				vuMetr = 1;
				PORTB &= ~(1<<PB3);		// выкл
				PORTB &= ~(1<<PB4);		// выкл
				PORTB |= (1<<PB5);	    // вкл
			break;
		}
		changeMode = 0;
	}

	
}

/*------------------------------------------------*/
/* Timer 1 Output COMPARE B Interrupt             */

ISR( TIMER1_COMPB_vect ) {

	OCR1B += (uint8_t) UINT8_MAX;
	
	if (pause == 0)	{ //затухание для ЦМУ
		//затухание для режимов с разделением по каналам
		if( fade >= fadespeed) { // счетчик циклов, при сробатывании обнуляется. чем он меньше, тем чаще будет происходить "затухание" при отсутствии нового "пика"
			for(uint8_t y = 0; y < CHANNELS; y++) if(peak[y]) peak[y]--;
			fade = 0;
		}
		
		// затухание для режима громкость
		if (vuMetr) {
			for(uint8_t y = 0; y < NUMLEDMAX; y++) {
					if (led[y].r >= volfade) led[y].r-= volfade; else led[y].r = 0;
					if (led[y].g >= volfade) led[y].g-= volfade; else led[y].g = 0;
					if (led[y].b >= volfade) led[y].b-= volfade; else led[y].b = 0;
			}
		}	
	} 

	fade++; // счетчик для затухания
	
}

void capture_wave(int16_t *buffer, uint16_t count) {
	ADMUX = _BV(ADLAR)|_BV(MUX2)|_BV(MUX0);				// channel
	
	do {
		ADCSRA = _BV(ADEN)|_BV(ADSC)|_BV(ADATE)|_BV(ADIF)|_BV(ADPS2)|_BV(ADPS1)|_BV(ADPS0);
		//ADCSRA = _BV(ADEN)|_BV(ADSC)|_BV(ADATE)|_BV(ADIF)|_BV(ADPS2)|_BV(ADPS1);
		while(bit_is_clear(ADCSRA, ADIF));
		*buffer++ = ADC - 32768;
	} while(--count);

	ADCSRA = 0;
}

void SetValueMode(){
	
	if (colorMusic)
	{
		devider = 300;
		min_value = 2200;
		fourChannels = 4;
	}
	if (soundSpectr) {
		devider = 150;
		min_value = 2000;
		fourChannels = 0;
		rainbowChannel = 18;
	}
	if (vuMetr){
		devider = 80;
		min_value = 2000;
		fourChannels = 0;
		rainbowChannel = 3;
	}
	max_value = ((UINT16_MAX / 2) + min_value); // установка нового максимума
}


/*------------------------------------------------*/
/*                                                */

int main (void) {
	uint16_t spectr;
	uint16_t vmetr = 0;
	uint8_t vled = 0;
	uint8_t direction = 0;

	TCCR1B = _BV(CS10);	// Таймер работает на полной частоте системы
	TIMSK1 |= _BV(OCIE1A) | _BV(OCIE1B);	// Включить прерывания
	
	PORTB |= (1<<PB0); 		// Кнопка "Fade Speed"

	//PORTC |= (1<<PC0);		// Кнопка "on/off runlight"
	//PORTD &= ~(1<<PD2);
	
	//DDRB |= (1<<PB2);		// Вывод - светодиод для индикации нажатия на кнопки
	
	DDRB = 0b00111100; // define PB2...PB5 as digital output
	//DDRC = 0b00011110; // define PC1...PC4 as digital output (Agressive modes status LEDs)
	//DDRD |= (1<<PD2);  // define PD2 as digital output
	
	sei();
	
	for(;;) {
		
		if (changeValue){
			SetValueMode();		// установка значений для разных режимов
			changeValue = 0;
		}

		// чистка массива перед повторным использованием
		memset( fht_input, 0, sizeof(fht_input) );
		
		capture_wave(fht_input, FHT_N); // put real data into bins
		fht_window(); // window the data for better frequency response
		fht_reorder(); // reorder the data before doing the fht
		fht_run(); // process the data in the fht
		fht_mag_lin(); // take the output of the fht
		
		 //чистка массива перед повторным использованием
		 for (uint8_t y = 0; y < CHANNELS; y++) {
			 capture[y] = 0;
		 }
		
		vmetr = 0;
				
		//==================================================+
		// Разложение спектра на каналы						|
		//==================================================+		
		
			for (uint16_t n = 2; n < FHT_N / 2; n++) {
				spectr = fht_lin_out[n];
				if (colorMusic){
						switch (n) {
						case 2 ... 7: capture[0] += spectr+spectr/4;// низкие частоты
						break;
						case 8 ... 22:capture[1] += spectr;
						break;
						case 23 ... 56: capture[2] += spectr;
						break;
						case 57 ... 127: capture[3] += spectr;
						break;
						} 
					} else {
						switch (n)
							{	
							case 2 ... 4: capture[0] += spectr+spectr/4;// низкие частоты
							break;
							case 5 ... 9:capture[1] += spectr;
							break;
							case 10 ... 19: capture[2] += spectr;
							break;
							case 20 ... 36: capture[3] += spectr;
							break;
							case 37 ... 56: capture[4] += spectr;
							break;
							case 57 ... 83: capture[5] += spectr;
							break;
							case 84 ... 109: capture[6] += spectr+spectr/2;
							break;
							case 110 ... 127: capture[7] += spectr+spectr/2;	// высокие частоты
							break;
					
							}
				}
			}
		
		//==================================================+
		// Установка значений яркости по каналам			|
		//==================================================+	
		for(uint8_t y = 0; y < CHANNELS-fourChannels; y++) {
			
				if (capture[y] > max_value) capture[y] = max_value; //  избавляемсяотси-туации, где capture[] будетбольшемакс.значения
				if(capture[y] > min_value) capture[y] -= min_value;	// убираем все, что не дотянуло до порогового значения
				else capture[y] = 0;
				capture[y] = capture[y] / devider;						// стандартный режим
							
			if(capture[y] > MaxBright) capture[y] = MaxBright;		// избавляемя от ситуации, где capture[] будет больше MaxBright
										
			if(capture[y] >= peak[y]) peak[y] = capture[y];							// установка нового пика и счетчика паузы затухания	
			
			if (vuMetr) vmetr = vmetr + capture[y];

		}
		
		if (pause==0) {
		//============================================================+
		// Заполнение массива цветом и яркостью для каждого	из ws2812b|
		//============================================================+		
			
		currentled = 0;
		vled = 0;
		direction = 1;
		
		if (vuMetr)  {
			vmetr = vmetr / (CHANNELS);
			if(vmetr > (NUMLEDMAX)) vmetr = NUMLEDMAX;
		}
		
		if (colorMusic){
			currentledp = LEDPERCANEL*2;
		} else currentledp = LEDPERCANEL;
		
			for(uint8_t y = 0; y < CHANNELS-fourChannels; y++) { // для каждого канала
				for(uint8_t x = 0; x < currentledp; x++) { // для каждого светодиода
					
					HSV.S = HSV_SAT_MAX;
					switch (cmumode) //режим работы цму
					{
					case 1: // постоянный цвет
						HSV.H = rainbow;
						if (HSV.H == 31) HSV.H -= 15; //правка для оранжевого
						HSV.V = BrightnessTable[peak[y]];

						hsvtorgb();	
						
						led[currentled].r=RGB.R;led[currentled].g=RGB.G;led[currentled].b=RGB.B;
						break;
						
					case 2: 
						HSV.H = y*64;
						
						HSV.V = BrightnessTable[peak[y]];
						
						if (direction == 1) {
							if (y==3) HSV.H = 213;
							vled = currentled;
							direction = 0;
							} else {
							if (y==3) HSV.H = 160;
							vled = NUMLEDMAX - (currentled + 1);
							direction = 1;
						}
						hsvtorgb();
						
						led[vled].r=RGB.R;led[vled].g=RGB.G;led[vled].b=RGB.B;
						break;
					
					case 3: 
						volfade = 2;
						if (vmetr > NUMLEDMAX/4) volfade = 3;	
						vled = currentled + ((NUMLEDMAX)/2);
						if (vled > NUMLEDMAX) vled = (NUMLEDMAX-1);
							if (currentled <= vmetr) {
								
								HSV.H = ABS((HSV_HUE_MAX - currentled*3)+rainbow);
								if (HSV.H > HSV_HUE_MAX) HSV.H -= HSV_HUE_MAX;
								HSV.V = 200;
								
								hsvtorgb();
								
									led[vled].r=RGB.R;led[vled].g=RGB.G;led[vled].b=RGB.B;
									led[NUMLEDMAX-vled-1].r=RGB.R;led[NUMLEDMAX-vled-1].g=RGB.G;led[NUMLEDMAX-vled-1].b=RGB.B;
								} 
								
						break;
					
					}
				if (direction == 1) currentled++;
				}
			}
			}
			
		//==================================================+
		// Отправка данных на ws2812b						|
		//==================================================+
		cli();
		ws2812_setleds(led, NUMLEDMAX);
		sei();
						
		//==================================================+
		// Определение отсутствия звука						|
		//==================================================+
		if((capture[2] > 0) || (capture[3] > 0) || (capture[4] > 0) || (capture[5] > 0) || (capture[6] > 0))	{ // если сигнал есть, то паузы не будет.		
				
			if (pause != 0) fade = 0;
			no_audio = 0;  
			pause = 0;
			
		}
		else{ // если сигнала нет, то в прерывании начнется отсчет псевдосекунд до включения паузы.
			no_audio = 1;
		}

	}
}
