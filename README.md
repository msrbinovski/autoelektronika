# Praćanje stanja akumulatora u automobilu

## Uvod
 Potrebno je napisati softver za simulaciju pracenja stanja akumulatora u automobilu. Trenutni napon na akumulatoru se simulira pomoću UniCom, simulatora “serijske” komunikacije.
 Posmatrati  vrednosti koje se dobijaju iz UniCom softvera  kao zilaz iz AD konvertora, koji ima opseg od od do 1023. Uzimati zadnjih 20 ocitavanja i usrednjavati ih. Poslati preko PC karaktere ADmin a onda minimalni napon AD konvertora (za koju AD konvertor daje 0), ili karaktere ADmax a onda maksimalni napon AD konvertora (za koju AD konvertor daje 1023).
 ### Periferije
 -Led bar cine dva stupca, prvi su tasteri a drugi diode. Pozivanje se vrsi pozivanjem naredbe LED_bars_plus.exe rR 
 
 -Sedmosegmentni displej sadrzi 9 cifara a poziva se naredbom Seg7_Mux.exe 9
 
 -Za serijsku komunikaciju neophodna su dva kanala cije se otvaranje vrsi pozivanjem sledecih naredbi: AdvUniCom.exe 0 i AdvUniCom.exe 1
 
 ### Nacin testiranja
 Sistem poseduje dva rezima - kontinualno i kontrolisano. Ako je poslato kontinualno, podrazumeva se da se akumulator puni kada je ukljucen automobil.Slanjem komande kontinu+ na kanal1 pale se druga dioda (koja govori da je punjac ukljuceni) i treca dioda (govori o rezimu rada). Ako je poslato kontrol+, rezim je kontrolisan i podrazumeva se da se akumualator puni po potrebi. U tom slucaju, ako se posalje vrednost manja od 12.5V ukljucuju se druga i cetvrta dioda ( strujno punjenje), ako se posalje vrednost veca od 13.5V ukljucuju se druga i treca dioda(naponsko punjenje), a ako se posalje vrednost veca od 14V iskljucuju se diode koje govore o nacinu punjenja. 
 Slanje vrednosti napona se vrsi pomocu auto opcije na kanalu 0 i slanjem triger signala ‘T’ svakih 100ms cime se dobijaju vrednosti kao izlaz AD konvertora. Slanjem komande na kanalu 1 npr admin00+ dobija se minimalna vrednost ad konvertora, dok se slanjem npr admin20+ dobija maksimalna vrednost ad konvertora. Sedmosegmentni displej prikazuje 9 cifara od kojih su prve tri za minimalnu vrednost napona, sledece tri za maksimalnu vrednsot napona i posledenje tri za trenutnu vrednost napona.
 
 ## Kratak opis taskova
 
 ### SerialReceive0_Task(void* pvParameters)
 Ovaj task ima za zadatak da obradi podatke koji stižu sa kanala 0 serijske komunikacije. On "čeka" semafor koji će da pošalje interrupt svaki put kada neki karakter stigne na kanal 0. Podatak se karakter po karakter smesta u niz koji se preko reda salje dalje. 
 ### SerialReceive1_Task(void* pvParameters);
 Ovaj task ima za zadatak da obradi podatke koji stižu sa kanala 1 serijske komunikacije. On "čeka" semafor koji će da pošalje interrupt svaki put kada neki karakter stigne na kanal 1. Podatak se karakter po karakter smesta u niz koji se preko reda salje dalje. 
 ### Obrada_podataka(void* pvParameters);
 U ovom tasku se vrsi obrada podataka sa senzora. Vrednosti dobijene iz reda se smestaju u niz i vrsi se provera da li je poslato admin,admax,kontinu ili kontrol. Takodje, vrsi se obrada podatka pristiglog sa led bara kao i obrada vrednosti napona koja je pristigla sa kanala 0. U zavisnosti od toga pale se odredjene led diode.
 ### Display_Task(void* pvParameters);
 Ovaj task sluzi za ispis na seg7 displeju. U zavisnosti od toga da li je pritisnut taster_min ili taster_max, vrsi se prikazivanje maksimalne i minimalne vrednosti na displeju u formatu: razmak, minimalna vrednost (dve cifre), razmak, maksimalna vrednost (dve cifre), razmak, trenutna vrednost (dve cifre)
 ### led_bar_tsk(void* pvParameters);
 Ovaj task u zavisnosti od toga da li je pritisnut neki taster, preko reda salje string koji je u formatu LXXX (gde je X cifra 0 ili 1) .
 ### SerialSend_Task(void* pvParameters);
 U zavisnosti od rezima rada, salje "kontinualno" ili kontrolisano" na serijsku komunikaciju kanala 1. Nakon toga ispisuje i trenutnu vrednost napona.
 ## MISRA
 Required MISRA pravila koja nisu ispostovana: 11.2 i 10.3
 Advisory MISRA pravila koja nisu ispostovana: 2.4, 2.7, 4.6, 4.8, 8.9, 8.13 i 12.1
 Sva pravila koja nisu ispostovana su zakomentarisana
