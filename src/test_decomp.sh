#!/bin/bash
FILE_DIR=./multipla/*.pla
USAGE="usage: $0 [OUTPUT-FILE]"

if [ $# -ne 1 ] 
then
    echo "numero errato di argomenti"
    echo "$USAGE"
    exit -1
fi

declare -a testperc=("1%" "2%" "5%" "10%" "12%" "15%" "20%")

echo "NomeFile; AND; AND1; AND2; AND5; AND10; AND12; AND15; AND20" > "$1"

if [ $? -ne 0 ]
then
   echo "impossibile scrivere sul file $3".
   exit -1
fi

for file in $FILE_DIR; do
    for val in ${testperc[@]}; do 
        timeout "2m" "./main" "-d" "-g" "$val" "$file" >> "$1"
        if [ $? -ne 0 ]
        then
            echo "errore con il test $file".
        fi
        sleep 0.1
    done
done

unix2dos "$3"

exit 0