# ProgettoReti-Green Pass 2022

![LogoGP](https://user-images.githubusercontent.com/56475652/149366490-3bea887e-148b-48a1-ae31-0d6a3d706626.png)



Green Pass
Progettare ed implementare un servizio di gestione dei green pass secondo le seguenti specifiche. Un utente, una volta effettuata la vaccinazione, tramite un client si collega ad un centro vaccinale e comunica il codice della propria tessera sanitaria. Il centro vaccinale comunica al ServerV il codice ricevuto dal client ed il periodo di validità  del green pass. Un ClientS, per verificare se un green pass è valido, invia il codice di una tessera sanitaria al ServerG il quale richiede al ServerV il controllo della validità. Un ClientT, inoltre, può invalidare o ripristinare la validità di un green pass comunicando al ServerG il contagio o la guarigione di una persona attraverso il codice della tessera sanitaria.

![Architettura Progetto](https://user-images.githubusercontent.com/56475652/147292141-d7951570-1c3c-45f3-8681-28c396eae4ef.png)


Si utilizzi il linguaggio C su piattaforma UNIX utilizzando i socket per la comunicazione tra processi. Corredare l’implementazione di adeguata documentazione.


 
### Presentazione
[Documentazione](https://github.com/dom0000D/ProgettoReti-GreenPass/tree/main/Documentazione)
