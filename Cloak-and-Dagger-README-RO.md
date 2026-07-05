🇬🇧 [English](README.md) · 🇷🇴 **Română** (acest document)

> *Traducere pentru comunitate. Termenii tehnici, numele protocoalelor (CLKnDGR, Cloak, Dagger), tickerele (QU, QX, Qswap, Qearn), comenzile și identificatorii propunerilor sunt păstrați în engleză, pentru a nu introduce erori. Corecturile din partea vorbitorilor nativi sunt binevenite.*

---

CLKnDGR-Readme
Explicație a contractului la care lucrez. - Misfitcompany K.I.R | D.O.M

### DECLINARE: Claude Coded

# Cloak and Dagger (CLKnDGR)

**Un protocol propus de lichiditate prin arbitraj pentru Qubic — un design în dezvoltare, publicat pentru feedback din partea comunității.**

> **Notă:** Acest document descrie un design în dezvoltare. Contractul CLKnDGR nu este încă implementat — dacă va ajunge sau nu pe rețeaua Qubic depinde de Guvernanța Computorilor, care ar trebui să îl aprobe. Protocoalele pe care se bazează — Qearn, QX și Qswap — sunt deja active on-chain; doar CLKnDGR este încă în construcție. Guvernanța acționarilor și a deponenților descrisă mai jos este guvernanța internă a contractului, care s-ar aplica odată ce acesta rulează. Tot ce urmează descrie modul în care contractul este menit să funcționeze.

---

## Ce este Cloak and Dagger?

Cloak and Dagger este un smart contract propus pentru rețeaua Qubic, conceput să ruleze arbitraj automatizat între QU și piețele tokenurilor native Qubic. Ar aduna capital de la două grupuri — acționari și deponenți în vault — și ar folosi acel capital pentru a găsi și a capta diferențele de preț dintre pool-uri, returnând profitul participanților la finalul fiecărei epoci (epoch).

Numele contractului este `CLKnDGR`. Acțiunile ar fi emise prin mecanismul IPO al Qubic, dacă se ajunge la unul.

---

## Două categorii de participanți

### Acționari (Shareholders)

Acționarii dețin tokenuri de guvernanță din IPO. Ei propun și votează modificări ale parametrilor contractului printr-un sistem de propuneri on-chain. Este necesară o supermajoritate pentru ca orice propunere să treacă, iar deponenții dețin un drept de veto care poate bloca deciziile acționarilor.

### Deponenți în vault (Vault Depositors)

Oricine poate depune QU în vault ca și capital suplimentar de tranzacționare. Deponenții primesc acțiuni în vault, evaluate în raport cu un NAV (net asset value — valoarea activului net) care crește sau scade odată cu performanța de tranzacționare. Depozitele sunt supuse unei **perioade de blocare de 26 de epoci**. Ieșirile timpurii sunt permise, dar implică o penalizare semnificativă — concepută pentru a-i proteja pe deponenții rămași de jocurile de sincronizare (timing).

Aceste două roluri sunt separate și nu se exclud reciproc. Un singur portofel poate deține atât acțiuni de guvernanță, cât și o poziție în vault.

---

## Cum funcționează vault-ul

- **Depunere:** Trimiți QU în vault. Acțiunile sunt emise la prețul curent al acțiunii (NAV per acțiune). Dacă vault-ul este la capacitate maximă (5.000 de deponenți), ești plasat pe o listă de așteptare (până la 500 de intrări, procesate în ordinea celor mai mari, pe măsură ce se eliberează locuri).
- **Prețul acțiunii:** Calculat din capitalul total al vault-ului împărțit la numărul total de acțiuni în circulație. Crește cu epocile profitabile; scade cu cele neprofitabile.
- **Perioada de blocare:** 26 de epoci. Poți ieși mai devreme cu o penalizare, sau te poți rebloca în ultimele 4 epoci — reblocarea îți resetează poziția la o nouă blocare de 26 de epoci din epoca curentă și necesită adăugarea unei sume minime de QU (minim inițial: 10M QU, guvernabil).
- **Retragere:** La expirarea blocării se aplică un comision de administrare de 2%. Ieșirile profitabile implică și un comision de performanță de 5%. Ieșirile timpurii plătesc pe deasupra o penalizare de 38%.
- **Veto-ul deponenților:** Deponenții cu suficient QU blocat pot pune veto pe propunerile acționarilor — un mecanism de echilibru și control integrat în sistemul de guvernanță.

