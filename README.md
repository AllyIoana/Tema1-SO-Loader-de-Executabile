# Organizare

Pentru rezolvarea temei trebuie să rulăm un program care nu are memorie și primeşte page fault. Astfel, sunt urmați pașii:
1. ne-am asigurat că semnalul primit este SIGSEGV;
2. am căutat segmentul din executabil unde a apărut page fault, încadrând adresa între un segment curent si următorul;
    -> dacă nu am găsit segmentul, atunci rulăm handler-ul default;
3. calculăm numărul de pagini din segmentul găsit şi aflăm pagina la care ne aflăm în segment;
4. verificăm dacă pagina găsită anterior a fost deja mapată şi pentru acest lucru vom crea un vector în care stocăm paginile prin care am trecut deja (pentru a afla unde se termină, după ultima valoare va fi mereu -1);
    -> acest vector va fi salvat în câmpul data din segmentul curent, pentru a-l transmite de fiecare dată;
    -> dacă pagina noastră este deja mapată, vom folosi handler-ul default; altfel o mapăm;
5. odată ce avem pagina mapată, începem să o completăm cu date din fişier şi tratăm separat posibilele erori; facem acest lucru până se termină segmentul din executabil sau până la finalul paginii;
6. la final setăm permisiunile memoriei din pagină la fel ca permisiunile segmentului.

Tema ne ajută să înţelegem mai bine noţiunea de handler, cum este implementat şi modul în care sunt folosite memoria virtuală şi cea fizică la rularea unui fişier de tip ELF. Nu cred că reuşeam să o implementez mai bine de atât.

# Implementare

Am rezolvat tema integral, fără funcţionalităţi extra. Mi s-a părut complicat să descopăr tot ce trebuie făcut şi să caut toate funcţiile folosite. Consider că erau necesare mai multe explicaţii sau resurse, am stat mai mult pe partea de research.

# Enunt

Să se implementeze sub forma unei biblioteci partajate/dinamice un loader de fișiere executabile în format ELF pentru Linux. Loader-ul va încărca fișierul executabil în memorie pagină cu pagină, folosind un mecanism de tipul demand paging - o pagină va fi încărcată doar în momentul în care este nevoie de ea. Pentru simplitate, loader-ul va rula doar executabile statice - care nu sunt link-ate cu biblioteci partajate/dinamice.

Pentru a rula un fișier executabil, loader-ul va executa următorii pași:

Își va inițializa structurile interne.
Va parsa fișierul binar - pentru a face asta aveți la dispozitie în scheletul temei un parser de fișiere ELF pe Linux. Găsiți mai multe detalii în secțiunea care descrie interfața parserului de executabile.
Va rula prima instrucțiune a executabilului (entry point-ul).
de-a lungul execuției, se va genera câte un page fault pentru fiecare acces la o pagină nemapată în memorie;
Va detecta fiecare acces la o pagină nemapată, și va verifica din ce segment al executabilului face parte.
dacă nu se găsește într-un segment, înseamnă că este un acces invalid la memorie – se rulează handler-ul default de page fault;
dacă page fault-ul este generat într-o pagină deja mapată, atunci se încearcă un acces la memorie nepermis (segmentul respectiv nu are permisiunile necesare) – la fel, se rulează handler-ul default de page fault;
dacă pagina se găsește într-un segment, și ea încă nu a fost încă mapată, atunci se mapează la adresa aferentă, cu permisiunile acelui segment;
Veți folosi funcția mmap (Linux) pentru a aloca memoria virtuală în cadrul procesului.
Pagina trebuie mapată fix la adresa indicată în cadrul segmentului.

# Interfața bibliotecii

Interfața de utilizare a bibliotecii loader-ului este prezentată în cadrul fișierul header loader.h. Acesta conține funcții de inițializare a loaderului(so_init_loader) și de executare a binarului (so_execute).

# Interfața parser

Pentru a ușura realizarea temei, vă punem la dispoziție în scheletul de cod un parser pentru ELF (Linux).
Interfața de parser pune la dispoziție două funcții:

