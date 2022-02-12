# ProgettoReti-Green Pass 2022

<p align="center">
  <img src="https://user-images.githubusercontent.com/56475652/153706779-9b0d2561-d935-400b-8e0c-efc88c33e8ef.png"/>
</p>


### Green Pass
Progettare ed implementare un servizio di gestione dei green pass secondo le seguenti specifiche. Un utente, una volta effettuata la vaccinazione, tramite un client si collega ad un centro vaccinale e comunica il codice della propria tessera sanitaria. Il centro vaccinale comunica al ServerV il codice ricevuto dal client ed il periodo di validità  del green pass. Un ClientS, per verificare se un green pass è valido, invia il codice di una tessera sanitaria al ServerG il quale richiede al ServerV il controllo della validità. Un ClientT, inoltre, può invalidare o ripristinare la validità di un green pass comunicando al ServerG il contagio o la guarigione di una persona attraverso il codice della tessera sanitaria.

<p align="center">
  <img src="https://user-images.githubusercontent.com/56475652/147292141-d7951570-1c3c-45f3-8681-28c396eae4ef.png"/>
</p>

Si utilizzi il linguaggio C su piattaforma UNIX utilizzando i socket per la comunicazione tra processi. Corredare l’implementazione di adeguata documentazione.

### DESCRIZIONE DETTAGLIATA
Il progetto proposto rappresenta tutto il mini-mondo per la gestione dei certificati vaccinali, cioè il Green Pass.
Un utente, dopo aver effettuato la vaccinazione, comunica i propri dati anagrafici e il numero di tessera sanitaria a un Centro Vaccinale, il quale innanzitutto comunicherà l’eventuale ricezione dei dati al cliente e invierà poi il codice della tessera sanitaria, con il periodo di validità del Green Pass a un ServerVaccinale che svolge un ruolo di database dove salva in un filesystem tutti i certificati verdi.
Abbiamo poi un ClientS che può essere visto come l’app che scansiona i Green Pass, ad esempio Verifica C19, che invia un codice di una tessera sanitaria al ServerG che a sua volta chiede al ServerV di inviargli un Green Pass: così facendo il ServerVerifica effettuerà l’operazione di scansione della validità per poi comunicarlo al ClientS.
Infine abbiamo un ClientT, identificabile come un’organizzazione sanitaria, come l’ASL, che può invalidare o ripristinare la validità di un Green Pass comunicando il contagio o la guarigione di una persona al ServerG mediante il codice della tessera sanitaria.
Per maggiori informazioni su progettazione e implementazione visionare la [documentazione](https://github.com/dom0000D/ProgettoReti-GreenPass/tree/main/Documentazione)
 
### Presentazione
- [Documentazione](https://github.com/dom0000D/ProgettoReti-GreenPass/tree/main/Documentazione)
- [Progettazione](https://github.com/dom0000D/ProgettoReti-GreenPass/tree/main/Progettazione)  
- [Codice](https://github.com/dom0000D/ProgettoReti-GreenPass/tree/main/Codice) 