---

## Cum funcționează arbitrajul

Contractul scanează pool-urile de active înregistrate de aproximativ **4 ori pe secundă** (o dată la fiecare tick de rețea) în căutarea diferențelor de preț profitabile între valoarea în QU a activului pe o parte și prețul de piață pe cealaltă. Când o diferență depășește un prag minim configurabil de profit **net** (după taxele de tranzacționare), contractul execută o tranzacție folosind capitalul disponibil.

Pool-urile sunt înregistrate on-chain prin sistemul de guvernanță (vot al acționarilor). Pool-urile active participă la arbitraj; pool-urile dezactivate sunt sărite, dar înregistrările lor sunt păstrate.

---

## Strategii de tranzacționare

Contractul rulează două strategii distincte, denumite, la fiecare tick, în ordine. Ele împart același registru de pool-uri și aceeași bază de capital, dar operează independent și servesc condiții de piață diferite.

### The Cloak — Swing Trading

Cloak este o strategie de swing trading pe termen mediu. Urmărește scăderi semnificative de preț pe Qswap folosind două medii mobile, **acumulează treptat (dollar-cost averaging / DCA)** într-o poziție pe măsură ce scăderea se adâncește, apoi **iese treptat** pe măsură ce prețul își revine.

**Condiția de intrare:** Media curentă a prețului pe 1 săptămână pentru un pool a scăzut la **≤ 90% din media prețului pe 3 luni** — o scădere semnificativă statistic față de istoricul recent.

**La intrare și acumulare (DCA-in):** Prima cumpărare pe o scădere nouă folosește **1% din capitalul de tranzacționare**. Dacă scăderea persistă la verificările lunare ulterioare, mai **adaugă câte 0,25%** de fiecare dată — făcând medie în jos — **până când baza de cost a poziției atinge 5% din capital**. Acel plafon se redeschide automat pe măsură ce fondul crește sau pe măsură ce poziția este vândută. Toate cumpărăturile folosesc o toleranță la slippage de până la 5%. *(Pachetul de 1% / 0,25% / 5% este **presetul implicit de dimensionare a poziției**, guvernabil prin Type 26. Scăderea de preț care definește „o scădere” este implicit **30% sub media pe 3 luni** (Type 27), iar nivelul de profit la care începe vânzarea este implicit **6% peste cost** (Type 28) — toate trei sunt guvernabile.)*

**Condiția de ieșire:** Prețul curent al pool-ului Qswap este **≥ 112% din costul mediu plătit** pentru poziția deținută.

**La ieșire:** o porțiune guvernabilă — **50% din poziția deținută, implicit** (reglabilă între 10–50% prin UPDATE_SWING_SELL_PCT) — este oferită pe QX ca ordin de vânzare (ask), la un preț cu 10% sub prețul curent al pool-ului Qswap, pentru a asigura execuția. Poziția este păstrată până la vânzarea completă, de-a lungul mai multor evenimente de ieșire.

**Stop-loss (ieșire pe pierdere):** contrapartea ieșirii pe câștig, astfel încât o poziție care continuă să scadă să nu fie ținută la nesfârșit. Dacă o poziție deținută scade la **costul său mediu minus o adâncime guvernabilă — 45% implicit** (UPDATE_STOP_LOSS_TRIGGER; `0` îl dezactivează), contractul taie o porțiune guvernabilă din ea — **60% implicit** (UPDATE_STOP_LOSS_SELL) — **pe Qswap** la verificarea lunară. Iese treptat, fără a arunca niciodată toată poziția dintr-odată: un activ muribund sângerează în jos în câteva tăieri, în timp ce unul care își revine păstrează restul. Ambele reglaje se mișcă în pași de 15 puncte. QU-ul recuperat este împărțit fix **90% înapoi în capitalul de tranzacționare / 10% ars în rezerva pentru taxe de execuție** — menținând motorul finanțat exact atunci când pierderile i-au secat alimentarea normală din profit. Fiind o pierdere realizată, **nimic** nu merge către acționari, Qearn sau CCF. (O protecție de siguranță sare peste vânzare dacă poziția este atât de lipsită de valoare încât încasările nu ar acoperi nici măcar taxa Qswap de 100K QU.)