- so_parse_exec - parsează executabilul și întoarce o structură de tipul so_exec_t. Aceasta poate fi folosită în continuare pentru a identifica segmentele executabilului și atributele lui.
- so_start_exec - pregătește environment-ul programului și începe execuția lui.

Începând din acest moment, se vor executa page fault-uri pentru fiecare acces de pagină nouă/nemapată.
Structurile folosit de interfață sunt:

- so_exec_t - descrie structura executabilului:
base_addr - indică adresa la care ar trebui încărcat executabilul
entry - adresa primei instrucțiuni executate de către executabil
segments_no - numărul de segmente din executabil
segments - un vector (de dimensiunea segments_no) care conține segmentele executabilului

- so_seg_t - descrie un segment din cadrul executabilului
vaddr - adresa la care ar trebui încărcat segmentul
file_size - dimensiunea în cadrul fișierului a segmentului
mem_size - dimensiunea ocupată de segment în memorie; dimensiunea segmentului în memorie poate să fie mai mare decât dimensiunea în fișier (spre exemplu pentru segmentul bss); în cazul acesta, diferența între spațiul din memorie și spațiul din fișier, trebuie zeroizată
offset - offsetul din cadrul fișierului la care începe segmentul
perm - o mască de biți reprezentând permisiunile pe care trebuie să le aibă paginile din segmentul curent
    PERM_R - permisiuni de citire
    PERM_W - permisiuni de srcriere
    PERM_X - permisiuni de execuție
data - un pointer opac pe care îl puteți folosi să atașați informații proprii legate de segmentul curent (spre exemplu, puteți stoca aici informații despre paginile din segment deja mapate)

# Implementare

- Implementarea page fault handler-ului se realizează prin intermediul unei rutine pentru tratarea semnalului SIGSEGV.
- Pentru a implementa logica de demand paging trebuie să interceptați page fault-urile produse în momentul unui acces nevalid la o zonă de memorie. La interceptarea page fault-urilor, tratați-o corespunzător, în funcție de segmentul din care face parte:
    dacă nu este într-un segment cunoscut, rulați handler-ul default;
    dacă este într-o pagină ne-mapată, mapați-o în memorie, apoi copiați din segmentul din fișier datele;
    dacă este într-o pagină deja mapată, rulați handler-ul default (întrucât este un acces ne-permis la memorie);
- Paginile din două segmente diferite nu se pot suprapune.
- Dimensiunea unui segment nu este aliniată la nivel de pagină; memoria care nu face parte dintr-un segment nu trebuie tratată în niciun fel – comportamentul unui acces în cadrul acelei zone este nedefinit.
- NU se vor depuncta resursele leak-uite datorită faptului că programul se termină înainte de a avea posibilitatea să fie eliberate:
    structurile rezultate în urma parsării executabilului (so_exec_t și so_seg_t);
    structurile alocate de voi și stocate în field-ul data al unui segment;
    paginile mapate în memorie în urma execuției on-demand.

# Precizări

- Pentru gestiunea memoriei virtuale folosiți funcțiile mmap, munmap și mprotect.
- Pentru interceptarea accesului nevalid la o zonă de memorie va trebui să interceptați semnalul SIGSEGV folosind apeluri din familia sigaction.
- Veți înregistra un handler în câmpul sa_sigaction al structurii struct sigaction.
- Pentru a determina adresa care a generat page fault-ul folosiți câmpul si_addr din cadrul structurii siginfo_t.
- În momentul în care este accesată o pagină nouă din cadrul unui segment, mapați pagina în care s-a generat page fault-ul, folosind MAP_FIXED, apoi copiați în pagină datele din executabil
- Tema se va rezolva folosind doar funcții POSIX. Se pot folosi de asemenea și funcțiile de citire/scriere cu formatare (scanf/printf), funcțiile de alocare/eliberare de memorie (malloc/free) și funcțiile de lucru cu șiruri de caractere (strcat, strdup etc.)
- Pentru partea de I/O se vor folosi doar funcții POSIX. De exemplu, funcțiile fopen, fread, fwrite, fclose nu trebuie folosite; în locul acestora folosiți open, read, write, close.
