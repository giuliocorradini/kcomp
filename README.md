# kcomp

Progetto di Linguaggi e Compilatori.

Giulio Corradini - a.a. 2023-2024

## Prerequisiti

È richiesto LLVM (versione minima 16), flex e bison.

Su macOS è necessario installare i pacchetti `bison` e `llvm` da brew. `flex` è già fornito dal sistema operativo (anche bison, ma è necessaria
una versione non inferiore alla 3.2).

Prima di compilare bisogna impostare come compilatore la versione di clang fornita col pacchetto `llvm` di brew. Per fare questo basta aggiungere al
proprio PATH la directory che contiene i binari di llvm.

```sh
brew install llvm bison
export PATH="/opt/homebrew/opt/llvm/bin:/opt/homebrew/opt/bison/bin:$PATH"
```

Viene fornito uno script `macos.sh` da attivare con il comando `source` della shell prima di invocare il Makefile.

```sh
source macos.sh
```

Su Debian verificare che nel PATH siano presenti gli eseguibili di LLVM e clang. In particolare il Makefile si aspetta di trovare
`clang++` e `llvm-config` nel PATH, e i comandi `llvm-as`, `llc` quando si eseguono i test.

È possibile impostare il nome del comando di clang modificando la variabile d'ambiente `CXX` presente all'inizio dei Makefile.

## Compilare kcomp

Per compilare kcomp chiamare il Makefile

```sh
make
```

che produrrà un eseguibile `kcomp`.

## Eseguire

Dato un sorgente Kaleidoscope in input, kcomp genera l'intermediate representation di LLVM in formato human readable e la restituisce su stderr.

```sh
./kcomp source.k
```

Per avere il risultato su un file basta redirezionare lo stream come segue:

```sh
./kcomp source.k 2> source.ll
```

## Test

La directory `test` contiene dei sorgenti in Kaleidoscope per testare le funzionalità del compilatore.

Per testare tutti i programmi lanciare

```sh
make
```

Si faccia quindi riferimento al README della directory test per sapere quali programmi sono disponibili, e quali funzionalità del compilatore
vengono testate.
