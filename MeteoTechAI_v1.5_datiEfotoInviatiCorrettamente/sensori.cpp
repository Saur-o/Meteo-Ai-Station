#include "sensori.h"
#include <Adafruit_BMP280.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_ADS1X15.h>

// Istanze degli oggetti (statiche per limitare l'ambito a questo file)
static Adafruit_BMP280 bmp;
static Adafruit_AHTX0 aht;
static Adafruit_ADS1115 ads;

static double R0_MQ7 = 1.55;
static double R0_MQ135 = 25.40;
const float RL = 10.0;
const float fattorePartitore = 1.515;

void inizializzaSensori() {
    // ads1115
  if (!ads.begin(0x48)) Serial.println("ADS1115 non trovato!");
    ads.setGain(GAIN_ONE);
    
    // Inizializza AHT20
  if (!aht.begin()) {
    Serial.println("AHT20 non trovato!");
  }

  // Inizializza BMP280
  // Nota: usiamo .begin(0x76) o .begin(0x77)
  if (!bmp.begin(0x76)) {
    Serial.println("BMP280 non trovato a 0x76, provo 0x77...");
    if (!bmp.begin(0x77)) {
      Serial.println("BMP280 non trovato! Controlla cablaggio.");
    }
  }

  //opzioni aggiuntive per migliorare la precisione del sensore BMP
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     // Operating Mode. 
                  Adafruit_BMP280::SAMPLING_X2,     // Temp. oversampling 
                  Adafruit_BMP280::SAMPLING_X16,    // Pressure oversampling 
                  Adafruit_BMP280::FILTER_X16);
}

/*
void leggiSensoriGas(){
    MQ7.setRegressionMethod(1); //_PPM =  a*ratio^b
  MQ7.setA(99.042); MQ7.setB(-1.518); // Configure the equation to calculate CO concentration value
  MQ7.init();
  
  MQ135.setRegressionMethod(1); //_PPM =  a*ratio^b
  MQ135.init();
  
 /*****************************  MQ-7 CAlibration ********************************************/
 /*
  Serial.print("Calibrating MQ7 sensor.");
  float calcR0_MQ7 = 0;
  for(int i = 1; i<=10; i ++)
  {
    MQ7.update(); // Update data, the arduino will read the voltage from the analog pin
    calcR0_MQ7 += MQ7.calibrate(RatioMQ7CleanAir);
    Serial.print(".");
  }
  MQ7.setR0(calcR0_MQ7/10);
  Serial.println("  done!.");
  
  if(isinf(calcR0_MQ7)) {Serial.println("Warning: Conection issue, R0 is infinite (Open circuit detected) please check your wiring and supply"); while(1);}
  if(calcR0_MQ7 == 0){Serial.println("Warning: Conection issue found, R0 is zero (Analog pin shorts to ground) please check your wiring and supply"); while(1);}

  MQ7.serialDebug(true);
/*****************************  MQ-135 CAlibration ********************************************/
/*
  Serial.print("Calibrating please wait.");
  float calcR0_MQ135 = 0;
  for(int i = 1; i<=10; i ++)
  {
    MQ135.update(); // Update data, the arduino will read the voltage from the analog pin
    calcR0_MQ135 += MQ135.calibrate(RatioMQ135CleanAir);
    Serial.print(".");
  }
  MQ135.setR0(calcR0_MQ135/10);
  Serial.println("  done!.");
  
  if(isinf(calcR0_MQ135)) {Serial.println("Warning: Conection issue, R0 is infinite (Open circuit detected) please check your wiring and supply"); while(1);}
  if(calcR0_MQ135 == 0){Serial.println("Warning: Conection issue found, R0 is zero (Analog pin shorts to ground) please check your wiring and supply"); while(1);}
  /*****************************  Calibrazione completata ********************************************/
  /*
  Serial.println("Calibrazione sensori completata");
}
*/

//inizializza i sensori bmp e aht (ricorda di iniziallizzare il protocollo I2c nel file -ino Wire.begin())
/*
void inizializzaSensoriBMO280_AHT20 (
  //definizione senosori per temperatura, umidità e pressione
  Adafruit_AHTX0 aht;
  Adafruit_BMP280 bmp; // Sensore solo Pressione + Temperatura

  // Inizializza AHT20
  if (!aht.begin()) {
    Serial.println("AHT20 non trovato!");
  }

  // Inizializza BMP280
  // Nota: usiamo .begin(0x76) o .begin(0x77)
  if (!bmp.begin(0x76)) {
    Serial.println("BMP280 non trovato a 0x76, provo 0x77...");
    if (!bmp.begin(0x77)) {
      Serial.println("BMP280 non trovato! Controlla cablaggio.");
    }
  }

  //opzioni aggiuntive per migliorare la precisione del sensore BMP
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     // Operating Mode. 
                  Adafruit_BMP280::SAMPLING_X2,     // Temp. oversampling 
                  Adafruit_BMP280::SAMPLING_X16,    // Pressure oversampling 
                  Adafruit_BMP280::FILTER_X16);  

)
*/

