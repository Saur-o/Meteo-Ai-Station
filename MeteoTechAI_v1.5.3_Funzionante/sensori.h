#ifndef SENSORI_H
#define SENSORI_H

// Inizializza tutti i sensori (da chiamare nel setup)
void inizializzaSensori();
void debug_sensori();

// Funzioni di lettura (ritornano double per l'IA)
double leggiPressione();
double leggiTemperatura();
double leggiUmidita();
double leggiLuce();
double leggiVento();
double leggiCO();
double leggiQualitaAria();


#endif