**Cadența verificărilor:** Cloak evaluează fiecare pool aproximativ **o dată pe lună** — pentru o primă cumpărare, o adăugire DCA-in sau o vânzare. Interoghează prețul de piață în timp real doar pentru pool-urile pe care le **deține efectiv** (necesar pentru decizia de vânzare); pool-urile inactive pe care doar le supraveghează pentru o scădere sunt evaluate din istoricul de prețuri săptămânal stocat, fără costul unui apel de piață. Astfel, strategia pe termen lung rămâne răbdătoare și ieftin de rulat în perioadele liniștite.

**Verificarea lichidității (doar la ieșire):** Înainte de a plasa ordinul de vânzare, contractul verifică dacă cel puțin 80% din încasările țintă există sub formă de oferte de cumpărare (bid) calificate pe QX. Dacă registrul de ordine este prea subțire, ieșirea este amânată până la următorul tick eligibil.

Fiecare pool rulează propria poziție Cloak independentă. Contractul deține o singură poziție de swing per pool, în care **acumulează treptat (DCA)** pe scăderi continue (până la plafonul de 5% din capital) și din care **iese treptat** pe măsură ce prețul își revine.

---

### The Dagger — Arbitraj Spot (bidirecțional)

Dagger este o strategie de arbitraj cross-exchange în timp real. Identifică și captează diferențele de preț dintre QX (registru de ordine) și Qswap (AMM — piață automată) într-un singur tick. Funcționează în **ambele direcții** — captând spread-ul indiferent în ce parte se înclină diferența de preț.

**Scanare filtrată de volatilitate (VIX).** Verificarea burselor costă taxe de execuție, așa că Dagger nu interoghează constant fiecare pool. În schimb, menține un **index de volatilitate** ieftin per token, eșantionat din pool-ul Qswap o dată pe zi implicit (guvernabil la de 2× sau 3× prin UPDATE_VIX_PULSE_RATE), care urmărește cât de mult se mișcă fiecare token — o citire rapidă (≈5 zile) față de o bază lentă (≈4 săptămâni). Când volatilitatea recentă a unui token depășește propria bază (breakout), Dagger se trezește și îl vânează la viteză maximă (atunci apar de fapt diferențele cross-exchange); când un token este liniștit, revine la o verificare rară, de siguranță. Astfel bugetul de taxe se concentrează pe tokenurile care se mișcă efectiv. Sensibilitatea la breakout (UPDATE_VIX_FACTOR) și pragul minim de mișcare (UPDATE_VIX_FLOOR) sunt ambele guvernabile.

**Direcția B — cumpără QX, vinde Qswap:** când cea mai bună ofertă de vânzare (*ask*) de pe QX este mai ieftină decât ce ar plăti Qswap pentru aceleași tokenuri.
- **Etapa 1:** Cumpără tokenuri pe QX la cel mai bun preț de ask disponibil.
- **Etapa 2:** Vinde imediat acele tokenuri pe Qswap, captând spread-ul.

**Direcția A — cumpără Qswap, vinde QX:** imaginea în oglindă, pentru când Qswap este mai ieftin decât cea mai bună ofertă de cumpărare (*bid*) de pe QX (un cumpărător care așteaptă pe QX oferă mai mult decât cere Qswap).
- **Etapa 1:** Cumpără tokenuri pe Qswap.
- **Etapa 2:** Vinde acele tokenuri către bid-ul de pe QX, captând spread-ul. (Vânzarea pe QX se potrivește imediat; QU-ul pe care îl plătește ajunge la tick-ul următor și intră în următorul ciclu de profit — deci profitul acestei părți este realizat un tick mai târziu, la fel ca vânzările Cloak.)

