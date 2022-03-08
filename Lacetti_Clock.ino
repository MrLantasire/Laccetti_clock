#include <math.h>
#include <TM1637Display.h>
#include <OneWire.h>
#include <TimeLib.h>
#include <DS1307RTC.h>

// Пины подключения дисплея
#define DIO 4
#define CLK 1
// Пин подключения датчика температуры
#define TDATA 3

// Список перечисления для сигналов с кнопок
typedef enum {
  ZERO = 0,
  DOWN,
  UP,
  SET,
  MODE
} Button_t;

// Создание объекта для управления дисплеем
TM1637Display display(CLK, DIO);
// Создание объекта для работы с датчиком температуры
OneWire ds(TDATA);

// Символ градуса Цельсия
const uint8_t SEG[] = {0x63};
// Костыль для отображения двоеточия, когда час равен 0
const uint8_t DZER[] = {0b10111111};
// Переменная для хранения значения температуры
int8_t temp = 0;
// Структура для хранения времени в формате
tmElements_t tm = {0};
// Флаг для перехода в режим настройки часов
bool flag_mode = false;
// Счетчик времени удержания кнопки
uint8_t сounter = 0;

void setup() {
  // Инициализация устройств
  display.clear();
  display.setBrightness(7);
  init_term();
  temp_mes();
  display.clear();
}

void loop() {

  // Переменная для хранения значения сигнала с кнопки
  Button_t button = ZERO;
  button = button_handler(analogRead(A0), &сounter);
  // Флаг для запуска измерения температуры (актуализация значения)
  static bool temp_reload = true;

  if (flag_mode)
  {
    // Настройа часов
    flag_mode = set_clock(&button);
    display.showNumberDecEx(tm.Minute,0b01000000,true,2,2);
    if (tm.Hour != 0)
    {
      display.showNumberDecEx(tm.Hour,0b01000000,false,2,0);
    }
    else
    {
      display.setSegments(DZER,1,1);
    }
   }
  else
  {
    // Нормальная работа
    // Проверка часов DS1307RTC на активность
    if (RTC.read(tm)) 
    {
      if (tm.Second%2 == 0)
      {
        display.showNumberDecEx(tm.Minute,0,true,2,2);
        display.showNumberDecEx(tm.Hour,0,false,2,0);

        if (temp_reload)
        {
          // Запуск измерения температуры (температура измеряется раз в секунду)
          temp_mes();
        }
        temp_reload = false; 
      }
      else
      {
        display.showNumberDecEx(tm.Minute,0b01000000,true,2,2);
        if (tm.Hour != 0)
        {
          display.showNumberDecEx(tm.Hour,0b01000000,false,2,0);
        }
        else
        {
          display.setSegments(DZER,1,1);
        }
        
        if (!temp_reload)
        {
          // Запуск измерения температуры (температура измеряется раз в секунду)
          temp_mes();
        }
        temp_reload = true;
      }
    }
    else
    {
      // Случай, когда часы DS1307RTC не запущены, переход в режим настройки времени
      tm.Hour = 0;
      tm.Minute = 0;
      flag_mode = true;    
    }

    // Переход в режим настройки часов
    if (button == MODE)
    {
      flag_mode = true;
    }

    // Показ температуры по нажатию на кнопку SET
    if ( button == SET )
    {
      if (get_temp(&temp))
      {
        display.showNumberDec(temp,false,3,0);
        display.setSegments(SEG,1,3);
      }
      else
      {
        // Ошибка датчика температуры
        display.showNumberDec(8888,false);
      } 
      delay(3000);    
    } 
  }
}

/* *****************************************
 * Процедура инициализации датчика температуры
 * ****************************************/
void init_term(){
  byte data[8];
  
  ds.reset();
  ds.skip();
  // Чтение настроек
  ds.write(0xBE);
  for (int i =0; i<8; i++)
  {
    data[i] = ds.read();
  }
  // Изменение точности измерения на высоку скорость
  data[4]=0x1F;
  ds.reset();
  ds.skip();
  ds.write(0x4E); 
  // Запись в SRAM датчика новой конфигурации
  for (int i =2; i<5; i++)
  {
    ds.write(data[i]);
  }
  }

