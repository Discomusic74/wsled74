#ifndef __MYDEFS_H
#define __MYDEFS_H

#if defined (__AVR_ATmega644__) || defined (__AVR_ATmega168__) || defined (__AVR_ATmega328P__)
#define TIMSK			TIMSK1
#endif

//#define min_value		2000								// Schwellwert Anzeige

/*so werfen wir zwar fast ein Bit in der Aufl�sung weg,       
ist aber egal, sp�ter wird noch viel mehr 'entsorgt',   
der OPV wird vermutlich eh nicht viel weiter aussteuern k�nnen*/
#define SCORE_MAX		((INT16_MAX / 2) + min_value)

#define CHANNELS		8 // �� ������ - ���������� �������
#define LEDPERCANEL		18 // ������ - ���������� ����������� �� ���� �����
#define NUMLEDMAX		CHANNELS * LEDPERCANEL // ����� ���������� �����������, ������������ � ��������

#define HSV_HUE_MAX		255
#define HSV_SAT_MAX		255
#define HSV_VAL_MAX		255

#define ABS(x) ((x) < 0 ? -(x) : (x)) // ������ �����

#if (CHANNELS != 8)
	#error Wrong set number of CHANNELS. CHANNELS can be only "8".
#endif

#endif