Pentru orice pool dat, la orice moment, doar o singură direcție poate fi profitabilă — un cumpărător nu poate plăti mai mult decât cere un vânzător pe o piață sănătoasă — așa că cele două nu concurează niciodată. Fiecare este verificată independent și cel mult una se declanșează per pool per tick.

**Prag minim de profit:** O tranzacție se execută doar dacă profitul net estimat depășește pragul `minProfitQu` (guvernabil), iar fiecare direcție re-verifică profitabilitatea față de execuția *reală* înainte de a angaja capital — nu vinde niciodată la o marjă subțire sau negativă. Tranzacțiile sub prag sunt sărite.

**Scalarea capitalului:** Când o diferență mare de preț ar produce profit mult peste prag, dimensiunea tranzacției este redusă proporțional. Astfel se evită ca un singur pool să consume cea mai mare parte a capitalului de tranzacționare într-un singur tick, păstrând competitive pool-urile verificate ulterior.

**Acumularea de rezervă:** O parte din fiecare tranzacție Dagger — pe oricare direcție — este reținută ca rezervă de tokenuri per pool, în loc să fie vândută imediat. Aceste rezerve se acumulează în timp și sunt lichidate în cele din urmă când pot fi vândute profitabil peste un prag minim de randament guvernabil.

**Cooldown per pool:** Fiecare pool are temporizatoare de cooldown independente — câte unul pe direcție, deoarece o oportunitate lipsă într-o direcție nu spune nimic despre cealaltă. Pool-urile care sunt inaccesibile financiar, nu au lichiditate sau nu există încă pe Qswap sunt puse pe un cooldown mai lung, pentru a nu irosi tick-uri reverificându-le.

---

### Sistemul de recuperare

Ambele strategii transferă drepturile de administrare a acțiunilor între contract, QX și Qswap ca parte a execuției lor. Dacă un transfer de drepturi eșuează în mijlocul unei tranzacții (un caz-limită la nivel de rețea), contractul nu pierde evidența tokenurilor. Un pas dedicat de recuperare rulează la începutul fiecărui tick, reîncercând în tăcere transferurile eșuate până reușesc. Tokenurile recuperate sunt fie returnate la poziția lor originală, fie integrate în rezerva pool-ului, în funcție de unde în tranzacție au rămas blocate.

---

## Distribuția profitului

La finalul fiecărei epoci, profitul din tranzacționare este împărțit între mai multe destinații. Împărțirea exactă este un parametru guvernabil — acționarii pot vota schimbarea presetului activ. Preseturile echilibrează reinvestirea (creșterea pool-ului de tranzacționare și finanțarea execuției), contribuția la ecosistem (susținerea Qearn și a Computor Controlled Fund) și recompensa acționarilor.

**O notă despre Qearn:** în loc să își blocheze alocarea Qearn pentru a câștiga randament pentru sine, Cloak and Dagger **donează** acea porțiune către bonus pool-ul Qearn — crescând recompensele de blocare câștigate de întreaga comunitate Qubic. Contractul nu reține nimic: susține Qearn în loc să profite de pe urma lui.

| Destinație | Rol |
|---|---|
| **Pool de tranzacționare** | Reinvestit ca și capital al vault-ului — beneficiază deponenții prin creșterea NAV |
| **Taxe de execuție** | Finanțează costurile de execuție on-chain |
| **Boost Qearn** | Donat către bonus pool-ul Qearn — crește recompensele de blocare pentru întreaga comunitate Qubic |
| **Acționari** | Dividend direct pe epocă pentru deținătorii de tokenuri de guvernanță |
| **Fond de dezvoltare (Dev fund)** | Rezervă pentru dezvoltarea protocolului (retragere controlată de acționari) |
| **CCF** | Contribuție la Qubic Computor Controlled Fund |

### Preseturile de distribuție

| Preset | Pool tranzacționare | Taxe execuție | Boost Qearn | Acționari | Dev fund | CCF |
|---|---|---|---|---|---|---|
| **0 — Implicit** | 55% | 30% | 3% | 10% | 1% | 1% |
| **1 — Creștere** | 61% | 27% | 3% | 7% | 1% | 1% |
| **2 — Agresiv** | 65% | 25% | 3% | 5% | 1% | 1% |
| **3 — Recuperare** | 0% | 100% | 0% | 0% | 0% | 0% |