//Funzione da sistemare, dividi la lettura del bmo dal aht
void debug_sensori() {
  
  Serial.println("--- DATI AMBIENTALI ---");
  Serial.print("Temperatura (AHT20): "); Serial.print(leggiTemperatura()); Serial.println(" °C");
  Serial.print("Umidità (AHT20): "); Serial.print(leggiUmidita()); Serial.println(" %");
  Serial.print("Pressione (BMP280): "); Serial.print(leggiPressione()); Serial.println(" hPa");
  Serial.print("Luce "); Serial.print(leggiLuce()); Serial.println(" %");
  Serial.print("Vento "); Serial.print(leggiVento()); Serial.println(" km/h");
  Serial.print("Monossido di carbonio (CO) "); Serial.print(leggiCO()); Serial.println(" ppm");
  Serial.print("Qualità Aria "); Serial.print(leggiQualitaAria()); Serial.println(" ppm");
  Serial.println("-----------------------");
}

//------------------------------------------------------------------------------------------------/
double leggiPressione(){
  double pressione = (double)bmp.readPressure() / 100.0F;
  return pressione;
}

double leggiTemperatura() {
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    return (double)temp.temperature;
}

double leggiUmidita() {
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp); // Legge comunque entrambi
    return (double)humidity.relative_humidity; // Ma restituisce solo l'umidità
}

double leggiLuce() {
    // 1. Leggiamo la tensione grezza dal canale A2 dell'ADS1115
    float volt = ads.computeVolts(ads.readADC_SingleEnded(2));

    // 2. Mappatura della tensione in percentuale (0V = 0%, 3.3V = 100%)
    double percentuale = (double)(volt / 3.3) * 100.0;

    // 3. Vincoli di sicurezza (Clamping)
    if (percentuale < 0.0) percentuale = 0.0;
    if (percentuale > 100.0) percentuale = 100.0;

    return percentuale;
}

double leggiVento() {
    // 1. Lettura della tensione grezza dal canale A3
    // Usiamo float per il calcolo intermedio, poi convertiamo in double
    float voltVento = ads.computeVolts(ads.readADC_SingleEnded(3));

    // 2. Definizione del risultato
    double velocitaKmh = 0.0;

    // 3. Filtro per il rumore di fondo (Deadband)
    // Sotto i 0.01V consideriamo l'anemometro fermo per evitare che 
    // l'IA legga "micro-venti" inesistenti dovuti a disturbi elettrici.
    if (voltVento > 0.01) {
        // Applichiamo la formula derivata dai tuoi test: 1.2V = 80km/h
        // Il rapporto è 80 / 1.2 = 66.666...
        velocitaKmh = (double)voltVento * 66.666667;
    }

    return velocitaKmh;
}

double leggiCO() {
    // 1. Lettura tensione e correzione per il partitore esterno
    float vRaw = ads.computeVolts(ads.readADC_SingleEnded(0));
    float vReal = vRaw * fattorePartitore; // Riportiamo a scala 5V

    // 2. Sicurezza: evitiamo divisioni per zero
    if (vReal < 0.1) vReal = 0.1;

    // 3. Calcolo della Resistenza attuale del sensore (RS)
    // Formula: RS = ((Vcc - Vout) * RL) / Vout
    double RS = ((5.0 - (double)vReal) * RL) / (double)vReal;

    // 4. Calcolo del Rapporto (Ratio) rispetto alla calibrazione
    double ratio = RS / R0_MQ7;

    // 5. Trasformazione in PPM usando la curva del datasheet (CO)
    // Formula: ppm = a * ratio^b -> Per MQ7: a=99.042, b=-1.518
    double ppm = 99.042 * pow(ratio, -1.518);

    return ppm;
}

double leggiQualitaAria() {
    // 1. Lettura tensione e correzione partitore
    float vRaw = ads.computeVolts(ads.readADC_SingleEnded(1));
    float vReal = vRaw * fattorePartitore;

    // 2. Sicurezza
    if (vReal < 0.1) vReal = 0.1;

    // 3. Calcolo RS
    double RS = ((5.0 - (double)vReal) * RL) / (double)vReal;

    // 4. Calcolo Ratio
    double ratio = RS / R0_MQ135;

    // 5. Trasformazione in PPM (Curva per inquinanti generici)
    // Formula: ppm = a * ratio^b -> Per MQ135: a=110.47, b=-2.862
    double ppm = 110.47 * pow(ratio, -2.862);

    return ppm;
}