/* ******************************************************
 * Процедура для запуска процесса измерения температуры
 * *****************************************************/
void temp_mes()
{
  ds.reset();
  ds.skip();
  ds.write(0x44);
}

/* ******************************************************
 * Функция чтения значения температуры 
 * value - переменная для записи значения температуры от датчика
 * функция возвращает:
 * true - значение получено
 * false - значение не считалось, датчик занят
 * *****************************************************/
bool get_temp(char *value)
{
  byte data[2];  
  
  if (ds.reset())
  {
    ds.skip();
    ds.write(0xBE);

    // Чтение только 2-х байт, содержащих темепературу
    data[0] = ds.read();
    data[1] = ds.read();

    if (bitRead(data[1], 3))
    {
      // Отрицательная температура
      *value =  round((~((data[1] << 8) | data[0]) + 0b00000001) * (-0.0625));
    } else {
      // Положительная температура
      *value =  round(((data[1] << 8) | data[0]) * 0.0625);
    }
    return true;
  } else {return false;}
}

/* ******************************************************
 * Функция установки значения часов
 * but - нажатая кнопка
 * функция возвращает:
 * true - установка времени не закончено
 * false - время установлено
 * *****************************************************/
bool set_clock(Button_t *but)
{
  bool out = true;
  static bool hour_minute = true;

  // Переключение поля ввода часы/минуты
  if (*but == SET)
  {
    hour_minute = !hour_minute;
  }

  // Значение поля +1
  if (*but == UP)
  {
    if(hour_minute)
    {
      tm.Hour = (tm.Hour + 1) % 24;
    }
    else
    {
      tm.Minute = (tm.Minute + 1) % 60;
    }
  }

  // Значение поля -1
  if (*but == DOWN)
  {
    if(hour_minute)
    {
      if (tm.Hour != 0)
      {
        tm.Hour = tm.Hour - 1;
      }
      else
      {
        tm.Hour = 23;
      }
    }
    else
    {
     if (tm.Minute != 0)
     {
        tm.Minute = tm.Minute - 1;
     }
     else
     {
        tm.Minute = 59;
     }
    }
  }

  // Установка времени
  if (*but == MODE)
  {
    tm.Second = 0;
    tm.Day = 1;
    tm.Month = 1;
    tm.Year = 2022;
    RTC.write(tm);
    hour_minute = true;
    out = false;
  }
  return out;
}

/* ******************************************************
 * Функция для определения нажатой кнопки
 * Т.к. 3 кнопки висят на одной ножке МК, то нажатая кнопка 
 * определяется по уровню напряжения на ножке МК
 * analog_signal - значение с АЦП 0..1023
 * count - счетчик длительности сигнала, для отсеивания шумов
 * функция возвращает нажатую кнопку (дискретный сигнал)
 * *****************************************************/
Button_t button_handler(uint16_t analog_signal, uint8_t *count)
{
  // Переменная для хранения максимального уровня напряжения
  static uint16_t max_sig = 0;
  // Сброс выходного значения дискретного сигнала
  Button_t out = ZERO;
 
  if( analog_signal >= 170U )
  {
    // Счет количества тактов, когда уровень сигнала превышал пороговое
    if (*count < 255)
    { 
      *count += 1;
    }
    // Определение максимального значения сигнала для диференциации по значениям дискретного сигнала
    max_sig = (analog_signal > max_sig)?analog_signal:max_sig;
  }
  else
  { 
    // После отпускания кнопки определяется дискретный сигнал по уровню напряжения
    if ( *count > 3 )
    {
      if (max_sig >= 510U)
      {
        if (max_sig >= 850U)
        {
          out = SET;
        }
        else
        {
          out = UP;
        }
      }
      else
      {
       out = DOWN;
      }

      // Дискретный сигнал MODT определяется как длительное нажатие на кнопку SET
      if ( (*count > 30) && (out == SET))
      {
        out = MODE;
      }
    }

    // Сброс переменных
    max_sig = 0;
    *count = 0;
  }
  
  return out;
}