Presetul 0 plătește acționarilor cel mai mult (10%) și finanțează execuția moderat. Preseturile 1 și 2 direcționează progresiv mai mult către pool-ul de tranzacționare și finanțarea taxelor de execuție, cu un dividend mai mic pentru acționari (7% → 5%). Boost-ul Qearn (3%), dev fund-ul (1%) și CCF (1%) sunt constante în preseturile 0–2. Acționarii votează schimbarea presetului activ.

**Presetul 3 — Recuperare / mod de avarie (limp mode)** este special: contractul **continuă să tranzacționeze normal**, dar direcționează **100% din profit** către rezerva pentru taxe de execuție, suspendând plățile către acționari, Qearn, dev fund și CCF — astfel își reconstruiește rapid bugetul de taxe on-chain, din propriile câștiguri. Se aplică **automat** ori de câte ori rezerva pentru taxe de execuție scade sub pragul guvernabil (vezi UPDATE_EXEC_RESERVE_FLOOR), iar acționarii îl pot selecta și manual. Contractul revine la presetul ales de la sine **odată ce rezerva urcă înapoi la 10% peste prag** — un tampon de histerezis, ca să nu poată intra și ieși rapid din recuperare la limită. (Presetul ales anterior este păstrat; recuperarea nu îl suprascrie niciodată.)

---

## Guvernanță on-chain

> **Important:** Nu este planificată nicio interfață front-end sau web pentru guvernanța Cloak and Dagger. Conform designului, toate acțiunile de depunere a propunerilor, de vot și de veto ar fi efectuate direct prin instrumentul de linie de comandă `qubic-cli`. O referință ABI completă, cu formatele exacte ale comenzilor pentru fiecare procedură, este furnizată alături de acest document. Acționarii și deponenții ar interacționa direct cu contractul — niciun instrument intermediar nu este necesar sau recomandat.

Propunerile de guvernanță sunt depuse de acționari (necesită deținerea a ≥1 acțiune CLKnDGR) și votate de grupul acționarilor. O propunere trece atunci când:

1. Participă un număr minim de votanți unici (implicit: 15)
2. A votat un număr total minim de acțiuni ponderate
3. Cel puțin 2/3 din voturile ponderate sunt „Da” (supermajoritate)
4. Au fost exprimate mai puțin de 500 de voturi „NU” calificate din partea deponenților (veto-ul deponenților)

Veto-ul deponenților există special pentru ca deponenții mari să nu poată fi prejudiciați de deciziile de guvernanță fără un control semnificativ. Voturile de veto sunt re-validate la finalul epocii față de NAV-ul curent — un deponent care a pierdut valoare semnificativă nu mai este calificat.

### Parametri guvernabili (28 de tipuri de propuneri)

| # | Propunere | Taxă | Ce modifică |
|---|---|---|---|
| 1 | **ADD_POOL** | 200M QU | Înregistrează un nou pool token/QU pentru arbitraj. Numele activului și emitentul sunt necesare. Maximum 255 de pool-uri în total. |
| 2 | **REMOVE_POOL** | 50M QU | Dezactivează un pool activ. Arbitrajul se oprește; înregistrarea pool-ului și orice rezerve deținute sunt păstrate. |
| 3 | **REACTIVATE_POOL** | 50M QU | Reactivează un pool dezactivat anterior. |
| 4 | **UPDATE_MIN_PROFIT** | 50M QU | Stabilește profitul **net** minim în QU per arbitraj (după taxa de swap Qswap) necesar înainte ca o tranzacție Dagger să se execute. Folosit și ca și capital minim al Cloak pentru a deschide o cumpărare. Valori permise: 100.100 / 250.100 / 420.000 / 676.420. |
| 5 | **WITHDRAW_QU_RESERVE** | 50M QU | Transferă QU din rezerva on-chain a dev fund-ului către o destinație specificată. Suma nu poate depăși rezerva curentă. |
| 6 | **UPDATE_PROPOSAL_FEE** | 50M QU | Modifică taxa implicită de depunere pentru toate tipurile de propuneri, cu excepția ADD_POOL și UPDATE_PAYOUT. |
| 7 | **UPDATE_PAYOUT** | 69M QU | Comută presetul activ de distribuție a profitului (0, 1, 2 sau 3 = recuperare — vezi tabelul de mai sus). |
| 8 | **UPDATE_FEE_ADD_POOL** | 50M QU | Modifică taxa de depunere specific pentru propunerile ADD_POOL. |
| 9 | **UPDATE_FEE_PAYOUT** | 50M QU | Modifică taxa de depunere specific pentru propunerile UPDATE_PAYOUT. |
| 10 | **UPDATE_MIN_QUORUM** | 50M QU | Modifică numărul minim de votanți unici calificați necesar pentru ca orice propunere să treacă (interval: 15–676). |
| 11 | **WITHDRAW_ASSET_RESERVE** | 50M QU | Transferă rezervele de tokenuri ale unui pool dezactivat către o adresă de destinație specificată. |
| 12 | **UPDATE_VAULT_TIER** | 50M QU | Modifică dimensiunea minimă a depozitului pentru vault folosind un index de nivel (0–8). La prețul inițial al acțiunii, aceasta variază de la 10.000 QU (nivel 0) la 10.000.000 QU (nivel 8, implicit). |
| 13 | **UPDATE_RESERVE_PROFIT_PCT** | 50M QU | Modifică procentul minim de profit necesar înainte ca contractul să vândă rezervele de tokenuri acumulate. Valori valide: 2%, 5%, 7% sau 10%. |
| 14 | **UPDATE_DEPOSITOR_VOTE_MIN** | 50M QU | Modifică QU-ul minim blocat necesar pentru ca votul de veto al unui deponent să fie calificat. Opțiuni: 50M, 150M (implicit), 250M sau 350M QU. |
| 15 | **UPDATE_RELOCK_AMOUNT** | 50M QU | Modifică QU-ul suplimentar minim pe care un deponent trebuie să îl adauge la reblocarea poziției. Opțiuni: 1M, 5M, 10M (implicit), 20M, 25M sau 50M QU. |
| 16 | **UPDATE_EXEC_RESERVE_FLOOR** | 50M QU | Stabilește pragul de siguranță al rezervei pentru taxe de execuție. Dacă bugetul de taxe on-chain scade sub el, contractul continuă să tranzacționeze, dar direcționează 100% din profit către rezerva de taxe (presetul de recuperare) până se reumple la 10% peste prag. Opțiuni: 0 (oprit, implicit), 1B, 5B, 10B sau 20B QU. |
| 17 | **SELL_POOL_TOKENS** | 50M QU | Vinde pe piață un procent ales (1–100%) din tokenurile deținute de un pool pe Qswap, în schimbul QU. Încasările rămân în contract și sunt integrate în următoarea împărțire a profitului — deci **atât deponenții din vault, cât și acționarii** beneficiază (spre deosebire de WITHDRAW_ASSET_RESERVE, care trimite tokenuri brute către o adresă externă). Butonul de „take profit” / „ieșire dintr-o poziție” al guvernanței. Funcționează pe pool-uri active (extragere de profit în timp ce tranzacționarea continuă) sau inactive (lichidarea unei poziții suspendate). Un prag de slippage de 10% anulează o execuție catastrofală. |
| 18 | **UPDATE_VIX_FACTOR** | 50M QU | Reglează sensibilitatea Dagger la breakout-ul de volatilitate — cât de mult trebuie să crească volatilitatea recentă (≈5 zile) a unui token peste propria bază (≈4 săptămâni) înainte ca Dagger să înceapă să îl vâneze. Stocat ×100. Opțiuni (multiplicator al propriei baze): 0,09; 0,18; 0,37; 0,75; 1,5; 2 (implicit); 2,25; 2,75; 3,5; 4,5; 5 — trimise ca 9 / 18 / 37 / 75 / 150 / 200 / 225 / 275 / 350 / 450 / 500. Sub 1× Dagger vânează aproape mereu (pragul devine filtrul); mai mare = mai selectiv. |
| 19 | **UPDATE_VIX_FLOOR** | 50M QU | Volatilitatea recentă absolută minimă (în basis points) pentru ca un token să conteze drept „în breakout” — împiedică un token aproape mort să se declanșeze la o mișcare minusculă. Opțiuni: 0 (doar raport), 10, 25 (implicit), 50, 100, 200 bps. |
| 20 | **UPDATE_VIX_PULSE_RATE** | 50M QU | De câte ori pe zi eșantionează Dagger prețul fiecărui pool pentru a-și actualiza citirea de volatilitate. Opțiuni: **1 (implicit)**, 2 sau 3 pe zi. Mai multe = detecție mai precisă/rapidă, dar cost de taxe mai mare; mai puține = mai ieftin, dar mai lent la observarea unui token care se încălzește. Orizonturile de volatilitate de 5 zile / 4 săptămâni rămân fixe indiferent — doar frecvența de eșantionare se schimbă. |
| 21 | **UPDATE_SWING_SELL_PCT** | 50M QU | Porțiunea de vânzare a Cloak — ce % dintr-o poziție deținută vinde de fiecare dată când se declanșează triggerul de vânzare pe raliu (Type 28). Opțiuni: 10, 15, 20, 25, 33, **50 (implicit)**. Mai mare = ia profit mai repede; mai mic = ține câștigătorii mai mult. |
| 22 | **UPDATE_BREAKOUT_RESCAN** | 50M QU | Cât de des reverifică Dagger un pool „fierbinte” (în breakout) în timp ce caută o diferență (tot tranzacționează la fiecare tick odată ce o diferență reală este găsită — asta doar temporizează verificările fără rezultat). Trimis în secunde: 30, 60, 120, 180, 240, **300 (implicit = 5 min)**. Mai scurt = prinde diferențe rapide, dar costă mai mult; mai lung = mai ieftin. |
| 23 | **UPDATE_QX_FEE_MODE** | 50M QU | Cum obține contractul taxa de transfer de acțiuni a QX pentru ordinele sale de vânzare. **0 (implicit)** = valoare stocată per epocă (cea mai ieftină; taxa QX este fixă la 100 QU astăzi). **1** = obținută live de la QX înainte de fiecare vânzare. Un comutator orientat spre viitor, **într-o singură direcție**: dacă QX își face vreodată taxa variabilă per tick, acționarii comută pe 1 (fără re-implementare). **Permanent odată activat** — nu poate fi comutat înapoi. |
| 24 | **UPDATE_STOP_LOSS_TRIGGER** | 50M QU | **Adâncimea stop-loss** a Cloak — cât de mult trebuie să scadă o poziție deținută **sub costul mediu** înainte ca contractul să înceapă să o taie. **0 = dezactivat.** Altfel, un pas de 15 puncte: 15, 30, **45 (implicit)**, 60, 75, 90 (%). Mai mic = taie rapid pierderile mici (mai puțin răbdător); mai mare = dă unei scăderi mai mult spațiu să își revină înainte de tăiere. |
| 25 | **UPDATE_STOP_LOSS_SELL** | 50M QU | Cât dintr-o poziție pe pierdere vinde stop-loss-ul **de fiecare dată când se declanșează** — iese treptat, fără a arunca toată poziția dintr-odată. Un pas de 15 puncte: 15, 30, 45, **60 (implicit)**, 75, 90 (%). La cadența ~lunară a Cloak, o poziție muribundă sângerează în jos în câteva tăieri, în timp ce una care își revine păstrează restul. |
| 26 | **UPDATE_SWING_SIZING** | 50M QU | **Presetul de dimensionare a poziției** al Cloak — grupează dimensiunea primei cumpărături, dimensiunea adăugirii DCA-in și plafonul de cost per token într-o singură setare. Opțiuni: **0 (implicit)** = 1% / 0,25% / 5%; 1 = 1% / 0,25% / 7,5%; 2 = 2% / 0,50% / 10%; 3 = 3% / 0,25% / 15%. Mai mare = desfășoară capitalul mai repede și permite poziții mai mari pe un singur token. |
| 27 | **UPDATE_SWING_DIP** | 50M QU | **Pragul de cumpărare pe scădere** al Cloak — cât de mult trebuie să scadă media pe 1 săptămână sub media pe 3 luni înainte ca un pool să conteze drept „în scădere” și Cloak să cumpere. Un pas de 5 puncte: 5, 10, 15, 20, 25, **30 (implicit)** (%). Mai mare = mai răbdător (cumpără doar scăderi mai adânci). |
| 28 | **UPDATE_SWING_RALLY** | 50M QU | **Pragul de vânzare pe raliu** al Cloak — cât de mult trebuie să crească prețul peste costul mediu al poziției înainte ca Cloak să înceapă să vândă (ieșind treptat cu porțiunea de vânzare din Type 21). Un pas de 6 puncte: **6 (implicit)**, 12, 18, 24, 30 (%). Mai mic = ia profit mai devreme. |

**Ce se întâmplă cu taxa:** fiecare taxă de propunere finanțează **rezerva pentru taxe de execuție** a contractului — niciun QU nu este ars din supply și niciunul nu este păstrat drept capital de tranzacționare.

| Rezultat | Propunătorul primește înapoi | Către rezerva de taxe de execuție |
|---|---|---|
| **Trecută** | Nimic — taxa consumată integral | **100%** |
| **Eșuată / Veto** | **69%** (rambursat la finalul epocii) | 31% |

Cei 31% nerambursabili sunt direcționați către rezervă la depunere; la trecere, cei 69% reținuți sunt trimiși și ei acolo (100% în total), iar la eșec acei 69% sunt rambursați propunătorului.

---

## Donații

Oricine poate sprijini contractul cu o donație de **orice sumă în QU**. **100% dintr-o donație merge către rezerva pentru taxe de execuție** a contractului — bugetul on-chain care permite contractului să continue să ruleze la fiecare tick. Nu există o sumă fixă și nici un minim; întreaga sumă trimisă este păstrată.

Donațiile nu cumpără acțiuni în vault, nu câștigă randament și nu modifică valoarea vault-ului — sunt pur și simplu o modalitate prin care susținătorii pot ajuta la menținerea contractului finanțat și operațional.

---

## Note despre designul de securitate

- Toate modificările de guvernanță necesită o propunere on-chain cu taxă de depunere și vot cu supermajoritate.
- Veto-ul deponenților este un control independent asupra puterii acționarilor.
- Penalizările pentru ieșire timpurie îi protejează pe deponenții care respectă blocarea de diluarea valorii de către cei care ies.
- Fără chei de administrator. Fără suprascrieri ale proprietarului. Guvernanța este singura cale de upgrade. (Dev Fund-ul este pentru remedieri urgente.)
- Contractul este open source; codul sursă ar fi publicat dacă vreodată avansează spre implementare.

---

## Ce căutăm

Acest document este o previzualizare a designului. Contractul este scris și în testare. Am aprecia contribuția comunității cu privire la:

1. **Preseturile de împărțire a profitului** — cele trei opțiuni acoperă gama de preferințe, sau lipsește ceva?
2. **Mecanica vault-ului** — este blocarea de 26 de epoci + penalizarea de ieșire timpurie compromisul potrivit? Prea lungă? Prea scurtă?
3. **Pragul de veto al deponenților** — 500 din până la 5.000 de deponenți, cu un minim de QU blocat. Prea ușor de pus veto? Prea greu?
4. **Donații** — direcționarea a 100% dintr-o donație către rezerva pentru taxe de execuție a contractului (menținându-l finanțat pentru a rula) pare corectă, sau donațiile ar trebui să facă altceva?
5. **Taxele de guvernanță pentru pool-uri** — ADD_POOL costă 200M QU. Este aceasta o barieră adecvată pentru a preveni pool-urile spam?
6. **Orice alt comentariu sau îngrijorare**

---

## Status

- Contract: scris, în testare unitară — o lucrare în curs, neimplementată încă.
- Implementare: depinde de Guvernanța Computorilor — ajungerea pe rețea ar necesita aprobarea contractului de către Computori, ceea ce nu s-a întâmplat și nu este garantat.
- Cod sursă: ar fi publicat dacă proiectul avansează spre implementare.

*Întrebările și feedback-ul sunt binevenite — acesta este un protocol propus și ne dorim să facem designul corect.*
